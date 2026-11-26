/* SPDX-License-Identifier: GPL-3.0-only
 * SLOOP 3's screens (ui.h), 4.0: SLOOP 2.8.5's screens, which its players know, drawn again for this firmware
 * (written for this project; the layout, the sizes and the colours are SLOOP 2.8.5's, Felucca's before it, by Leo
 * Kuroshita, Hügelton Instruments, GPL-3.0). 4.0.1 (Sam): in SLOOP 3's smoother, bigger lettering (fonts.c, Inter Bold),
 * placed in SLOOP 2.8.5's 16 px cells (font_s), its 32 px ones (font_l), its values in font_m where they fit.
 *   - the sound's pages: a status line (transport, tempo, track, battery), four 60 px columns (the knobs' label,
 *     value and gauge), a picture (or the four values large, 2 x 2 as the knobs), the steps and the engine, sound and
 *     page at the foot;
 *   - the live screens (TRACKS, DRUMS, REC): the tempo large, the transport, the four beats; the tracks; the dials;
 *   - the layers (a button held): the title large, 16 tiles as the 16 white keys, the dials.
 * White is what you touch, red is recording. */
#include "ui.h"
#include "gfx.h"
#include "vis.h"
#include "../engine/engine.h"   /* (SP_*: which value is which) */

/* ---------------------------------------------------------------- the colours */
/* SLOOP 2.8.5's: the screen in five steps of one colour (MONO by default) on black, white the accent, its greys for the
 * tiles and the dials, the tracks' colours in three strengths */
ui_palette ui_pal = { 0, RGB(40, 40, 40), RGB(80, 80, 80), RGB(40, 40, 40), RGB(226, 226, 226), RGB(130, 130, 130) };
const char *const ui_theme_name[UI_NTHEMES] = { "MONO", "GREEN", "AMBER", "CYAN", "RED", "SOFT" };
static const uint16_t PAL5[UI_NTHEMES][5] = {
    { RGB(40, 40, 40), RGB(80, 80, 80), RGB(130, 130, 130), RGB(186, 186, 186), RGB(226, 226, 226) },
    { RGB(0, 40, 12), RGB(0, 84, 30), RGB(16, 140, 54), RGB(56, 200, 92), RGB(120, 255, 146) },
    { RGB(60, 26, 0), RGB(110, 50, 0), RGB(170, 82, 0), RGB(225, 120, 8), RGB(255, 166, 40) },
    { RGB(0, 30, 50), RGB(0, 62, 96), RGB(16, 112, 160), RGB(56, 172, 222), RGB(140, 222, 255) },
    { RGB(52, 8, 8), RGB(100, 18, 14), RGB(170, 36, 26), RGB(226, 64, 48), RGB(255, 112, 92) },
    { RGB(36, 36, 44), RGB(70, 70, 84), RGB(120, 120, 140), RGB(176, 176, 196), RGB(220, 220, 236) } };
static uint16_t P[5] = { RGB(40, 40, 40), RGB(80, 80, 80), RGB(130, 130, 130), RGB(186, 186, 186), RGB(226, 226, 226) };
void ui_theme_own(const uint16_t c[5], uint16_t bg)
{
    for (int k = 0; k < 5; k++) P[k] = c[k];
    ui_pal.bg = bg; ui_pal.line = c[0]; ui_pal.panel = c[0]; ui_pal.panel2 = c[1]; ui_pal.dim = c[2]; ui_pal.txt = c[4];
}
void ui_theme(int i) { ui_theme_own(PAL5[i >= 0 && i < UI_NTHEMES ? i : 0], 0); }
#define BG      ui_pal.bg
#define C_LINE  P[0]                       /* rules, separators */
#define C_DIM   P[1]                       /* inactive, empty steps, units */
#define C_GRAY  P[2]                       /* labels */
#define C_AMB   P[3]                       /* secondary text */
#define C_HI    P[4]                       /* values, curves */
#define WHITE   RGB(255, 255, 255)
#define TE_RED  RGB(255, 44, 52)           /* recording, erasing */
#define GREEN   RGB(60, 220, 110)
static uint16_t G(int k)                   /* the greys of the tiles: 1 tiles, 2 empty steps, 3 labels, 4 secondary */
{
    static const uint8_t A[5] = { 0, 3, 7, 15, 25 };
    return gfx_mix(BG, WHITE, A[k]);
}
#define G1 G(1)
#define G2 G(2)
#define G3 G(3)
#define G4 G(4)
/* the knobs' colours, SLOOP 2.8.5's (the colours of tracks 1 .. 4) */
const uint16_t ui_knob_col[4] = { RGB(40, 124, 255), RGB(30, 204, 112), RGB(255, 198, 24), RGB(255, 98, 26) };
static uint16_t scale565(uint16_t c, int num, int den)
{
    int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return (uint16_t)(((r * num / den) << 11) | ((g * num / den) << 5) | (b * num / den));
}
static uint16_t dimc(uint16_t c) { return scale565(c, 1, 3); }   /* TE_DIM */

/* ---------------------------------------------------------------- text: Terminus Bold, by the top of its 16 px cell */
#define CX_S 90                                  /* (Inter is wider than Terminus: the small type a little narrower) */
#define CX_L 92
static int pend;                                  /* (cut: the small type narrower still for the next text, so that it fits) */
static int ts(int x, int y, const char *s, uint16_t c) { gfx_condense(pend ? pend : CX_S); pend = 0; int r = gfx_text(&font_s, x, y + 2, s, c); gfx_condense(100); return r; }
static int tl(int x, int y, const char *s, uint16_t c) { pend = 0; gfx_condense(CX_L); int r = gfx_text(&font_l, x, y + 3, s, c); gfx_condense(100); return r; }
static int tm(int x, int y, const char *s, uint16_t c) { gfx_condense(CX_S); int r = gfx_text(&font_m, x, y, s, c); gfx_condense(100); return r; }
static int ws(const char *s) { gfx_condense(pend ? pend : CX_S); int r = gfx_text_w(&font_s, s); gfx_condense(100); return r; }
static int wl(const char *s) { gfx_condense(CX_L); int r = gfx_text_w(&font_l, s); gfx_condense(100); return r; }
static int wm(const char *s) { gfx_condense(CX_S); int r = gfx_text_w(&font_m, s); gfx_condense(100); return r; }
static void tsc(int cx, int y, const char *s, uint16_t c) { ts(cx - ws(s) / 2, y, s, c); }
static void tsr(int rx, int y, const char *s, uint16_t c) { ts(rx - ws(s), y, s, c); }
/* s cut to n characters and to w px (b holds 24) */
static const char *cut(char *b, const char *s, int n, int w)   /* (n characters: a UTF-8 one is never split) */
{
    int i = 0, c = 0;
    if (!s) s = "";
    while (s[i] && c < n) {
        int l = ((uint8_t)s[i] & 0xE0) == 0xC0 ? 2 : ((uint8_t)s[i] & 0xF0) == 0xE0 ? 3 : 1;
        if (i + l > 23) break;
        for (int k = 0; k < l && s[i]; k++, i++) b[i] = s[i];
        c++;
    }
    b[i] = 0;
    pend = 0;
    if (ws(b) <= w) return b;
    for (pend = 84; pend > 70 && ws(b) > w; pend -= 6) { }   /* (narrower first: the whole word kept) */
    while (i && ws(b) > w) { do i--; while (i && ((uint8_t)b[i] & 0xC0) == 0x80); b[i] = 0; }
    return b;
}
static void rect(int x, int y, int w, int h, uint16_t c) { if (w > 0 && h > 0) gfx_fill(x, y, w, h, c); }
static void frame1(int x, int y, int w, int h, uint16_t c)
{
    rect(x, y, w, 1, c); rect(x, y + h - 1, w, 1, c); rect(x, y, 1, h, c); rect(x + w - 1, y, 1, h, c);
}
static char *put_u(char *p, uint32_t v) { char t[12]; int n = 0; do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *p++ = t[--n]; return p; }

/* ---------------------------------------------------------------- maths for the pictures */
static void sincos_(float a, float *sn, float *cs)       /* a in radians, to about 1e-4 */
{
    float q = a / 1.5707963f; int k = (int)(q + (q >= 0.0f ? 0.5f : -0.5f));
    float z = a - (float)k * 1.5707963f, z2 = z * z;
    float s0 = z * (1.0f - z2 / 6.0f * (1.0f - z2 / 20.0f)), c0 = 1.0f - z2 / 2.0f * (1.0f - z2 / 12.0f);
    int m = ((k % 4) + 4) % 4;
    *cs = m == 0 ? c0 : m == 1 ? -s0 : m == 2 ? -c0 : s0;
    *sn = m == 0 ? s0 : m == 1 ? c0 : m == 2 ? -s0 : -c0;
}
static void line(int x0, int y0, int x1, int y1, uint16_t c) { gfx_line(x0 * 16 + 8, y0 * 16 + 8, x1 * 16 + 8, y1 * 16 + 8, c); }
static void disc(int cx, int cy, int r, uint16_t c)
{
    if (!gfx_visible(cx - r, cy - r, 2 * r + 1, 2 * r + 1)) return;
    for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) if (x * x + y * y <= r * r + r) rect(cx + x, cy + y, 1, 1, c);
}

/* ---------------------------------------------------------------- shared parts */
static int is_drum_v(const ui_view *v) { return v->kind[v->trk] && v->kind[v->trk][0] == 'D' && v->kind[v->trk][1] == 'R'; }
static uint16_t tcol(const ui_view *v) { return v->col[v->trk] ? v->col[v->trk] : ui_knob_col[0]; }
static int hot(const ui_view *v, int k) { return v->knob[k].hot || v->hot_k == k; }
/* the battery: three bars, a host there (USB) they fill in turn; x the right edge (4.0.2, Sam: the icon only) */
static void battery(const ui_view *v, int rx, int y)
{
    int bx = rx - 19, lvl = v->batt_usb ? 1 + (v->batt_usb - 1) % 3 : v->batt > 100 ? 0 : v->batt >= 60 ? 3 : v->batt >= 30 ? 2 : v->batt >= 8 ? 1 : 0;
    uint16_t c = C_GRAY;
    rect(bx, y + 4, 17, 1, c); rect(bx, y + 12, 17, 1, c); rect(bx, y + 4, 1, 9, c); rect(bx + 16, y + 4, 1, 9, c);
    rect(bx + 17, y + 6, 2, 5, c);
    for (int k = 0; k < lvl; k++) rect(bx + 2 + k * 5, y + 6, 3, 5, !v->batt_usb && v->batt < 15 ? (v->blink ? TE_RED : C_DIM) : C_HI);
}
static void dial3(int cx, int cy, int r, int t, int val, uint16_t c, uint16_t needle);
/* the dial strip of the live screens and the layers, from y0: KNOB 1 .. 4, a dial, its label, its value (white while
 * turned); a message takes its place (a white bar) */
static void dials(const ui_view *v, int y0, const char *msg, int bad)
{
    if (!gfx_visible(0, y0, GFX_W, GFX_H - y0)) return;
    if (msg && msg[0]) {
        char b[32];
        rect(0, y0 + 10, 240, 32, bad ? TE_RED : WHITE);
        tsc(120, y0 + 18, cut(b, msg, 29, 232), bad ? WHITE : RGB(0, 0, 0));
        return;
    }
    for (int k = 0; k < 4; k++) {
        const ui_knob *kn = &v->knob[k];
        int cx = 30 + 60 * k;
        char b[24];
        if (!kn->label || !kn->label[0]) continue;
        if (kn->dial && kn->bar >= 0) dial3(cx, y0 + 12, 11, 4, kn->bar, ui_knob_col[k], hot(v, k) ? RGB(255, 210, 60) : WHITE);   /* (4.0.1: SLOOP 3's dials) */
        else gfx_rframe(cx - 11, y0 + 1, 22, 22, 11, 2, gfx_mix(BG, ui_knob_col[k], 12));
        tsc(cx, y0 + 24, cut(b, kn->label, 7, 58), G3);
        tsc(cx, y0 + 40, cut(b, kn->value, 7, 58), hot(v, k) ? WHITE : G4);
    }
}
static void play_icon(int x, int y, int playing)
{
    if (playing) for (int i = 0; i < 12; i++) rect(x + i, y + i / 2, 1, 14 - i, WHITE);
    else rect(x, y + 1, 12, 12, G3);
}
/* the header of the live screens: the tempo large, the transport, bar.beat, the four beats; REC; the title right,
 * the pattern playing beside it */
static void te_header(const ui_view *v, const char *title, uint16_t tc)
{
    if (!gfx_visible(0, 0, GFX_W, 40)) return;
    char b[16];
    *put_u(b, v->bpm) = 0;
    tl(4, 4, b, G4);
    ts(4 + wl(b) + 4, 20, "BPM", G3);
    play_icon(104, 6, v->playing);
    if (v->playing) { char *p = put_u(b, v->bar); *p++ = '.'; *put_u(p, v->beat) = 0; ts(122, 5, b, WHITE); }
    else ts(122, 5, v->status[0] == 'S' && v->status[1] == ' ' ? "SONG" : "LOOP", G3);
    for (int k = 0; k < 4; k++) rect(104 + k * 9, 26, 7, 7, v->playing && v->beat == k + 1 ? (k ? WHITE : TE_RED) : G2);
    if (v->recording || v->armed) {
        disc(224, 12, 8, v->recording || v->blink ? TE_RED : dimc(TE_RED));
        ts(176, 4, "REC", TE_RED);
    } else if (!title[0]) {                              /* (4.0.3, Sam) TRACKS: the pattern's square alone, large, in the
                                                            corner; the battery under it */
        if (v->pat_badge >= 0) {
            int nx = v->pat_next < 8;
            char sb[2] = { (char)('1' + ((nx ? v->pat_next : v->pat_badge) & 7)), 0 };
            uint16_t bc = nx ? RGB(255, 198, 24) : v->playing ? tcol(v) : dimc(tcol(v));
            if (!nx || v->blink) { rect(210, 1, 28, 22, bc); tm(224 - wm(sb) / 2, 1, sb, RGB(0, 0, 0)); }
            else { frame1(210, 1, 28, 22, bc); tm(224 - wm(sb) / 2, 1, sb, bc); }
        }
        battery(v, 238, 22);
    } else {
        char tb[24]; cut(tb, title, 10, 66);
        int tx = 240 - ws(tb) - 4;
        ts(tx, 5, tb, tc);
        if (v->pat_badge >= 0 && tx - (ws("1") + 8) - 6 >= 158) {   /* the pattern playing, in the track's colour (room: past LOOP);
                                                            the one waiting for the bar instead, blinking yellow */
            int nx = v->pat_next < 8;
            char sb[3] = { (char)('1' + ((nx ? v->pat_next : v->pat_badge) & 7)), 0, 0 };
            int bw = ws(sb) + 8, bx = tx - bw - 6;
            uint16_t bc = nx ? RGB(255, 198, 24) : v->playing ? tcol(v) : dimc(tcol(v));
            if (!nx || v->blink) { rect(bx, 3, bw, 14, bc); ts(bx + 4, 5, sb, RGB(0, 0, 0)); }
            else { frame1(bx, 3, bw, 14, bc); ts(bx + 4, 5, sb, bc); }
        }
        battery(v, 238, 20);
    }
    rect(0, 39, 240, 1, G1);
}

/* ---------------------------------------------------------------- the sound's pages */
#define H_HEAD 20
#define Y_LABEL 26
#define Y_VALUE 44
#define Y_GAUGE 64
#define Y_SEP_END 70
#define Y_GRAPH 74
#define H_GRAPH 124
#define G_OY 24
#define Y_FOOT 202
/* the status line: the transport, REC, the tempo, the octave, the track, the battery; a message takes it */
static void draw_head(const ui_view *v, const char *msg, int bad)
{
    if (!gfx_visible(0, 0, GFX_W, H_HEAD + 1)) return;
    char b[24];
    rect(0, H_HEAD, 240, 1, C_LINE);
    if (msg && msg[0]) { ts(4, 1, cut(b, msg, 29, 232), bad ? TE_RED : C_HI); return; }
    if (v->playing) for (int i = 0; i < 5; i++) rect(4 + i * 2, 4 + i, 2, 10 - 2 * i, WHITE);
    else rect(4, 5, 8, 8, C_HI);
    if (v->recording || v->armed) disc(21, 9, 4, v->recording || v->blink ? TE_RED : C_GRAY);
    /* the metronome, then the tempo */
    *put_u(b, v->bpm) = 0;
    int x = ts(34, 1, b, C_HI);
    if (v->octave) {
        char *p = b; if (v->octave > 0) *p++ = '+'; else *p++ = '-';
        *put_u(p, (uint32_t)(v->octave > 0 ? v->octave : -v->octave)) = 0;
        ts(x + 12, 1, "OCT", C_GRAY); ts(x + 40, 1, b, C_HI);
    }
    /* the track: its number on its colour */
    b[0] = (char)('1' + v->trk); b[1] = 0;
    rect(168, 2, 12, 16, tcol(v)); ts(170, 1, b, RGB(0, 0, 0));
    battery(v, 236, 0);
}
/* one column: LABEL / value / gauge, KNOB c's colour (white while turned, dim when it does nothing) */
static void column(const ui_view *v, int c)
{
    const ui_knob *k = &v->knob[c];
    int x0 = c * 60 + 4;
    char b[24];
    if (!k->label || !k->label[0] || !gfx_visible(x0, Y_LABEL, 56, Y_SEP_END - Y_LABEL)) return;
    uint16_t vc = hot(v, c) ? WHITE : ui_knob_col[c];
    if (k->value[0] == '-' && k->value[1] == '-') vc = C_DIM;
    ts(x0, Y_LABEL, cut(b, k->label, 6, 54), C_GRAY);
    if (wm(k->value) <= 55) tm(x0, Y_VALUE - 1, k->value, vc);   /* (4.0.1: the value larger where it fits) */
    else ts(x0, Y_VALUE, cut(b, k->value, 9, 55), vc);
    if (k->bar >= 0) {
        int gy = Y_GAUGE, fx = k->bar * 52 / 100;
        rect(x0, gy + 1, 52, 1, C_LINE);
        rect(x0, gy, fx, 3, vc == C_DIM ? C_DIM : dimc(ui_knob_col[c]));
        rect(x0 + fx, gy - 1, 1, 5, vc == C_DIM ? C_HI : vc);
    }
}
static void columns(const ui_view *v)
{
    for (int i = 1; i < 4; i++) rect(i * 60 - 1, H_HEAD + 4, 1, Y_SEP_END - H_HEAD - 4, C_LINE);
    for (int c = 0; c < 4; c++) column(v, c);
}
/* the four values large, 2 x 2 as the knobs (one alone: in the middle), the one turned in white */
static void graph_big(const ui_view *v)
{
    int n = 0;
    for (int c = 0; c < 4; c++) n += v->knob[c].label && v->knob[c].label[0];
    if (!gfx_visible(0, Y_GRAPH, GFX_W, H_GRAPH)) return;
    if (n > 1) { rect(119, Y_GRAPH + 6, 1, 112, C_LINE); rect(6, Y_GRAPH + 62, 228, 1, C_LINE); }
    for (int c = 0; c < 4; c++) {
        const ui_knob *k = &v->knob[c];
        char b[24];
        if (!k->label || !k->label[0]) continue;
        int x0 = c & 1 ? 126 : 6, y0 = Y_GRAPH + (c & 2 ? 64 : 2);
        cut(b, k->value, 12, 112);
        if (wl(b) > 112) {                               /* (too long for the big type: the small one, twice as high) */
            if (n == 1) { x0 = (240 - ws(b)) / 2; y0 = Y_GRAPH + 30; }
            ts(x0, y0 + 2, k->label, ui_knob_col[c]);
            ts(x0, y0 + 28, b, hot(v, c) ? WHITE : C_HI);
            continue;
        }
        if (n == 1) { x0 = (240 - wl(b)) / 2; y0 = Y_GRAPH + 30; }
        ts(x0, y0 + 2, cut(b, k->label, 8, 112), ui_knob_col[c]);
        cut(b, k->value, 12, 112);
        tl(x0, y0 + 20, b, k->value[0] == '-' && k->value[1] == '-' ? C_DIM : hot(v, c) ? WHITE : C_HI);
    }
}
/* the envelope as SLOOP 2.8.5 draws it: attack straight, decay and release falling as the voice does */
static void graph_adsr(const ui_view *v, uint16_t c)
{
    int oy = Y_GRAPH + G_OY;
    if (!gfx_visible(0, oy, GFX_W, 100)) return;
    int a = 4 + v->prm[SP_ATK] * 50 / 100, d = 6 + v->prm[SP_DEC] * 50 / 100, r = 6 + v->prm[SP_REL] * 60 / 100;
    int top = 8, bot = 90, sus = v->prm[SP_SUS] * 10, x0 = 6, x1 = x0 + a, x3 = 232 - r;
#define EGY(l) (oy + bot - (l) * (bot - top) / 1000)
    line(x0, oy + bot, x1, oy + top, c);
    int px = x1, py = oy + top; float e = 1.0f, k = 1.0f - 4.6f / (float)d;
    for (int i = 1; i <= d; i++) { e *= k; int l = sus + (int)((float)(1000 - sus) * e); line(px, py, x1 + i, EGY(l), c); px = x1 + i; py = EGY(l); }
    line(px, py, x3, EGY(sus), c); px = x3; py = EGY(sus);
    e = 1.0f; k = 1.0f - 4.6f / (float)r;
    for (int i = 1; i <= r; i++) { e *= k; int l = (int)((float)sus * e); line(px, py, x3 + i, EGY(l), c); px = x3 + i; py = EGY(l); }
    rect(0, oy + bot + 1, 240, 1, C_LINE);
#undef EGY
}
static void graph_lfo(const ui_view *v, uint16_t c)
{
    int oy = Y_GRAPH + G_OY;
    if (!gfx_visible(0, oy, GFX_W, 100)) return;
    int wv = (v->prm[SP_LWAVE] * 4 + 50) / 100, py = 50;
    uint32_t r = 0x2545F491u; float sh = 0.0f; int cyc = -1;
    rect(0, oy + 50, 240, 1, C_LINE);
    for (int x = 0; x < 240; x++) {
        float t = (float)x * (2.0f / 240.0f), ph = t - (float)(int)t, l;
        switch (wv) {
        case 0: { float sn, cs; sincos_(ph * 6.2831853f, &sn, &cs); l = sn; break; }
        case 1: l = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; break;
        case 2: l = 1.0f - 2.0f * ph; break;
        case 3: l = ph < 0.5f ? 1.0f : -1.0f; break;
        default:
            if ((int)(t * 6.0f) != cyc) { cyc = (int)(t * 6.0f); r ^= r << 13; r ^= r >> 17; r ^= r << 5; sh = (float)(int32_t)r * (1.0f / 2147483648.0f); }
            l = sh; break;
        }
        int y = 50 - (int)(l * 38.0f);
        if (x) line(x - 1, oy + py, x, oy + y, c);
        py = y;
    }
}
/* four bars, as the knobs (FX's sends) */
static void graph_bars(const ui_view *v, uint16_t c)
{
    int oy = Y_GRAPH + G_OY;
    if (!gfx_visible(0, oy, GFX_W, 100)) return;
    for (int i = 0; i < 4; i++) {
        if (!v->knob[i].label || v->knob[i].bar < 0) continue;
        int h = v->knob[i].bar * 80 / 100, x = i * 60 + 28;
        rect(x, oy + 10, 1, 80, C_LINE);
        rect(x, oy + 90 - h, 1, h, c);
        rect(x - 3, oy + 90 - h, 7, 1, c);
    }
}
/* the arpeggiator: what it plays of the chord held, 16 steps */
static void graph_arp(const ui_view *v, uint16_t c)
{
    int oy = Y_GRAPH + G_OY;
    if (!gfx_visible(0, oy, GFX_W, 100)) return;
    for (int i = 0; i < v->arp_n && i < 16; i++) {
        int x = 6 + i * 14 + i / 4 * 4;
        if (v->arp_seq[i] == 255) { rect(x, oy + 85, 8, 1, C_DIM); continue; }
        int y = oy + 80 - v->arp_seq[i] * 60 / 100;
        rect(x, y, 8, 6, c);
    }
}
/* the foot: the 16 steps in view as thin bars (the playhead under), then the engine, the sound, the page */
static void draw_foot(const ui_view *v)
{
    if (!gfx_visible(0, Y_FOOT - 2, GFX_W, 40)) return;
    char b[24];
    rect(0, Y_FOOT - 2, 240, 1, C_LINE);
    for (int i = 0; i < 16; i++) {
        uint8_t f = v->cells[v->trk][i];
        int sx = 6 + i * 14 + i / 4 * 4;
        if (f & UC_OUT) continue;
        if (f & UC_NOTE) rect(sx, Y_FOOT + 2, 2, 9, tcol(v));
        else rect(sx, Y_FOOT + 10, 1, 1, C_DIM);
        if (f & UC_PLAY) rect(sx - 1, Y_FOOT + 13, 3, 3, WHITE);
    }
    const char *en = v->kind[v->trk] ? v->kind[v->trk] : "";
    int x = ts(4, Y_FOOT + 20, cut(b, en, 8, 72), C_HI);
    char ti[24]; cut(ti, v->page_name, 16, 120);
    int room = 236 - ws(ti) - 10 - (x + 10);
    ts(x + 10, Y_FOOT + 20, cut(b, v->sound[v->trk], 12, room), C_AMB);
    tsr(236, Y_FOOT + 20, ti, C_GRAY);
}
static void fit3(const gfx_font *big, int x, int y, int w, int h, const char *s, uint16_t c, int align);
/* (3.4.4) a line too long for the title (HOME > SYSTEM: the last stop, "RESTART IN MIX/FX · ECHO · 3 MIN · CPU 97% ·
 * 24 V · R04"): on two lines over the title, cut at the " · " nearest its middle */
static void long_info(const char *s)
{
    if (!gfx_visible(0, 0, GFX_W, 44)) return;
    int n = 0; while (s[n]) n++;
    int at = -1;
    for (int i = 0; i + 3 < n; i++)
        if (s[i] == ' ' && (unsigned char)s[i + 1] == 0xC2 && (unsigned char)s[i + 2] == 0xB7 && s[i + 3] == ' ' &&
            (at < 0 || (i > n / 2 ? i - n / 2 : n / 2 - i) < (at > n / 2 ? at - n / 2 : n / 2 - at))) at = i;
    char a[48], b[48];
    int la = at < 0 ? n : at; if (la > 47) la = 47;
    for (int i = 0; i < la; i++) a[i] = s[i];
    a[la] = 0;
    int lb = 0; if (at >= 0) for (const char *q = s + at + 4; *q && lb < 47; q++) b[lb++] = *q;
    b[lb] = 0;
    rect(0, 0, 240, 44, BG);
    fit3(&font_s, 4, 3, 232, font_s.cap, a, WHITE, 0);
    if (lb) fit3(&font_s, 4, 22, 232, font_s.cap, b, WHITE, 0);
    rect(4, 41, 232, 2, RGB(255, 210, 60));
}
static void page_frame(const ui_view *v)
{
    const char *msg = v->toast ? v->toast : v->info;
    if (v->saving) msg = "SAVING";
    draw_head(v, msg, v->toast && v->toast_kind);
    uint16_t c = tcol(v);
    columns(v);
    switch (v->page) {
    case UI_ENV: graph_adsr(v, c); break;
    case UI_LFO: graph_lfo(v, c); break;
    case UI_FX: graph_bars(v, c); break;
    case UI_ARP: graph_arp(v, c); break;
    case UI_SLICER:                                      /* (4.0.3) as SLOOP 2.8.5: a step that plays a full bar; GATE a closed
                                                            step as high as it stays open; STUT hatched (it repeats); the playing one */
        if (gfx_visible(0, Y_GRAPH, GFX_W, H_GRAPH))
            for (int i = 0; i < 16; i++) {
                int x = 6 + i * 14 + i / 4 * 2, y0 = Y_GRAPH + 14, h = 90; uint16_t col = v->slc_mode ? c : C_DIM;
                if ((v->slc_pat >> i) & 1u) rect(x, y0, 11, h, col);
                else if (v->slc_mode == 2) for (int y = y0; y < y0 + h; y += 4) rect(x, y, 11, 1, col);
                else { int o = h * (100 - v->slc_depth) / 100; rect(x, y0 + h - 1, 11, 1, C_LINE); if (o) rect(x, y0 + h - o, 11, o, C_DIM); }
                if (v->slc_mode && i == v->slc_at) rect(x, y0 + h + 4, 11, 3, WHITE);
            }
        break;
    case UI_SOON:
        if (gfx_visible(0, Y_GRAPH, GFX_W, H_GRAPH)) {
            char b[40];
            tsc(120, Y_GRAPH + 50, v->sub[0] ? cut(b, v->sub, 29, 236) : "DRUM TRACK", C_HI);
            tsc(120, Y_GRAPH + 76, "SEQ  TRACKS  GLO", C_DIM);
        }
        break;
    default: graph_big(v); break;
    }
    if (!v->toast && !v->saving && v->info && v->page == UI_GENERIC && ws(v->info) > 232) long_info(v->info);
    draw_foot(v);
}

/* ---------------------------------------------------------------- TRACKS */
static void page_tracks(const ui_view *v)
{
    te_header(v, "", G3);                                /* (4.0.3, Sam: no title, the pattern's square in the corner) */
    int n = v->ntrk ? v->ntrk : 4, rh = 144 / n;     /* (five tracks: 28 px each, SLOOP 2.8.5's four: 36) */
    for (int i = 0; i < n; i++) {
        int y = 40 + i * rh, sel = i == v->trk, silent = v->mute[i] || !v->level[i];
        if (!gfx_visible(0, y, GFX_W, rh)) continue;
        uint16_t col = v->col[i], dm = dimc(col);
        char b[24], e[24], nm[2] = { (char)('1' + i), 0 };
        int big = rh >= 34, th = big ? 30 : rh - 4, sy = big ? 21 : rh - 11, sh = big ? 9 : 6;
        rect(2, y + 2, 26, th, sel ? col : dm);
        ts(11, y + 2 + (th - 16) / 2, nm, sel ? RGB(0, 0, 0) : col);
        cut(b, v->sound[i] ? v->sound[i] : "", 13, 160); { int pn = pend;
        ts(34, y + (big ? 1 : 0), b, sel ? WHITE : G4); if (pn) e[0] = 0; }
        { int i2 = 0; const char *kd = v->kind[i] ? v->kind[i] : ""; for (; kd[i2] && i2 < 23; i2++) e[i2] = kd[i2]; e[i2] = 0; }
        if (34 + ws(b) + 6 + ws(e) < 196) ts(34 + ws(b) + 6, y + (big ? 1 : 0), e, sel ? col : G3);
        if (v->rec[i]) { rect(200, y + 2, 36, 15, TE_RED); ts(206, y + 1, "REC", WHITE); }
        else if (v->solo[i]) { rect(200, y + 2, 36, 15, WHITE); ts(202, y + 1, "SOLO", RGB(0, 0, 0)); }
        else if (v->mute[i]) ts(204, y + 1, "MUTE", G3);
        for (int j = 0; j < 16; j++) {
            uint8_t f = v->cells[i][j];
            int x = 34 + j * 10;
            if (f & UC_OUT) continue;
            uint16_t cc = (f & UC_NOTE) ? (silent ? G3 : sel ? col : dm) : G1;
            rect(x, y + sy, 8, sh, cc);
            if ((f & UC_NOTE) && !sel && !silent) rect(x, y + sy, 8, 2, col);
            if (f & UC_PLAY) rect(x, y + sy + sh + 1, 8, 2, WHITE);
        }
        rect(198, y + sy + 2, 38, 5, G1);
        if (!silent) rect(198, y + sy + 2, v->level[i] * 38 / 100, 5, sel ? col : G3);
    }
    dials(v, 184, v->toast ? v->toast : v->info, v->toast && v->toast_kind);
}

/* ---------------------------------------------------------------- DRUMS (grid, kit, sound) */
static void page_drums(const ui_view *v)
{
    uint16_t dc = tcol(v);
    char b[24];
    te_header(v, "DRUMS", dc);
    int page = v->page == UI_DRUMS ? 0 : v->dpage ? v->dpage : 1;
    if (gfx_visible(0, 40, GFX_W, 44)) {
        rect(2, 44, 34, 34, dc);
        tsc(19, 53, "KIT", RGB(0, 0, 0));
        { const char *kn = v->sound[v->trk] ? v->sound[v->trk] : "DRUMS"; cut(b, kn, 12, 140); if (wl(b) <= 140) tl(44, 46, b, WHITE); else if (wm(b) <= 140) tm(44, 52, b, WHITE); else ts(44, 54, b, WHITE); }
        ts(194, 41, "GRID", page == 0 ? WHITE : G3);
        ts(194, 55, "KIT", page == 1 ? WHITE : G3);
        ts(194, 69, "SOUND", page == 2 ? WHITE : G3);
        rect(188, 45 + page * 14, 3, 9, dc);
    }
    if (gfx_visible(0, 84, GFX_W, 100)) {               /* KIT, SOUND: the 16 pads, lit on each hit (the GRID: page_drums3) */
        for (int i = 0; i < 16; i++) {
            int x = 2 + (i % 4) * 60, y = 84 + (i / 4) * 25, lit = v->pad_hit == i;
            rect(x, y, 56, 23, lit ? dc : G1);
            if (page == 2 && i == v->drum_sel) frame1(x, y, 56, 23, WHITE);
            if ((v->pad_mute >> i) & 1) rect(x + 3, y + 3, 4, 4, TE_RED);
            tsc(x + 28, y + 4, v->pad_names ? cut(b, v->pad_names[i], 6, 54) : "", lit ? RGB(0, 0, 0) : i == v->drum_sel ? WHITE : G3);
        }
    }
    dials(v, 184, v->toast ? v->toast : v->info, v->toast && v->toast_kind);
}

/* ---------------------------------------------------------------- REC: armed, its count-in, a free take */
static void rec_rows(const ui_view *v, int y0)
{
    int n = v->ntrk ? v->ntrk : 4, rh = n > 4 ? 15 : 19;
    for (int i = 0; i < n; i++) {
        int y = y0 + i * rh, armed = i == v->trk;
        char b[24];
        if (!gfx_visible(0, y, GFX_W, rh)) continue;
        if (armed) { rect(0, y, 240, rh - 1, TE_RED); rect(1, y + 1, 238, rh - 3, BG); }
        rect(4, y + 2, 11, rh - 5, armed ? v->col[i] : dimc(v->col[i]));
        ts(22, y + (rh - 16) / 2, cut(b, v->sound[i], 10, 80), armed ? WHITE : G3);
        for (int j = 0; j < 16; j++) {
            uint8_t f = v->cells[i][j];
            rect(106 + j * 8, y + 4, 6, rh - 8, (f & UC_NOTE) ? (armed ? v->col[i] : dimc(v->col[i])) : (f & UC_OUT) ? BG : G1);
        }
    }
}
static void page_rec(const ui_view *v)
{
    static const char *const TT[5] = { "REC READY", "REC READY", "REC READY", "COUNT-IN", "FREE TAKE" };
    te_header(v, TT[v->rec_kind % 5], TE_RED);
    char b[24];
    if (v->rec_kind == 4) {                              /* a free take: its seconds, the loop it would make */
        if (gfx_visible(0, 42, GFX_W, 106)) {
            int n = v->take_secs > 99 ? 99 : v->take_secs;
            disc(18, 72, 9, v->blink ? TE_RED : dimc(ui_knob_col[3]));
            *put_u(b, (uint32_t)n) = 0; tl(108 - wl(b), 60, b, WHITE);   /* (4.0.1: the seconds in the large type) */
            ts(112, 86, "S", G3);
            rect(132, 48, 1, 54, G1);
            if (v->take_bars) {
                *put_u(b, v->take_bars) = 0;
                tl(146, 46, b, tcol(v));
                ts(146 + wl(b) + 6, 60, v->take_bars == 1 ? "BAR" : "BARS", G3);
                *put_u(b, v->take_bpm) = 0;
                int x = ts(146, 84, b, WHITE); ts(x + 4, 84, "BPM", G3);
            } else tl(146, 46, "-", G3);
            tsc(120, 122, "PRESS REC ON THE 1", TE_RED);
        }
        rec_rows(v, 148);
        if (gfx_visible(0, 224, GFX_W, 16)) { ts(4, 224, "REC: CLOSE", G3); tsr(236, 224, "PLAY: DROP", G3); }
        return;
    }
    if (gfx_visible(0, 42, GFX_W, 64)) {
        static const char *const L1[3] = { "PLAY FREELY", "PLAY A NOTE", "PRESS PLAY" };
        static const char *const L2[3] = { "THEN REC ON THE 1", "IT STARTS THE LOOP", "4 CLICKS, THEN REC" };
        static const char *const L3[3] = { "THE TEMPO FOLLOWS", "PLAY: GO  REC: OFF", "REC: CANCEL" };
        if (v->rec_kind == 3) {
            b[0] = (char)('0' + v->count_n % 10); b[1] = 0; tl(100, 58, b, WHITE);
            disc(60, 73, 12, TE_RED);
            ts(144, 56, "COUNT-IN", G4);
            ts(144, 76, "REC: CANCEL", G3);
        } else {
            int m = v->rec_kind % 3;
            disc(28, 72, 18, v->blink ? TE_RED : dimc(ui_knob_col[3]));
            disc(28, 72, 7, BG);
            ts(60, 48, L1[m], WHITE);
            ts(60, 66, L2[m], G4);
            ts(60, 84, L3[m], G3);
        }
    }
    rec_rows(v, 106);
    dials(v, 184, v->toast ? v->toast : v->info, v->toast && v->toast_kind);
}

/* ---------------------------------------------------------------- the layers: 16 tiles as the 16 white keys */
static void layer_title(const ui_view *v)
{
    if (!gfx_visible(0, 0, GFX_W, 40)) return;
    char b[40];
    uint16_t col = v->title_col ? v->title_col : tcol(v);
    int x = tl(4, 2, v->title, col);
    const char *sub = v->toast ? v->toast : v->info ? v->info : v->sub;
    uint16_t sc = v->toast ? (v->toast_kind ? TE_RED : WHITE) : v->info ? WHITE : G3;
    int lw = ws("LOCK") + 8;                             /* (4.0.2, Sam: PUNCH's FADE beside the title, seen locked too;
                                                            the LOCK on the top line only, the line under it whole) */
    int room = 236 - (x + 10) - (v->locked && !v->head_r[0] ? 46 : 0);
    if (ws(sub) > room && !v->locked) room = 236 - (x + 10);
    if (ws(sub) <= room) ts(x + 10, 18, sub, sc);
    else {                                               /* (long: under the title, from the left) */
        cut(b, sub, 29, room > 0 ? room : 0);
        ts(x + 10, 18, b, sc);
    }
    if (v->locked) { rect(236 - lw, 2, lw, 15, WHITE); ts(240 - lw, 2, "LOCK", RGB(0, 0, 0)); }
    if (v->head_r[0]) ts(x + 10, 2, cut(b, v->head_r, 9, 236 - lw - 6 - (x + 10)), G3);
    rect(0, 38, 240, 1, G1);
}
static void tile_mark_cond(int x, int flags, uint16_t fg, int y)
{
    if (flags & TF_FILL) { rect(x + 4, y + 7, 2, 7, fg); rect(x + 4, y + 7, 5, 2, fg); rect(x + 4, y + 10, 4, 2, fg); }
    else if (flags & TF_NOFILL) for (int d = 0; d < 6; d++) { rect(x + 4 + d, y + 7 + d, 2, 2, fg); rect(x + 9 - d, y + 7 + d, 2, 2, fg); }
}
static void tiles(const ui_view *v)
{
    for (int i = 0; i < 16; i++) {
        const ui_tile *t = &v->tile[i];
        int r = i / 4, c = i % 4, x = 2 + c * 60, y = 40 + r * 36 + 2;
        if (!gfx_visible(x, y, 56, 32) || t->style == T_NONE) continue;
        uint16_t col = t->col ? t->col : tcol(v), bg = G1, fg = G3, top = 0;
        int blink_off = (t->flags & TF_BLINK) && !v->blink;
        switch (t->style) {
        case T_ON: bg = col; fg = RGB(0, 0, 0); break;
        case T_WHITE: bg = WHITE; fg = RGB(0, 0, 0); break;
        case T_RED: bg = TE_RED; fg = WHITE; break;
        case T_FRAME: bg = G1; fg = col; top = col; break;
        case T_DIM: bg = G1; fg = G4; break;
        case T_GAUGE: bg = G1; fg = WHITE; break;
        default: bg = G1; fg = G3; break;               /* T_EMPTY */
        }
        if (blink_off) { bg = G1; fg = col; top = col; }
        if (t->flags & TF_BAR) top = t->style == T_ON ? WHITE : col;
        if (t->flags & TF_PLAY) top = WHITE;
        rect(x, y, 56, 32, bg);
        if (t->style == T_GAUGE) rect(x, y, 56 * t->lvl / 100, 32, col);
        if (t->flags & TF_LEVEL) { rect(x + 4, y + 26, 48, 3, G2); rect(x + 4, y + 26, 48 * t->lvl / 100, 3, t->style == T_ON ? RGB(0, 0, 0) : col); }
        if (top) rect(x, y, 56, 3, top);
        char b[24];
        const char *s = t->text;
        int sp = -1;
        for (int k = 0; s[k]; k++) if (s[k] == ' ') sp = k;
        pend = 72; int narrow = ws(s) <= 54; pend = 0;      /* (one line while it fits, narrowed: TOM H, CRASH) */
        if (narrow || ws(s) <= 54 || sp < 0) tsc(x + 28, y + 7 + ((t->flags & TF_LEVEL) ? -3 : 0), cut(b, s, 7, 56), fg);
        else {                                           /* two words: on two lines */
            int k; for (k = 0; k < sp && k < 23; k++) b[k] = s[k]; b[k] = 0; tsc(x + 28, y, cut(b, b, 7, 56), fg);
            tsc(x + 28, y + 15, cut(b, s + sp + 1, 7, 56), fg);
        }
        if (t->flags & TF_DOT) rect(x + 50, y + 5, 3, 3, fg);
        tile_mark_cond(x, t->flags, fg, y - 2);
    }
}
/* FX held: the 27 keys as a keyboard, four rows of four white keys with the black keys between them */
static void punch_tiles(const ui_view *v)
{
    static const uint8_t AFTER[11] = { 0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14 };   /* the white key each black one follows */
    char b[24];
    for (int r = 0; r < 4; r++) {
        int y0 = 40 + r * 36;
        if (!gfx_visible(0, y0, GFX_W, 36)) continue;
        for (int c = 0; c < 4; c++) {
            const ui_tile *t = &v->tile[r * 4 + c];
            int x = 2 + c * 60, on = t->style == T_WHITE;
            rect(x, y0 + 19, 56, 17, on ? WHITE : G1);
            if (!on) rect(x, y0 + 19, 56, 2, dimc(ui_knob_col[r]));
            tsc(x + 28, y0 + 20, cut(b, t->text, 7, 56), on ? RGB(0, 0, 0) : G4);
        }
        for (int i = 0; i < 11; i++) {
            const ui_tile *t = &v->btile[i];
            int w = AFTER[i], x;
            if (w / 4 != r) continue;
            x = w % 4 < 3 ? 60 * (w % 4 + 1) - 26 : 238 - 52;
            int on = t->style == T_WHITE;
            if (!on) rect(x, y0, 52, 17, G2);
            rect(x + 1, y0 + 1, 50, 15, on ? WHITE : BG);
            tsc(x + 26, y0 + 1, cut(b, t->text, 6, 50), on ? RGB(0, 0, 0) : G3);
        }
    }
}

/* ---------------------------------------------------------------- SONG: the song, line by line */
static void page_song(const ui_view *v)
{
    layer_title(v);
    if (gfx_visible(0, 40, GFX_W, 144)) {
        char b[24];
        for (int i = 0; i < v->sg_n; i++) {
            const ui_songrow *r = &v->sg[i];
            int y = 44 + i * 27, sel = r->flags & SR_SEL;
            rect(2, y, 236, 24, sel ? G2 : G1);
            if (sel) rect(2, y, 3, 24, WHITE);
            *put_u(b, r->num) = 0; ts(10, y + 4, b, sel ? WHITE : G3);
            b[0] = (char)('1' + r->pat); b[1] = 0;
            rect(50, y + 3, 22, 18, r->flags & SR_EMPTY ? G1 : tcol(v)); tsc(61, y + 4, b, r->flags & SR_EMPTY ? G3 : RGB(0, 0, 0));
            char *p = put_u(b, r->bars); const char *u = r->bars == 1 ? " BAR" : " BARS"; while (*u) *p++ = *u++; *p = 0;
            ts(84, y + 4, b, sel ? WHITE : G4);
            if (r->flags & SR_MIX) ts(170, y + 4, "MIX", G3);
            if (r->flags & SR_MUTE) ts(200, y + 4, "OFF", TE_RED);
            if (r->flags & SR_PLAY) { rect(2, y + 22, 236, 2, WHITE); }
        }
        if (!v->sg_n) tsc(120, 100, "EMPTY: SAVE + 14, PLAY", G3);
    }
    dials(v, 184, 0, 0);
}

/* ---------------------------------------------------------------- the menu (HOME held), SLOOP 2.8.5's */
static void page_menu(const ui_view *v)
{
    static const char *const MS[5] = { "SCREEN", "LIGHTS", "AUDIO", "SEQ", "SYSTEM" };
    int sec = v->dpage % 5;
    char b[24];
    if (gfx_visible(0, 0, GFX_W, 21)) {
        const char *m = v->toast ? v->toast : v->info;
        if (m && m[0]) ts(4, 1, cut(b, m, 29, 232), C_HI);
        else { ts(4, 1, "MENU", C_HI); char *p = b; const char *q = MS[sec]; while (*q) *p++ = *q++; *p++ = ' '; p = put_u(p, (uint32_t)sec + 1u); *p++ = '/'; *p++ = '5'; *p = 0; tsr(236, 1, b, C_GRAY); }
        rect(0, H_HEAD, 240, 1, C_LINE);
    }
    int x = 4;
    for (int i = 0; i < 5; i++) {                        /* the sections as tabs: this one lit */
        int w = ws(MS[i]);
        if (i == sec) rect(x - 2, H_HEAD + 3, w + 4, 18, C_LINE);
        ts(x, H_HEAD + 4, MS[i], i == sec ? WHITE : C_DIM);
        x += w + 6;
    }
    for (int r = 0; r < 4; r++) {                        /* KNOB r + 1: its row, the value large */
        const ui_knob *k = &v->knob[r];
        int y = H_HEAD + 26 + r * 50;
        if (!k->label || !k->label[0] || !gfx_visible(0, y, GFX_W, 50)) continue;
        int cur = hot(v, r);
        rect(4, y + 2, 4, 42, cur ? WHITE : ui_knob_col[r]);
        char kk[3] = { 'K', (char)('1' + r), 0 };
        int xv = ts(14, y, kk, ui_knob_col[r]);
        ts(xv + 6, y, cut(b, k->label, 22, 236 - xv - 6), cur ? WHITE : C_GRAY);
        if (k->value[0] && wl(k->value) <= 222) tl(14, y + 16, k->value, cur ? WHITE : C_HI);
        else if (k->value[0]) ts(14, y + 24, k->value, cur ? WHITE : C_HI);
        else ts(14, y + 24, "TURN RIGHT", cur ? WHITE : C_DIM);
    }
}

/* ---------------------------------------------------------------- (4.0.1, Sam) SLOOP 3's sequencer screens: NOTES and the
 * DRUMS grid as SLOOP 3.3.8 drew them, with its dials (the rest keeps SLOOP 2.8.5's layout) */
#define TXT    WHITE
#define DIM    gfx_mix(BG, WHITE, 20)
#define PANEL  G1
#define PANEL2 G2
#define LINE   gfx_mix(BG, WHITE, 9)
#define YEL    RGB(255, 210, 60)
#define FOOT_Y 180
static void fit3(const gfx_font *big, int x, int y, int w, int h, const char *s, uint16_t c, int align)
{
    const gfx_font *f = big;
    int sp = 0;
    if (f == &font_l && gfx_text_w(f, s) > w) f = &font_m;
    if (f == &font_m && gfx_text_w(f, s) > w) f = &font_s;
    if (f == &font_s) while (sp > -2 && (gfx_spacing(sp), gfx_text_w(f, s)) > w) sp--;
    gfx_spacing(sp);
    if (f == &font_s && gfx_text_w(f, s) > w) {           /* (still too wide: the letters condensed, down to 75 %) */
        gfx_spacing(-1);
        for (int c = 92; c >= 62 && (gfx_condense(c), gfx_text_w(f, s)) > w; c -= 4) { }
    }
    char cut[48];
    if (gfx_text_w(f, s) > w) {                           /* (still too wide: cut at its end, never over its neighbours) */
        int n = 0; while (s[n] && n < 47) { cut[n] = s[n]; n++; }
        cut[n] = 0;
        while (n > 1 && gfx_text_w(f, cut) > w) { do n--; while (n > 1 && ((unsigned char)cut[n] & 0xC0) == 0x80); cut[n] = 0; }
        s = cut;
    }
    int tw = gfx_text_w(f, s), tx = align == 0 ? x : align == 1 ? x + (w - tw) / 2 : x + w - tw;
    gfx_text(f, tx, y + (h - f->cap) / 2, s, c);
    gfx_spacing(0); gfx_condense(100);
}
static float sqrt_(float x)                              /* a guess from the exponent, three Newton steps */
{
    if (x <= 0.0f) return 0.0f;
    union { float f; uint32_t u; } r; r.f = x; r.u = (r.u >> 1) + 0x1FC00000u;
    float y = r.f;
    y = 0.5f * (y + x / y); y = 0.5f * (y + x / y); y = 0.5f * (y + x / y);
    return y;
}
static float atan2_(float y, float x)                    /* radians, to about 1e-5 */
{
    float ax = x < 0.0f ? -x : x, ay = y < 0.0f ? -y : y;
    float a = (ax > ay ? ay / (ax + 1e-12f) : ax / (ay + 1e-12f)), s = a * a;
    float r = a * (0.9998660f + s * (-0.3302995f + s * (0.1801410f + s * (-0.0851330f + s * 0.0208351f))));
    if (ay > ax) r = 1.5707963f - r;
    if (x < 0.0f) r = 3.1415927f - r;
    return y < 0.0f ? -r : r;
}
static void dial3(int cx, int cy, int r, int t, int val, uint16_t c, uint16_t needle)
{
    if (!gfx_visible(cx - r - 1, cy - r - 1, 2 * r + 3, 2 * r + 3)) return;
    /* a ring of 270 degrees open at the bottom, lit up to the value: each pixel's distance (the ring, its edges
     * anti-aliased) and its angle (lit or not) */
    float u = (float)val * 0.01f, ro = (float)r, ri = (float)(r - t);
    for (int y = cy - r - 1; y <= cy + r + 1; y++) for (int x = cx - r - 1; x <= cx + r + 1; x++) {
        if (!gfx_visible(x, y, 1, 1)) continue;
        float dx = (float)x + 0.5f - (float)cx, dy = (float)cy - ((float)y + 0.5f), d = sqrt_(dx * dx + dy * dy);
        float cov = d < ri ? d - ri + 1.0f : d > ro ? ro + 1.0f - d : 1.0f;
        if (cov <= 0.0f) continue;
        if (cov > 1.0f) cov = 1.0f;
        float a = atan2_(dy, dx) * 57.29578f;                 /* degrees, from the right, counter-clockwise */
        float pos = (225.0f - (a < -90.0f ? a + 360.0f : a)) / 270.0f;   /* 0 at its start (down left) .. 1 */
        if (pos < 0.0f || pos > 1.0f) continue;               /* the gap at the bottom */
        uint16_t px = pos <= u + 0.004f ? c : gfx_mix(BG, c, 9);
        gfx_fill(x, y, 1, 1, gfx_mix(BG, px, (unsigned)(cov * 32.0f + 0.5f)));
    }
    /* the needle, from the centre */
    float sn, cs; sincos_((225.0f - 270.0f * u) * 0.017453293f, &sn, &cs);
    for (float s = 0.0f; s < ro - 1.5f; s += 0.5f) {
        float fx = (float)cx + cs * s, fy = (float)cy - sn * s;
        gfx_fill((int)(fx - 0.5f), (int)(fy - 0.5f), 2, 2, needle);
    }
}
static void footer3(const ui_view *v)
{
    if (!gfx_visible(0, FOOT_Y, GFX_W, GFX_H - FOOT_Y)) return;
    for (int i = 0; i < 4; i++) {
        const ui_knob *k = &v->knob[i];
        int x = i * 60, top = FOOT_Y;
        if (!k->label || !k->label[0]) continue;
        uint16_t kc = ui_knob_col[i];
        if (k->dial && k->bar >= 0) dial3(x + 30, top + 12, 12, 4, k->bar, kc, k->hot ? YEL : TXT);
        else if (k->dial) gfx_rframe(x + 18, top, 24, 24, 12, 2, gfx_mix(BG, kc, 12));
        else gfx_rfill(x + 8, top + 9, 44, 5, 2, kc);
        fit3(&font_s, x + 2, top + 26, 56, font_s.cap, k->label, k->hot ? YEL : DIM, 1);
        fit3(&font_m, x + 2, top + 40, 56, font_m.cap, k->value, k->hot ? YEL : TXT, 1);   /* (3.3.6: 2 px up, the battery's line under) */
    }
}

static void info_line3(const char *info, int x, int y, int w, int h)
{
    gfx_fill(x, y, w, h, BG);
    gfx_fill(x, y + h - 2, w, 2, YEL);
    const gfx_font *f = gfx_text_w(&font_m, info) <= w - 4 ? &font_m : &font_s;   /* (the larger letters when they fit) */
    fit3(f, x + 2, y + (h - 2 - f->cap) / 2, w - 4, f->cap, info, TXT, 0);
}

/* (4.0.2, Sam) SLOOP 3's MIX (GLO held): its title, a track a column (mute above, solo below), its dials */
#define CYAN RGB(80, 220, 255)
static void layer_title3(const ui_view *v)
{
    if (!gfx_visible(0, 0, GFX_W, 46)) return;
    int e = gfx_text(&font_l, 4, 8, v->title, v->title_col ? v->title_col : TXT), x = e + 8, w = 236 - x;
    if (v->info) { info_line3(v->info, x, 4, w, 34); gfx_fill(4, 41, 232, 1, LINE); return; }
    if (!v->sub[0]) { gfx_fill(4, 41, 232, 1, LINE); return; }
    /* the line beside the title: on one line if it fits, else two, cut at its " · " */
    if (gfx_text_w(&font_s, v->sub) <= w) gfx_text(&font_s, x, 8 + font_l.cap - font_s.cap, v->sub, DIM);
    else {
        char a[40], b[40]; int i = 0, j = 0, cut = -1;
        for (int k = 0; v->sub[k]; k++) if (v->sub[k] == ' ' && (unsigned char)v->sub[k + 1] == 0xC2) { cut = k; break; }
        if (cut < 0) {                                        /* (squeezed if it fits so; else left out) */
            int n = 0; while (v->sub[n]) n++;
            if (gfx_text_w(&font_s, v->sub) - 2 * n <= w) fit3(&font_s, x, 8 + font_l.cap - font_s.cap, w, font_s.cap, v->sub, DIM, 0);
            gfx_fill(4, 41, 232, 1, LINE); return;
        }
        for (; i < cut && i < 39; i++) a[i] = v->sub[i];
        a[i] = 0;
        const char *r = v->sub + cut + 4;                     /* past " · " (the dot is two bytes) */
        while (*r == ' ') r++;
        for (; r[j] && j < 39; j++) b[j] = r[j];
        b[j] = 0;
        fit3(&font_s, x, 6, w, font_s.cap, a, DIM, 0);
        fit3(&font_s, x, 23, w, font_s.cap, b, DIM, 0);
    }
    gfx_fill(4, 41, 232, 1, LINE);
}
static void tiles3(const ui_view *v)
{
    int nc = v->cols == 5 ? 5 : 4, pitch = nc == 5 ? 47 : 59, tw = nc == 5 ? 44 : 56;
    for (int i = 0; i < 16; i++) {
        const ui_tile *t = &v->tile[i];
        int x = 3 + (i % nc) * pitch, y = 48 + (i / nc) * 33, w = tw, h = 30;
        if (!gfx_visible(x - 4, y - 4, w + 8, h + 8) || t->style == T_NONE) continue;
        uint16_t c = t->col ? t->col : TXT, fill = PANEL, tc = DIM;
        switch (t->style) {
        case T_ON: fill = c; tc = BG; break;
        case T_DIM: fill = PANEL2; tc = TXT; break;
        case T_FRAME: fill = PANEL; tc = c; break;
        case T_WHITE: fill = TXT; tc = BG; break;
        case T_RED: fill = TE_RED; tc = TXT; break;
        default: break;
        }
        if (t->style == T_GAUGE) {                            /* (3.3.8) a hold, a save: filled from the left */
            int gw = w * (t->lvl > 100 ? 100 : t->lvl) / 100;
            gfx_rfill(x, y, w, h, 4, PANEL2);
            if (gw > 0) gfx_rfill(x, y, gw < 8 ? 8 : gw, h, 4, c);
            gfx_rframe(x, y, w, h, 4, 2, c);
            tc = t->lvl >= 50 ? BG : TXT;
        } else gfx_rfill(x, y, w, h, 4, fill);
        if (t->flags & TF_BAR) gfx_fill(x + 4, y, w - 8, 3, c);
        if (t->style == T_FRAME) gfx_rframe(x, y, w, h, 4, 2, (t->flags & TF_BLINK) && !v->blink ? dimc(c) : c);
        if (t->flags & TF_PLAY) gfx_rframe(x, y, w, h, 4, 2, TXT);
        if (t->flags & TF_LEVEL) {                            /* the track's level, as GLOBAL's faders lying down */
            uint16_t lc = t->style == T_ON ? BG : c;
            gfx_fill(x + 4, y + h - 6, w - 8, 3, t->style == T_ON ? dimc(c) : PANEL2);
            gfx_fill(x + 4, y + h - 6, (w - 8) * (t->lvl > 100 ? 100 : t->lvl) / 100, 3, lc);
        }
        fit3(&font_m, x + 3, y, w - 6, t->flags & TF_LEVEL ? h - 7 : h, t->text, tc, 1);
        uint16_t mk = t->style == T_ON || t->style == T_WHITE ? BG : YEL;
        if (t->flags & TF_DOT) gfx_fill(x + w - 8, y + 4, 4, 4, mk);
        if (t->flags & TF_FILL) gfx_fill(x + 4, y + 4, 4, 4, t->style == T_ON ? BG : CYAN);
        if (t->flags & TF_NOFILL) { gfx_fill(x + 4, y + 4, 4, 4, t->style == T_ON ? BG : CYAN); gfx_fill(x + 9, y + 4, 4, 4, t->style == T_ON ? BG : CYAN); }
    }
}

static void page_drums3(const ui_view *v)
{
    /* every sound of the kit, its 16 steps: the sound the keys write on in its colour, the others dim */
    if (gfx_visible(0, 0, GFX_W, 46)) {
        gfx_text(&font_l, 4, 8, v->drum_sheet ? v->page_name : "DRUMS", v->col[v->trk]);
        if (v->pad_names) gfx_text_r(&font_m, 236, 8 + font_l.cap - font_m.cap, v->pad_names[v->drum_sel & 15], TXT);
        gfx_fill(4, 41, 232, 1, LINE);
    }
    for (int r = 0; r < 16; r++) {
        int y = 46 + r * 8;
        if (!gfx_visible(0, y - 3, GFX_W, 11)) continue;
        int sel = r == v->drum_sel;
        if (sel) gfx_fill(0, y - 1, GFX_W, 9, PANEL);
        for (int i = 0; i < 16; i++) {
            int x = 8 + i * 14 + (i / 4) * 2, on = (v->drum_row[r] >> i) & 1;
            uint16_t f = on ? (sel ? v->col[v->trk] : TXT) : (i == v->drum_at ? LINE : PANEL2);
            if (!on && sel) f = PANEL2;
            if (on && sel) {                                 /* (3.3.8) the sound picked: its level a shade, GHOST darkest */
                static const uint8_t SH[4] = { 12, 18, 25, 32 };
                int lv = (int)(v->drum_lvl >> (2 * i)) & 3;
                f = gfx_mix(PANEL2, v->col[v->trk], SH[lv]);
            }
            gfx_fill(x, y, 12, 7, f);
            if (on && sel) { int rt = (int)(v->drum_rat >> (2 * i)) & 3; for (int j = 0; j < rt; j++) gfx_fill(x + 2 + j * 3, y + 5, 2, 2, BG); }   /* its ratchets: notches */
            if (sel && i == v->drum_cur) gfx_rframe(x - 1, y - 1, 14, 9, 1, 1, TXT);   /* (3.3.8) the step picked */
            if (i == v->drum_at && r == 0) gfx_fill(x, y - 3, 12, 2, TXT);   /* the playhead, above the grid */
        }
    }
}
static void page_notes3(const ui_view *v)
{
    /* the whole screen, as DRUMS: the page's name in the track's colour, the track's number beside, then the roll
     * from under the title to the dials; the black keys' rows a shade darker, each C ruled and named */
    uint16_t c = v->col[v->trk];
    if (gfx_visible(0, 0, GFX_W, 46)) {
        fit3(&font_l, 4, 8, 190, font_l.cap, v->page_name, c, 0);
        gfx_rfill(204, 6, 32, 30, 6, c);
        char n[2] = { (char)('1' + v->trk), 0 };
        gfx_text_c(&font_m, 220, 6 + (30 - font_m.cap) / 2, n, BG);
        gfx_fill(4, 41, 232, 1, LINE);
    }
    const int x0 = 30, w = 206, y0 = 45, h = FOOT_Y - 4 - y0;
    if (!gfx_visible(0, y0 - 2, GFX_W, h + 4)) return;
    int lo = v->nr_lo, hi = v->nr_hi > v->nr_lo ? v->nr_hi : v->nr_lo + 1, rows = hi - lo + 1, rh = h / rows;
    if (rh < 3) rh = 3;
    int top = y0 + (h - rh * rows) / 2;
    /* (3.3.8) the lines by the track's DIV: a beat (16T: every 6 steps, 8T: 3), the bars darker; many steps: a bar */
    int cols = v->nr_cols ? v->nr_cols : 16, bar = v->nr_bar ? v->nr_bar : 16, rule = bar % 4 ? bar / 2 : bar / 4;
    if (rule < 1) rule = 1;
    if (cols > 32 || cols / rule > 16) rule = bar;
    for (int n = lo; n <= hi; n++) {
        int y = top + (hi - n) * rh, k = n % 12;
        int black = k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
        gfx_fill(x0, y, w, rh, black ? BG : PANEL);
        if (k == 0) gfx_fill(x0, y + rh - 1, w, 1, LINE);
    }
    if (v->nr_cur < cols) { int cx = x0 + v->nr_cur * w / cols, cw = x0 + (v->nr_cur + 1) * w / cols - cx;
        gfx_fill(cx, top, cw < 2 ? 2 : cw, rh * rows, gfx_mix(PANEL2, TXT, 5)); }
    for (int b = 0; b <= cols; b += rule) gfx_fill(x0 + b * w / cols, top, 1, rh * rows, b % bar ? LINE : DIM);
    /* the names: each C, else the lowest; the edited note's row in white */
    static const char NM[12][3] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int named = 0, sel_p = -1;
    for (int i = 0; i < v->nr_n && i < UI_NOTES_MAX; i++) if (v->nr[i].flags & NR_SEL) sel_p = v->nr[i].pitch;
    int ys[16], ny = 0;                                   /* (the edited note's name first; no name over another: 3.1.2) */
    for (int pass = 0; pass < 2; pass++) for (int n = lo; n <= hi; n++) {
        if (pass == 0 ? n != sel_p : (n == sel_p || (n % 12 && !(n == lo && !named && hi - lo < 12)))) continue;
        if (n % 12 == 0) named = 1;
        char b[8], *p = b; { const char *q = NM[n % 12]; while (*q) *p++ = *q++; }   /* (the octave below C0: -1, 3.1.4) */
        if (n < 12) { *p++ = '-'; *p++ = '1'; } else p = put_u(p, (uint32_t)(n / 12 - 1));
        *p = 0;
        int y = top + (hi - n) * rh + rh / 2 - font_s.cap / 2;
        if (y < y0) y = y0;
        if (y > top + rows * rh - font_s.cap) y = top + rows * rh - font_s.cap;
        int clash = 0; for (int i = 0; i < ny; i++) if (y - ys[i] < font_s.cap + 2 && ys[i] - y < font_s.cap + 2) clash = 1;
        if (clash || ny >= 16) continue;
        ys[ny++] = y;
        fit3(&font_s, 0, y, 28, font_s.cap, b, n == sel_p ? TXT : DIM, 2);
    }
    for (int i = 0; i < v->nr_n && i < UI_NOTES_MAX; i++) {
        const ui_note *n = &v->nr[i];
        if (n->pitch < lo || n->pitch > hi) continue;
        int x = x0 + n->x * w / 64, nw = n->len * w / 64, y = top + (hi - n->pitch) * rh;
        if (nw < 4) nw = 4;
        if (x + nw > x0 + w) nw = x0 + w - x;
        int bh = rh > 3 ? rh - 1 : rh, by = y;
        if (n->flags & NR_GHOST) gfx_rframe(x, by, nw, bh, 2, 1, c);
        else gfx_rfill(x, by, nw, bh, 2, (n->flags & NR_SOFT) ? gfx_mix(BG, c, 18) : c);
        if (n->flags & NR_ACC) gfx_fill(x + 1, by, nw - 2, bh > 5 ? 3 : 1, TXT);
        if (n->flags & NR_SLIDE) gfx_fill(x + nw, by + bh / 2, 8, 2, TXT);
        if (n->flags & NR_SEL) gfx_rframe(x - 2, by - 2, nw + 4, bh + 4, 3, 2, TXT);
    }
    if (v->nr_play < 64) gfx_fill(x0 + v->nr_play * w / 64, top, 2, rh * rows, GREEN);
}

/* ---------------------------------------------------------------- the big SAVED (3.3.8, Sam: a save you can see) */
static void saved_banner(const ui_view *v)
{
    if (!v->saved || !gfx_visible(4, 2, 232, 50)) return;
    uint16_t c = v->saved == 1 ? GREEN : TE_RED;
    gfx_rfill(4, 2, 232, 50, 6, c);
    if (v->saved == 1) {
        for (int d = -40; d <= 40; d += 8) {
            gfx_line(30 * 16, 27 * 16 + d, 40 * 16, 37 * 16 + d, BG);
            gfx_line(40 * 16, 37 * 16 + d, 58 * 16, 15 * 16 + d, BG);
        }
        tl(136 - wl("SAVED") / 2, 11, "SAVED", BG);
    } else tl(120 - wl("SAVE FAILED") / 2, 11, "SAVE FAILED", WHITE);
}

void ui_draw(const ui_view *v)
{
    gfx_spacing(0); gfx_condense(100);
    if (v->page == UI_VIS) {
        vis_draw();
        const char *m = v->toast ? v->toast : v->info;
        if (m && m[0] && gfx_visible(0, 0, GFX_W, 20)) { char b[32]; rect(0, 0, 240, 20, BG); ts(4, 2, cut(b, m, 29, 232), C_HI); }
        saved_banner(v);
        return;
    }
    if (v->page == UI_SPLASH) { vis_splash_draw(); return; }
    gfx_fill(0, 0, GFX_W, GFX_H, BG);
    switch (v->page) {
    case UI_TRACKS: page_tracks(v); break;
    case UI_LAYER: if (v->cols == 5) { layer_title3(v); tiles3(v); footer3(v); } else { layer_title(v); tiles(v); dials(v, 184, 0, 0); } break;
    case UI_PUNCH: layer_title(v); punch_tiles(v); dials(v, 184, 0, 0); break;
    case UI_DRUMS: page_drums3(v); footer3(v); if ((v->toast || v->info) && gfx_visible(0, 0, GFX_W, 40)) info_line3(v->toast ? v->toast : v->info, 0, 2, GFX_W, 36); break;
    case UI_NOTES: page_notes3(v); footer3(v); if ((v->toast || v->info) && gfx_visible(0, 0, GFX_W, 40)) info_line3(v->toast ? v->toast : v->info, 0, 2, GFX_W, 36); break;
    case UI_KIT: if (is_drum_v(v)) page_drums(v); else page_frame(v); break;
    case UI_REC: page_rec(v); break;
    case UI_SONG: page_song(v); break;
    case UI_MENU: page_menu(v); break;
    default: page_frame(v); break;
    }
    saved_banner(v);
}
