/* SPDX-License-Identifier: GPL-3.0-only
 * A project as bytes (project.h). Written for this project. */
#include "project.h"

#define PMAGIC 0x4A504247u                      /* "GBPJ" */
#define PVERSION 17u                              /* 2: conditions, ratchets, locks, the song; 3: the effects, the
                                                   * presets; 4: the swing; 5: 24 sound parameters (the LFO); 6: the
                                                   * master (DUST, DUCK, the DJ filter, the punch-ins' fade) and each
                                                   * track's filter; 7: each track's DIV and GATE, its chorus
                                                   * send; 8: the drums' tune, decay and mute per sound
                                                   * 9: STRUM and VLEAD; 10: the MIDI settings; 11: each pattern's own sound;
                                                   * 12: SLOOP's sound model, 48 values (1 to 11 still load: their
                                                   * notes, their sounds back to their engine's defaults);
                                                   * 13: the device's settings (HOME); 14: the delay's COLR and MIX,
                                                   * the reverb's DAMP, the chorus's rate; 15: the patterns' notes
                                                   * packed (ev_pack); 16 (4.0): each track's pattern as last
                                                   * saved; 17 (3.4.0): nothing new, but the drum kits 3 x louder: a
                                                   * project from before has its drums' level taken down as much */
#define PRJ_NTRK 8                                /* the tracks a file has room for (the sequencer has SEQ_NTRK) */
#define PRJ_NPAT 16                               /* the patterns a track of a file may have (SEQ_NPAT here) */
_Static_assert(SEQ_NTRK <= PRJ_NTRK && PRJ_NTRK <= GB_NTRACK, "a project's tracks");
static uint8_t bip(float x) { int v = (int)(x * 100.0f + 100.5f); return (uint8_t)(v < 0 ? 0 : v > 200 ? 200 : v); }   /* -1..1 -> 0..200 */
static float unbip(uint32_t v) { return v > 200u ? 0.0f : ((float)v - 100.0f) * 0.01f; }
/* 3.3.5: a project may be larger than any buffer (one project, up to 52 KB): the writer keeps the file's bytes
 * [lo, hi) only (a part, for a page of the flash), counts them all, and may keep their CRC; a buffer of cap bytes is
 * the part [0, cap), strict (beyond it: bad). The reader reads a buffer, or the flash through fetch (256 bytes at a
 * time into a cache its sub-readers share) */
typedef struct { uint8_t *p; uint32_t n, lo, hi; int bad, strict, crc_on; uint32_t crc; } wr;
typedef struct { const uint8_t *p; uint32_t n, len, base; int bad; project_fetch fetch; uint8_t *cache; uint32_t *cat; } rd;
void (*project_yield)(void);
#ifdef HOST
#define PY() do { tw_since_py = 0; if (project_yield) project_yield(); } while (0)
#else
#define PY() do { if (project_yield) project_yield(); } while (0)
#endif
static const uint32_t *crc_t;                             /* (3.4.3: the CRC a byte at a time, kept inverted while written) */
#ifdef HOST                                                /* (3.4.3) the twin: the FM-1's time for these bytes */
#include <stdlib.h>
void tw_charge(uint32_t ns);
static uint32_t tw_bytes; static int tw_bns = -1;
#include <execinfo.h>
#include <unistd.h>
static uint32_t tw_since_py; static int tw_bt;
static void tw_byte(void)
{
    if (tw_bns < 0) { const char *e = getenv("TWIN_BYTE_NS"); tw_bns = e ? atoi(e) : 0; }
    if (tw_bns && ++tw_bytes >= 64u) { tw_bytes = 0; tw_charge(64u * (uint32_t)tw_bns); }
    if (++tw_since_py == 3000u && getenv("TWIN_PY_TRACE") && tw_bt++ < 6) { void *f[12]; int n = backtrace(f, 12); write(1, "PY-GAP\n", 7); backtrace_symbols_fd(f, n, 1); }
}
#define TWB() tw_byte()
#else
#define TWB() ((void)0)
#endif
static void w8(wr *w, uint32_t v)
{
    TWB();
    if (w->crc_on) w->crc = crc_t[(w->crc ^ v) & 255u] ^ (w->crc >> 8);
    if (w->n >= w->lo && w->n < w->hi) w->p[w->n - w->lo] = (uint8_t)v;
    else if (w->strict && w->n >= w->hi) w->bad = 1;
    w->n++;
}
static void w16(wr *w, uint32_t v) { w8(w, v); w8(w, v >> 8); }
static void w32(wr *w, uint32_t v) { w16(w, v); w16(w, v >> 16); }
static void wf(wr *w, float f) { union { float f; uint32_t u; } x; x.f = f; w32(w, x.u); }
static uint32_t r8(rd *r)
{
    TWB();
    if (r->n + 1 > r->len) { r->bad = 1; return 0; }
    if (!r->fetch) return r->p[r->n++];
    uint32_t at = r->base + r->n++;
    if (at < *r->cat || at >= *r->cat + 256u) {           /* (the cache: the 256 bytes around it) */
        *r->cat = at & ~255u;
        if (r->fetch(*r->cat, r->cache, 256u)) { r->bad = 1; *r->cat = 0xFFFFFF00u; return 0; }
    }
    return r->cache[at - *r->cat];
}
static rd sub_of(const rd *r, uint32_t nb)              /* the next nb bytes as a reader of their own */
{
    rd s = *r; s.p = r->p ? r->p + r->n : 0; s.base = r->base + r->n; s.n = 0; s.len = nb; s.bad = 0;
    return s;
}
static uint32_t r16(rd *r) { uint32_t a = r8(r); return a | r8(r) << 8; }
static uint32_t r32(rd *r) { uint32_t a = r16(r); return a | r16(r) << 16; }
static float rf(rd *r) { union { float f; uint32_t u; } x; x.u = r32(r); return x.f == x.f ? x.f : 0.0f; }   /* no NaN in */

/* 15: a pattern's notes packed (3.3.5: "8 loops of 8 bars, every track full, each its own sound: does it fit?" At 8
 * bytes a note, a dense project did not fit its place in the flash). The notes in their order (by time); each starts
 * with a byte: Z (bit 7) at the same time as the note before (a chord, drum hits on one step), V (6) its velocity
 * follows (else the one before's), L (5) its length follows (else the one before's), X (4) its condition and
 * ratchet follow (else ALWAYS, x1), then the note from the one before, -7..+7 (0..14; 15: the note follows). Then,
 * when not Z, the time since the note before; then what V, L, X say. Times and lengths as a number of 7-bit bytes,
 * the grid's own counted in steps (a note on a step: its time in one byte). A pattern's notes are kept as they were
 * (8 bytes each: mode 0) when packing them would not be smaller. */
static void wv(wr *w, uint32_t v) { while (v >= 0x80u) { w8(w, (v & 0x7Fu) | 0x80u); v >>= 7; } w8(w, v); }
static uint32_t rv(rd *r) { uint32_t v = 0; for (int k = 0; k < 21; k += 7) { uint32_t b = r8(r); v |= (b & 0x7Fu) << k; if (!(b & 0x80u)) return v; } r->bad = 1; return 0; }
static uint32_t gcode(uint32_t v)                       /* a time or length: in steps when on the grid */
{
    if (v % SEQ_TPS == 0u) return v / SEQ_TPS << 2;
    if ((v + 2u) % SEQ_TPS == 0u) return (v + 2u) / SEQ_TPS << 2 | 1u;    /* (a step's note: its length less 2 ticks) */
    return v << 2 | 2u;
}
static uint32_t gdecode(uint32_t c)
{
    uint32_t v = c >> 2;
    return (c & 3u) == 0u ? v * SEQ_TPS : (c & 3u) == 1u ? v * SEQ_TPS - 2u : v;
}
static void ev_pack(wr *w, const seq_pat *p)
{
    uint32_t pt = 0, pv = 100, pl = SEQ_TPS - 2u, pn = 60;
    for (int e = 0; e < p->n; e++) {
        const seq_ev *v = &p->ev[e];
        uint32_t dt = (uint32_t)v->tick - pt, b = 0;
        int dn = (int)v->note - (int)pn;
        if (e && dt == 0u) b |= 0x80u;
        if (v->vel != pv) b |= 0x40u;
        if (v->len != pl) b |= 0x20u;
        if (v->cond != SEQ_ALWAYS || v->ratchet != 1u) b |= 0x10u;
        b |= dn >= -7 && dn <= 7 ? (uint32_t)(dn + 7) : 15u;
        w8(w, b);
        if ((b & 15u) == 15u) w8(w, v->note);
        if (!(b & 0x80u)) wv(w, gcode(dt));
        if (b & 0x40u) w8(w, v->vel);
        if (b & 0x20u) wv(w, gcode(v->len));
        if (b & 0x10u) { w8(w, v->cond); w8(w, v->ratchet); }
        pt = v->tick; pv = v->vel; pl = v->len; pn = v->note;
        if ((e & 31) == 31) PY();
    }
}
static int ev_unpack(rd *r, seq_pat *p, uint32_t n)      /* 0: n notes read, each in range (p 0: only checked) */
{
    uint32_t pt = 0, pv = 100, pl = SEQ_TPS - 2u, pn = 60;
    for (uint32_t e = 0; e < n; e++) {
        uint32_t b = r8(r), note = (b & 15u) == 15u ? r8(r) : (uint32_t)((int)pn + (int)(b & 15u) - 7);
        uint32_t t = pt + ((b & 0x80u) && e ? 0u : gdecode(rv(r)));
        uint32_t vel = b & 0x40u ? r8(r) : pv, len = b & 0x20u ? gdecode(rv(r)) : pl;
        uint32_t cond = SEQ_ALWAYS, rat = 1;
        if (b & 0x10u) { cond = r8(r); rat = r8(r); }
        if (r->bad || note > 127u || t > 0xFFFFu || len > 0xFFFFu) return -1;
        pt = t; pv = vel; pl = len; pn = note;
        if ((e & 31) == 31) PY();
        if (!p) continue;                             /* (the dry run: read, checked, not kept) */
        seq_ev *v = &p->ev[e];
        v->tick = (uint16_t)t; v->len = (uint16_t)len; v->note = (uint8_t)note; v->vel = (uint8_t)(vel & 127u); v->cond = (uint8_t)cond;
        uint32_t rn = rat & 7u; v->ratchet = (uint8_t)((rn < 1 ? 1 : rn > 4 ? 4 : rn) | (rat & 0x70u));   /* (3.4.2: SEQ_FREE kept) */
    }
    return 0;
}

static void save_body(wr *wp, const seq_t *q, const gb_sound *s, const project_extra *x)
{
    wr w = *wp;
    w32(&w, PMAGIC); w8(&w, PVERSION); wf(&w, q->bpm);
    for (int t = 0; t < PRJ_NTRK; t++) {
        const gb_track *k = &s->trk[t];
        const gb_patch *p = &k->patch;
        w8(&w, p->engine); w8(&w, p->poly); w8(&w, p->unison); w8(&w, p->protect);
        for (int i = 0; i < GB_NPARAM; i++) wf(&w, p->p[i]);
        wf(&w, k->level.tgt); wf(&w, k->pan.tgt); wf(&w, k->send_dly.tgt); wf(&w, k->send_rev.tgt);
        if (t < SEQ_NTRK) { w8(&w, q->t[t].mute); w8(&w, q->t[t].quant); w8(&w, q->t[t].cur); }
        else { w8(&w, 0); w8(&w, 0); w8(&w, 0); }
    }
    uint32_t count = 0;
    for (int t = 0; t < SEQ_NTRK; t++) for (int i = 0; i < SEQ_NPAT; i++) if (q->t[t].pat[i].n || q->t[t].pat[i].nlock || q->t[t].pat[i].steps != 16) count++;
    w16(&w, count);
    for (int t = 0; t < SEQ_NTRK; t++) for (int i = 0; i < SEQ_NPAT; i++) {
        const seq_pat *p = &q->t[t].pat[i];
        if (!p->n && !p->nlock && p->steps == 16) continue;
        PY();
        w8(&w, (uint32_t)t); w8(&w, (uint32_t)i); w16(&w, p->steps); w16(&w, p->n); w8(&w, p->nlock);
        wr cnt = { 0, 0, 0, 0, 0, 0, 0, 0 };               /* 15: packed, counted first; kept as they are when not smaller */
        ev_pack(&cnt, p);
        if (cnt.n < 8u * p->n) { w8(&w, 1u); w16(&w, cnt.n); ev_pack(&w, p); }
        else {
            w8(&w, 0u); w16(&w, 8u * p->n);
            for (int e = 0; e < p->n; e++) {
                w16(&w, p->ev[e].tick); w16(&w, p->ev[e].len); w8(&w, p->ev[e].note); w8(&w, p->ev[e].vel);
                w8(&w, p->ev[e].cond); w8(&w, p->ev[e].ratchet);
                if ((e & 31) == 31) PY();
            }
        }
        for (int l = 0; l < p->nlock; l++) { w8(&w, p->lk[l].step); w8(&w, p->lk[l].param); w8(&w, p->lk[l].value); }
    }
    w16(&w, q->song_n); w16(&w, q->song_len);
    for (int i = 0; i < q->song_n; i++) {
        w16(&w, q->song[i].bar); w8(&w, q->song[i].mute);
        for (int t = 0; t < PRJ_NTRK; t++) w8(&w, t < SEQ_NTRK ? q->song[i].pat[t] : 0u);
    }
    w8(&w, x ? x->dly_div : 0xFFu); wf(&w, s->dly_fb); wf(&w, s->rev_fb);
    for (int t = 0; t < PRJ_NTRK; t++) w8(&w, x && t < SEQ_NTRK ? x->preset[t] : 0xFFu);
    w8(&w, q->swing);
    for (int t = 0; t < PRJ_NTRK; t++) w8(&w, t < SEQ_NTRK ? q->t[t].swing : 0u);
    for (int t = 0; t < PRJ_NTRK; t++) for (int i = 0; i < 5; i++) w8(&w, x && t < SEQ_NTRK ? x->harm[t][i] : (i == 4 ? 64u : 0u));
    /* 6 */
    w8(&w, (uint32_t)(s->dust * 100.0f + 0.5f)); w8(&w, (uint32_t)(s->duck * 100.0f + 0.5f)); w8(&w, bip(s->mfilt.tgt));
    w8(&w, x ? x->fade : 0u);
    for (int t = 0; t < PRJ_NTRK; t++) w8(&w, bip(s->trk[t].filt.tgt));
    /* 7 */
    for (int t = 0; t < PRJ_NTRK; t++) {
        w8(&w, t < SEQ_NTRK ? q->t[t].div : SEQ_DIV_16); w8(&w, t < SEQ_NTRK ? q->t[t].gate : 100u); w8(&w, (uint32_t)(s->trk[t].send_cho.tgt * 100.0f + 0.5f));
        for (int i = 0; i < 8; i++) w8(&w, x && t < SEQ_NTRK ? x->arp[t][i] : 0u);
    }
    w8(&w, (uint32_t)(s->cho_depth * 100.0f + 0.5f));
    /* 8 */
    for (int t = 0; t < PRJ_NTRK; t++) {
        const gb_patch *p = &s->trk[t].patch;
        for (int i = 0; i < 16; i++) { w8(&w, (uint8_t)p->dtune[i]); w8(&w, (uint8_t)p->ddec[i]); }
        w16(&w, p->dmute);
    }
    /* 9 */
    for (int t = 0; t < PRJ_NTRK; t++) { w8(&w, x && t < SEQ_NTRK ? (uint8_t)x->strum[t] : 0u); w8(&w, x && t < SEQ_NTRK ? x->vlead[t] : 0u); }
    /* 10 */
    for (int i = 0; i < 8; i++) w8(&w, x ? x->midi[i] : 0u);
    /* 11: each pattern's own sound, where it is not the current pattern's (that one: the track's, above) */
    for (int t = 0; t < SEQ_NTRK; t++) {
        const psounds *ps = x ? x->ps : 0;
        int cur = q->t[t].cur;
        uint32_t mask = 0;
        if (ps) for (int i = 0; i < SEQ_NPAT; i++) if (i != cur && !ps_same(ps, t, i, cur)) mask |= 1u << i;
        w16(&w, mask);
        for (int i = 0; i < SEQ_NPAT; i++) if ((mask >> i) & 1u) {
            uint8_t b[PS_BYTES + PS_DRUM_BYTES]; int n = ps_write(ps, t, i, b);
            for (int k = 0; k < n; k++) w8(&w, b[k]);
            PY();
        }
    }
    /* 13: the device's settings (HOME), kept with the projects (taken at power-on only) */
    for (int i = 0; i < PRJ_HOME; i++) w8(&w, x ? x->home[i] : 0xFFu);
    for (int t = 0; t < PRJ_NTRK; t++) w16(&w, x && t < SEQ_NTRK ? x->preset16[t] : 0xFFFFu);   /* (each track's sound: 15 bits, bit 15 edited) */
    for (int i = 0; i < 8; i++) w8(&w, x ? x->tex[i] : 0xFFu);
    for (int i = 0; i < 4; i++) w8(&w, x ? x->fxv[i] : 0xFFu);   /* 14 */
    /* 16 (4.0, SLOOP 2.8's sections): each track's current pattern as last saved, and its sound as last saved (the
     * header holds the sound as it is now): a pattern played again comes back as it was saved */
    for (int t = 0; t < SEQ_NTRK; t++) {
        int cur = q->t[t].cur;
        const seq_pat *k = x && x->keep ? &x->keep[t] : 0, *c = &q->t[t].pat[cur];
        int diff = k && (k->n != c->n || k->steps != c->steps || k->nlock != c->nlock ||
                         memcmp(k->ev, c->ev, sizeof k->ev[0] * k->n) || memcmp(k->lk, c->lk, sizeof k->lk[0] * k->nlock));
        uint32_t fl = (x && x->ps ? 1u : 0u) | (diff ? 2u : 0u), need = 1u, rest = 0;
        if (fl & 1u) need += PS_BYTES + (t == PS_DRUM ? PS_DRUM_BYTES : 0u);
        for (int u = t + 1; u < SEQ_NTRK; u++) rest += 1u + (x && x->ps ? PS_BYTES + (u == PS_DRUM ? PS_DRUM_BYTES : 0u) : 0u);
        if (diff && w.n + need + 5u + 8u * k->n + 3u * k->nlock + rest > STORE_PMAX) fl &= ~2u;   /* (the largest project: it must fit
                                                         its place; the pattern as it is counts as saved then) */
        w8(&w, fl); PY();
        if (fl & 1u) { uint8_t b[PS_BYTES + PS_DRUM_BYTES]; int n = ps_write(x->ps, t, cur, b); for (int i = 0; i < n; i++) w8(&w, b[i]); }
        if (fl & 2u) {
            w16(&w, k->steps); w16(&w, k->n); w8(&w, k->nlock);
            for (int e = 0; e < k->n; e++) { w16(&w, k->ev[e].tick); w16(&w, k->ev[e].len); w8(&w, k->ev[e].note); w8(&w, k->ev[e].vel); w8(&w, k->ev[e].cond); w8(&w, k->ev[e].ratchet); if ((e & 31) == 31) PY(); }
            for (int l = 0; l < k->nlock; l++) { w8(&w, k->lk[l].step); w8(&w, k->lk[l].param); w8(&w, k->lk[l].value); }
        }
    }
    *wp = w;
}
int project_save(const seq_t *q, const gb_sound *s, const project_extra *x, uint8_t *buf, uint32_t cap)
{
    wr w = { buf, 0, 0, cap, 0, 1, 0, 0 };
    save_body(&w, q, s, x);
    return w.bad ? -1 : (int)w.n;
}
int project_part(const seq_t *q, const gb_sound *s, const project_extra *x, uint8_t *buf, uint32_t lo, uint32_t n, uint32_t *crc)
{
    wr w = { buf, 0, lo, lo + n, 0, 0, crc != 0, 0xFFFFFFFFu };
    crc_t = crc32_table();
    save_body(&w, q, s, x);
    if (crc) *crc = ~w.crc;
    return (int)w.n;
}

static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
int project_check(const uint8_t *b, uint32_t len)
{
    return len >= 16 && (b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24) == PMAGIC && b[4] >= 1u && b[4] <= PVERSION ? 0 : -1;
}
static int load_body(seq_t *q, gb_sound *s, project_extra *x, rd r, int check_only)
{
    uint32_t len = r.len;
    uint32_t ver;
    if (r32(&r) != PMAGIC || (ver = r8(&r)) < 1u || ver > PVERSION) return -1;
    const uint32_t evb = ver >= 2 ? 8u : 6u;
    /* read it all into place only once it has checked out: first a dry run over the bytes */
    rd chk = r;
    rf(&chk);
    const int np = ver >= 12 ? GB_NPARAM : ver >= 5 ? 24 : 16;
    for (int t = 0; t < PRJ_NTRK; t++) { chk.n += 4 + 4 * (uint32_t)np + 16 + 3; }
    uint32_t count = r16(&chk);
    for (uint32_t c = 0; c < count && !chk.bad; c++) {
        uint32_t t = r8(&chk), i = r8(&chk), steps = r16(&chk), n = r16(&chk), nl = ver >= 2 ? r8(&chk) : 0u;
        if (t >= PRJ_NTRK || i >= PRJ_NPAT || steps < 1 || steps > SEQ_MAXSTEPS || n > SEQ_PATEV || nl > SEQ_LOCKS) return -1;
        if (ver >= 15) {
            uint32_t md = r8(&chk), nb = r16(&chk);
            if (md > 1u || (md == 0u && nb != 8u * n) || chk.bad || chk.n + nb > len) return -1;
            if (md == 1u) { PY(); rd sub = sub_of(&chk, nb); if (ev_unpack(&sub, 0, n) || sub.n != nb) return -1; }
            chk.n += nb + nl * 3u;
        }
        else chk.n += n * evb + nl * 3u;
    }
    if (ver >= 2) {
        uint32_t sn = r16(&chk); r16(&chk);
        if (sn > SEQ_SONG) return -1;
        chk.n += sn * (3u + PRJ_NTRK);
    }
    if (ver >= 3) chk.n += 1u + 8u + PRJ_NTRK;
    if (ver >= 4) chk.n += 1u + PRJ_NTRK;
    if (ver >= 5) chk.n += 5u * PRJ_NTRK;
    if (ver >= 6) chk.n += 4u + PRJ_NTRK;
    if (ver >= 7) chk.n += 11u * PRJ_NTRK + 1u;
    if (ver >= 8) chk.n += 34u * PRJ_NTRK;
    if (ver >= 9) chk.n += 2u * PRJ_NTRK;
    if (ver >= 10) chk.n += 8u;
    if (ver >= 11) for (int t = 0; t < SEQ_NTRK && !chk.bad; t++) {
        uint32_t m = r16(&chk);
        for (int i = 0; i < 16; i++) if ((m >> i) & 1u) chk.n += (ver >= 13 ? PS_BYTES : ver >= 12 ? PS_BYTES12 : 31u) + (t == PS_DRUM ? PS_DRUM_BYTES : 0u);
    }
    if (ver >= 13) chk.n += PRJ_HOME + 2u * PRJ_NTRK + 8u;
    if (ver >= 14) chk.n += 4u;
    if (ver >= 16) for (int t = 0; t < SEQ_NTRK && !chk.bad; t++) {
        uint32_t fl = r8(&chk);
        if (fl & 1u) chk.n += PS_BYTES + (t == PS_DRUM ? PS_DRUM_BYTES : 0u);
        if (fl & 2u) {
            uint32_t steps = r16(&chk), n = r16(&chk), nl = r8(&chk);
            if (steps < 1 || steps > SEQ_MAXSTEPS || n > SEQ_PATEV || nl > SEQ_LOCKS) return -1;
            chk.n += 8u * n + 3u * nl;
        }
    }
    if (chk.bad || chk.n > len) return -1;
    if (check_only) return 0;
    /* the real load */
    seq_play(q, 0);
    seq_set_bpm(q, rf(&r));
    for (int t = 0; t < PRJ_NTRK; t++) {
        gb_track *k = &s->trk[t];
        gb_patch *p = &k->patch;
        uint32_t eng = r8(&r);
        p->engine = (uint8_t)(eng < (uint32_t)gb_nengines ? eng : 0);
        p->poly = (uint8_t)r8(&r); p->unison = (uint8_t)r8(&r); p->protect = (uint8_t)r8(&r);
        if (!p->poly) p->poly = 1;
        if (!p->unison) p->unison = 1;
        for (int i = 0; i < np; i++) { float f = clamp01(rf(&r)); if (i < GB_NPARAM) p->p[i] = f; }
        if (ver < 12) gb_patch_default(p, p->engine);   /* (another sound model: its values mean nothing here) */
        { float lv = clamp01(rf(&r)); if (ver < 17u && p->engine == ENG_DRUM) lv *= (1.0f / 3.0f); sm_set(&k->level, lv); }   /* (3.4.0: the kits 3 x louder; a project from before as it sounded) */
        sm_set(&k->pan, clamp01(rf(&r)));
        sm_set(&k->send_dly, clamp01(rf(&r))); sm_set(&k->send_rev, clamp01(rf(&r)));
        sm_set(&k->filt, 0.0f);
        uint32_t mu = r8(&r), qu = r8(&r), cu = r8(&r);
        if (t >= SEQ_NTRK) continue;                  /* (a track this firmware does not have) */
        q->t[t].mute = (uint8_t)(mu != 0);
        q->t[t].quant = (uint8_t)(qu > 100 ? 100 : qu);
        q->t[t].cur = (uint8_t)(cu < SEQ_NPAT ? cu : 0); q->t[t].next = SEQ_NONE;
        for (int i = 0; i < SEQ_NPAT; i++) { q->t[t].pat[i].n = 0; q->t[t].pat[i].nlock = 0; q->t[t].pat[i].steps = 16; }
    }
    count = r16(&r);
    for (uint32_t c = 0; c < count; c++) {
        uint32_t t = r8(&r), i = r8(&r);
        if (t >= SEQ_NTRK || i >= SEQ_NPAT) {         /* a track or a pattern this firmware does not have: passed over */
            r16(&r); uint32_t n = r16(&r), nl = ver >= 2 ? r8(&r) : 0u;
            if (ver >= 15) { r8(&r); r.n += r16(&r) + nl * 3u; }
            else r.n += n * evb + nl * 3u;
            continue;
        }
        seq_pat *p = &q->t[t].pat[i];
        PY();
        p->steps = (uint16_t)r16(&r); p->n = (uint16_t)r16(&r); p->nlock = (uint8_t)(ver >= 2 ? r8(&r) : 0u);
        uint32_t md = ver >= 15 ? r8(&r) : 0u, nb = ver >= 15 ? r16(&r) : 0u;
        if (md == 1u) {                               /* 15: packed (their bytes: nb, checked above) */
            rd sub = sub_of(&r, nb);
            if (ev_unpack(&sub, p, p->n) || sub.n != nb) { p->n = 0; p->nlock = 0; return -1; }
            r.n += nb;
        } else for (int e = 0; e < p->n; e++) {
            p->ev[e].tick = (uint16_t)r16(&r); p->ev[e].len = (uint16_t)r16(&r);
            p->ev[e].note = (uint8_t)(r8(&r) & 127u); p->ev[e].vel = (uint8_t)(r8(&r) & 127u);
            p->ev[e].cond = (uint8_t)(ver >= 2 ? r8(&r) : SEQ_ALWAYS);
            uint32_t ra = ver >= 2 ? r8(&r) : 1u, rn = ra & 7u; p->ev[e].ratchet = (uint8_t)((rn < 1 ? 1 : rn > 4 ? 4 : rn) | (ra & 0x70u));   /* (+ ACCENT, SLIDE) */
            if ((e & 31) == 31) PY();
        }
        for (int l = 0; l < p->nlock; l++) { p->lk[l].step = (uint8_t)r8(&r); p->lk[l].param = (uint8_t)(r8(&r) & 63u); p->lk[l].value = (uint8_t)r8(&r); }
        if (ver < 12) p->nlock = 0;                   /* (locks of another sound model) */
        /* kept sorted, as the sequencer needs (a foreign file might not be) */
        for (int a = 1; a < p->n; a++) { seq_ev ea = p->ev[a]; int b = a; while (b > 0 && p->ev[b - 1].tick > ea.tick) { p->ev[b] = p->ev[b - 1]; b--; } p->ev[b] = ea; }
    }
    q->song_n = 0; q->song_len = 0; q->song_play = 0; q->song_rec = 0;
    if (ver >= 2) {
        uint32_t sn = r16(&r), sl = r16(&r);
        for (uint32_t i = 0; i < sn; i++) {
            seq_song_ev *e = &q->song[i];
            e->bar = (uint16_t)r16(&r); e->mute = (uint8_t)r8(&r);
            for (int t = 0; t < PRJ_NTRK; t++) { uint32_t pt = r8(&r); if (t < SEQ_NTRK) e->pat[t] = (uint8_t)(pt < SEQ_NPAT ? pt : 0); }
        }
        q->song_n = (uint16_t)sn; q->song_len = (uint16_t)sl;
    }
    if (ver >= 3) {
        uint32_t dd = r8(&r);
        float fb = rf(&r), rv = rf(&r);
        s->dly_fb = fb < 0.0f ? 0.0f : fb > 0.95f ? 0.95f : fb;
        s->rev_fb = rv < 0.0f ? 0.0f : rv > 0.97f ? 0.97f : rv;
        if (x) x->dly_div = (uint8_t)dd;
        for (int t = 0; t < PRJ_NTRK; t++) { uint32_t pr = r8(&r); if (x && t < SEQ_NTRK) x->preset[t] = (uint8_t)pr; }
    }
    q->swing = 0;
    for (int t = 0; t < SEQ_NTRK; t++) q->t[t].swing = 0;
    if (ver >= 4) {
        uint32_t sw = r8(&r); q->swing = (uint8_t)(sw > 100 ? 100 : sw);
        for (int t = 0; t < PRJ_NTRK; t++) { sw = r8(&r); if (t < SEQ_NTRK) q->t[t].swing = (uint8_t)(sw > 100 ? 100 : sw); }
    }
    for (int t = 0; t < PRJ_NTRK; t++) for (int i = 0; i < 5; i++) {
        uint32_t h = ver >= 5 ? r8(&r) : (i == 4 ? 64u : 0u);
        if (t >= SEQ_NTRK) continue;
        static const uint8_t MAXV[5] = { 11, 15, 2, 5, 112 };   /* (harmony.h's ranges; transpose +-24 around 64) */
        if (i == 4 && (h < 40 || h > 88)) h = 64;
        if (i < 4 && h > MAXV[i]) h = 0;
        if (x) x->harm[t][i] = (uint8_t)h;
    }
    s->dust = 0.0f; s->duck = 0.0f; sm_set(&s->mfilt, 0.0f);
    if (x) x->fade = 0;
    if (ver >= 6) {
        uint32_t du = r8(&r), dk = r8(&r), mf = r8(&r), fd = r8(&r);
        s->dust = (float)(du > 100 ? 100 : du) * 0.01f; s->duck = (float)(dk > 100 ? 100 : dk) * 0.01f;
        sm_set(&s->mfilt, unbip(mf));
        if (x) x->fade = (uint8_t)(fd > 15 ? 15 : fd);
        for (int t = 0; t < PRJ_NTRK; t++) sm_set(&s->trk[t].filt, unbip(r8(&r)));
    }
    for (int t = 0; t < SEQ_NTRK; t++) { q->t[t].div = SEQ_DIV_16; q->t[t].gate = 100; }
    for (int t = 0; t < PRJ_NTRK; t++) sm_set(&s->trk[t].send_cho, 0.0f);
    if (ver >= 7) for (int t = 0; t < PRJ_NTRK; t++) {
        uint32_t dv = r8(&r), gt = r8(&r), ch = r8(&r);
        if (t < SEQ_NTRK) { q->t[t].div = (uint8_t)(dv < SEQ_NDIV ? dv : SEQ_DIV_16); q->t[t].gate = (uint8_t)(gt < 10 ? 10 : gt > 200 ? 200 : gt); }
        sm_set(&s->trk[t].send_cho, (float)(ch > 100 ? 100 : ch) * 0.01f);
        for (int i = 0; i < 8; i++) { uint32_t a = r8(&r); if (x && t < SEQ_NTRK) x->arp[t][i] = (uint8_t)a; }
    }
    if (ver >= 7) { uint32_t cd = r8(&r); s->cho_depth = (float)(cd > 100 ? 100 : cd) * 0.01f; }
    else if (x) memset(x->arp, 0, sizeof x->arp);
    for (int t = 0; t < PRJ_NTRK; t++) {
        gb_patch *p = &s->trk[t].patch;
        memset(p->dtune, 0, sizeof p->dtune); memset(p->ddec, 0, sizeof p->ddec); p->dmute = 0;
        if (ver < 8) continue;
        for (int i = 0; i < 16; i++) {
            int a = (int8_t)r8(&r), b = (int8_t)r8(&r);
            p->dtune[i] = (int8_t)(a < -8 ? -8 : a > 7 ? 7 : a); p->ddec[i] = gb_dpack(GB_DDEC(b), gb_dlev((int8_t)b));   /* (3.4.2: its level above) */
        }
        p->dmute = (uint16_t)r16(&r);
    }
    if (x) { memset(x->strum, 0, sizeof x->strum); memset(x->vlead, 0, sizeof x->vlead); }
    if (ver >= 9) for (int t = 0; t < PRJ_NTRK; t++) {
        uint32_t a = r8(&r), b = r8(&r);
        if (x && t < SEQ_NTRK) { { int sa = (int8_t)a; x->strum[t] = (int8_t)(sa < -100 ? -100 : sa > 100 ? 100 : sa); } x->vlead[t] = (uint8_t)(b != 0); }
    }
    if (x) memset(x->midi, 0xFF, sizeof x->midi);   /* (0xFF: none kept, an older project) */
    if (ver >= 10) for (int i = 0; i < 8; i++) { uint32_t a = r8(&r); if (x) x->midi[i] = (uint8_t)a; }
    /* the patterns' own sounds kept apart (11): read into x->ps, ps_mask says which (the others: the track's, the
     * firmware fills them once it has the track's sound whole: its gain, its preset) */
    if (x) memset(x->ps_mask, 0, sizeof x->ps_mask);
    if (ver >= 11) for (int t = 0; t < SEQ_NTRK; t++) {
        uint32_t m = r16(&r);
        for (int i = 0; i < 16; i++) if ((m >> i) & 1u) {
            uint32_t n = (ver >= 13 ? PS_BYTES : ver >= 12 ? PS_BYTES12 : 31u) + (t == PS_DRUM ? PS_DRUM_BYTES : 0u);
            PY();
            if (ver >= 12 && x && x->ps && i < SEQ_NPAT && r.n + n <= r.len) {
                uint8_t b[PS_BYTES + PS_DRUM_BYTES]; rd sb = sub_of(&r, n);
                for (uint32_t k = 0; k < n && k < sizeof b; k++) b[k] = (uint8_t)r8(&sb);
                if (n <= sizeof b && ps_read(x->ps, t, i, b, n, ver < 13)) x->ps_mask[t] |= (uint16_t)(1u << i);
            }
            r.n += n;
        }
    }
    if (x) memset(x->home, 0xFF, sizeof x->home);   /* (0xFF: none kept) */
    if (ver >= 13) for (int i = 0; i < PRJ_HOME; i++) { uint32_t a = r8(&r); if (x) x->home[i] = (uint8_t)a; }
    for (int t = 0; t < PRJ_NTRK; t++) { uint32_t v = ver >= 13 ? r16(&r) : 0xFFFFu; if (x && t < SEQ_NTRK) x->preset16[t] = (uint16_t)v; }
    for (int i = 0; i < 8; i++) { uint32_t v = ver >= 13 ? r8(&r) : 0xFFu; if (x) x->tex[i] = (uint8_t)v; }
    for (int i = 0; i < 4; i++) { uint32_t v = ver >= 14 ? r8(&r) : 0xFFu; if (x) x->fxv[i] = (uint8_t)v; }
    if (x) { memset(x->keep_got, 0, sizeof x->keep_got); memset(x->pscur_got, 0, sizeof x->pscur_got); }
    if (ver >= 16) for (int t = 0; t < SEQ_NTRK; t++) {   /* 16: the current patterns and their sounds as last saved */
        uint32_t fl = r8(&r), cur = q->t[t].cur;
        PY();
        if (fl & 1u) {
            uint32_t n = PS_BYTES + (t == PS_DRUM ? PS_DRUM_BYTES : 0u);
            if (x && x->ps && r.n + n <= r.len) {
                uint8_t b[PS_BYTES + PS_DRUM_BYTES]; rd sb = sub_of(&r, n);
                for (uint32_t k = 0; k < n; k++) b[k] = (uint8_t)r8(&sb);
                if (ps_read(x->ps, t, (int)cur, b, n, 0)) x->pscur_got[t] = 1;
            }
            r.n += n;
        }
        if (fl & 2u) {
            seq_pat tmp, *k = x && x->keep ? &x->keep[t] : &tmp;
            k->steps = (uint16_t)r16(&r); k->n = (uint16_t)r16(&r); k->nlock = (uint8_t)r8(&r);
            for (int e = 0; e < k->n; e++) {
                k->ev[e].tick = (uint16_t)r16(&r); k->ev[e].len = (uint16_t)r16(&r);
                k->ev[e].note = (uint8_t)(r8(&r) & 127u); k->ev[e].vel = (uint8_t)(r8(&r) & 127u); k->ev[e].cond = (uint8_t)r8(&r);
                uint32_t ra = r8(&r), rn = ra & 7u; k->ev[e].ratchet = (uint8_t)((rn < 1 ? 1 : rn > 4 ? 4 : rn) | (ra & 0x70u));
                if ((e & 31) == 31) PY();
            }
            for (int l = 0; l < k->nlock; l++) { k->lk[l].step = (uint8_t)r8(&r); k->lk[l].param = (uint8_t)(r8(&r) & 63u); k->lk[l].value = (uint8_t)r8(&r); }
            for (int a = 1; a < k->n; a++) { seq_ev ea = k->ev[a]; int b = a; while (b > 0 && k->ev[b - 1].tick > ea.tick) { k->ev[b] = k->ev[b - 1]; b--; } k->ev[b] = ea; }
            if (x && x->keep) x->keep_got[t] = 1;
        }
    }
    if (ver < 3 && x) {                               /* older: the effects as they were, no preset known */
        x->dly_div = 0xFF;
        for (int t = 0; t < SEQ_NTRK; t++) x->preset[t] = 0xFF;
    }
    if (ver < 12 && x) for (int t = 0; t < SEQ_NTRK; t++) x->preset[t] = 0xFF;   /* (another list of sounds) */
    if (ver < 13 && x) for (int t = 0; t < SEQ_NTRK; t++)   /* (the 7 bits of format 3, 0x80 edited) */
        x->preset16[t] = (uint16_t)(x->preset[t] == 0xFF ? 0xFFFFu : (x->preset[t] & 0x7Fu) | (x->preset[t] & 0x80u ? 0x8000u : 0u));
    if (q->hist) q->hist->n_undo = q->hist->n_redo = 0;
    return 0;
}
int project_load(seq_t *q, gb_sound *s, project_extra *x, const uint8_t *buf, uint32_t len)
{
    rd r = { buf, 0, len, 0, 0, 0, 0, 0 };
    return load_body(q, s, x, r, 0);
}
int project_load_from(seq_t *q, gb_sound *s, project_extra *x, project_fetch fetch, uint32_t len, int check_only)
{
    uint8_t cache[256] __attribute__((aligned(4))); uint32_t cat = 0xFFFFFF00u;
    rd r = { 0, 0, len, 0, 0, fetch, cache, &cat };
    return load_body(q, s, x, r, check_only);
}
