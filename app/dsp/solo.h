/* SPDX-License-Identifier: GPL-3.0-only */
/* Solo engine for AMB-1: polyphonic sine/triangle/saw/square synthesizer with
 * Arpeggiator, ADSR envelope, resonant filter, dedicated delay/reverb sends, and preset system. */
#pragma once
#include <stdint.h>
#include "plat.h"

#define NSOLO_VOICES 8
#define NSOLO_FACTORY 12
#define NSOLO_PRESETS 13
#define NSOLO_PAGES 6
#define USER_PRESET_IDX (NSOLO_PRESETS - 1)

enum {
    SOLO_PAGE_PRESET,
    SOLO_PAGE_ADSR,
    SOLO_PAGE_TONE,
    SOLO_PAGE_DELAY,
    SOLO_PAGE_REVERB,
    SOLO_PAGE_ARP
};

enum {
    /* Arp page (0) */
    SP_ARP_MODE, /* 0: Off, 1: Up, 2: Down, 3: Up/Dn, 4: Random, 5: Repeat */
    SP_ARP_RATE, /* 0: 1/2, 1: 1/4, 2: 1/4T, 3: 1/8, 4: 1/8T, 5: 1/16, 6: 1/16T, 7: 1/32 */
    SP_ARP_GATE, /* 10..100 % */
    SP_ARP_OCT,  /* 1..3 */

    /* ADSR page (1) */
    SP_ATTACK,   /* ms: 1..500 */
    SP_DECAY,    /* ms: 20..3000 */
    SP_SUSTAIN,  /* %: 0..100 */
    SP_RELEASE,  /* ms: 20..4000 */

    /* Tone page (2) */
    SP_WAVE,     /* 0: Sine, 1: Tri, 2: Saw, 3: Square */
    SP_CUTOFF,   /* 0..100 (200 Hz .. 12 kHz) */
    SP_RES,      /* 0..100 (Q 0.5 .. 8.0) */
    SP_OCTAVE,   /* -2 .. +2 */

    /* Delay page (3) */
    SP_DLY_MIX,  /* 0..100 % */
    SP_DLY_TIME, /* 50 .. 1000 ms */
    SP_DLY_FB,   /* 0 .. 85 % */
    SP_DLY_DAMP, /* 0 .. 100 % */

    /* Reverb page (4) */
    SP_REV_MIX,  /* 0..100 % */
    SP_REV_SIZE, /* 0..100 % */
    SP_REV_DAMP, /* 0..100 % */
    SP_REV_WID,  /* 0..100 % */

    /* Preset page (5) */
    SP_PRESET,   /* 0 .. NSOLO_PRESETS - 1 (0..14 Factory, 15: User) */
    SP_VOL,      /* 0 .. 100 % */
    SP_PAN,      /* -50 .. +50 % (Left to Right) */
    SP_GLIDE,    /* 0: Off, 1..100: 8..800 ms portamento */

    SOLO_NPARAMS
};

typedef struct {
    int16_t par[SOLO_NPARAMS];
} solo_preset_t;

/* Engine functions */
void solo_init(void);
void solo_note(uint8_t note, uint8_t vel, uint8_t on);
void solo_set(int param, int val);
int solo_get(int param);
void solo_param_range(int param, int *lo, int *hi);
const char *solo_param_name(int param);
void solo_param_text(int param, int val, char *buf);
const char *solo_page_name(int page);
const char *solo_preset_name(int idx);
void solo_load_preset(int idx);
void solo_save_preset(int idx);
void solo_reset_init(void);
void solo_panic(void);
void solo_arp_hold_set(int on);
int solo_arp_hold_get(void);
uint32_t solo_arp_latched_keys(void);
void solo_arp_capture_keys(uint32_t mask);
int solo_arp_toggle_note(uint8_t note);
int solo_arp_last_mode_get(void);
void solo_arp_stop(void);
void solo_get_user_preset(solo_preset_t *dst);
void solo_set_user_preset(const solo_preset_t *src);
uint8_t solo_get_and_clear_triggered_note(void);

/* Render block: generates stereo audio (dry into mixl/mixr, sends into esend/rsend) */
void solo_render(float *mixl, float *mixr, float *esend, float *rsend, uint32_t n);
