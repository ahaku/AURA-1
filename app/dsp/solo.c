/* SPDX-License-Identifier: GPL-3.0-only */
/* Solo engine implementation */
#include "solo.h"
#include "fastmath.h"

#ifndef EN_SR
#define EN_SR 44100.0f
#endif

typedef enum {
    ENV_OFF = 0,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
} env_stage_t;

typedef struct {
    float cph, mph;
    float cinc, minc;
    float target_cinc;
    float amp;
    float cur_amp;
    float flt_y1, flt_y2;
    env_stage_t stage;
    uint8_t note, vel;
    uint8_t active;
} solo_voice_t;

static solo_voice_t voices[NSOLO_VOICES];
static float solo_last_played_cinc = 0.0f;
static int16_t cur_par[SOLO_NPARAMS];
static solo_preset_t presets[NSOLO_PRESETS];

static const char *const PAGE_NAMES[NSOLO_PAGES] = {
    "Preset", "ADSR", "Tone", "Delay", "Reverb", "Arp"
};

static const char *const PRESET_NAMES[NSOLO_PRESETS] = {
    "Pure", "Mallet", "Chime", "Sub", "Dream", "Harp", "Flute", "Tape",
    "Space", "Vangelis", "Astral", "Nebula", "User"
};

static const char *const ARP_MODES[6] = {
    "Off", "Up", "Down", "Up/Dn", "Random", "Repeat"
};

static const char *const ARP_RATES[8] = {
    "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32"
};

static const float ARP_DIVS[8] = {
    2.0f, 1.0f, 2.0f / 3.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f
};

static const char *const WAVE_NAMES[4] = {
    "Sine", "Tri", "Saw", "Square"
};

/* Arpeggiator state */
#define MAX_ARP_HELD 16
static uint8_t arp_held[MAX_ARP_HELD];
static uint8_t arp_vel[MAX_ARP_HELD];
static int n_arp_held = 0;
static int arp_step = 0;
static int arp_dir = 1;
static int arp_oct_cycle = 0;
static float arp_timer = 0.0f;
static int arp_gate_off = 0;
static uint8_t arp_sounding_note = 0;
static uint8_t arp_sounding_active = 0;
static int arp_hold = 0;
static int arp_n_phys_held = 0;
static int arp_latched = 0;
static volatile uint8_t solo_last_trig_note = 0;

#define SOLO_DLY_LEN 24000
static int16_t solo_dly_buf[SOLO_DLY_LEN] EN_POOL;
static int solo_dly_w = 0;
static float solo_dly_lp = 0.0f;
static float solo_rev_damp_lp = 0.0f;
static uint8_t solo_dly_sub = 0;
static float solo_dly_out_l = 0.0f;
static float solo_dly_out_r = 0.0f;

static const solo_preset_t FACTORY_PRESETS[NSOLO_FACTORY] = {
    /* 0: Pure (pure sine ambient tone, gentle drop) */
    {{
        0, 3, 50, 1,           /* Arp: Off, 1/8, 50%, 1 oct */
        4, 380, 0, 450,        /* ADSR: A:4ms, D:380ms, S:0%, R:450ms */
        0, 80, 15, 0,          /* Tone: Sine, Cutoff:80%, Res:15%, Oct:0 */
        30, 320, 40, 50,       /* Delay: Mix:30%, Time:320ms, FB:40%, Damp:50% */
        50, 70, 45, 65,        /* Reverb: Mix:50%, Size:70%, Damp:45%, Wid:65% */
        0, 80, 0, 0            /* Main: Preset:0, Vol:80%, Pan:0, Glide:0 */
    }},
    /* 1: Mallet (warm triangle marimba/kalimba) */
    {{
        0, 3, 50, 1,           /* Arp: Off, 1/8, 50%, 1 oct */
        4, 450, 0, 250,        /* ADSR: A:4ms, D:450ms, S:0%, R:250ms */
        1, 50, 25, 0,          /* Tone: Triangle, Cutoff:50%, Res:25%, Oct:0 */
        25, 280, 30, 65,       /* Delay: Mix:25%, Time:280ms, FB:30%, Damp:65% */
        45, 55, 50, 50,        /* Reverb: Mix:45%, Size:55%, Damp:50%, Wid:50% */
        1, 85, 0, 0            /* Main: Preset:1, Vol:85%, Pan:0, Glide:0 */
    }},
    /* 2: Chime (soft crystalline celeste / music box chime, gentle sine pluck) */
    {{
        0, 3, 50, 1,           /* Arp: Off, 1/8, 50%, 1 oct */
        6, 750, 0, 950,        /* ADSR: A:6ms, D:750ms, S:0%, R:950ms */
        0, 70, 15, 1,          /* Tone: Sine, Cutoff:70%, Res:15%, Oct:+1 */
        35, 360, 45, 40,       /* Delay: Mix:35%, Time:360ms, FB:45%, Damp:40% */
        60, 85, 30, 70,        /* Reverb: Mix:60%, Size:85%, Damp:30%, Wid:70% */
        2, 75, 0, 0            /* Main: Preset:2, Vol:75%, Pan:0, Glide:0 */
    }},
    /* 3: Sub (deep warm sub bass) */
    {{
        0, 3, 50, 1,           /* Arp: Off, 1/8, 50%, 1 oct */
        5, 500, 0, 350,        /* ADSR: A:5ms, D:500ms, S:0%, R:350ms */
        0, 35, 10, -1,         /* Tone: Sine, Cutoff:35%, Res:10%, Oct:-1 */
        15, 250, 20, 85,       /* Delay: Mix:15%, Time:250ms, FB:20%, Damp:85% */
        30, 40, 75, 30,        /* Reverb: Mix:30%, Size:40%, Damp:75%, Wid:30% */
        3, 90, 0, 0            /* Main: Preset:3, Vol:90%, Pan:0, Glide:0 */
    }},
    /* 4: Dream (warm ambient swell / velvet triangle pad with lush space) */
    {{
        0, 3, 50, 1,           /* Arp: Off, 1/8, 50%, 1 oct */
        90, 1200, 65, 1600,    /* ADSR: A:90ms, D:1200ms, S:65%, R:1600ms */
        1, 55, 15, 0,          /* Tone: Triangle, Cutoff:55%, Res:15%, Oct:0 */
        35, 420, 40, 65,       /* Delay: Mix:35%, Time:420ms, FB:40%, Damp:65% */
        65, 80, 60, 75,        /* Reverb: Mix:65%, Size:80%, Damp:60%, Wid:75% */
        4, 75, 0, 0            /* Main: Preset:4, Vol:75%, Pan:0, Glide:0 */
    }},
    /* 5: Harp (gentle acoustic harp pluck with warm cathedral space) */
    {{
        0, 3, 50, 1,           /* Arp: Off, 1/8, 50%, 1 oct */
        6, 850, 12, 1400,      /* ADSR: A:6ms, D:850ms, S:12%, R:1400ms */
        1, 46, 16, 0,          /* Tone: Triangle, Cutoff:46%, Res:16%, Oct:0 */
        30, 340, 40, 70,       /* Delay: Mix:30%, Time:340ms, FB:40%, Damp:70% */
        55, 80, 55, 75,        /* Reverb: Mix:55%, Size:80%, Damp:55%, Wid:75% */
        5, 80, 0, 0            /* Main: Preset:5, Vol:80%, Pan:0, Glide:0 */
    }},
    /* 6: Flute (breathy meditative bamboo flute) */
    {{
        0, 3, 50, 1,           /* Arp: Off, 1/8, 50%, 1 oct */
        65, 700, 75, 700,      /* ADSR: A:65ms, D:700ms, S:75%, R:700ms */
        0, 75, 20, 0,          /* Tone: Sine, Cutoff:75%, Res:20%, Oct:0 */
        35, 450, 35, 60,       /* Delay: Mix:35%, Time:450ms, FB:35%, Damp:60% */
        60, 85, 50, 70,        /* Reverb: Mix:60%, Size:85%, Damp:50%, Wid:70% */
        6, 85, 0, 0            /* Main: Preset:6, Vol:85%, Pan:0, Glide:0 */
    }},
    /* 7: Tape (nostalgic Lo-Fi warm electric piano / tape Rhodes) */
    {{
        0, 3, 50, 1,           /* Arp: Off, 1/8, 50%, 1 oct */
        5, 950, 25, 850,       /* ADSR: A:5ms, D:950ms, S:25%, R:850ms */
        1, 42, 22, 0,          /* Tone: Triangle, Cutoff:42%, Res:22%, Oct:0 */
        45, 380, 50, 80,       /* Delay: Mix:45%, Time:380ms, FB:50%, Damp:80% */
        45, 65, 65, 60,        /* Reverb: Mix:45%, Size:65%, Damp:65%, Wid:60% */
        7, 85, 0, 0            /* Main: Preset:7, Vol:85%, Pan:0, Glide:0 */
    }},
    /* 8: Space (ethereal singing theremin / cosmic lead) */
    {{
        0, 3, 50, 1,           /* Arp: Off, 1/8, 50%, 1 oct */
        120, 1500, 85, 2200,   /* ADSR: A:120ms, D:1500ms, S:85%, R:2200ms */
        0, 85, 10, 0,          /* Tone: Sine, Cutoff:85%, Res:10%, Oct:0 */
        45, 520, 55, 50,       /* Delay: Mix:45%, Time:520ms, FB:55%, Damp:50% */
        70, 95, 45, 85,        /* Reverb: Mix:70%, Size:95%, Damp:45%, Wid:85% */
        8, 75, 0, 0            /* Main: Preset:8, Vol:75%, Pan:0, Glide:0 */
    }},
    /* 9: Vangelis (majestic CS-80 warm analog saw brass lead with rich space) */
    {{
        0, 3, 50, 1,           /* Arp: Off, 1/8, 50%, 1 oct */
        45, 1100, 70, 1100,    /* ADSR: A:45ms, D:1100ms, S:70%, R:1100ms */
        2, 58, 22, 0,          /* Tone: Saw, Cutoff:58%, Res:22%, Oct:0 */
        35, 360, 45, 55,       /* Delay: Mix:35%, Time:360ms, FB:45%, Damp:55% */
        65, 85, 45, 80,        /* Reverb: Mix:65%, Size:85%, Damp:45%, Wid:80% */
        9, 80, 0, 0            /* Main: Preset:9, Vol:80%, Pan:0, Glide:0 */
    }},
    /* 10: Astral (sparkling arpeggiated raindrop synth, Arp Up/Dn enabled by default!) */
    {{
        3, 3, 40, 2,           /* Arp: Up/Dn, 1/8, 40%, 2 oct */
        4, 350, 0, 400,        /* ADSR: A:4ms, D:350ms, S:0%, R:400ms */
        0, 75, 20, 1,          /* Tone: Sine, Cutoff:75%, Res:20%, Oct:+1 */
        45, 320, 50, 45,       /* Delay: Mix:45%, Time:320ms, FB:50%, Damp:45% */
        65, 85, 35, 80,        /* Reverb: Mix:65%, Size:85%, Damp:35%, Wid:80% */
        10, 80, 0, 0           /* Main: Preset:10, Vol:80%, Pan:0, Glide:0 */
    }},
    /* 11: Nebula (interstellar floating warm saw pad/lead) */
    {{
        0, 3, 50, 1,           /* Arp: Off, 1/8, 50%, 1 oct */
        150, 2000, 85, 2500,   /* ADSR: A:150ms, D:2000ms, S:85%, R:2500ms */
        2, 45, 15, -1,         /* Tone: Saw, Cutoff:45%, Res:15%, Oct:-1 */
        40, 500, 55, 70,       /* Delay: Mix:40%, Time:500ms, FB:55%, Damp:70% */
        75, 95, 60, 90,        /* Reverb: Mix:75%, Size:95%, Damp:60%, Wid:90% */
        11, 75, 0, 0           /* Main: Preset:11, Vol:75%, Pan:0, Glide:0 */
    }}
};

void solo_init(void)
{
    int i;
    __builtin_memset(voices, 0, sizeof(voices));
    __builtin_memset(solo_dly_buf, 0, sizeof(solo_dly_buf));
    solo_dly_w = 0;
    solo_dly_lp = 0.0f;
    solo_rev_damp_lp = 0.0f;
    solo_dly_sub = 0;
    solo_dly_out_l = solo_dly_out_r = 0.0f;

    n_arp_held = 0;
    arp_step = 0;
    arp_dir = 1;
    arp_oct_cycle = 0;
    arp_timer = 0.0f;
    arp_gate_off = 0;
    arp_sounding_note = 0;
    arp_sounding_active = 0;
    arp_hold = 0;
    arp_n_phys_held = 0;
    arp_latched = 0;
    solo_last_trig_note = 0;

    for (i = 0; i < NSOLO_FACTORY; i++)
        presets[i] = FACTORY_PRESETS[i];
    presets[USER_PRESET_IDX] = FACTORY_PRESETS[0];
    solo_load_preset(0);
}

void solo_load_preset(int idx)
{
    if (idx < 0) idx = 0;
    if (idx >= NSOLO_PRESETS) idx = NSOLO_PRESETS - 1;

    int keep_arp = (arp_hold && cur_par[SP_ARP_MODE] > 0);
    int16_t saved_arp[4];
    if (keep_arp) {
        saved_arp[0] = cur_par[SP_ARP_MODE];
        saved_arp[1] = cur_par[SP_ARP_RATE];
        saved_arp[2] = cur_par[SP_ARP_GATE];
        saved_arp[3] = cur_par[SP_ARP_OCT];
    }

    if (idx < NSOLO_FACTORY)
        __builtin_memcpy(cur_par, FACTORY_PRESETS[idx].par, sizeof(cur_par));
    else
        __builtin_memcpy(cur_par, presets[USER_PRESET_IDX].par, sizeof(cur_par));

    if (keep_arp) {
        cur_par[SP_ARP_MODE] = saved_arp[0];
        cur_par[SP_ARP_RATE] = saved_arp[1];
        cur_par[SP_ARP_GATE] = saved_arp[2];
        cur_par[SP_ARP_OCT]  = saved_arp[3];
    } else {
        solo_arp_stop();
    }

    cur_par[SP_PRESET] = (int16_t)idx;
    pl_decay_t = 0.50f + 0.48f * ((float)cur_par[SP_REV_SIZE] * 0.01f);
    pl_decay = pl_decay_t;
}

void solo_save_preset(int idx)
{
    (void)idx;
    __builtin_memcpy(presets[USER_PRESET_IDX].par, cur_par, sizeof(cur_par));
    presets[USER_PRESET_IDX].par[SP_PRESET] = USER_PRESET_IDX;
}

void solo_reset_init(void)
{
    solo_last_played_cinc = 0.0f;
    solo_load_preset(0);
    solo_panic();
    __builtin_memset(solo_dly_buf, 0, sizeof(solo_dly_buf));
    solo_dly_w = 0;
    solo_dly_lp = 0.0f;
    solo_rev_damp_lp = 0.0f;
}

void solo_get_user_preset(solo_preset_t *dst)
{
    if (dst)
        *dst = presets[USER_PRESET_IDX];
}

void solo_set_user_preset(const solo_preset_t *src)
{
    if (src)
        presets[USER_PRESET_IDX] = *src;
}

void solo_panic(void)
{
    int i;
    for (i = 0; i < NSOLO_VOICES; i++) {
        voices[i].stage = ENV_OFF;
        voices[i].active = 0;
        voices[i].cur_amp = 0.0f;
    }
    n_arp_held = 0;
    arp_sounding_active = 0;
    arp_timer = 0.0f;
    arp_latched = 0;
    arp_n_phys_held = 0;
    solo_last_trig_note = 0;
    solo_last_played_cinc = 0.0f;
    arp_hold = 0;
}

void solo_arp_stop(void)
{
    int i;
    n_arp_held = 0;
    arp_n_phys_held = 0;
    arp_latched = 0;
    arp_hold = 0;
    arp_sounding_active = 0;
    arp_timer = 0.0f;
    for (i = 0; i < NSOLO_VOICES; i++) {
        if (voices[i].active && voices[i].stage != ENV_RELEASE)
            voices[i].stage = ENV_RELEASE;
    }
}

void solo_arp_hold_set(int on)
{
    arp_hold = on ? 1 : 0;
    if (!arp_hold) {
        arp_latched = 0;
        solo_arp_stop();
    }
}

int solo_arp_hold_get(void)
{
    return arp_hold;
}

uint32_t solo_arp_latched_keys(void)
{
    if (!arp_hold || n_arp_held <= 0) return 0;
    uint32_t mask = 0;
    int i;
    for (i = 0; i < n_arp_held; i++) {
        int k = (int)arp_held[i] - 53;
        if (k >= 0 && k < NKEYS)
            mask |= (1u << k);
    }
    return mask;
}

static int solo_last_arp_mode = 1;

int solo_arp_last_mode_get(void)
{
    return solo_last_arp_mode > 0 ? solo_last_arp_mode : 1;
}

void solo_arp_capture_keys(uint32_t mask)
{
    if (!mask) return;
    int i;
    for (i = 0; i < NSOLO_VOICES; i++) {
        voices[i].stage = ENV_OFF;
        voices[i].active = 0;
        voices[i].cur_amp = 0.0f;
    }
    n_arp_held = 0;
    arp_n_phys_held = 0;
    for (i = 0; i < NKEYS; i++) {
        if (mask & (1u << i)) {
            uint8_t note = (uint8_t)(53 + i);
            if (n_arp_held < MAX_ARP_HELD) {
                arp_held[n_arp_held] = note;
                arp_vel[n_arp_held] = 100;
                n_arp_held++;
            }
            arp_n_phys_held++;
        }
    }
    if (n_arp_held > 0) {
        arp_step = 0;
        arp_dir = 1;
        arp_oct_cycle = 0;
        arp_timer = 9999999.0f;
        arp_gate_off = 0;
        arp_sounding_active = 0;
        arp_latched = 0;
    }
}

int solo_arp_toggle_note(uint8_t note)
{
    int i, found = -1;
    for (i = 0; i < n_arp_held; i++) {
        if (arp_held[i] == note) {
            found = i;
            break;
        }
    }
    if (found >= 0) {
        /* Note is active: remove it */
        for (i = found; i < n_arp_held - 1; i++) {
            arp_held[i] = arp_held[i + 1];
            arp_vel[i] = arp_vel[i + 1];
        }
        n_arp_held--;
        if (arp_step >= n_arp_held && n_arp_held > 0)
            arp_step = 0;
        if (n_arp_held == 0) {
            /* All notes removed: silence voices */
            for (i = 0; i < NSOLO_VOICES; i++) {
                if (voices[i].active && voices[i].stage != ENV_RELEASE)
                    voices[i].stage = ENV_RELEASE;
            }
            arp_sounding_active = 0;
            arp_timer = 0.0f;
        }
    } else {
        /* Note is not active: add it into pitch-sorted order */
        if (n_arp_held < MAX_ARP_HELD) {
            int pos = n_arp_held;
            while (pos > 0 && arp_held[pos - 1] > note) {
                arp_held[pos] = arp_held[pos - 1];
                arp_vel[pos] = arp_vel[pos - 1];
                pos--;
            }
            arp_held[pos] = note;
            arp_vel[pos] = 100;
            n_arp_held++;
            if (n_arp_held == 1) {
                /* First note added: trigger immediately */
                arp_step = 0;
                arp_dir = 1;
                arp_oct_cycle = 0;
                arp_timer = 9999999.0f;
                arp_gate_off = 0;
                arp_sounding_active = 0;
            }
        }
    }
    arp_latched = (n_arp_held > 0);
    return found >= 0 ? 0 : 1;
}

void solo_set(int param, int val)
{
    if (param >= 0 && param < SOLO_NPARAMS) {
        int lo, hi;
        solo_param_range(param, &lo, &hi);
        if (val < lo) val = lo;
        if (val > hi) val = hi;
        cur_par[param] = (int16_t)val;
        if (param == SP_ARP_MODE) {
            if (val == 0)
                solo_arp_hold_set(0);
            else
                solo_last_arp_mode = val;
        } else if (param == SP_PRESET) {
            solo_load_preset(val);
        } else if (param == SP_REV_SIZE) {
            pl_decay_t = 0.50f + 0.48f * ((float)val * 0.01f);
            pl_decay = pl_decay_t;
        }
    }
}

int solo_get(int param)
{
    if (param >= 0 && param < SOLO_NPARAMS)
        return cur_par[param];
    return 0;
}

void solo_param_range(int param, int *lo, int *hi)
{
    switch (param) {
    case SP_ARP_MODE: *lo = 0;   *hi = 5;    break;
    case SP_ARP_RATE: *lo = 0;   *hi = 7;    break;
    case SP_ARP_GATE: *lo = 10;  *hi = 100;  break;
    case SP_ARP_OCT:  *lo = 1;   *hi = 3;    break;

    case SP_ATTACK:   *lo = 1;   *hi = 500;  break;
    case SP_DECAY:    *lo = 20;  *hi = 3000; break;
    case SP_SUSTAIN:  *lo = 0;   *hi = 100;  break;
    case SP_RELEASE:  *lo = 20;  *hi = 4000; break;

    case SP_WAVE:     *lo = 0;   *hi = 3;    break;
    case SP_CUTOFF:   *lo = 5;   *hi = 100;  break;
    case SP_RES:      *lo = 0;   *hi = 100;  break;
    case SP_OCTAVE:   *lo = -2;  *hi = 2;    break;

    case SP_DLY_MIX:  *lo = 0;   *hi = 100;  break;
    case SP_DLY_TIME: *lo = 50;  *hi = 1000; break;
    case SP_DLY_FB:   *lo = 0;   *hi = 85;   break;
    case SP_DLY_DAMP: *lo = 0;   *hi = 100;  break;

    case SP_REV_MIX:  *lo = 0;   *hi = 100;  break;
    case SP_REV_SIZE: *lo = 10;  *hi = 100;  break;
    case SP_REV_DAMP: *lo = 0;   *hi = 100;  break;
    case SP_REV_WID:  *lo = 0;   *hi = 100;  break;

    case SP_PRESET:   *lo = 0;   *hi = NSOLO_PRESETS - 1; break;
    case SP_VOL:      *lo = 0;   *hi = 100;  break;
    case SP_PAN:      *lo = -50; *hi = 50;   break;
    case SP_GLIDE:    *lo = 0;   *hi = 100;  break;
    default:          *lo = 0;   *hi = 100;  break;
    }
}

const char *solo_param_name(int param)
{
    switch (param) {
    case SP_ARP_MODE: return "Mode";
    case SP_ARP_RATE: return "Rate";
    case SP_ARP_GATE: return "Gate";
    case SP_ARP_OCT:  return "Octave";

    case SP_ATTACK:   return "Attack";
    case SP_DECAY:    return "Decay";
    case SP_SUSTAIN:  return "Sustain";
    case SP_RELEASE:  return "Release";

    case SP_WAVE:     return "Wave";
    case SP_CUTOFF:   return "Cutoff";
    case SP_RES:      return "Reso";
    case SP_OCTAVE:   return "Octave";

    case SP_DLY_MIX:  return "Level";
    case SP_DLY_TIME: return "Time";
    case SP_DLY_FB:   return "Feedback";
    case SP_DLY_DAMP: return "Damp";

    case SP_REV_MIX:  return "Level";
    case SP_REV_SIZE: return "Size";
    case SP_REV_DAMP: return "Tone";
    case SP_REV_WID:  return "Width";

    case SP_PRESET:   return "Preset";
    case SP_VOL:      return "Level";
    case SP_PAN:      return "Pan";
    case SP_GLIDE:    return "Glide";
    default:          return "";
    }
}

static void solo_copy_text(char *buf, const char *text)
{
    int i;
    for (i = 0; i < 11 && text[i]; i++)
        buf[i] = text[i];
    buf[i] = 0;
}

void solo_param_text(int param, int val, char *buf)
{
    switch (param) {
    case SP_ARP_MODE:
        solo_copy_text(buf, ARP_MODES[(unsigned)val < 6u ? val : 0]);
        break;
    case SP_ARP_RATE:
        solo_copy_text(buf, ARP_RATES[(unsigned)val < 8u ? val : 0]);
        break;
    case SP_ARP_OCT:
        buf[0] = (char)('0' + val);
        buf[1] = ' ';
        buf[2] = 'o';
        buf[3] = 'c';
        buf[4] = 't';
        buf[5] = 0;
        break;

    case SP_ATTACK:
    case SP_DECAY:
    case SP_RELEASE:
    case SP_DLY_TIME:
        if (val >= 1000) {
            int s = val / 1000, ms = (val % 1000) / 100;
            buf[0] = (char)('0' + s);
            buf[1] = '.';
            buf[2] = (char)('0' + ms);
            buf[3] = 's';
            buf[4] = 0;
        } else {
            int i = 0, v = val;
            char tmp[8];
            if (v == 0) tmp[i++] = '0';
            while (v > 0) { tmp[i++] = (char)('0' + (v % 10)); v /= 10; }
            int k = 0;
            while (i > 0) buf[k++] = tmp[--i];
            buf[k++] = 'm'; buf[k++] = 's'; buf[k] = 0;
        }
        break;
    case SP_WAVE:
        solo_copy_text(buf, WAVE_NAMES[(unsigned)val < 4u ? val : 0]);
        break;
    case SP_OCTAVE:
        if (val > 0) { buf[0] = '+'; buf[1] = (char)('0' + val); buf[2] = 0; }
        else if (val < 0) { buf[0] = '-'; buf[1] = (char)('0' - val); buf[2] = 0; }
        else solo_copy_text(buf, "0");
        break;
    case SP_PRESET:
        solo_copy_text(buf, solo_preset_name(val % NSOLO_PRESETS));
        break;
    case SP_PAN:
        if (val == 0) solo_copy_text(buf, "Center");
        else if (val < 0) {
            int v = -val, i = 1;
            buf[0] = 'L';
            if (v >= 10) { buf[i++] = (char)('0' + v / 10); v %= 10; }
            buf[i++] = (char)('0' + v);
            buf[i] = 0;
        } else {
            int v = val, i = 1;
            buf[0] = 'R';
            if (v >= 10) { buf[i++] = (char)('0' + v / 10); v %= 10; }
            buf[i++] = (char)('0' + v);
            buf[i] = 0;
        }
        break;
    case SP_GLIDE:
        if (val == 0) solo_copy_text(buf, "Off");
        else {
            int ms = val * 5 + (val * val * 35) / 100;
            if (ms >= 1000) {
                int s = ms / 1000, frac = (ms % 1000) / 100;
                buf[0] = (char)('0' + s);
                buf[1] = '.';
                buf[2] = (char)('0' + frac);
                buf[3] = 's';
                buf[4] = 0;
            } else {
                int rounded_ms = ((ms + 5) / 10) * 10;
                if (rounded_ms < 5) rounded_ms = ms;
                int i = 0, v = rounded_ms;
                char tmp[8];
                while (v > 0) { tmp[i++] = (char)('0' + (v % 10)); v /= 10; }
                int k = 0;
                while (i > 0) buf[k++] = tmp[--i];
                buf[k++] = 'm'; buf[k++] = 's'; buf[k] = 0;
            }
        }
        break;
    default: {
        /* percentage */
        int i = 0, v = val;
        char tmp[8];
        if (v == 0) tmp[i++] = '0';
        while (v > 0) { tmp[i++] = (char)('0' + (v % 10)); v /= 10; }
        int k = 0;
        while (i > 0) buf[k++] = tmp[--i];
        buf[k++] = '%'; buf[k] = 0;
        break;
    }
    }
}

const char *solo_page_name(int page)
{
    if (page >= 0 && page < NSOLO_PAGES)
        return PAGE_NAMES[page];
    return "";
}

uint8_t solo_get_and_clear_triggered_note(void)
{
    uint8_t n = solo_last_trig_note;
    solo_last_trig_note = 0;
    return n;
}

const char *solo_preset_name(int idx)
{
    if (idx >= 0 && idx < NSOLO_PRESETS)
        return PRESET_NAMES[idx];
    return "";
}

static void solo_voice_on(uint8_t note, uint8_t vel)
{
    solo_last_trig_note = note;
    int i, best = -1;
    int eff_note = (int)note + cur_par[SP_OCTAVE] * 12;
    if (eff_note < 12) eff_note = 12;
    if (eff_note > 120) eff_note = 120;

    /* Find voice playing this note or oldest/quietest */
    float min_amp = 999.0f;
    for (i = 0; i < NSOLO_VOICES; i++) {
        if (voices[i].active && voices[i].note == note) {
            best = i;
            break;
        }
        if (!voices[i].active) {
            best = i;
            break;
        }
        if (voices[i].cur_amp < min_amp) {
            min_amp = voices[i].cur_amp;
            best = i;
        }
    }
    if (best >= 0) {
        solo_voice_t *v = &voices[best];
        v->active = 1;
        v->note = note;
        v->vel = vel;
        v->stage = ENV_ATTACK;
        v->amp = (float)vel * (1.0f / 127.0f);
        v->cph = 0.0f;
        v->mph = 0.0f;
        float target = mtof((float)eff_note) * (1.0f / EN_SR);
        v->target_cinc = target;
        int glide_val = cur_par[SP_GLIDE];
        if (glide_val > 0 && solo_last_played_cinc > 0.0f) {
            v->cinc = solo_last_played_cinc;
        } else {
            v->cinc = target;
        }
        v->minc = v->cinc * 2.0f;
        solo_last_played_cinc = target;
        v->flt_y1 = v->flt_y2 = 0.0f;
    }
}

static void solo_voice_off(uint8_t note)
{
    int i;
    for (i = 0; i < NSOLO_VOICES; i++) {
        if (voices[i].active && voices[i].note == note && voices[i].stage != ENV_RELEASE) {
            voices[i].stage = ENV_RELEASE;
        }
    }
}

void solo_note(uint8_t note, uint8_t vel, uint8_t on)
{
    int i;
    int arp_on = (cur_par[SP_ARP_MODE] > 0);

    if (!arp_on) {
        if (on)
            solo_voice_on(note, vel);
        else
            solo_voice_off(note);
        return;
    }

    /* Arp is active: manage held notes */
    if (on) {
        arp_n_phys_held++;
        if (arp_hold && arp_latched) {
            /* First note of a new chord while latched: clear old latched chord */
            n_arp_held = 0;
            arp_step = 0;
            arp_dir = 1;
            arp_oct_cycle = 0;
            arp_timer = 9999999.0f;
            arp_gate_off = 0;
            arp_sounding_active = 0;
            arp_latched = 0;
        }

        /* Add note into arp_held in pitch-sorted order if not already present */
        for (i = 0; i < n_arp_held; i++)
            if (arp_held[i] == note) return;

        if (n_arp_held < MAX_ARP_HELD) {
            int pos = n_arp_held;
            while (pos > 0 && arp_held[pos - 1] > note) {
                arp_held[pos] = arp_held[pos - 1];
                arp_vel[pos] = arp_vel[pos - 1];
                pos--;
            }
            arp_held[pos] = note;
            arp_vel[pos] = vel;
            n_arp_held++;
        }

        if (n_arp_held == 1) {
            /* First note pressed: fire immediately */
            arp_step = 0;
            arp_dir = 1;
            arp_oct_cycle = 0;
            arp_timer = 9999999.0f; /* force immediate trigger on next render block */
            arp_gate_off = 0;
            arp_sounding_active = 0;
        }
    } else {
        /* Note off */
        if (arp_n_phys_held > 0)
            arp_n_phys_held--;

        if (arp_hold) {
            /* In Hold mode, notes remain latched and are never removed on note off! */
            if (arp_n_phys_held == 0) {
                arp_latched = 1;
            }
        } else {
            /* Normal note off without hold: remove from held list */
            int found = -1;
            for (i = 0; i < n_arp_held; i++) {
                if (arp_held[i] == note) { found = i; break; }
            }
            if (found >= 0) {
                for (i = found; i < n_arp_held - 1; i++) {
                    arp_held[i] = arp_held[i + 1];
                    arp_vel[i] = arp_vel[i + 1];
                }
                n_arp_held--;
            }

            if (n_arp_held == 0) {
                /* All keys released: silence voice */
                for (i = 0; i < NSOLO_VOICES; i++) {
                    if (voices[i].active && voices[i].stage != ENV_RELEASE)
                        voices[i].stage = ENV_RELEASE;
                }
                arp_sounding_active = 0;
                arp_timer = 0.0f;
            }
        }
    }
}

void solo_render(float *mixl, float *mixr, float *esend, float *rsend, uint32_t n)
{
    (void)esend;
    uint32_t k;
    int i;
    float att_step = 1.0f / (fm_maxf(1.0f, (float)cur_par[SP_ATTACK]) * 0.001f * EN_SR);
    float dec_step = 1.0f / (fm_maxf(10.0f, (float)cur_par[SP_DECAY]) * 0.001f * EN_SR);
    float sus_lvl  = (float)cur_par[SP_SUSTAIN] * 0.01f;
    float rel_dec  = fm_expf(-5.0f / (fm_maxf(10.0f, (float)cur_par[SP_RELEASE]) * 0.001f * EN_SR));

    /* Arpeggiator step calculation */
    int arp_mode = cur_par[SP_ARP_MODE];
    if (arp_mode > 0 && n_arp_held > 0) {
        float bpm = (float)par[P_TEMPO];
        if (bpm < 30.0f) bpm = 30.0f;
        if (bpm > 140.0f) bpm = 140.0f;
        float q_samps = (60.0f / bpm) * EN_SR;
        int rate_idx = cur_par[SP_ARP_RATE];
        if (rate_idx < 0) rate_idx = 0;
        if (rate_idx > 7) rate_idx = 7;
        float step_samps = q_samps * ARP_DIVS[rate_idx];
        float gate_pct = (float)cur_par[SP_ARP_GATE] * 0.01f;
        float gate_samps = step_samps * gate_pct;

        arp_timer += (float)n;

        /* Check gate off */
        if (arp_sounding_active && !arp_gate_off && arp_timer >= gate_samps) {
            if (arp_mode == 5) {
                /* Repeat mode: release all voices */
                for (i = 0; i < NSOLO_VOICES; i++) {
                    if (voices[i].active && voices[i].stage != ENV_RELEASE)
                        voices[i].stage = ENV_RELEASE;
                }
            } else {
                solo_voice_off(arp_sounding_note);
            }
            arp_gate_off = 1;
        }

        /* Check next step trigger */
        if (arp_timer >= step_samps) {
            if (arp_timer >= 999990.0f) {
                arp_timer = 0.0f;
            } else {
                arp_timer -= step_samps;
                if (arp_timer > step_samps) arp_timer = 0.0f;
            }

            int oct_range = cur_par[SP_ARP_OCT];
            if (oct_range < 1) oct_range = 1;
            if (oct_range > 3) oct_range = 3;

            int trigger_note = 0;
            uint8_t trigger_vel = 100;

            if (arp_mode == 1) {
                /* Up */
                if (arp_step >= n_arp_held) {
                    arp_step = 0;
                    arp_oct_cycle = (arp_oct_cycle + 1) % oct_range;
                }
                trigger_note = arp_held[arp_step];
                trigger_vel = arp_vel[arp_step];
                arp_step++;
            } else if (arp_mode == 2) {
                /* Down */
                if (arp_step < 0 || arp_step >= n_arp_held) {
                    arp_step = n_arp_held - 1;
                    arp_oct_cycle = (arp_oct_cycle + 1) % oct_range;
                }
                trigger_note = arp_held[arp_step];
                trigger_vel = arp_vel[arp_step];
                arp_step--;
            } else if (arp_mode == 3) {
                /* Up/Dn */
                if (n_arp_held <= 1) {
                    arp_step = 0;
                    arp_dir = 1;
                    trigger_note = arp_held[0];
                    trigger_vel = arp_vel[0];
                    arp_oct_cycle = (arp_oct_cycle + 1) % oct_range;
                } else {
                    if (arp_step >= n_arp_held) {
                        arp_dir = -1;
                        arp_step = n_arp_held - 2;
                        arp_oct_cycle = (arp_oct_cycle + 1) % oct_range;
                    } else if (arp_step < 0) {
                        arp_dir = 1;
                        arp_step = 1;
                    }
                    trigger_note = arp_held[arp_step];
                    trigger_vel = arp_vel[arp_step];
                    arp_step += arp_dir;
                }
            } else if (arp_mode == 4) {
                /* Random */
                int idx = irand(n_arp_held);
                trigger_note = arp_held[idx];
                trigger_vel = arp_vel[idx];
                arp_oct_cycle = irand(oct_range);
            } else if (arp_mode == 5) {
                /* Repeat / Chord pulse */
                arp_oct_cycle = 0;
            }

            if (arp_mode == 5) {
                /* Trigger all held notes simultaneously */
                for (i = 0; i < n_arp_held; i++)
                    solo_voice_on(arp_held[i], arp_vel[i]);
            } else {
                int final_note = trigger_note + arp_oct_cycle * 12;
                if (final_note > 120) final_note = 120;
                solo_voice_on((uint8_t)final_note, trigger_vel);
                arp_sounding_note = (uint8_t)final_note;
            }

            arp_sounding_active = 1;
            arp_gate_off = 0;
        }
    }

    /* Filter params: Cutoff (200Hz .. 12kHz), Res (0.5 .. 8.0) */
    float fc = 200.0f + 11800.0f * ((float)cur_par[SP_CUTOFF] * 0.01f) * ((float)cur_par[SP_CUTOFF] * 0.01f);
    float q  = 0.5f + 7.5f * ((float)cur_par[SP_RES] * 0.01f);
    float flt_f = fm_clampf(fc * (FM_TWO_PI / EN_SR), 0.01f, 0.85f);
    float flt_fb = q > 1.0f ? 1.0f - (1.0f / (2.0f * q)) : 0.0f;

    float vol = (float)cur_par[SP_VOL] * 0.01f * 0.65f;
    float pan = (float)cur_par[SP_PAN] * 0.02f; /* -1.0 .. +1.0 */
    float pan_l = pan <= 0.0f ? 1.0f : (1.0f - pan);
    float pan_r = pan >= 0.0f ? 1.0f : (1.0f + pan);

    /* Delay parameters */
    float dly_mix = (float)cur_par[SP_DLY_MIX] * 0.01f;
    int t_ms = cur_par[SP_DLY_TIME];
    if (t_ms < 50) t_ms = 50;
    if (t_ms > 1000) t_ms = 1000;
    float d_samps = (float)t_ms * 22.05f;
    if (d_samps < 10.0f) d_samps = 10.0f;
    if (d_samps > (float)(SOLO_DLY_LEN - 4)) d_samps = (float)(SOLO_DLY_LEN - 4);
    float dly_damp = (float)cur_par[SP_DLY_DAMP] * 0.01f;
    float damp_coef = 0.85f * (1.0f - dly_damp * 0.80f);
    float dly_fb = (float)cur_par[SP_DLY_FB] * 0.01f;
    if (dly_fb > 0.86f) dly_fb = 0.86f;

    /* Reverb send parameters */
    float rev_mix = (float)cur_par[SP_REV_MIX] * 0.01f;
    float rev_damp = (float)cur_par[SP_REV_DAMP] * 0.01f;
    float rev_damp_alpha = 0.85f * (1.0f - rev_damp * 0.75f);

    int wave_type = cur_par[SP_WAVE];
    int g_val = cur_par[SP_GLIDE];
    float glide_alpha = 1.0f;
    if (g_val > 0) {
        float ms = (float)g_val * 5.0f + (float)(g_val * g_val) * 0.35f;
        float samps = ms * (0.001f * EN_SR);
        if (samps < 1.0f) samps = 1.0f;
        glide_alpha = 4.6f / samps;
        if (glide_alpha > 0.5f) glide_alpha = 0.5f;
    }

    for (k = 0; k < n; k++) {
        float sample = 0.0f;
        for (i = 0; i < NSOLO_VOICES; i++) {
            solo_voice_t *v = &voices[i];
            if (!v->active || v->stage == ENV_OFF) continue;

            /* ADSR envelope */
            switch (v->stage) {
            case ENV_ATTACK:
                v->cur_amp += att_step;
                if (v->cur_amp >= 1.0f) {
                    v->cur_amp = 1.0f;
                    v->stage = ENV_DECAY;
                }
                break;
            case ENV_DECAY:
                if (v->cur_amp > sus_lvl) {
                    v->cur_amp -= dec_step;
                    if (v->cur_amp <= sus_lvl) {
                        v->cur_amp = sus_lvl;
                        v->stage = (sus_lvl <= 0.001f) ? ENV_OFF : ENV_SUSTAIN;
                        if (v->stage == ENV_OFF) v->active = 0;
                    }
                } else {
                    v->stage = ENV_SUSTAIN;
                }
                break;
            case ENV_SUSTAIN:
                v->cur_amp = sus_lvl;
                break;
            case ENV_RELEASE:
                v->cur_amp *= rel_dec;
                if (v->cur_amp < 0.0005f) {
                    v->cur_amp = 0.0f;
                    v->stage = ENV_OFF;
                    v->active = 0;
                }
                break;
            default:
                break;
            }

            if (!v->active) continue;

            /* Portamento glide */
            if (v->cinc != v->target_cinc) {
                v->cinc += (v->target_cinc - v->cinc) * glide_alpha;
                if (fm_fabsf(v->target_cinc - v->cinc) < 1e-6f)
                    v->cinc = v->target_cinc;
                v->minc = v->cinc * 2.0f;
            }

            /* Oscillator waveforms */
            float osc = 0.0f;
            if (wave_type == 0) {
                /* Pure sine */
                osc = fm_sinf(v->cph * FM_TWO_PI);
            } else if (wave_type == 1) {
                /* Triangle */
                float p = v->cph;
                osc = p < 0.5f ? (4.0f * p - 1.0f) : (3.0f - 4.0f * p);
            } else if (wave_type == 2) {
                /* Warm Analog Saw with PolyBLEP anti-aliasing */
                float p = v->cph;
                float dt = v->cinc;
                float raw_saw = 2.0f * p - 1.0f;
                if (p < dt) {
                    float t = p / dt;
                    raw_saw -= (t + t - t * t - 1.0f);
                } else if (p > 1.0f - dt) {
                    float t = (p - 1.0f) / dt;
                    raw_saw -= (t * t + t + t + 1.0f);
                }
                osc = raw_saw;
            } else {
                /* Warm Square / Pulse with PolyBLEP anti-aliasing */
                float p = v->cph;
                float dt = v->cinc;
                float raw_sq = p < 0.5f ? 1.0f : -1.0f;
                if (p < dt) {
                    float t = p / dt;
                    raw_sq += (t + t - t * t - 1.0f);
                } else if (p > 1.0f - dt) {
                    float t = (p - 1.0f) / dt;
                    raw_sq -= (t * t + t + t + 1.0f);
                }
                if (p > 0.5f - dt && p < 0.5f + dt) {
                    float t = (p - 0.5f) / dt;
                    raw_sq -= (1.0f - t * t);
                }
                osc = raw_sq * 0.70f;
            }
            v->cph += v->cinc;
            if (v->cph >= 1.0f) v->cph -= 1.0f;

            /* 2-pole resonant lowpass filter */
            float in_sig = osc * (v->amp * v->cur_amp);
            v->flt_y1 += flt_f * (in_sig - v->flt_y1 + flt_fb * (v->flt_y1 - v->flt_y2));
            v->flt_y2 += flt_f * (v->flt_y1 - v->flt_y2);
            sample += v->flt_y2;
        }

        float dry = sample * vol;

        /* Stereo Tape Delay processing (running at half sample rate 22050 Hz) */
        solo_dly_sub ^= 1;
        if (solo_dly_sub == 0) {
            /* Head 1 (Left tap) */
            float rpos_l = (float)solo_dly_w - d_samps;
            if (rpos_l < 0.0f) rpos_l += (float)SOLO_DLY_LEN;
            int il0 = (int)rpos_l;
            int il1 = il0 + 1;
            if (il1 >= SOLO_DLY_LEN) il1 = 0;
            float fl = rpos_l - (float)il0;
            solo_dly_out_l = ((float)solo_dly_buf[il0] + fl * ((float)solo_dly_buf[il1] - (float)solo_dly_buf[il0])) * (1.0f / 12000.0f);

            /* Head 2 (Right tap: 75% delay time for stereo ping-pong bounce) */
            float rpos_r = (float)solo_dly_w - d_samps * 0.75f;
            if (rpos_r < 0.0f) rpos_r += (float)SOLO_DLY_LEN;
            int ir0 = (int)rpos_r;
            int ir1 = ir0 + 1;
            if (ir1 >= SOLO_DLY_LEN) ir1 = 0;
            float fr = rpos_r - (float)ir0;
            solo_dly_out_r = ((float)solo_dly_buf[ir0] + fr * ((float)solo_dly_buf[ir1] - (float)solo_dly_buf[ir0])) * (1.0f / 12000.0f);

            /* Feedback lowpass damping filter */
            solo_dly_lp += damp_coef * (solo_dly_out_l - solo_dly_lp);
            if (fm_fabsf(solo_dly_lp) < 1e-6f) solo_dly_lp = 0.0f;

            /* Feedback loop write */
            float to_write = dry + solo_dly_lp * dly_fb;
            to_write = fm_clampf(to_write, -2.5f, 2.5f);
            solo_dly_buf[solo_dly_w] = (int16_t)(to_write * 12000.0f);
            if (++solo_dly_w >= SOLO_DLY_LEN) solo_dly_w = 0;
        }

        /* Mix dry and delay repeats to stereo output */
        mixl[k] += dry * pan_l + solo_dly_out_l * dly_mix;
        mixr[k] += dry * pan_r + solo_dly_out_r * dly_mix;

        /* Reverb send: dry sound + delay echoes filtered by Tone control */
        float rev_in = dry + (solo_dly_out_l + solo_dly_out_r) * 0.35f * dly_mix;
        solo_rev_damp_lp += rev_damp_alpha * (rev_in - solo_rev_damp_lp);
        if (fm_fabsf(solo_rev_damp_lp) < 1e-6f) solo_rev_damp_lp = 0.0f;
        rsend[k] = solo_rev_damp_lp * rev_mix * 1.5f;
    }
}
