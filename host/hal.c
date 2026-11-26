/* SPDX-License-Identifier: GPL-3.0-only
 * The PC twin's FM-1 (host/twin.c): the board's parts as the firmware sees them (hal/fm1/*.h), in simulated time.
 *   the flash: 1 MB, NOR (a write only clears bits; an erase sets a sector to FF), erases and writes that take time
 *     and keep the window off meanwhile; the window: a copy of the flash at flash 0x4000, read only through its cache
 *     (a rewritten line reads stale until flash_xip_forget), enciphered below flash_plain_from (what the firmware
 *     wrote there itself reads back as garbage, as on the FM-1), and unreadable while the window is off (a read
 *     then stops the twin: what SLOOP 3's rule forbids);
 *   the screen (240 x 240), the panel (buttons, keys, knobs, LEDs), the audio's two halves, the second core (its job
 *   run when waited for), USB MIDI (a queue each way), the pot. */
#define _GNU_SOURCE
#include <pthread.h>
#include "twin.h"
#include <execinfo.h>
#include "../hal/fm1/board.h"
#include "../hal/fm1/audio.h"
#include "../hal/fm1/cpu1.h"
#include "../hal/fm1/adc.h"
#include "../hal/fm1/panel.h"
#include "../hal/fm1/flash.h"
#include "../hal/fm1/usb.h"
#include "../hal/fm1/uart.h"
#include "../hal/fm1/irq.h"
#include "../hal/fm1/sys.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

/* ---- time */
uint64_t tw_ns;
static uint32_t lsb_mhz = 60;
/* 3.3.3: the watchdog (hal/fm1/sys.c): armed, ~4 s without a feed is a restart on the FM-1: here, the end, said */
static int wdt_on; static uint64_t wdt_ns;
uint32_t tw_wdt_feeds;
static void wdt_check(void) { if (wdt_on && tw_ns - wdt_ns > 4000000000ull) { wdt_on = 0; fprintf(stderr, "!! TWIN: the watchdog: no feed for 4 s (the FM-1 would restart)\n"); twin_end(6); } }
#include <execinfo.h>
static uint64_t next_half_v(void);
static void adv(uint32_t ns)
{
    tw_ns += ns; wdt_check(); twin_tick();
    if (getenv("TWIN_ADV_TRACE")) { static uint64_t told; uint64_t due = next_half_v(); if (due && tw_ns > due && told != due) { told = due; void *f[14]; int n = backtrace(f, 14); printf("ADV-LATE %u ns\n", ns); fflush(stdout); backtrace_symbols_fd(f, n, 1); } }
}
void tw_draw_cost(void) { static int us = -1; if (us < 0) { const char *e = getenv("TWIN_DRAW_US"); us = e ? atoi(e) : 0; } if (us) adv((uint32_t)us * 1000u); }
/* 3.3.4: the timer read before time_init started it: on the FM-1 a wait that never ends (3.3.3's black screen) */
static int time_on;
static void time_check(void) { if (!time_on) { fprintf(stderr, "!! TWIN: the timer used before time_init (on the FM-1: it may not run, a wait never ends)\n"); twin_end(7); } }
static pthread_t fw_thread; static int fw_thread_set;     /* (the firmware's own thread: the scenario's is not checked) */
void time_init(uint32_t m) { lsb_mhz = m; time_on = 1; fw_thread = pthread_self(); fw_thread_set = 1; }
/* TWIN_DENORM=1 (3.1.8): the FPU traps on a denormal operand and on an underflow; each place is counted once per
 * arming (the clock's reads arm it: several times a block) and listed at the end, for addr2line. The firmware's own
 * loop, the sequencer, the screen: what tests/denorm_test.c does for the sounds alone. */
#if defined(__x86_64__) && defined(__linux__)
#include <signal.h>
#include <ucontext.h>
#include <xmmintrin.h>
static int dn_on = -1; static unsigned long dn_site[64]; static long dn_hits[64]; static int dn_n;
static void dn_fpe(int sig, siginfo_t *si, void *uc_)
{
    ucontext_t *uc = uc_; unsigned long ip = (unsigned long)uc->uc_mcontext.gregs[REG_RIP];
    int i = 0; for (; i < dn_n; i++) if (dn_site[i] == ip) break;
    if (i == dn_n && dn_n < 64) dn_site[dn_n++] = ip;
    if (i < 64) dn_hits[i]++;
    uc->uc_mcontext.fpregs->mxcsr |= 0x0100 | 0x0800; uc->uc_mcontext.fpregs->mxcsr &= ~0x003Fu;
    (void)sig; (void)si;
}
void tw_denormals(void) { if (dn_on <= 0) return; for (int i = 0; i < dn_n; i++) { extern char __executable_start; printf("TWIN-DENORMAL %#lx %ld\n", dn_site[i] - (unsigned long)&__executable_start, dn_hits[i]); } printf("TWIN-DENORMALS: %d places\n", dn_n); fflush(stdout); }
static void dn_arm(void)
{
    if (dn_on < 0) {
        dn_on = getenv("TWIN_DENORM") != 0;
        if (dn_on) { struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_sigaction = dn_fpe; sa.sa_flags = SA_SIGINFO; sigaction(SIGFPE, &sa, 0); }
    }
    if (dn_on) _mm_setcsr((_mm_getcsr() & ~(0x0100u | 0x0800u)) & ~0x003Fu);
}
#else
static void dn_arm(void) {}
void tw_denormals(void) {}
#endif
uint32_t time_ticks(void) { time_check(); dn_arm(); adv(50); return (uint32_t)(tw_ns * lsb_mhz / 1000u); }
uint32_t time_mhz(void) { return lsb_mhz; }
void time_wait_us(uint32_t us) { time_check(); adv(us * 1000u); }

/* ---- the flash and its window */
uint8_t tw_flash[TW_FLASH];
static uint8_t rt_written[TW_FLASH / 8];      /* bytes the firmware wrote (bit), for the cipher below plain_from */
uint8_t *host_win;                            /* the window: flash 0x4000.. at host_win */
static size_t win_len;
static uint32_t plain_from = 0xFFFFFFFFu;
static int win_off, busy_kind; static uint64_t busy_until; static uint32_t busy_addr, busy_n;
static uint8_t busy_old[4096];                /* what the operation under way replaces (a power cut: partly kept) */
static uint8_t stale[TW_FLASH / 4096];        /* a sector of the window: rewritten since its lines were read */
int tw_window_reads_while_off, tw_stale_reads;
static int rtw(uint32_t a) { return (rt_written[a >> 3] >> (a & 7)) & 1; }
static void win_refresh(uint32_t a, uint32_t n)          /* the window's view of flash [a, a + n) */
{
    if (a < 0x4000u) { if (a + n <= 0x4000u) return; n -= 0x4000u - a; a = 0x4000u; }
    for (uint32_t i = 0; i < n && a + i < TW_FLASH; i++) {
        uint32_t f = a + i;
        host_win[f - 0x4000u] = (uint8_t)(f < plain_from && rtw(f) ? tw_flash[f] ^ (uint8_t)(0x5Au + f * 7u) : tw_flash[f]);
    }
}
static void protect(void)                                /* the window as it can be read now */
{
    if (win_off) { mprotect(host_win, win_len, PROT_NONE); return; }
    mprotect(host_win, win_len, PROT_READ);
    for (uint32_t s = 4; s < TW_FLASH / 4096; s++) if (stale[s]) mprotect(host_win + (s - 4) * 4096u, 4096, PROT_NONE);
}
static void on_segv(int sig, siginfo_t *si, void *uc)
{
    (void)uc;
    uint8_t *p = (uint8_t *)si->si_addr;
    if (p >= host_win && p < host_win + win_len) {
        uint32_t f = (uint32_t)(p - host_win) + 0x4000u;
        if (win_off) {
            tw_window_reads_while_off++;
            char b[160]; int n = snprintf(b, sizeof b, "TWIN: the window read while it is off (flash 0x%05X): a breach of the window rule\n", f);
            write(2, b, (size_t)n);
            { void *bt[24]; int k = backtrace(bt, 24); backtrace_symbols_fd(bt, k, 2); }      /* (who read it: addr2line) */
            twin_fail_hook(); _exit(70);
        }
        uint32_t s = f / 4096u;
        if (stale[s]) {                          /* a line rewritten and not forgotten: what the cache may still hold */
            tw_stale_reads++;
            char b[160]; int n = snprintf(b, sizeof b, "TWIN: the window read flash 0x%05X, rewritten since, without flash_xip_forget\n", f);
            write(2, b, (size_t)n);
            stale[s] = 0; mprotect(host_win + (s - 4) * 4096u, 4096, PROT_READ | PROT_WRITE);
            win_refresh(s * 4096u, 4096); mprotect(host_win + (s - 4) * 4096u, 4096, PROT_READ);
            return;
        }
    }
    signal(sig, SIG_DFL); raise(sig);
}
void tw_flash_init(void)
{
    win_len = TW_FLASH - 0x4000u;
    host_win = mmap(0, win_len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memset(rt_written, 0, sizeof rt_written); memset(stale, 0, sizeof stale);
    mprotect(host_win, win_len, PROT_READ | PROT_WRITE); win_refresh(0x4000u, win_len); protect();
    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_sigaction = on_segv; sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGSEGV, &sa, 0); sigaction(SIGBUS, &sa, 0);
}
static void win_set(int off) { if (win_off != off) { win_off = off; protect(); } }
static void finish(void)                                 /* an erase or a write done */
{
    busy_kind = 0;
}
int tw_flash_busy_now(void) { return busy_kind && tw_ns < busy_until; }
/* a power cut now: the operation under way left half done (an erase: part of the sector still old; a write: its
 * bytes partly in) */
void tw_flash_cut(void)
{
    if (!busy_kind || tw_ns >= busy_until) return;
    for (uint32_t i = 0; i < busy_n; i++) if (rand() & 1) tw_flash[busy_addr + i] = busy_old[i];
    busy_kind = 0;
}
int flash_open(void) { return 0; }
uint32_t flash_jedec(void) { return 0x00EF4014u; }
int flash_read(uint32_t a, void *dst, uint32_t n)
{
    if (a > TW_FLASH || n > TW_FLASH - a) return -1;
    while (flash_busy()) { }
    adv(n * 400u);                                       /* (20 MHz SPI: 0.4 us a byte) */
    memcpy(dst, tw_flash + a, n);
    return 0;
}
int flash_busy(void)
{
    adv(2000);
    if (!busy_kind) { win_set(0); return 0; }
    if (tw_ns < busy_until) return 1;
    finish(); win_set(0); return 0;
}
void flash_wait_idle(void) { while (flash_busy()) { } }
/* (3.3.8) the window off, the code that started the work must be RAM's: the twin runs the store's window functions
 * (XT) natively, so a call from one is caught here (3.3.8's store_proj_prep: the FM-1 froze at the logo) */
extern char __start_tw_xt[] __attribute__((weak)), __stop_tw_xt[] __attribute__((weak));
static void from_ram(const void *ret, const char *what)
{
    if (__start_tw_xt && (const char *)ret >= __start_tw_xt && (const char *)ret < __stop_tw_xt) {
        fprintf(stderr, "TWIN: %s started from a window function (XT, %p): it cannot go on with the window off\n", what, ret);
        twin_fail_hook(); exit(70);
    }
}
/* (3.4.1) a window function (tw_xt: the window's files, COLD, XT) entered while the window is off: on the FM-1 the
 * CPU fetches it from a flash that is not there (a freeze, or a restart by the watchdog). Every call into the twin's
 * code is seen (-finstrument-functions, Makefile) */
__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void *fn, void *site)
{
    if (win_off && __start_tw_xt && (char *)fn >= __start_tw_xt && (char *)fn < __stop_tw_xt && fw_thread_set &&
        pthread_equal(pthread_self(), fw_thread)) {
        char b[160]; int n = snprintf(b, sizeof b, "TWIN: a window function (%p, called from %p) ran with the window off: the FM-1 would freeze\n", fn, site);
        write(2, b, (size_t)n);
        { void *bt[24]; int k = backtrace(bt, 24); backtrace_symbols_fd(bt, k, 2); }
        twin_fail_hook(); _exit(72);
    }
}
__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void *fn, void *site) { (void)fn; (void)site; }
int flash_erase_start(uint32_t a)
{
    from_ram(__builtin_return_address(0), "an erase");
    if ((a & 4095u) || a >= TW_FLASH || flash_busy()) return -1;
    if (a < 0x4000u) { fprintf(stderr, "TWIN: an erase of m-bootloader's sector 0x%X\n", a); twin_fail_hook(); exit(71); }
    twin_flash_op(1, a, 4096);
    memcpy(busy_old, tw_flash + a, 4096); busy_n = 4096;
    memset(tw_flash + a, 0xFF, 4096);
    for (uint32_t i = 0; i < 4096u; i++) rt_written[(a + i) >> 3] |= (uint8_t)(1u << ((a + i) & 7));
    if (a >= 0x4000u) stale[a / 4096u] = 1;
    static int ems = -1; if (ems < 0) { const char *e = getenv("TWIN_ERASE_MS"); ems = e ? atoi(e) : 45; }   /* (3.4.1: a slower part's) */
    win_set(1); busy_kind = 1; busy_addr = a; busy_until = tw_ns + (uint64_t)ems * 1000000ull;   /* 45 ms */
    return 0;
}
int flash_program_start(uint32_t a, const void *src, uint32_t n)
{
    from_ram(__builtin_return_address(0), "a write");
    if (!n || (a & 255u) + n > 256u || a + n > TW_FLASH || flash_busy()) return -1;
    if (a < 0x4000u) { fprintf(stderr, "TWIN: a write to m-bootloader at 0x%X\n", a); twin_fail_hook(); exit(71); }
    twin_flash_op(2, a, n);
    memcpy(busy_old, tw_flash + a, n); busy_n = n;
    const uint8_t *p = src;
    for (uint32_t i = 0; i < n; i++) {
        if ((tw_flash[a + i] & p[i]) != p[i]) twin_flash_unerased(a + i);
        tw_flash[a + i] &= p[i]; rt_written[(a + i) >> 3] |= (uint8_t)(1u << ((a + i) & 7));
    }
    if (a >= 0x4000u) stale[a / 4096u] = 1;
    win_set(1); busy_kind = 2; busy_addr = a; busy_until = tw_ns + 700000ull + n * 3000ull;
    return 0;
}
int flash_xip_on(void) { return !win_off; }
void flash_plain_from(uint32_t from)
{
    plain_from = from;
    mprotect(host_win, win_len, PROT_READ | PROT_WRITE);
    win_refresh(0x4000u, win_len);       /* (the cipher's range changed: its lines read again) */
    memset(stale, 0, sizeof stale);
    protect();
}
void flash_xip_forget(uint32_t off, uint32_t len)
{
    mprotect(host_win, win_len, PROT_READ | PROT_WRITE);
    win_refresh(off, len);
    for (uint32_t s = off / 4096u; s <= (off + len - 1u) / 4096u && s < TW_FLASH / 4096u; s++) {
        int whole = s * 4096u >= off && s * 4096u + 4096u <= off + len;
        if (whole) stale[s] = 0;
        else win_refresh(s * 4096u, 4096), stale[s] = 0;
    }
    protect();
}

/* ---- the screen */
uint16_t tw_lcd[240 * 240];
static uint64_t tile_until;
void lcd_init(void) { memset(tw_lcd, 0, sizeof tw_lcd); }
void lcd_fill(int x, int y, int w, int h, uint16_t c)
{
    for (int j = y; j < y + h && j < 240; j++) for (int i = x; i < x + w && i < 240; i++) if (i >= 0 && j >= 0) tw_lcd[j * 240 + i] = c;
}
void lcd_text(int col, int row, const char *s, uint16_t fg, uint16_t bg) { (void)col; (void)row; (void)s; (void)fg; (void)bg; }
void lcd_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale) { (void)x; (void)y; (void)c; (void)fg; (void)bg; (void)scale; }
void lcd_tile_start(int x, int y, int w, int h, const void *px)
{
    const uint8_t *b = px;
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
        int X = x + i, Y = y + j; const uint8_t *q = b + 2 * (j * w + i);
        if (X >= 0 && X < 240 && Y >= 0 && Y < 240) tw_lcd[Y * 240 + X] = (uint16_t)(q[0] << 8 | q[1]);
    }
    tile_until = tw_ns + (uint64_t)w * h * 16u * 1000u / 12u;   /* (the FM-1's SPI: 12 MHz, hal/fm1/board.c; 40 until 3.1.6, which
                                                                    hid that a whole screen takes 77 ms to send) */
}
int lcd_tile_busy(void) { adv(3000); return tw_ns < tile_until; }
void keys_init(void) { }
int key_down(uint32_t c) { (void)c; return 0; }

/* ---- the panel */
uint32_t tw_btn, tw_keys; int tw_enc[7]; uint8_t tw_led[41];
static uint32_t btn_seen, keys_seen; static int steps;
void panel_init(void) { }
int panel_step(void) { adv(60000); return ++steps % 11 == 0; }
void panel_dark(void) { }
void panel_ticking(void) { }
void panel_tick(void) { }
void panel_glow_us(int us) { (void)us; }
uint32_t panel_keys(void) { return tw_keys; }
uint32_t panel_buttons(void) { return tw_btn; }
uint32_t panel_key_edges(uint32_t *rel) { uint32_t p = tw_keys & ~keys_seen; *rel = keys_seen & ~tw_keys; keys_seen = tw_keys; return p; }
uint32_t panel_button_edges(uint32_t *rel) { uint32_t p = tw_btn & ~btn_seen; *rel = btn_seen & ~tw_btn; btn_seen = tw_btn; return p; }
int panel_enc_take(int e) { int v = tw_enc[e]; tw_enc[e] = 0; return v; }
void panel_led(int id, int on) { if (id >= 0 && id < 41) tw_led[id] = (uint8_t)on; }
uint8_t tw_glow[41]; uint32_t tw_glow_offs;
void panel_glow(int id) { if (id >= 0 && id < 41) tw_glow[id] = 1; }
uint8_t tw_lvl[41];
void panel_glow_clear(void) { for (int i = 0; i < 41; i++) { tw_glow[i] = 0; tw_lvl[i] = 0; } }
void panel_level(int id, int lvl) { if (id < 0 || id >= 41) return; tw_lvl[id] = (uint8_t)lvl; tw_led[id] = (uint8_t)(lvl >= 4); tw_glow[id] = (uint8_t)(lvl == 1); }
void panel_glow_off(void) { tw_glow_offs++; }
int panel_map(const uint8_t btn[14], const uint8_t enc[7], const int8_t dir[7])
{
    for (int i = 0; i < 14; i++) if (btn[i] > 13) return -1;
    for (int i = 0; i < 7; i++) if (enc[i] > 6 || (dir[i] != 1 && dir[i] != -1)) return -1;
    return 0;
}
uint32_t panel_raw_buttons(void) { return tw_btn; }
int panel_raw_enc_take(int e) { return panel_enc_take(e); }

/* ---- the audio */
static int32_t *abuf; static uint32_t ahalf; static int afill; static uint64_t next_half;
#define HALF_NS(n) ((uint64_t)(n) / 2u * 544u * 1000u / 24u)  /* (n words: n / 2 frames at 24 MHz / 544) */
int audio_start(int32_t *buf, uint32_t half_words) { abuf = buf; ahalf = half_words; afill = 0; next_half = tw_ns; return AUDIO_OK; }
int audio_half_free(void) { return abuf && tw_ns >= next_half; }
static uint64_t next_half_v(void) { return abuf ? next_half + HALF_NS(ahalf) : 0; }
uint64_t tw_half_cpu_ns;                       /* the host's time to render the last half (the cost's proxy) */
static uint64_t half_t0;
static uint64_t cpu_ns(void) { struct timespec t; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t); return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec; }
/* (3.4.1) TWIN_HALF_US, or tw_half_us (a scenario's): each half's sound costs that much CPU time (a heavy project:
 * the load guard, CPU OVERLOAD) */
uint32_t tw_half_us; int tw_quiet_half;
int32_t *audio_free_half(void)
{
    static int us = -1; if (us < 0) { const char *e = getenv("TWIN_HALF_US"); us = e ? atoi(e) : 0; }
    half_t0 = cpu_ns();
    if (tw_quiet_half) tw_quiet_half = 0;
    else if (tw_half_us) adv((uint32_t)tw_half_us * 1000u); else if (us) adv((uint32_t)us * 1000u);
    return abuf + (uint32_t)afill * ahalf;
}
int audio_playing_half(void) { return afill ^ 1; }
uint32_t tw_underruns; uint64_t tw_underrun_at;
void tw_charge(uint32_t ns) { if (ns) adv(ns); }
/* (3.4.3) a scenario's erase (oversave) started by the firmware's thread at its main loop's top, where the firmware
 * starts its own (store_step): the scenario's thread gets turns inside the interface's round too (audio_yield), where
 * the firmware never leaves the window off */
volatile uint32_t tw_erase_at; volatile int tw_erase_r;
void tw_loop_top(void) { uint32_t a = tw_erase_at; if (a) { tw_erase_r = flash_erase_start(a); tw_erase_at = 0; } }
void audio_half_done(void)
{
    tw_half_cpu_ns = cpu_ns() - half_t0;
    if (tw_ns > 3000000000ull && tw_ns > next_half + HALF_NS(ahalf)) { tw_underruns++; tw_underrun_at = tw_ns; if (tw_underruns <= 12) printf("[%8.3f] !! TWIN: a half %.2f ms late, in %s\n", (double)tw_ns * 1e-9, (double)(tw_ns - next_half - HALF_NS(ahalf)) * 1e-6, tw_job_name()); }   /* (3.4.3: due before the other half ended) */
    twin_audio(abuf + (uint32_t)afill * ahalf, ahalf / 2u);
    afill ^= 1;
    next_half += HALF_NS(ahalf);
    if (next_half + HALF_NS(ahalf) * 4u < tw_ns) next_half = tw_ns;   /* (far behind: the twin's own pauses) */
}
void audio_stop(void) { abuf = 0; }
uint32_t audio_rate_milli(void) { return 44117647u; }

/* ---- the second core: its job done when it is waited for; or (TWIN_CPU1=thread) on a thread of its own, at the
 * same time as the first core's part, as on the FM-1 (for the thread sanitizer) */
#include <pthread.h>
static cpu1_fn job; static uint32_t job_arg, jobs; static int c1_thread, c1_busy;
static pthread_mutex_t c1_mx = PTHREAD_MUTEX_INITIALIZER; static pthread_cond_t c1_cv = PTHREAD_COND_INITIALIZER;
static void *c1_main(void *a)
{
    (void)a;
    pthread_mutex_lock(&c1_mx);
    for (;;) {
        while (!c1_busy) pthread_cond_wait(&c1_cv, &c1_mx);
        cpu1_fn f = job; uint32_t g = job_arg;
        pthread_mutex_unlock(&c1_mx);
        f(g);
        pthread_mutex_lock(&c1_mx); c1_busy = 0; jobs++; pthread_cond_broadcast(&c1_cv);
    }
    return 0;
}
int cpu1_start(void)
{
    const char *e = getenv("TWIN_CPU1");
    if (e && !strcmp(e, "thread")) { pthread_t t; c1_thread = 1; pthread_create(&t, 0, c1_main, 0); }
    return 0;
}
int cpu1_running(void) { return 1; }
int cpu1_run(cpu1_fn fn, uint32_t arg)
{
    if (!c1_thread) { job = fn; job_arg = arg; return 1; }
    pthread_mutex_lock(&c1_mx); job = fn; job_arg = arg; c1_busy = 1; pthread_cond_broadcast(&c1_cv); pthread_mutex_unlock(&c1_mx);
    return 1;
}
int cpu1_wait(uint32_t t)
{
    (void)t;
    if (!c1_thread) { if (job) { cpu1_fn f = job; job = 0; f(job_arg); jobs++; } return 0; }
    pthread_mutex_lock(&c1_mx); while (c1_busy) pthread_cond_wait(&c1_cv, &c1_mx); pthread_mutex_unlock(&c1_mx);
    return 0;
}
void cpu1_stop(void) { job = 0; }
void cpu0_traps_quiet(void) {}
uint32_t cpu1_jobs(void) { return jobs; }
uint32_t cpu1_misreads(void) { return 0; }
uint32_t cpu1_core_id(void) { return 1; }
void sys_download_mode(void) { fprintf(stderr, "TWIN: the download mode asked for\n"); twin_end(4); }
void sys_supplies_stock(void) { for (int i = 0; i < 8; i++) time_wait_us(100); }   /* (as the FM-1's: 4 + 4 steps of 100 us) */
uint32_t sys_reset_bits(void) { const char *e = getenv("TWIN_RST"); return e ? (uint32_t)strtoul(e, 0, 0) : 1u; }
void sys_wdt_arm(void) { wdt_on = 1; wdt_ns = tw_ns; }
void sys_wdt_feed(void) { wdt_ns = tw_ns; tw_wdt_feeds++; }
void sys_wdt_stop(void) { wdt_on = 0; }
uint32_t sys_p33_timeouts(void) { return 0; }

/* ---- the interrupt (the USB tick): nothing on the twin */
void irq_start(void (*fn)(void)) { (void)fn; }
uint32_t irq_ticks(void) { return (uint32_t)(tw_ns / 500000u); }
void irq_off(void) { }
void irq_on(void) { }

/* ---- USB MIDI: the twin's queues */
#define QN 65536
static uint8_t qin[QN][4], qout[QN][4]; static uint32_t qin_r, qin_w, qout_n, lost;
int tw_usb_put_in(const uint8_t pk[4]) { if (qin_w - qin_r >= QN) return 0; memcpy(qin[qin_w++ % QN], pk, 4); return 1; }
int tw_usb_in_empty(void) { return qin_r == qin_w; }
uint32_t tw_usb_take_out(uint8_t (*out)[4], uint32_t cap) { uint32_t n = qout_n < cap ? qout_n : cap; memcpy(out, qout, n * 4u); memmove(qout, qout + n, (qout_n - n) * 4u); qout_n -= n; return n; }
void usb_midi_open(void) { }
void usb_midi_poll(void) { adv(1000); }
int tw_usb_on = 1;
int usb_midi_ready(void) { return 1; }
int usb_host_live(void) { return tw_usb_on; }   /* (3.3.8: the cable pulled: tw_usb_on 0) */
void usb_midi_close(void) { }
int usb_midi_take(uint8_t pk[4]) { if (qin_r == qin_w) return 0; memcpy(pk, qin[qin_r++ % QN], 4); return 1; }
int usb_midi_put(const uint8_t pk[4]) { if (qout_n >= QN) { lost++; return 0; } memcpy(qout[qout_n++], pk, 4); return 1; }
uint32_t usb_midi_lost(void) { return lost; }
void usb_tick(void) { }
void usb_by_tick(void) { }
void usb_audio_put(const int32_t *lr24, uint32_t frames) { (void)lr24; (void)frames; }
void usb_audio_rate(int mode) { (void)mode; }
void trs_midi_open(void) { }
int trs_midi_byte(void) { return -1; }
uint32_t trs_midi_count(void) { return 0; }
int tw_pot = 512;
void adc_init(void) { }
int32_t tw_batt = 600;                     /* (3.3.6) the battery's ADC: 4.06 V, about 85 % */
int32_t adc_read(uint32_t ch) { adv(200000); return ch == 3u ? tw_batt : tw_pot; }   /* (ch 3: ADC_CH_BATT) */
