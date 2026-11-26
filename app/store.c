/* SPDX-License-Identifier: GPL-3.0-only
 * Projects in flash (store.h). Written for this project. */
#include "store.h"
#include "../core/gb.h"
#if defined(__PI32V2__)
#define XT __attribute__((section(".xtext.store")))      /* (3.3.5: what never runs while the flash works: from the window) */
#elif defined(HOST)
#define XT __attribute__((section("tw_xt")))       /* (the twin: what calls an erase or a write from here is caught) */
#else
#define XT
#endif

#define MAGIC 0x32504247u                         /* "GBP2": the areas (the betas' slots, "GBP1", are not read) */
typedef struct { uint32_t magic, seq, len, dcrc; uint8_t slot, pad[3]; uint32_t hcrc; } hdr;   /* 24 bytes */

static const store_flash *F;
static struct { int state; int slot, copy; const uint8_t *data; uint32_t len, seq, at, pages, done_ops, all_ops, vat, vcrc; hdr h; } W;
static uint8_t page[256] __attribute__((aligned(4))) GB_BOOTRAM;   /* (3.4.1: in m-bootloader's RAM, written before read: the slot's room) */
static uint8_t chk[512] __attribute__((aligned(4))) GB_LOWRAM;    /* (a save's reading back: read into before checked; 3.4.3: 512, the main stack 512 more) */
/* (3.3.5) the one project's save: see the banks below */
static struct { int state, known; uint32_t bank, off, len, given, prog, vat, vcrc, need_n; const uint8_t *d; uint32_t d_off, d_n; hdr h; } P;


/* a byte at a time through a table (1 KB, made at the first call): a 20 KB project in ~0.3 ms, between two audio
 * blocks */
static uint32_t CRC_T[256];
const uint32_t *crc32_table(void) { if (!CRC_T[1]) crc32_ieee(0, "", 0); return CRC_T; }
uint32_t crc32_ieee(uint32_t crc, const void *d, uint32_t n)
{
    if (!CRC_T[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1; CRC_T[i] = c; }
    const uint8_t *p = (const uint8_t *)d;
    crc = ~crc;
    while (n--) crc = CRC_T[(crc ^ *p++) & 255u] ^ (crc >> 8);
    return ~crc;
}
/* the areas: 0..8 the projects', 9 and 10 the user sounds' two copies (4 KB, 0x95000 and 0x96000) */
#define SND_AREA STORE_AREAS
static uint32_t base_of(int area) { return area < SND_AREA ? STORE_BASE + (uint32_t)area * STORE_COPY : STORE_SOUNDS_AT + (uint32_t)(area - SND_AREA) * 4096u; }
static uint32_t size_of(int area) { return area < SND_AREA ? STORE_COPY : 4096u; }
static int hdr_read(int area, hdr *h)
{
    if (F->read(base_of(area), h, sizeof *h)) return 0;
    if (h->magic != MAGIC || h->hcrc != crc32_ieee(0, h, 20)) return 0;
    return area < SND_AREA ? h->slot < STORE_SLOTS && h->len <= STORE_MAX : h->slot == STORE_SOUNDS_SLOT && h->len <= STORE_SOUNDS_MAX;
}
/* an area is good when its data's CRC matches too (read through buf) */
XT static int area_good(int area, hdr *h, void *buf, uint32_t cap)
{
    if (!hdr_read(area, h) || h->len > cap) return 0;
    if (F->read(base_of(area) + 256u, buf, h->len)) return 0;
    return crc32_ieee(0, buf, h->len) == h->dcrc;
}
XT void store_init(const store_flash *f) { F = f; W.state = 0; }

XT int store_old_load(int slot, void *buf, uint32_t cap)
{
    uint32_t *seq = 0;
    hdr hs[STORE_AREAS]; int ok[STORE_AREAS];
    for (int a = 0; a < STORE_AREAS; a++) ok[a] = hdr_read(a, &hs[a]) && hs[a].slot == slot;
    /* the newest first; if its data is not whole, the one before */
    for (;;) {
        int best = -1;
        for (int a = 0; a < STORE_AREAS; a++) if (ok[a] && (best < 0 || hs[a].seq > hs[best].seq)) best = a;
        if (best < 0) return -1;
        hdr h;
        if (area_good(best, &h, buf, cap)) { if (seq) *seq = h.seq; return (int)h.len; }
        ok[best] = 0;
    }
}
XT static uint32_t newest_seq(int *slot_out)
{
    uint32_t best = 0; int bs = -1;
    for (int a = 0; a < STORE_AREAS; a++) {
        hdr h;
        if (hdr_read(a, &h) && (bs < 0 || h.seq > best)) { best = h.seq; bs = h.slot; }
    }
    if (slot_out) *slot_out = bs;
    return best;
}
XT int store_old_latest(void) { int s; newest_seq(&s); return s; }

/* one flash operation at a time: the sectors erased, the data's pages written, the header's page last, checked */
/* a raw job (the web editor's sample slots, SLOOP's "FSMP" at 0xA0000..): a sector erased (or not), then bytes
 * programmed at an offset in it, page by page, then read back and compared */
static struct { int state; uint32_t sector, off, n, at, pages, erase; const uint8_t *data; } R;
XT int store_raw_begin(uint32_t sector, uint32_t off, const void *data, uint32_t n, int erase)
{
    if (W.state == 1 || R.state == 1 || (P.state > 0 && P.state < 7) || (sector & 4095u) || off + n > 4096u) return -1;
    R.sector = sector; R.off = off; R.n = n; R.data = (const uint8_t *)data; R.erase = erase ? 1u : 0u; R.at = 0;
    R.pages = 0; for (uint32_t a = off; a < off + n; a = (a | 255u) + 1u) R.pages++;
    R.state = 1;
    return 0;
}
static int raw_step(void)
{
    if (F->busy()) return 1;
    if (R.at < R.erase) { if (F->erase_start(R.sector)) { R.state = 0; return -1; } R.at++; return 1; }
    if (R.at < R.erase + R.pages) {                             /* page by page, a page's part at a time */
        uint32_t k = R.at - R.erase, a = R.off, i = 0;
        while (i < k) { a = (a | 255u) + 1u; i++; }
        uint32_t end = (a | 255u) + 1u; if (end > R.off + R.n) end = R.off + R.n;
        for (uint32_t j = 0; j < end - a; j++) page[j] = R.data[a - R.off + j];
        if (F->program_start(R.sector + a, page, end - a)) { R.state = 0; return -1; }
        R.at++; return 1;
    }
    for (uint32_t a = 0; a < R.n; a += 256u) {                  /* read back */
        uint32_t m = R.n - a > 256u ? 256u : R.n - a;
        if (F->read(R.sector + R.off + a, page, (m + 3u) & ~3u)) { R.state = 0; return -1; }
        for (uint32_t j = 0; j < m; j++) if (page[j] != R.data[a + j]) { R.state = 0; return -1; }
    }
    R.state = 0; return 2;
}
/* ---------------------------------------------------------------- 3.3.5: the one project's two banks */
#define MAGIC3 0x33504247u                        /* "GBP3" */
#define PSECT (STORE_PBANK / 4096u)
static uint32_t pbank(uint32_t b) { return STORE_BASE + b * STORE_PBANK; }
/* (3.4.0, Sam: "saving a pattern while it plays cuts the sound") a copy may start at any sector of its bank (the
 * header's pad[0] says which): a save while it plays goes after the newest copy, where the flash is still erased, and
 * needs no erase (the window off for milliseconds: the samples silent). Only when no erased room is left is a sector
 * erased, as before. */
static uint32_t psect(uint32_t len) { return (256u + len + 4095u) / 4096u; }
XT static int phdr(uint32_t b, uint32_t s, hdr *h)
{
    if (F->read(pbank(b) + s * 4096u, h, sizeof *h)) return 0;
    return h->magic == MAGIC3 && h->hcrc == crc32_ieee(0, h, 20) && h->pad[0] == s && h->len >= 16u && s * 4096u + 256u + h->len <= STORE_PBANK;
}
void (*store_yield)(void);
int store_no_erase;
XT static int pwhole(uint32_t b, uint32_t s, hdr *h)     /* its header, and its data with the CRC the header says */
{
    if (!phdr(b, s, h)) return 0;
    uint32_t c = 0, at = pbank(b) + s * 4096u + 256u;
    for (uint32_t a = 0; a < h->len; a += 256u) {
        uint32_t n = h->len - a > 256u ? 256u : h->len - a;
        if (F->read(at + a, page, (n + 3u) & ~3u)) return 0;
        c = crc32_ieee(c, page, n);
        if (store_yield && (a & 1023u) == 768u) store_yield();   /* (3.4.3: a large project, 52 KB: the sound served) */
    }
    return c == h->dcrc;
}
/* every copy's header: the newest whole one (its bank, -1 none; its sector in *ps), the highest number of all (*top),
 * and where each bank's copies end (end[b]: the first sector after them) */
XT static int pscan(hdr *best, uint32_t *ps, uint32_t *top, uint32_t *end)
{
    uint32_t lim = ~0u; *top = 0; end[0] = end[1] = 0;
    for (;;) {
        int kb = -1; uint32_t ks = 0; hdr h, kh;
        for (uint32_t b = 0; b < 2; b++) for (uint32_t s = 0; s < PSECT; s++) if (phdr(b, s, &h)) {
            if (h.seq > *top) *top = h.seq;
            if (s + psect(h.len) > end[b]) end[b] = s + psect(h.len);
            if (h.seq < lim && (kb < 0 || h.seq > kh.seq)) { kb = (int)b; ks = s; kh = h; }
        }
        if (kb < 0) return -1;
        if (pwhole((uint32_t)kb, ks, &kh)) { if (best) *best = kh; *ps = ks; return kb; }
        lim = kh.seq;
    }
}
XT static int pnewest(hdr *h) { uint32_t s, t, e[2]; return pscan(h, &s, &t, e); }
static int blank4k(uint32_t a)                           /* a sector all erased (read, 512 bytes at a time) */
{
    for (uint32_t o = 0; o < 4096u; o += sizeof chk) {
        if (F->read(a + o, chk, sizeof chk)) return 0;
        for (uint32_t i = 0; i < sizeof chk / 4u; i++) if (((const uint32_t *)chk)[i] != ~0u) return 0;
    }
    return 1;
}
XT int store_proj_find(uint32_t *data_at, uint32_t *len, uint32_t *crc)
{
    hdr h; uint32_t s, t, e[2]; int k = pscan(&h, &s, &t, e);
    if (k < 0) return -1;
    if (data_at) *data_at = pbank((uint32_t)k) + s * 4096u + 256u;
    if (len) *len = h.len;
    if (crc) *crc = h.dcrc;
    return 0;
}
/* (3.3.8) a save while the music plays: an erase keeps the flash window off for milliseconds, and what the sound reads
 * from the flash (the samples, the kits) is silent meanwhile: a stutter. So the bank the next save goes to is erased
 * beforehand, a sector at a time, while it is stopped and nobody plays (store_proj_prep); a save then only writes pages
 * (each well under a millisecond, between two audio halves). Only the sectors the project fills are erased, too. The
 * newest whole copy is never touched: the bank prepared is the other one */
static uint8_t pre_known; static uint32_t pre_bank, pre_n;     /* pre_n: its sectors from the first, erased */
/* (3.3.8 fix) in RAM, not XT: it starts an erase, the window goes off, and it must still return (from the window: the
 * FM-1 froze at the logo, then the watchdog restarted it) */
int store_proj_prep(void)
{
    if (W.state == 1 || R.state == 1 || (P.state > 0 && P.state < 7) || F->busy()) return 0;
    if (!pre_known) { hdr h; int k = pnewest(&h); pre_bank = k == 0 ? 1u : 0u; pre_n = 0; pre_known = 1; return 1; }   /* (read: a pass of its own) */
    if (pre_n >= PSECT) return 0;
    uint32_t a = pbank(pre_bank) + pre_n++ * 4096u;
    if (!blank4k(a) && F->erase_start(a)) { pre_known = 0; return 0; }   /* (one already erased: not again) */
    return 1;
}
XT int store_proj_ready(void) { return pre_known && pre_n >= PSECT; }
int store_erasing(void) { return P.state == 1 || (W.state == 1 && W.at < size_of(W.copy) / 4096u); }
XT int store_proj_begin(uint32_t len, uint32_t crc, int known)
{
    if (W.state == 1 || R.state == 1 || (P.state > 0 && P.state < 7) || len < 16u || len > STORE_PMAX) return -1;
    hdr h; uint32_t s, seq, end[2]; int k = pscan(&h, &s, &seq, end);
    uint32_t kb = k < 0 ? 0u : (uint32_t)k, ob = kb ^ 1u;
    P.need_n = psect(len);
    /* (3.4.0) after the newest copy, in its bank; else after the other bank's copies; else that bank from its start
     * (prepared while stopped: erased already). The newest whole copy is never touched */
    if (end[kb] + P.need_n <= PSECT) { P.bank = kb; P.off = end[kb]; }
    else { P.bank = ob; P.off = end[ob] + P.need_n <= PSECT ? end[ob] : 0u; }
    /* (3.4.3) asked not to erase (it plays: an erase keeps the flash window off for tens of ms, and the samples and
     * kits read from it go silent): -3 if a sector it would take is not erased already, nothing begun */
    if (store_no_erase) for (uint32_t i = 0; i < P.need_n; i++) { if (store_yield) store_yield(); if (!blank4k(pbank(P.bank) + (P.off + i) * 4096u)) return -3; }
    P.len = len; P.known = known; P.given = 0; P.prog = 0; P.vat = 0; P.vcrc = 0; P.d = 0; P.d_n = 0; P.d_off = 0;
    P.h.magic = MAGIC3; P.h.seq = seq + 1u; P.h.len = len; P.h.dcrc = crc; P.h.slot = 0; P.h.pad[0] = (uint8_t)P.off; P.h.pad[1] = P.h.pad[2] = 0;
    P.h.hcrc = crc32_ieee(0, &P.h, 20);
    pre_known = 0;                                       /* (written now: prepared again after it) */
    W.done_ops = 0; W.all_ops = P.need_n + (len + 255u) / 256u + (len + 511u) / 512u + 1u;
    P.state = 1;                                         /* the sectors */
    return 0;
}
XT uint32_t store_proj_need(void) { return P.given; }
XT int store_proj_written(void) { return P.state == 3 && P.prog >= P.len; }
XT uint32_t store_proj_target(void) { return pbank(P.bank) + P.off * 4096u + 256u; }
XT int store_proj_wants(void) { return (P.state == 1 || P.state == 2) && P.given < P.len && (!P.d || P.prog >= P.d_off + P.d_n); }
XT uint32_t store_proj_given(void) { return P.given; }
XT void store_proj_give(const void *d, uint32_t n)
{
    if (P.state != 2 && P.state != 1) return;
    if (P.given + n > P.len) n = P.len - P.given;
    P.d = (const uint8_t *)d; P.d_off = P.given; P.d_n = n; P.given += n;
}
XT int store_proj_end(uint32_t crc)
{
    if (P.known || P.given != P.len || (P.state != 2 && P.state != 3)) return -1;
    P.h.dcrc = crc; P.h.hcrc = crc32_ieee(0, &P.h, 20); P.known = 1;
    return 0;
}
XT void store_proj_abort(void) { if (P.state > 0 && P.state < 7) P.state = 0; }
static int proj_step(void)
{
    if (F->busy()) return 1;
    uint32_t base = pbank(P.bank) + P.off * 4096u;
    switch (P.state) {
    case 1:                                              /* the sectors, one at a time: erased only if they are not */
        if (P.vat < P.need_n) { uint32_t a = base + P.vat++ * 4096u; W.done_ops++; if (!blank4k(a) && F->erase_start(a)) { P.state = 0; return -1; } return 1; }
        P.vat = 0; P.state = 2; return 1;
    case 2:                                              /* the data: what was given, a page's part at a time */
        if (P.prog >= P.len) { P.state = 3; return 1; }
        if (!P.d || P.prog >= P.d_off + P.d_n) { P.d = 0; return P.given >= P.len ? 1 : 3; }
        {
            uint32_t a = P.prog, end = (a | 255u) + 1u, lim = P.d_off + P.d_n;
            if (end > lim) end = lim;
            const uint8_t *src = P.d + (a - P.d_off);
            for (uint32_t i = 0; i < end - a; i++) page[i] = src[i];
            if (F->program_start(base + 256u + a, page, end - a)) { P.state = 0; return -1; }
            P.prog = end; if ((end & 255u) == 0u || end == P.len) W.done_ops++;
            return 1;
        }
    case 3:                                              /* every byte written: read back, 512 bytes a step */
        if (!P.known) return 1;                          /* (an upload: its CRC still to come) */
        {
            uint32_t n = P.len - P.vat > sizeof chk ? sizeof chk : P.len - P.vat;
            if (F->read(base + 256u + P.vat, chk, (n + 3u) & ~3u)) { P.state = 0; return -1; }
            P.vcrc = crc32_ieee(P.vcrc, chk, n); P.vat += n; W.done_ops++;
            if (P.vat < P.len) return 1;
            if (P.vcrc != P.h.dcrc) { P.state = 0; return -2; }   /* (changed while it was written, or written wrong) */
            P.state = 4; return 1;
        }
    case 4:                                              /* the header: from now on this copy counts */
        if (F->program_start(base, &P.h, sizeof P.h)) { P.state = 0; return -1; }
        P.state = 5; return 1;
    case 5: {                                            /* read back */
        hdr h;
        if (F->read(base, &h, sizeof h)) { P.state = 0; return -1; }
        const uint8_t *a = (const uint8_t *)&h, *b = (const uint8_t *)&P.h; int same = 1;
        for (uint32_t i = 0; i < sizeof h; i++) if (a[i] != b[i]) same = 0;
        W.done_ops = W.all_ops;
        P.state = 7; return same ? 2 : -1;
    }
    default: return 0;
    }
}

int store_step(void)
{
    if (P.state > 0 && P.state < 7) return proj_step();
    if (R.state == 1) return raw_step();
    if (W.state != 1) { int s = W.state; if (s == 2) W.state = 0; return s; }
    if (F->busy()) return 1;
    uint32_t base = base_of(W.copy), sectors = size_of(W.copy) / 4096u;
    if (W.at < sectors) {                                        /* erase */
        if (F->erase_start(base + W.at * 4096u)) { W.state = -1; return -1; }
    } else if (W.at < sectors + W.pages) {                      /* the data, page by page */
        uint32_t k = W.at - sectors, off = k * 256u, n = W.len - off > 256u ? 256u : W.len - off;
        for (uint32_t i = 0; i < n; i++) page[i] = W.data[off + i];
        if (F->program_start(base + 256u + off, page, n)) { W.state = -1; return -1; }
    } else if (W.at == sectors + W.pages) {                     /* the header: from now on this copy counts */
        if (F->program_start(base, &W.h, sizeof W.h)) { W.state = -1; return -1; }
    } else {                                                     /* checked as a load would, 512 bytes a step */
        if (W.vat == 0) {
            hdr h;
            if (!hdr_read(W.copy, &h) || h.slot != W.slot || h.seq != W.seq || h.len != W.len || h.dcrc != W.h.dcrc) { W.state = -1; return -1; }
            W.vcrc = 0;
        }
        uint32_t n = W.len - W.vat > sizeof chk ? sizeof chk : W.len - W.vat;
        if (n && F->read(base_of(W.copy) + 256u + W.vat, chk, n)) { W.state = -1; return -1; }
        W.vcrc = crc32_ieee(W.vcrc, chk, n); W.vat += n;
        if (W.vat >= W.len) { W.state = W.vcrc == W.h.dcrc ? 2 : -1; return W.state; }
        if (!n) { W.state = -1; return -1; }
        return 1;
    }
    W.at++; W.done_ops++;
    return 1;
}
/* the user sounds: two 4 KB copies, the newer whole one counts */
XT int store_sounds_load(void *buf, uint32_t cap)
{
    hdr a, b; int oa = hdr_read(SND_AREA, &a), ob = hdr_read(SND_AREA + 1, &b);
    for (int k = 0; k < 2; k++) {
        int c = ob && (!oa || b.seq > a.seq) ? (k ? 0 : 1) : (k ? 1 : 0);
        hdr h;
        if ((c ? ob : oa) && area_good(SND_AREA + c, &h, buf, cap)) return (int)h.len;
    }
    return -1;
}
XT int store_sounds_begin(const void *data, uint32_t len)
{
    if (W.state == 1 || (P.state > 0 && P.state < 7) || len > STORE_SOUNDS_MAX) return -1;
    hdr a, b; int oa = hdr_read(SND_AREA, &a), ob = hdr_read(SND_AREA + 1, &b);
    int target = !oa ? SND_AREA : !ob ? SND_AREA + 1 : a.seq < b.seq ? SND_AREA : SND_AREA + 1;   /* (never the newer) */
    W.copy = target; W.slot = STORE_SOUNDS_SLOT; W.data = (const uint8_t *)data; W.len = len;
    W.seq = (oa && a.seq > (ob ? b.seq : 0u) ? a.seq : ob ? b.seq : 0u) + 1u;
    W.pages = (len + 255u) / 256u;
    W.at = 0; W.vat = 0; W.done_ops = 0; W.all_ops = 1u + W.pages + 1u;
    W.h.magic = MAGIC; W.h.seq = W.seq; W.h.len = len; W.h.dcrc = crc32_ieee(0, data, len);
    W.h.slot = STORE_SOUNDS_SLOT; W.h.pad[0] = W.h.pad[1] = W.h.pad[2] = 0; W.h.hcrc = crc32_ieee(0, &W.h, 20);
    W.state = 1;
    return 0;
}
XT uint32_t store_progress(void) { return W.all_ops ? W.done_ops * 100u / W.all_ops : 0; }
