/* SPDX-License-Identifier: GPL-3.0-only
 * A project as bytes (project.c): the tempo, each track's sound, mix, mute, grid pull and pattern, and every
 * pattern that is not empty. Little-endian, versioned; a load checks every length before using it.
 * Format 3 adds the effects (the delay's division and feedback, the reverb's size) and each track's preset; 6 the
 * master bus and each track's filter. A file has room for 8 tracks; the sequencer has SEQ_NTRK. */
#pragma once
#include <stdint.h>
#include "../engine/engine.h"
#include "../seq/seq.h"
#include "psound.h"
#include "store.h"
#define PRJ_HOME 80
/* what the firmware keeps beside the sound and the sequencer */
typedef struct {
    uint8_t dly_div; uint8_t preset[SEQ_NTRK];       /* 0xFF: none (an older project) */
    uint8_t harm[SEQ_NTRK][5];                       /* format 5: root, scale, keys, chord, transpose + 64 */
    uint8_t fade;                                    /* format 6: the punch-ins' fade, 0..15 */
    uint8_t arp[SEQ_NTRK][8];                        /* format 7: each track's arpeggiator (mode, rate, oct, gate,
                                                        swing, prob, hold, order); all 0: none kept */
    int8_t strum[SEQ_NTRK]; uint8_t vlead[SEQ_NTRK];  /* format 9: SEL 2/2 (3.4.2: STRUM < 0 from the highest note down) */
    uint8_t midi[8];                                 /* format 10: IN, SYNC, OUT, CLK OUT, BATT; 3 to come (0xFF: none) */
    psounds *ps;                                     /* format 11: each pattern's own sound (0: none kept, none read) */
    uint16_t ps_mask[SEQ_NTRK];                      /* (load) the patterns whose own sound was read into ps */
    uint8_t home[PRJ_HOME];                          /* format 13: the device's settings (fw/main.c HOME; 0xFF: none) */
    uint16_t preset16[SEQ_NTRK];                     /* format 13: each track's sound (0xFFFF none, bit 15 edited); preset[] keeps 7 bits */
    uint8_t tex[8];                                  /* format 13: TEXTURE (SRC LEVEL TONE MOVE RATE WHEN DUCK; 0xFF none) */
    uint8_t fxv[4];                                  /* format 14: the delay's COLR, MIX, the reverb's DAMP, the chorus's rate (0..127 as SLOOP's; 0xFF none) */
    seq_pat *keep;                                   /* format 16 (4.0): each track's current pattern as last saved (SEQ_NTRK of them;
                                                        0: none kept, none read); the sound as last saved: x->ps's current one */
    uint8_t keep_got[SEQ_NTRK], pscur_got[SEQ_NTRK]; /* (load) read from the file (else: the pattern, the sound as they are) */
} project_extra;
int project_save(const seq_t *q, const gb_sound *s, const project_extra *x, uint8_t *buf, uint32_t cap);   /* its length, or -1 */
/* 3.3.5: the file's bytes [lo, lo + n) into buf (a part: a page, a chunk), its whole length returned, its CRC (32,
 * IEEE) in *crc when asked */
int project_part(const seq_t *q, const gb_sound *s, const project_extra *x, uint8_t *buf, uint32_t lo, uint32_t n, uint32_t *crc);
/* (3.4.3) called often while a project is written or read (between its patterns, every 32 notes): the FM-1's sound
 * served meanwhile (fw/main.c audio_yield). 0: nothing */
extern void (*project_yield)(void);
/* a project read through fetch (the flash: n bytes at the file's offset off into dst, 0 = read), len bytes long;
 * check_only: only checked, nothing loaded */
typedef int (*project_fetch)(uint32_t off, void *dst, uint32_t n);
int project_load_from(seq_t *q, gb_sound *s, project_extra *x, project_fetch fetch, uint32_t len, int check_only);
int project_load(seq_t *q, gb_sound *s, project_extra *x, const uint8_t *buf, uint32_t len);
int project_check(const uint8_t *buf, uint32_t len);   /* 0: a project file of a format we read (its magic, its version) */               /* 0 = loaded */
