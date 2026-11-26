/* SPDX-License-Identifier: GPL-3.0-only
 * Projects in flash (store.c), safe from a power cut (G12). 3.3.5: one project, in two banks of 13 sectors (52 KB):
 * a save goes to the bank that does not hold the latest version, its header written last, after the data is read
 * back and its CRC found right. A bank counts only with its header and its CRC right, so a cut at any byte leaves
 * the version before. The flash work is started and polled (store_step), between two audio halves: the sound never
 * waits.
 * The flash (the final version's map, docs/PLAN-FINAL.md): the firmware from 0x4000, the sample bank at 0x49400 (3.4.2; 0x48800 from 4.0.1, 0x48000 from 3.3.1)
 * (both in the application area: the installer writes them), the projects [0x7A000, 0x95000); SLOOP 2.8's data
 * (0x97000 .. 0xFFFFF: its projects, samples, user presets, settings) is never written: going back to SLOOP 2.8
 * loses nothing, and SLOOP 3 imports its projects. */
#pragma once
#include <stdint.h>

#define STORE_BASE 0x7A000u
#define STORE_APP_MAX (0x49400u - 0x4000u)       /* the firmware must end below the sample bank (3.4.2: 0x49400; 4.0.1: 0x48800; 3.3.1: 0x48000; 0x4B778 from 3.1.6) */
#define STORE_SLOTS 8
#define STORE_AREAS 9
#define STORE_COPY 0x3000u                       /* 12 KB: three sectors */
#define STORE_MAX (STORE_COPY - 256u)            /* the data after the header's page */

typedef struct {
    int (*read)(uint32_t a, void *dst, uint32_t n);
    int (*erase_start)(uint32_t a);
    int (*program_start)(uint32_t a, const void *src, uint32_t n);
    int (*busy)(void);
} store_flash;

void store_init(const store_flash *f);
/* 3.3.5: ONE project ("8 loops of 8 bars on every track, each its own sound: it must fit"): two banks of 13 sectors
 * (0x7A000, 0x87000), each a header page then up to STORE_PMAX bytes. A save goes to the bank that does not hold the
 * newest whole copy: its sectors erased, the file given part by part (store_step returns 3: store_proj_need says from
 * where; store_proj_give hands those bytes, kept until written), read back, and only if its CRC is the one the save
 * began with (the project unchanged meanwhile, the flash right) the header page, last: a cut at any point leaves the
 * copy before. A file's size is bounded: the largest project the sequencer can hold fits (tests/store_test.c). */
#define STORE_PBANK 0xD000u
#define STORE_PMAX (STORE_PBANK - 256u)          /* 52992 bytes */
int store_proj_find(uint32_t *data_at, uint32_t *len, uint32_t *crc);   /* the newest whole copy: its data's flash address */
int store_proj_prep(void);                        /* (3.3.8) stopped and idle: the next save's bank erased, a sector a call; 1 busy */
int store_proj_ready(void);                       /* (3.3.8) that bank all erased: a save writes pages only */
int store_erasing(void);                          /* (3.3.8) a save's erase under way (the window off for milliseconds) */
int store_proj_begin(uint32_t len, uint32_t crc, int known);          /* known 0 (an upload): the CRC comes with store_proj_end */
uint32_t store_proj_need(void);                  /* the file's offset the store waits for, when store_step returned 3 */
void store_proj_give(const void *d, uint32_t n); /* those bytes (they must stay as they are until store_step returns 3 or ends) */
int store_proj_end(uint32_t crc);                /* (an upload) every byte given: its CRC; 0 = taken */
uint32_t store_proj_given(void);                 /* the bytes given so far */
int store_proj_written(void);                    /* (an upload) every byte given is in the flash: its CRC awaited */
uint32_t store_proj_target(void);                /* (an upload) the flash address its data goes to (checked there before the end) */
int store_proj_wants(void);                      /* 1: the bytes given before are written (or none): more may be given */
void store_proj_abort(void);                     /* (an upload stopped) the bank left without a header: not a copy */
/* the 8 places of 3.0 to 3.3.4 ("GBP2", 9 areas of 12 KB from 0x7A000): read once, the newest kept (fw/main.c) */
int store_old_latest(void);                      /* the place saved last, or -1 */
int store_old_load(int slot, void *buf, uint32_t cap);   /* its length, or -1 */
int store_step(void);                            /* 1 working, 2 done, 3 data needed, -1 the flash failed, -2 not the data begun with */
/* (the web editor's sample slots) a 4 KB sector at `sector`: erased if asked, then n bytes at off programmed and read back */
int store_raw_begin(uint32_t sector, uint32_t off, const void *data, uint32_t n, int erase);                            /* 0 idle, 1 working, 2 just done, -1 failed */
uint32_t store_progress(void);                   /* 0..100 */
uint32_t crc32_ieee(uint32_t crc, const void *d, uint32_t n);
const uint32_t *crc32_table(void);
extern void (*store_yield)(void);
extern int store_no_erase;                         /* (3.4.3) set: store_proj_begin gives -3 rather than erase */                  /* (3.4.3) called every KB while a copy is checked whole (the sound served) */                 /* (3.4.3) the table, made if not yet: a byte at a time without a call */
/* the user sounds (usersnd.c): two copies of a sector at 0x95000 and 0x96000, saved as a project is (store_step) */
#define STORE_SOUNDS_AT 0x95000u
#define STORE_SOUNDS_SLOT 0x53u
#define STORE_SOUNDS_MAX (4096u - 256u)
int store_sounds_load(void *buf, uint32_t cap);     /* its length, or -1 */
int store_sounds_begin(const void *data, uint32_t len);
