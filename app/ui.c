/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   keys           the scale: each key adds or removes its note from the pool (the root stays)
 *   PLAY           the garden grows / rests        ARP   the next chord, now
 *   REC            FREEZE: the chord holds, the reverb rings on    OCT- / OCT+   the bells' octave
 *   ENV            the pad's colour: Rhodes / Voices (lit: Voices), kept with the piece
 *   SELECT tempo   ALGORITHM mode (loads a pool)   PRESETS the piece (Root is on the Setup page)
 *   HOME SEQ FX SEL GLO   the pages: LAYERS, MOTION, SPACE, TONE, SETUP (EDIT steps through them)
 *   KNOB 1-4       the page's four values
 *   SAVE           saves (it also saves by itself a few seconds after a change, while resting);
 *                  held 1.5 s: the piece back to its recipe */
#include "fastmath.h"

/* the pages, one a layer (a button each); SELECT steps through them in this order */
enum { V_GARDEN, V_TEXTURE, V_PAD, V_BELLS, V_BASS, V_BOWLS, V_PULSE, V_SPACE, V_SETUP, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Garden", "Texture", "Pad", "Bells", "Bass", "Bowls", "Pulse", "Space", "Setup"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_HOME, B_ENV, B_ARP, B_LFO, B_LFO, B_SEQ, B_FX, B_GLO};

static const struct {
    uint8_t btn, first, n;
} GROUP[] = {
    {B_HOME, V_GARDEN,  2},
    {B_ENV,  V_PAD,     1},
    {B_ARP,  V_BELLS,   1},
    {B_LFO,  V_BASS,    2},
    {B_SEQ,  V_PULSE,   1},
    {B_FX,   V_SPACE,   1},
    {B_GLO,  V_SETUP,   1},
};
#define NGROUP (int)(sizeof GROUP / sizeof GROUP[0])

static int group_of(int v)
{
    int g;
    for (g = 0; g < NGROUP; g++)
        if (v >= GROUP[g].first && v < GROUP[g].first + GROUP[g].n)
            return g;
    return -1;
}

/* the four knobs of each page: a parameter, or one of these; and what the knob is called there */
enum { K_LEDS = 100, K_CPU, K_NONE };
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_TEMPO, P_EVOLVE, P_MOTION, P_DRIFT},
    {P_TEXT_VOL, P_TEXT_TYPE, P_TEXT_TONE, P_TEXT_DENS},
    {P_PAD, P_SWELL, P_BRIGHT, P_PADTONE},
    {P_BELLS, P_DENSITY, P_BTONE, P_BOCT},
    {P_BASS, P_BASSOCT, P_GLIDE, P_FIFTH},
    {P_BOWLS_VOL, P_BOWLS_MODE, P_BOWLS_OCT, P_BOWLS_BLUR},
    {P_PULSE, P_HITS, P_SWING, P_HATS},
    {P_ECHO, P_ETIME, P_REVERB, P_SIZE},
    {P_ROOT, P_TUNE, P_MIDI, K_LEDS},
};
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Tempo", "Evolve", "Motion", "Drift"},
    {"Level", "Type", "Tone", "Density"},
    {"Level", "Swell", "Bright", "Colour"},
    {"Level", "Density", "Tone", "Octave"},
    {"Level", "Octave", "Glide", "Fifth"},
    {"Level", "Mode", "Octave", "Blur"},
    {"Level", "Hits", "Swing", "Hats"},
    {"Echo", "Echo time", "Reverb", "Size"},
    {"Root", "Tune", "MIDI out", "Lights"},
};

/* the screen's colours: per-piece atmospheric themes */
typedef struct {
    uint16_t bg, panel, line, dim, text, acc;
    uint16_t bell, bell_t;
    uint16_t pad, pad_t;
    uint16_t pulse, pulse_t;
    uint16_t bass, bass_t;
    uint16_t frost, frost_t;
} piece_theme_t;

static const piece_theme_t PIECE_THEMES[EN_NPIECES] = {
    /* 0: Garden (original warm botanical paper) */
    {RGB(244, 240, 214), RGB(236, 230, 196), RGB(208, 214, 150), RGB(112, 106, 58), RGB(29, 27, 2), RGB(64, 128, 124),
     RGB(84, 153, 149), RGB(176, 214, 206), RGB(112, 142, 62), RGB(208, 220, 146), RGB(70, 66, 28), RGB(214, 214, 160),
     RGB(138, 128, 52), RGB(222, 218, 160), RGB(60, 132, 160), RGB(170, 210, 216)},
    /* 1: Departures (morning mist & pearl dawn) */
    {RGB(16, 20, 28), RGB(24, 30, 42), RGB(46, 56, 76), RGB(100, 116, 140), RGB(240, 244, 252), RGB(244, 135, 175),
     RGB(180, 210, 255), RGB(35, 50, 75), RGB(245, 155, 190), RGB(65, 35, 55), RGB(140, 185, 235), RGB(30, 45, 65),
     RGB(190, 175, 225), RGB(45, 40, 60), RGB(180, 230, 255), RGB(35, 60, 85)},
    /* 2: Voices (cathedral twilight & stained glass) */
    {RGB(12, 10, 24), RGB(22, 18, 40), RGB(45, 38, 72), RGB(110, 95, 150), RGB(245, 240, 255), RGB(255, 195, 60),
     RGB(255, 210, 95), RGB(65, 48, 15), RGB(185, 95, 255), RGB(52, 22, 78), RGB(250, 160, 50), RGB(60, 35, 10),
     RGB(90, 145, 255), RGB(20, 35, 75), RGB(195, 175, 255), RGB(50, 40, 75)},
    /* 3: Afternoon (sunlit golden meadow) */
    {RGB(20, 22, 14), RGB(34, 38, 22), RGB(60, 68, 38), RGB(135, 145, 95), RGB(255, 252, 235), RGB(255, 190, 35),
     RGB(145, 225, 45), RGB(38, 62, 12), RGB(255, 195, 40), RGB(68, 50, 10), RGB(240, 140, 40), RGB(65, 32, 10),
     RGB(180, 215, 60), RGB(45, 58, 15), RGB(190, 240, 160), RGB(45, 65, 38)},
    /* 4: Night Bus (neon highway & wet asphalt) */
    {RGB(8, 12, 18), RGB(20, 22, 30), RGB(42, 45, 60), RGB(95, 105, 130), RGB(240, 245, 255), RGB(255, 55, 95),
     RGB(45, 215, 255), RGB(12, 50, 65), RGB(255, 65, 105), RGB(68, 12, 25), RGB(255, 215, 40), RGB(65, 52, 8),
     RGB(145, 75, 255), RGB(40, 15, 75), RGB(120, 235, 255), RGB(25, 60, 75)},
    /* 5: Shoreline (coastline & aquamarine surf) */
    {RGB(8, 18, 26), RGB(15, 32, 46), RGB(28, 58, 80), RGB(75, 125, 155), RGB(230, 248, 255), RGB(30, 225, 215),
     RGB(220, 250, 255), RGB(35, 65, 78), RGB(25, 210, 205), RGB(8, 55, 58), RGB(235, 185, 105), RGB(60, 44, 20),
     RGB(15, 140, 235), RGB(6, 38, 70), RGB(140, 240, 255), RGB(28, 65, 80)},
    /* 6: Glass (prism crystal dispersion) */
    {RGB(12, 10, 18), RGB(24, 20, 36), RGB(52, 44, 75), RGB(120, 105, 150), RGB(255, 255, 255), RGB(245, 110, 255),
     RGB(85, 225, 255), RGB(20, 55, 72), RGB(215, 95, 245), RGB(58, 20, 68), RGB(255, 120, 160), RGB(68, 25, 40),
     RGB(120, 150, 255), RGB(30, 40, 75), RGB(210, 240, 255), RGB(45, 60, 75)},
    /* 7: Tides (bioluminescence & deep ocean) */
    {RGB(6, 14, 22), RGB(12, 26, 40), RGB(24, 50, 75), RGB(65, 115, 145), RGB(215, 250, 255), RGB(45, 245, 185),
     RGB(50, 225, 235), RGB(10, 55, 62), RGB(35, 235, 175), RGB(8, 58, 44), RGB(75, 175, 255), RGB(15, 45, 75),
     RGB(30, 190, 240), RGB(8, 48, 68), RGB(120, 250, 230), RGB(25, 65, 60)},
    /* 8: Dreamscape (vapor twilight & pastel sunset) */
    {RGB(18, 12, 26), RGB(32, 22, 44), RGB(65, 46, 85), RGB(140, 115, 165), RGB(255, 242, 250), RGB(255, 145, 115),
     RGB(235, 125, 235), RGB(65, 28, 68), RGB(255, 155, 125), RGB(75, 38, 30), RGB(120, 215, 255), RGB(25, 55, 75),
     RGB(205, 115, 240), RGB(58, 25, 70), RGB(255, 190, 215), RGB(70, 45, 55)},
    /* 9: Solaris (solar flare & golden corona) */
    {RGB(18, 8, 14), RGB(36, 16, 22), RGB(80, 36, 40), RGB(155, 95, 80), RGB(255, 250, 230), RGB(255, 215, 0),
     RGB(255, 235, 90), RGB(70, 55, 15), RGB(255, 95, 25), RGB(70, 22, 8), RGB(255, 170, 25), RGB(65, 40, 10),
     RGB(230, 60, 25), RGB(60, 15, 10), RGB(255, 255, 200), RGB(75, 70, 45)},
    /* 10: Cassette (analog tape deck, VFD mint & ferric ribbon) */
    {RGB(16, 18, 22), RGB(28, 32, 38), RGB(52, 60, 70), RGB(110, 125, 140), RGB(246, 242, 230), RGB(45, 225, 190),
     RGB(255, 190, 45), RGB(55, 45, 15), RGB(180, 110, 60), RGB(50, 30, 15), RGB(255, 60, 60), RGB(65, 15, 18),
     RGB(60, 150, 215), RGB(15, 40, 60), RGB(180, 230, 240), RGB(40, 60, 65)},
    /* 11: Rain (petrichor & wet storm slate) */
    {RGB(12, 16, 22), RGB(22, 30, 40), RGB(42, 56, 72), RGB(95, 120, 145), RGB(235, 245, 255), RGB(110, 205, 255),
     RGB(165, 225, 255), RGB(30, 55, 75), RGB(75, 160, 225), RGB(18, 42, 65), RGB(140, 175, 210), RGB(32, 46, 60),
     RGB(95, 140, 195), RGB(22, 38, 58), RGB(190, 235, 255), RGB(38, 62, 78)},
    /* 12: Pulsar (laser grid & radar neon) */
    {RGB(6, 9, 14), RGB(14, 20, 30), RGB(26, 44, 65), RGB(65, 110, 155), RGB(235, 250, 255), RGB(0, 235, 255),
     RGB(50, 255, 180), RGB(10, 65, 45), RGB(0, 215, 255), RGB(6, 52, 68), RGB(255, 255, 255), RGB(45, 65, 85),
     RGB(25, 195, 255), RGB(6, 48, 70), RGB(100, 255, 245), RGB(20, 68, 65)},
    /* 13: Bamboo (wabi-sabi bamboo & moss) */
    {RGB(14, 16, 14), RGB(24, 28, 24), RGB(48, 56, 48), RGB(115, 128, 115), RGB(240, 245, 238), RGB(120, 215, 135),
     RGB(175, 225, 125), RGB(42, 58, 28), RGB(105, 205, 130), RGB(25, 52, 32), RGB(165, 185, 145), RGB(40, 48, 35),
     RGB(135, 175, 115), RGB(32, 45, 26), RGB(195, 235, 205), RGB(45, 65, 50)},
    /* 14: Aurora (northern lights & glacial night) */
    {RGB(6, 11, 18), RGB(12, 22, 34), RGB(24, 45, 68), RGB(70, 115, 155), RGB(230, 250, 255), RGB(35, 245, 165),
     RGB(245, 255, 255), RGB(40, 70, 90), RGB(45, 235, 160), RGB(8, 58, 40), RGB(155, 115, 255), RGB(42, 26, 75),
     RGB(30, 160, 240), RGB(8, 42, 70), RGB(150, 255, 235), RGB(28, 70, 65)},
    /* 15: Dusk (midnight noir amber & copper) */
    {RGB(14, 8, 10), RGB(22, 18, 24), RGB(48, 40, 52), RGB(125, 110, 120), RGB(245, 238, 235), RGB(245, 125, 45),
     RGB(190, 160, 145), RGB(52, 42, 38), RGB(230, 115, 40), RGB(65, 30, 8), RGB(175, 125, 95), RGB(50, 32, 22),
     RGB(215, 95, 35), RGB(58, 24, 6), RGB(225, 175, 155), RGB(60, 42, 38)},
};

static piece_theme_t cur_theme;
static int cur_piece_theme = -1;

#define K_BG      (cur_theme.bg)
#define K_PANEL   (cur_theme.panel)
#define K_LINE    (cur_theme.line)
#define K_DIM     (cur_theme.dim)
#define K_TEXT    (cur_theme.text)
#define K_ACC     (cur_theme.acc)
#define K_BELL    (cur_theme.bell)
#define K_BELL_T  (cur_theme.bell_t)
#define K_PAD     (cur_theme.pad)
#define K_PAD_T   (cur_theme.pad_t)
#define K_PULSE   (cur_theme.pulse)
#define K_PULSE_T (cur_theme.pulse_t)
#define K_BASS    (cur_theme.bass)
#define K_BASS_T  (cur_theme.bass_t)
#define K_FROST   (cur_theme.frost)
#define K_FROST_T (cur_theme.frost_t)

static struct {
    uint8_t view;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];               /* detents in the current quick run, per knob (accel) */
    uint32_t down_t[NB];               /* when each button went down */
    uint32_t turn_t;                   /* the last knob turn */
    uint8_t help;                      /* a button's card is up: the button + 1 */
    uint8_t reset_done;                /* SAVE held: the piece went back to its recipe this hold */
    uint8_t dirty;                     /* the project changed since the last save */
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint8_t solo_mode;                 /* 1: in Solo Play Mode, 0: normal scale mode */
    uint8_t solo_page;                 /* 0..4 (ADSR, Tone, Delay, Reverb, Preset) */
    uint32_t solo_held_keys;           /* bitmask of keys currently held in solo mode */
    int8_t solo_touched;               /* knob touched in solo mode: 0..3 */
    uint32_t solo_touch_until;
    uint32_t solo_save_arm_t;          /* timestamp when 4th knob save was armed */
    uint8_t cosmic_mode;               /* 1: Cosmic Visualizer / Screensaver active */
    uint32_t frame;
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
} ui;

static void update_theme(void)
{
    int p = (unsigned)proj.piece < EN_NPIECES ? (int)proj.piece : 0;
    if (cur_piece_theme != p) {
        cur_piece_theme = p;
        cur_theme = PIECE_THEMES[p];
        ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;
    }
}

static const uint8_t SOLO_PAGE_KNOBS[NSOLO_PAGES][4] = {
    { SP_PRESET, SP_VOL, SP_PAN, SP_GLIDE },
    { SP_ATTACK, SP_DECAY, SP_SUSTAIN, SP_RELEASE },
    { SP_WAVE, SP_CUTOFF, SP_RES, SP_OCTAVE },
    { SP_DLY_MIX, SP_DLY_TIME, SP_DLY_FB, SP_DLY_DAMP },
    { SP_REV_MIX, SP_REV_SIZE, SP_REV_DAMP, SP_REV_WID },
    { SP_ARP_MODE, SP_ARP_RATE, SP_ARP_GATE, SP_ARP_OCT }
};

/* the screen reader (announce.c; speak() below): a message went to it this frame, so speak() leaves what it
 * changed be; and the transport as last said (PLAY says its own: the engine takes it a block later) */
static uint8_t sp_said;
static struct {
    uint8_t playing;
    uint32_t play_t;                   /* when PLAY (or the start) set it: the engine catches up within ms */
} sp_tp;
static void sp_piece(void);
static void msg_text(const char *a, const char *b, char *t)   /* the message as a reader gets it: "a b" */
{
    uint32_t i, k = 0;
    for (i = 0; i < 23u && a && a[i]; i++)
        t[k++] = a[i];
    if (b && b[0])
        t[k++] = ' ';
    for (i = 0; i < 23u && b && b[i]; i++)
        t[k++] = b[i];
    while (k && t[k - 1] == '.')               /* "UPDATE MODE IN 3..": the dots are for the eye */
        k--;
    t[k] = 0;
}
/* a message for a moment in the header; flags: how a screen reader gets it (0: the caller tells it) */
static void say_as(const char *a, const char *b, uint32_t flags)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    if (flags && ann_on()) {
        msg_text(a, b, t);
        ann_say(t, 0, flags);
        sp_said = 1;
    }
}
static void say(const char *a, const char *b) { say_as(a, b, ANN_NOW); }
/* something did not happen: a screen reader says it whole (what follows does not cut it off) */
static void warn(const char *a, const char *b) { say_as(a, b, ANN_NOW | ANN_KEEP); }
void ui_say(const char *a, const char *b) { say(a, b); }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void mark_dirty(void) { ui.dirty = 1; }
int ui_dirty(void) { return ui.dirty != 0; }

static int pool_count(uint32_t p)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return n;
}
static uint32_t pool_abs(void)                 /* the pool as absolute pitch classes */
{
    uint32_t p = proj.pool, r = (uint32_t)proj.par[P_ROOT];
    return ((p << r) | (p >> (12u - r))) & 0xFFFu;
}
static const char *mode_name(void)
{
    return proj.pool == EN_MODE_POOL[proj.par[P_MODE]] ? EN_MODE_NAME[proj.par[P_MODE]] : "Custom";
}

/* ------------------------------------------------------------ values --- */
static int knob_get(int k)
{
    if (k < P_NPARAMS)
        return proj.par[k];
    if (k == K_LEDS)
        return proj.leds;
    if (k == K_CPU)
        return (int)plat_cpu_pct();
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    if (k < P_NPARAMS) {
        *lo = en_param_info(k)->lo;
        *hi = en_param_info(k)->hi;
    } else {
        *lo = 0;
        *hi = k == K_LEDS ? 2 : k == K_CPU ? 100 : 0;
    }
}
static const char *knob_name(int k)
{
    if (k < P_NPARAMS)
        return en_param_info(k)->name;
    return k == K_LEDS ? "Lights" : k == K_CPU ? "CPU" : "";
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k < P_NPARAMS) {
        en_param_text(k, v, b);
    } else if (k == K_LEDS) {                    /* On: the keys, and the unlit buttons glow; Keys: no glow */
        const char *n = v == 2 ? "Keys" : v ? "On" : "Off";
        while ((*b++ = *n++))
            ;
    } else if (k == K_CPU) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else {
        b[0] = 0;
    }
}

static void knob_set(int k, int v)
{
    int lo, hi;
    if (k == K_CPU || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (k < P_NPARAMS) {
        proj.par[k] = (int16_t)v;
        en_set(k, v);
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    }
    mark_dirty();
}

/* knob acceleration by speed (X0X's): a few clicks are one step each however quick, so small
 * corrections stay exact; a longer quick turn speeds up to 8 a detent, so half a turn sweeps 0-100.
 * Small ranges never accelerate. */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;                                      /* per detent */
    ui.enc_run[role] = (uint8_t)(dt < 60u ? (ui.enc_run[role] + a > 255u ? 255u : ui.enc_run[role] + a) : a);
    if (range <= 24 || accel_off || ui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
    if (range < 100 && m > 3u)
        m = 3u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (!e || k == K_NONE || k == K_CPU)
        return;
    knob_range(k, &lo, &hi);
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

/* ------------------------------------------------------------- the pool --- */
static void toggle_note(int pc)                 /* a key: its note in or out of the scale */
{
    int rel = (pc - proj.par[P_ROOT] + 12) % 12;
    uint32_t p = proj.pool ^ (1u << rel);
    if (rel == 0) {
        warn("THE ROOT STAYS", "(SETUP)");
        return;
    }
    if (pool_count(p) < 3) {
        warn("3 NOTES", "AT LEAST");
        return;
    }
    proj.pool = (uint16_t)p;
    en_pool(p);
    say(p >> rel & 1u ? "ADDED" : "REMOVED", EN_NOTE_NAME[pc]);
    mark_dirty();
}

static void set_mode(int m)
{
    m = (m % EN_NMODES + EN_NMODES) % EN_NMODES;
    proj.par[P_MODE] = (int16_t)m;
    proj.pool = EN_MODE_POOL[m];
    en_set(P_MODE, m);
    en_pool(proj.pool);
    say("MODE", EN_MODE_NAME[m]);
    mark_dirty();
}

static void set_piece(int i)                    /* the piece playing kept as it is; piece i as it was left */
{
    project_piece(i);
    project_apply();
    en_seed(plat_ms());
    en_regrow();
    say_as("PIECE", piece_name(proj.piece), 0);
    sp_piece();                                  /* a screen reader: the piece with its key and tempo */
    mark_dirty();
}

static uint32_t ui_rng_state = 0x9E3779B9u;
static uint32_t ui_rand(void)
{
    ui_rng_state = ui_rng_state * 1664525u + 1013904223u;
    return ui_rng_state;
}
static int ui_rand_range(int min_val, int max_val)
{
    if (min_val >= max_val) return min_val;
    return min_val + (int)(ui_rand() % (uint32_t)(max_val - min_val + 1));
}

static void randomize_world(void)
{
    ui_rng_state ^= plat_ms() + (ui_rng_state << 5);

    /* 1. Scale and Root */
    int root = ui_rand_range(0, 11);
    int mode = ui_rand_range(0, EN_NMODES - 1);
    proj.par[P_ROOT] = (int16_t)root;
    proj.par[P_MODE] = (int16_t)mode;
    proj.pool = EN_MODE_POOL[mode];

    /* 2. Tempo and Garden dynamics */
    proj.par[P_TEMPO] = (int16_t)ui_rand_range(48, 86);
    proj.par[P_EVOLVE] = (int16_t)ui_rand_range(25, 65);
    proj.par[P_DRIFT] = (int16_t)ui_rand_range(15, 55);
    proj.par[P_MOTION] = (int16_t)ui_rand_range(30, 80);

    /* 3. Pad layer */
    proj.par[P_PAD] = (int16_t)ui_rand_range(45, 85);
    proj.par[P_PADTONE] = (int16_t)(ui_rand() % 2);
    proj.par[P_BRIGHT] = (int16_t)ui_rand_range(20, 70);
    proj.par[P_SWELL] = (int16_t)ui_rand_range(40, 85);

    /* 4. Bells layer */
    proj.par[P_BELLS] = (int16_t)ui_rand_range(40, 80);
    proj.par[P_DENSITY] = (int16_t)ui_rand_range(25, 60);
    proj.par[P_BTONE] = (int16_t)ui_rand_range(10, 75);
    proj.par[P_BOCT] = (int16_t)ui_rand_range(-1, 1);

    /* 5. Bass layer */
    int has_bass = (ui_rand() % 100) < 75;
    proj.par[P_BASS] = (int16_t)(has_bass ? ui_rand_range(25, 60) : 0);
    proj.par[P_BASSOCT] = (int16_t)ui_rand_range(-1, 0);
    proj.par[P_GLIDE] = (int16_t)ui_rand_range(15, 50);
    proj.par[P_FIFTH] = (int16_t)ui_rand_range(0, 50);

    /* 6. Bowls layer */
    int has_bowls = (ui_rand() % 100) < 70;
    proj.par[P_BOWLS_VOL] = (int16_t)(has_bowls ? ui_rand_range(15, 45) : 0);
    proj.par[P_BOWLS_MODE] = (int16_t)ui_rand_range(0, 3);
    proj.par[P_BOWLS_OCT] = (int16_t)ui_rand_range(-1, 0);
    proj.par[P_BOWLS_BLUR] = (int16_t)ui_rand_range(25, 80);

    /* 7. Pulse layer */
    int has_pulse = (ui_rand() % 100) < 55;
    proj.par[P_PULSE] = (int16_t)(has_pulse ? ui_rand_range(20, 55) : 0);
    proj.par[P_HITS] = (int16_t)ui_rand_range(15, 55);
    proj.par[P_SWING] = (int16_t)(ui_rand_range(0, 3) * 10);
    proj.par[P_HATS] = (int16_t)ui_rand_range(20, 65);

    /* 8. Textures */
    int has_text = (ui_rand() % 100) < 50;
    proj.par[P_TEXT_VOL] = (int16_t)(has_text ? ui_rand_range(15, 45) : 0);
    proj.par[P_TEXT_TYPE] = (int16_t)ui_rand_range(0, 4);
    proj.par[P_TEXT_TONE] = (int16_t)ui_rand_range(30, 70);
    proj.par[P_TEXT_DENS] = (int16_t)ui_rand_range(30, 70);

    /* 9. Space */
    proj.par[P_ECHO] = (int16_t)ui_rand_range(30, 75);
    proj.par[P_ETIME] = (int16_t)ui_rand_range(1, 5);
    proj.par[P_REVERB] = (int16_t)ui_rand_range(55, 90);
    proj.par[P_SIZE] = (int16_t)ui_rand_range(65, 95);

    /* 10. Update state and engine */
    proj.random_slots |= (uint16_t)(1u << proj.piece);
    proj.is_random = 1;
    project_apply();
    en_seed(plat_ms());
    en_regrow();
    say("RANDOM WORLD", "GENERATED");
    sp_piece();
    mark_dirty();
    ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;
}

/* ------------------------------------------------------------ buttons --- */
static void cosmic_seed_stars(void);
static void cosmic_trigger_stars_note(uint8_t note);
static void aura_trigger_note(uint8_t note);
static void draw_aura(void);

static void set_view(int v)
{
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static void button(int b)
{
    if (ui.cosmic_mode) {
        if (b == B_HOME) {
            ui.cosmic_mode = (ui.cosmic_mode == 1) ? 2 : 1;
            if (ui.cosmic_mode == 1) {
                say("SLOT 1", "AURA");
            } else {
                cosmic_seed_stars();
                say("SLOT 2", "STARS");
            }
            return;
        }
        ui.cosmic_mode = 0;
        ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;
    }
    int g;
    for (g = 0; g < NGROUP; g++)
        if (GROUP[g].btn == b) {
            if (ui.solo_mode) {
                if (b == B_ARP) {
                    /* Special case: in Solo mode, ARP button toggles Arpeggiator On/Off */
                    int arp_running = (solo_get(SP_ARP_MODE) > 0 && solo_arp_hold_get());
                    if (arp_running) {
                        /* Turn ARP completely OFF */
                        solo_set(SP_ARP_MODE, 0);
                        solo_arp_stop();
                        ui.solo_held_keys = 0;
                        say("ARP", "OFF");
                    } else {
                        /* Turn ARP ON with Hold */
                        int mode = solo_get(SP_ARP_MODE);
                        if (mode == 0) mode = solo_arp_last_mode_get();
                        solo_set(SP_ARP_MODE, mode > 0 ? mode : 1);
                        solo_arp_hold_set(1);
                        if (ui.solo_held_keys) {
                            solo_arp_capture_keys(ui.solo_held_keys);
                        }
                        ui.solo_held_keys = 0;
                        ui.solo_page = 5; /* Switch directly to 6/6 Arp page */
                        say("ARP", "ON");
                    }
                    ui.sig[0] = ui.sig[2] = 0;
                    ui.dirty = 1;
                    return;
                }
                ui.solo_mode = 0;
                if (!solo_arp_hold_get())
                    en_solo_panic();
                ui.solo_held_keys = 0;
                ui.sig[0] = ui.sig[2] = 0;
                if (b == B_HOME)
                    say("GARDEN", solo_arp_hold_get() ? "ARP RUNNING" : "SCALE MODE");
            }
            int v = ui.view;
            if (group_of(v) == g && GROUP[g].n > 1)
                set_view(GROUP[g].first + (v - GROUP[g].first + 1) % GROUP[g].n);
            else
                set_view(GROUP[g].first);
            return;
        }
    switch (b) {
    case B_SEL:
        ui.solo_mode = !ui.solo_mode;
        if (ui.solo_mode) {
            say("SOLO", "PLAY MODE");
        } else {
            if (solo_arp_hold_get()) {
                say("SOLO OFF", "ARP RUNNING");
            } else {
                say("SOLO OFF", "SCALE MODE");
                en_solo_panic();
            }
            ui.solo_held_keys = 0;
        }
        ui.sig[0] = ui.sig[2] = 0;
        ui.dirty = 1;
        break;
    case B_EDIT:
        en_next();
        say("NEXT CHORD", en_playing ? "ON THE BAR" : 0);
        break;
    case B_PLAY:
        sp_tp.playing = !en_playing;
        sp_tp.play_t = plat_ms();
        if (en_playing) {
            en_play(0);
            say("RESTING", 0);
        } else {
            en_seed(plat_ms());
            en_play(1);
            say("GROWING", 0);
        }
        break;
    case B_REC:
        say("FREEZE", en_frozen ? "OFF" : "ON");
        en_freeze(!en_frozen);                   /* the engine takes it at its next block */
        break;
    case B_OCTDN: case B_OCTUP:
        knob_set(P_BOCT, proj.par[P_BOCT] + (b == B_OCTUP ? 1 : -1));
        {
            char t[8];
            en_param_text(P_BOCT, proj.par[P_BOCT], t);
            say("BELL OCTAVE", t);
        }
        break;
    case B_SAVE:
        if (ui.solo_mode) {
            solo_save_preset(USER_PRESET_IDX);
            solo_set(SP_PRESET, USER_PRESET_IDX);
            proj.solo_preset = USER_PRESET_IDX;
            if (project_save() == 0) {
                ui.dirty = 0;
                say("SAVED TO", "USER");
            } else {
                say("NOT SAVED", "FLASH ERROR");
            }
            ui.sig[0] = ui.sig[2] = 0;
        } else {
            if (project_save() == 0) {
                ui.dirty = 0;
                say("SAVED", 0);
            } else {
                warn("NOT SAVED", "FLASH ERROR");
            }
        }
        break;
    default: break;
    }
}

/* MIDI in (USB and the TRS jack): channel 1 notes add or remove their note from the scale; clock and
 * start / stop drive the grid */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt, 0)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        else if (st == 0x90u && v) {
            if (ui.solo_mode) {
                en_solo_note((uint8_t)n, (uint8_t)v, 1);
                if (n >= 53 && n < 53 + NKEYS)
                    ui.solo_held_keys |= (1u << (n - 53));
            } else
                toggle_note((int)(n % 12u));
        } else if ((st == 0x80u || (st == 0x90u && !v)) && ui.solo_mode) {
            en_solo_note((uint8_t)n, 0, 0);
            if (n >= 53 && n < 53 + NKEYS)
                ui.solo_held_keys &= ~(1u << (n - 53));
        }
    }
}

/* HELP CARDS (X0X's): hold one button alone for HELP_HOLD and a card says what it does. A key, a
 * turn or another button puts it away; letting go after its card does nothing else. PLAY acts on
 * its press: it has no card. */
#define HELP_HOLD 900u
#define RESET_HOLD 1500u
static void help_tick(uint32_t btn, uint32_t keys)
{
    uint32_t now = plat_ms();
    int i;
    if (ui.help) {
        uint32_t m = 1u << (ui.help - 1);
        if (btn != m || keys || (int32_t)(ui.turn_t - ui.down_t[ui.help - 1]) > 0)
            ui.help = 0;
        return;
    }
    if (!btn || (btn & (btn - 1u)) || keys)
        return;
    for (i = 0; i < NB; i++)
        if (btn == 1u << i && i != B_PLAY && i != B_HOME && i != B_EDIT && !(ui.btn_used & btn) && now - ui.down_t[i] >= HELP_HOLD &&
            (int32_t)(ui.turn_t - ui.down_t[i]) <= 0) {
            ui.help = (uint8_t)(i + 1);
            ui.btn_used |= btn;                  /* its release now does nothing */
        }
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = plat_ms();
            if (i == B_SAVE)
                ui.reset_done = 0;
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            button((int)i);
        }
    }
#define RANDOM_HOLD 800u
    /* Long press EDIT (Random World generator) */
    if ((btn >> B_EDIT & 1u) && !(ui.btn_used & (1u << B_EDIT)) &&
        plat_ms() - ui.down_t[B_EDIT] >= RANDOM_HOLD &&
        (int32_t)(ui.turn_t - ui.down_t[B_EDIT]) <= 0) {
        ui.btn_used |= 1u << B_EDIT;
        ui.help = 0;
        if (ui.cosmic_mode) {
            ui.cosmic_mode = 0;
            ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;
        }
        randomize_world();
    }
#define COSMIC_HOLD 800u
    /* Long press HOME (Cosmic Visualizer / Screensaver) */
    if ((btn >> B_HOME & 1u) && !(ui.btn_used & (1u << B_HOME)) &&
        plat_ms() - ui.down_t[B_HOME] >= COSMIC_HOLD &&
        (int32_t)(ui.turn_t - ui.down_t[B_HOME]) <= 0) {
        ui.btn_used |= 1u << B_HOME;
        ui.help = 0;
        if (ui.cosmic_mode) {
            ui.cosmic_mode = 0;
            say("SAVER", "EXIT");
        } else {
            ui.cosmic_mode = 1; /* Slot 1: Aura Visualizer */
            say("AURA", "ACTIVE");
        }
        ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    if (ui.solo_mode) {
        if (solo_arp_hold_get()) {
            /* Interactive Arp Note Sculpting Mode: tapping a key toggles it in/out of the arpeggio! */
            for (i = 0; i < NKEYS; i++) {
                uint32_t m = 1u << i;
                if ((ch & keys) & m) {
                    uint8_t note = (uint8_t)(53 + i);
                    int added = solo_arp_toggle_note(note);
                    say(added ? "ADDED" : "REMOVED", EN_NOTE_NAME[note % 12]);
                }
            }
        } else {
            /* Normal live performance playing */
            for (i = 0; i < NKEYS; i++) {
                uint32_t m = 1u << i;
                if (ch & m) {
                    int note = 53 + (int)i;
                    if (keys & m) {
                        ui.solo_held_keys |= m;
                        en_solo_note((uint8_t)note, 100, 1);
                    } else {
                        ui.solo_held_keys &= ~m;
                        en_solo_note((uint8_t)note, 0, 0);
                    }
                }
            }
        }
    } else {
        for (i = 0; i < NKEYS; i++)
            if ((ch & keys) >> i & 1u) {
                toggle_note((int)((5u + i) % 12u));  /* key 0 is F3 */
                if (ui.cosmic_mode) {
                    cosmic_trigger_stars_note((uint8_t)(53 + i));
                    aura_trigger_note((uint8_t)(53 + i));
                }
            }
    }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* SELECT: the pages; with HOME held, the tempo */
        if (ui.cosmic_mode) { ui.cosmic_mode = 0; ui.sig[0] = ui.sig[1] = ui.sig[2] = 0; }
        if (ui.solo_mode) {
            int sp = (int)ui.solo_page + (e > 0 ? 1 : -1);
            if (sp < 0) sp = NSOLO_PAGES - 1;
            if (sp >= NSOLO_PAGES) sp = 0;
            ui.solo_page = (uint8_t)sp;
            ui.solo_touched = -1;
            ui.sig[0] = ui.sig[2] = 0;
        } else if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_GARDEN) {           /* Tempo is the Garden's first knob */
                ui.touched = 0;
                ui.touch_until = plat_ms() + 1200u;
            } else {
                char t[8];
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else {
            int v = (int)ui.view + (e > 0 ? 1 : -1);
            if (v >= 0 && v < NVIEWS)
                set_view(v);
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        if (ui.cosmic_mode) { ui.cosmic_mode = 0; ui.sig[0] = ui.sig[1] = ui.sig[2] = 0; }
        set_mode(proj.par[P_MODE] + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        if (ui.cosmic_mode) { ui.cosmic_mode = 0; ui.sig[0] = ui.sig[1] = ui.sig[2] = 0; }
        set_piece(proj.piece + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((btn >> B_SAVE & 1u) && !ui.reset_done && plat_ms() - ui.down_t[B_SAVE] > RESET_HOLD) {
        ui.reset_done = 1;                     /* held: the piece back to its recipe (not saved) */
        ui.btn_used |= 1u << B_SAVE;
        ui.help = 0;
        if (ui.solo_mode) {
            en_solo_reset();
            proj.solo_preset = 0;
            say("RESET", "PURE");
            ui.sig[0] = ui.sig[2] = 0;
            mark_dirty();
        } else {
            project_piece_reset();
            project_apply();
            en_regrow();
            say("RESET", piece_name(proj.piece));
            mark_dirty();
        }
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            if (ui.cosmic_mode) { ui.cosmic_mode = 0; ui.sig[0] = ui.sig[1] = ui.sig[2] = 0; }
            if (ui.solo_mode) {
                int p = SOLO_PAGE_KNOBS[ui.solo_page][i];
                int step = 1;
                if (p == SP_ATTACK || p == SP_DECAY || p == SP_RELEASE || p == SP_DLY_TIME)
                    step = e > 1 || e < -1 ? 25 : 5;
                else if (p == SP_CUTOFF || p == SP_RES || p == SP_DLY_MIX || p == SP_REV_MIX || p == SP_VOL || p == SP_GLIDE)
                    step = e > 1 || e < -1 ? 5 : 1;
                else if (p == SP_PAN || p == SP_ARP_GATE)
                    step = e > 1 || e < -1 ? 5 : 1;
                int prev_mode = solo_get(SP_ARP_MODE);
                int cur = solo_get(p) + e * step;
                en_solo_set(p, cur);
                if (p == SP_ARP_MODE) {
                    if (solo_get(SP_ARP_MODE) == 0) {
                        /* Turned to Off: completely stop and reset everything! */
                        solo_arp_stop();
                        ui.solo_held_keys = 0;
                        ui.sig[0] = ui.sig[2] = 0;
                    } else if (prev_mode == 0 && solo_get(SP_ARP_MODE) > 0) {
                        solo_arp_hold_set(0);
                        ui.solo_held_keys = 0;
                        ui.sig[0] = ui.sig[2] = 0;
                    }
                }
                if (p == SP_PRESET) {
                    proj.solo_preset = (uint8_t)solo_get(SP_PRESET);
                    if (!solo_arp_hold_get())
                        ui.solo_held_keys = 0;
                }
                ui.solo_touched = (int8_t)i;
                ui.solo_touch_until = plat_ms() + 1200u;
                ui.sig[2] = 0;
                mark_dirty();
            } else {
                turn(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
                ui.touched = (int8_t)i;
                ui.touch_until = plat_ms() + 1200u;
            }
            ui.act_t = ui.turn_t = plat_ms();
        }
    help_tick(btn, keys);
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the garden (144), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

/* a filled circle of radius r (0..6) centred on (cx, cy): HW[r][d] is the half width d rows from the middle */
static void cv_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    static const uint8_t HW[7][7] = {{0}, {1, 0}, {2, 1, 0}, {3, 2, 2, 0}, {4, 4, 3, 2, 0}, {5, 5, 4, 4, 3, 0},
                                     {6, 6, 5, 5, 4, 3, 0}};
    int32_t j;
    if (r <= 0) {
        cv_rect(cx, cy, 1, 1, c);
        return;
    }
    if (r > 6)
        r = 6;
    for (j = -r + 1; j < r; j++) {
        int32_t w = HW[r][j < 0 ? -j : j];
        cv_rect(cx - w, cy + j, 2 * w + 1, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

/* anti-aliased drawing (X0X's): a pixel blended over the canvas, a disc, and a knob's ring */
static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}

static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            px_blend(x, y, c, (uint32_t)(fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * 16.0f + 0.5f));
        }
}

/* atan2 in turns, 0..1 from +x going clockwise on screen (y down) */
static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;   /* atan on [0, 1] */
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

/* a knob: a 270-degree track from 7:30 to 4:30, anti-aliased, round ends; the value filled from
 * the start (or from the top, for a centred value) and a round handle */
static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, uint16_t bg, int bipolar)
{
    const float th = 3.4f, a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th, rm = ro - th * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
    int32_t x, y;
    v = fm_clampf(v, 0.0f, 1.0f);
    lo = bipolar ? (v < 0.5f ? v : 0.5f) : 0.0f;
    hi = bipolar ? (v < 0.5f ? 0.5f : v) : v;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, (t >= lo && t <= hi) ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e0 = a0 * FM_TWO_PI, e1 = (a0 + sweep) * FM_TWO_PI, ev = (a0 + sweep * v) * FM_TWO_PI;
        float el = (a0 + sweep * lo) * FM_TWO_PI;
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, th * 0.5f, fill);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 1.3f, bg);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 0.6f, fill);
    }
}

static void caps(char *d, const char *s, int n)  /* s in capitals, at most n - 1 characters */
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r, g, bl;
    if (t <= 0)
        return a;
    if (t >= 16)
        return b;
    r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16;
    g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16;
    bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static int q16(float x) { return x <= 0.0f ? 0 : x >= 1.0f ? 16 : (int)(x * 16.0f + 0.5f); }

static void tag(int32_t *x, const char *s, uint16_t bg, uint16_t fg)
{
    int32_t w = text_w(&FONT_XS, s) + 10;
    cv_round(*x, 6, w, 16, 6, bg);
    cv_text(*x + 5, 7, &FONT_XS, s, fg);
    *x += w + 4;
}

/* the battery (plat_battery): 0 (low) .. 3 bars, 4 a USB host powers the FM-1, -1 not read yet. The
 * simulator's "battery N" (host.c) stands in for it */
#ifdef PLAT_HOST
static int8_t batt_host = -2;                  /* -2: plat_battery's */
static int batt(void) { return batt_host > -2 ? batt_host : plat_battery(); }
#else
#define batt() plat_battery()
#endif
#define K_LOW RGB(196, 84, 52)                 /* the battery at one bar or none */
#define BATT_W 20                              /* the body 18 wide and its nub */
/* lying down at the header's right, its middle on the beat light's: three bars, rust at one or none; a
 * bolt instead on USB power; nothing until it is read */
static void draw_batt(int32_t x, int b)
{
    static const int8_t BOLT[7][2] = {{10, 2}, {9, 2}, {8, 2}, {7, 5}, {9, 2}, {8, 2}, {7, 2}};
    uint16_t c = b <= 1 ? K_LOW : K_DIM;
    int i;
    cv_round(x, 9, 18, 10, 2, c);
    cv_rect(x + 1, 10, 16, 8, K_BG);
    cv_rect(x + 18, 12, 2, 4, c);
    if (b == 4) {
        for (i = 0; i < 7; i++)
            cv_rect(x + BOLT[i][0], 10 + i, BOLT[i][1], 1, K_TEXT);
        return;
    }
    for (i = 0; i < b && i < 3; i++)
        cv_rect(x + 2 + i * 5, 11, 4, 6, b <= 1 ? K_LOW : K_PAD);
}
static const char *batt_words(int b)
{
    static const char *const W[5] = {"Battery low", "Battery one third", "Battery two thirds", "Battery full",
                                     "On USB power"};
    return b >= 0 && b <= 4 ? W[b] : 0;
}

/* the header: the piece in a tag, its root and mode; on the right, the battery, the tempo with a slow
 * beat light (a play triangle while resting) and a dot while there is something unsaved. Messages take
 * its place for a moment. */
static void draw_header(void)
{
    if (ui.solo_mode) {
        int msg = plat_ms() < ui.msg_until;
        uint32_t h = hash(2166136261u, (uint32_t)ui.solo_page | ((uint32_t)msg << 8) | ((uint32_t)solo_arp_hold_get() << 16));
        if (msg)
            h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
        if (h == ui.sig[0])
            return;
        ui.sig[0] = h;
        cv_begin(240, HDR_H, K_BG);
        if (msg) {
            int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
            cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
        } else {
            cv_round(4, 4, 50, 20, 5, K_ACC);
            text_c(4 + 50 / 2, 6, &FONT_B, "SOLO", K_PANEL);
            char pt[24];
            const char *page = solo_page_name(ui.solo_page);
            int i;
            pt[0] = (char)('1' + ui.solo_page);
            pt[1] = '/';
            pt[2] = (char)('0' + NSOLO_PAGES);
            pt[3] = ' ';
            for (i = 0; i < 16 && page[i]; i++)
                pt[4 + i] = page[i];
            pt[4 + i] = 0;
            cv_text(60, 6, &FONT_B, pt, K_TEXT);
            if (solo_arp_hold_get()) {
                cv_round(188, 4, 48, 20, 4, K_PULSE);
                text_c(188 + 24, 6, &FONT_B, "HOLD", K_BG);
            }
        }
        cv_blit(0, 0);
        return;
    }
    char t[24];
    int msg = plat_ms() < ui.msg_until;
    uint32_t h = hash(hash(hash(2166136261u, proj.piece | (uint32_t)proj.par[P_ROOT] << 8 | (uint32_t)proj.pool << 12),
                           en_playing | ui.dirty << 4 | (uint32_t)msg << 5 | (uint32_t)en_ext << 6 | (uint32_t)proj.par[P_MODE] << 8 | (uint32_t)proj.is_random << 16),
                      (uint32_t)proj.par[P_TEMPO] | (ui.view == V_SETUP ? plat_cpu_pct() << 8 : 0u) |
                      (uint32_t)(batt() + 1) << 24);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (en_playing)
        h = hash(h, (en_bars / 4u) & 1u);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 232, w;
        if (batt() >= 0) {
            x -= BATT_W;
            draw_batt(x, batt());
            x -= 6;
        }
        if (ui.dirty) {                            /* unsaved */
            cv_round(x - 5, 12, 5, 5, 2, K_DIM);
            x -= 11;
        }
        if (en_ext) {
            t[0] = 'E';
            t[1] = 'X';
            t[2] = 'T';
            t[3] = 0;
        } else {
            itoa_u((uint32_t)proj.par[P_TEMPO], t);
        }
        if (ui.view == V_SETUP) {                 /* Setup: the audio load too */
            char c[12] = "CPU ";
            itoa_u(plat_cpu_pct(), c + 4);
            x -= text_w(&FONT_XS, c);
            cv_text(x, 8, &FONT_XS, c, K_DIM);
            x -= 10;
        }
        x -= text_w(&FONT_S, t);
        cv_text(x, 6, &FONT_S, t, en_playing ? K_TEXT : K_DIM);
        x -= 16;
        if (en_playing) {                         /* a slow beat light */
            int on = ((en_bars / 4u) & 1u) == 0u;
            cv_round(x, 9, 10, 10, 5, on ? K_PAD : K_PAD_T);
        } else {
            int32_t j;                            /* resting: a play triangle */
            for (j = 0; j < 5; j++)
                cv_rect(x + 2 + j, 9 + j, 1, 11 - 2 * j, K_DIM);
        }
        int is_rnd = proj.is_random || (((proj.random_slots >> proj.piece) & 1u) != 0);
        uint16_t tag_bg = is_rnd ? K_ACC : K_TEXT;
        w = text_w(&FONT_B, piece_name(proj.piece)) + 12;   /* the piece, in a coral/accent tag */
        cv_round(4, 4, w, 20, 6, tag_bg);
        cv_text(10, 6, &FONT_B, piece_name(proj.piece), K_BG);
        w += 10;                                  /* the root, then the mode, where they clear the right side */
        if (w + text_w(&FONT_XS, EN_NOTE_NAME[proj.par[P_ROOT]]) + 4 <= x) {
            w = cv_text(w, 8, &FONT_XS, EN_NOTE_NAME[proj.par[P_ROOT]], K_TEXT);
            if (w + 3 + text_w(&FONT_XS, mode_name()) + 4 <= x)
                cv_text(w + 3, 8, &FONT_XS, mode_name(), K_DIM);
        }
    }
    cv_blit(0, 0);
}

/* the garden: the scale as a ring of twelve, the chord lit in it; the four bell loops around it, each
 * a ring of its steps with its notes on it and its playhead going round; the three pulses on the
 * right, the four layers' levels on the left */
#define CX 120
#define CY 84
static const int32_t LOOP_R[EN_NLOOP] = {36, 43, 50, 57};

static void polar(int32_t cx, int32_t cy, float r, int i, int n, int32_t *x, int32_t *y)
{
    float a = (float)i * (FM_TWO_PI / (float)n);  /* step i of n, clockwise from the top */
    float sx = fm_sinf(a) * r, sy = fm_cosf(a) * r;
    *x = cx + (int32_t)(sx + (sx < 0.0f ? -0.5f : 0.5f));
    *y = cy - (int32_t)(sy + (sy < 0.0f ? -0.5f : 0.5f));
}

static const char *chord_now(char *nm)          /* the chord's name, as the garden shows it ("Em7") */
{
    int pcs[EN_NPAD + 1], k;
    pcs[0] = en_chord_root;
    for (k = 0; k < EN_NPAD; k++)
        pcs[k + 1] = en_chord[k] % 12;
    return en_chord_name(pcs, EN_NPAD + 1, nm);
}

static void draw_ring(void)
{
    uint32_t abs_pool = pool_abs(), chord = 0;
    int i, l;
    for (i = 0; i < EN_NPAD; i++)
        chord |= 1u << (en_chord[i] % 12u);
    for (l = 0; l < EN_NLOOP; l++) {
        int len = EN_LOOP_LEN[l], s, pos = en_loop_pos[l], g = q16(en_loop_glow[l]);
        for (s = 0; s < len; s++) {
            int32_t x, y;
            polar(CX, CY, (float)LOOP_R[l], s, len, &x, &y);
            if (en_loop_note[l][s] >= 0)
                cv_disc(x, y, 2, (s == pos && en_playing) ? mix(K_BELL, K_FROST, g / 2) : mix(K_BELL_T, K_BELL, 12));
            else
                cv_rect(x, y, 2, 2, K_LINE);
        }
        if (en_playing) {                        /* the playhead */
            int32_t x, y;
            polar(CX, CY, (float)LOOP_R[l], pos, len, &x, &y);
            cv_disc(x, y, 1 + g / 6, mix(K_DIM, K_BELL, g));
        }
    }
    for (i = 0; i < 12; i++) {                  /* the scale: C at the top */
        int32_t x, y;
        int in = (int)(abs_pool >> i & 1u), ch = (int)(chord >> i & 1u);
        polar(CX, CY, 24.0f, i, 12, &x, &y);
        if (i == proj.par[P_ROOT])
            cv_disc(x, y, 5, K_TEXT);           /* the root: ringed in ink */
        if (ch)
            cv_disc(x, y, 4, mix(K_PAD_T, K_PAD, 6 + q16(en_pad_level) * 10 / 16));
        else if (in)
            cv_disc(x, y, 3, mix(K_PAD_T, K_PAD, 7));
        else
            cv_disc(x, y, 1, K_LINE);
    }
    {   /* the chord's name, in the middle */
        char nm[16];
        chord_now(nm);
        if (text_w(&FONT_B, nm) <= 38)
            text_c(CX, CY - 8, &FONT_B, nm, en_frozen ? K_FROST : K_TEXT);
        else
            text_c(CX, CY - 6, &FONT_XS, nm, en_frozen ? K_FROST : K_TEXT);
    }
}

static void draw_meters(void)                    /* left: the four layers */
{
    const uint16_t C[4] = {K_PAD, K_BELL, K_BASS, K_PULSE}, T[4] = {K_PAD_T, K_BELL_T, K_BASS_T, K_PULSE_T};
    float lv[4], bm = 0.0f, dm = 0.0f;
    int i;
    for (i = 0; i < EN_NLOOP; i++)
        bm = fm_maxf(bm, en_loop_glow[i]);
    for (i = 0; i < EN_NDRUM; i++)
        dm = fm_maxf(dm, en_drum_glow[i]);
    lv[0] = en_pad_level * (float)proj.par[P_PAD] * 0.01f;
    lv[1] = bm * (float)proj.par[P_BELLS] * 0.01f;
    lv[2] = en_bass_level * (float)proj.par[P_BASS] * 0.01f;
    lv[3] = dm * (float)proj.par[P_PULSE] * 0.01f;
    float lv_bowls = en_bowls_level * (float)proj.par[P_BOWLS_VOL] * 0.01f;
    int32_t hd = (int32_t)(fm_clampf(lv_bowls, 0.0f, 1.0f) * 80.0f + 0.5f);

    for (i = 0; i < 4; i++) {
        int32_t x = 10 + i * 11, h = (int32_t)(fm_clampf(lv[i], 0.0f, 1.0f) * 80.0f + 0.5f);
        cv_round(x, 46, 7, 86, 3, T[i]);
        if (i == 2 && proj.par[P_BOWLS_VOL] > 0) {
            /* Bass & Bowls dual meter on bar 3 */
            if (proj.par[P_BASS] > 0) {
                if (h > 0)
                    cv_round(x, 46 + 86 - h - 2, 3, h + 2, 1, C[i]);
                if (hd > 0)
                    cv_round(x + 4, 46 + 86 - hd - 2, 3, hd + 2, 1, K_FROST);
            } else {
                if (hd > 0)
                    cv_round(x, 46 + 86 - hd - 2, 7, hd + 2, 3, K_FROST);
            }
        } else {
            if (h > 0)
                cv_round(x, 46 + 86 - h - 2, 7, h + 2, 3, C[i]);
        }
    }
}

static void draw_pulse(void)                     /* right: the three Euclidean rings */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++) {
        int32_t cx = 212, cy = 52 + d * 34, s, n = EN_DRUM_N[d];
        int g = q16(en_drum_glow[d]);
        uint32_t p = en_drum_pat[d];
        for (s = 0; s < n; s++) {
            int32_t x, y;
            int cur = en_playing && s == en_drum_pos[d];
            polar(cx, cy, 13.0f, s, n, &x, &y);
            if ((p >> s & 1u) && en_pulse_rest)   /* sitting a few bars out: faded */
                cv_disc(x, y, 2, K_LINE);
            else if (p >> s & 1u)
                cv_disc(x, y, cur ? 3 : 2, cur ? K_PULSE : mix(K_PULSE_T, K_PULSE, 10));
            else if (cur)
                cv_disc(x, y, 1, K_DIM);
            else
                cv_rect(x, y, 2, 2, K_LINE);
        }
        if (g)
            cv_disc(cx, cy, 1 + g / 4, mix(K_BG, K_PULSE, g));
    }
}

/* a button's card (help_tick): its name, then what it does, in the garden's place. A line starting with a
 * space goes on from the one before (a screen reader joins them: sp_line) */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0;
#define L(t) (l[n++] = (t))
    switch (b) {
    case B_HOME: L("THE GARDEN"); L("PAGE 1: MOTION  2: TEXTURE"); L("+ SELECT: TEMPO"); L("HOLD: COSMIC SAVER"); break;
    case B_SEL:
        if (ui.solo_mode) {
            L("SOLO PLAY MODE");
            L("KEYS PLAY THE SYNTH");
            L("SELECT: 6 PAGES");
            L("PRESS TO EXIT TO SCALE");
        } else {
            L("SOLO PLAY MODE");
            L("KEYS BECOME A SYNTH");
            L("6 PAGES & ARPEGGIATOR");
            L("PRESS TO ENTER");
        }
        break;
    case B_ENV: L("THE PAD"); L("LEVEL, SWELL, BRIGHT,"); L("COLOUR"); break;
    case B_ARP: L("THE BELLS"); L("LEVEL, DENSITY, TONE,"); L("OCTAVE"); break;
    case B_LFO: L("BASS & BOWLS"); L("P1: BASS  P2: BOWLS"); L("MODE, OCTAVE, BLUR CLOUD"); break;
    case B_SEQ: L("THE PULSE"); L("LEVEL, HITS, SWING, HATS"); break;
    case B_FX: L("THE SPACE"); L("ECHO, ECHO TIME, REVERB,"); L("SIZE"); break;
    case B_GLO: L("SETUP"); L("ROOT, TUNE, MIDI OUT, LIGHTS"); break;
    case B_EDIT: L("THE NEXT CHORD,"); L("ON THE NEXT BAR"); L("HOLD: RANDOM WORLD"); break;
    case B_REC: L("FREEZE: THE CHORD HOLDS"); L(" AND THE REVERB RINGS ON"); L("AGAIN: THAW"); break;
    case B_OCTDN: L("THE BELLS AN OCTAVE DOWN"); L("WITH OCT+, 5 S: UPDATE MODE"); break;
    case B_OCTUP: L("THE BELLS AN OCTAVE UP"); L("WITH OCT-, 5 S: UPDATE MODE"); break;
    case B_SAVE:
        if (ui.solo_mode) {
            L("SAVES TO USER PRESET");
            L("FLASH & LOCALSTORAGE");
            L("HOLD 1.5S: RESET TO");
            L("INIT SOLO");
        } else {
            L("SAVES ALL 16 PIECES");
            L("KEEP HOLDING: THE PIECE");
            L(" BACK TO ITS RECIPE");
        }
        break;
    default: break;
    }
#undef L
    return n;
}

static void draw_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    cv_round(16, 14, 208, 116, 6, K_PANEL);
    cv_round(16, 14, 208, 26, 6, K_TEXT);
    cv_rect(16, 34, 208, 6, K_TEXT);
    text_c(120, 18, &FONT_B, BTN_LABEL[ui.help - 1], K_BG);
    for (k = 0; k < n; k++)
        text_c(120, 56 + k * 20, &FONT_S, l[k] + (l[k][0] == ' '), K_TEXT);
}

static void draw_main(void)
{
    uint32_t h = hash(2166136261u, pool_abs() | (uint32_t)proj.par[P_ROOT] << 12 | (uint32_t)en_frozen << 16 | proj.piece << 24 |
                                        (uint32_t)en_playing << 17 | (uint32_t)proj.par[P_MODE] << 20), i;
    if ((ui.frame % 3u) && ui.sig[1] != 0)       /* the garden moves slowly: 20 frames a second is plenty */
        return;
    if (ui.help) {
        h = hash(0x48454C50u, ui.help);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_help();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    for (i = 0; i < EN_NPAD; i++)
        h = hash(h, en_chord[i]);
    for (i = 0; i < EN_NLOOP; i++) {
        uint32_t s;
        h = hash(h, en_loop_pos[i] | (uint32_t)q16(en_loop_glow[i]) << 8);
        for (s = 0; s < EN_LOOP_LEN[i]; s++)
            h = hash(h, (uint32_t)(en_loop_note[i][s] >= 0));
    }
    for (i = 0; i < EN_NDRUM; i++)
        h = hash(h, en_drum_pos[i] | (uint32_t)q16(en_drum_glow[i]) << 8 | en_drum_pat[i] << 13 | (uint32_t)en_pulse_rest << 31);
    h = hash(h, (uint32_t)(en_drift * 40.0f) | (uint32_t)q16(en_pad_level) << 8 | (uint32_t)q16(en_bass_level) << 13 | (uint32_t)q16(en_bowls_level) << 18);
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)proj.par[i]);
    h = hash(h, (uint32_t)proj.par[P_BOWLS_VOL]);
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    if (en_frozen) {                             /* the way to the next chord, top right (held: frozen) */
        cv_text(232 - text_w(&FONT_XS, "held"), 4, &FONT_XS, "held", K_FROST);
    } else {
        int32_t w = 30, f = (int32_t)(en_drift * (float)w + 0.5f);
        cv_round(232 - w, 8, w, 6, 3, K_LINE);
        if (f > 0)
            cv_round(232 - w, 8, f < 6 ? 6 : f, 6, 3, K_PAD);
    }
    draw_ring();
    draw_meters();
    draw_pulse();
    cv_blit(0, MAIN_Y);
}

/* the knob strip (X0X's): a rule with the page's name and five page dots at its right end; under
 * it a cell a knob: its name, a ring (or pips, for a switch of a few positions), the value. The
 * knob being turned is drawn on a tint. Only redrawn when what it shows changes (the rings cost a
 * square root and an arctangent per pixel). */
static void draw_knobs(void)
{
    if (ui.solo_mode) {
        int now_touch = plat_ms() < ui.solo_touch_until;
        uint32_t h = hash(0x534F4C4Fu, ui.solo_page | (uint32_t)ui.solo_touched << 8 | (uint32_t)proj.piece << 16 | ((uint32_t)solo_arp_hold_get() << 24)), i;
        for (i = 0; i < 4; i++)
            h = hash(h, (uint32_t)solo_get(SOLO_PAGE_KNOBS[ui.solo_page][i]));
        h = hash(h, (uint32_t)now_touch);
        if (h == ui.sig[2])
            return;
        ui.sig[2] = h;
        cv_begin(240, KNB_H, K_BG);
        {
            char t[16];
            int k;
            int32_t xd = 237 - NSOLO_PAGES * 8, xt;
            caps(t, solo_page_name(ui.solo_page), sizeof t);
            xt = xd - 5 - text_w(&FONT_XS, t);
            cv_rect(4, 5, xt - 9, 1, K_LINE);
            cv_text(xt, -1, &FONT_XS, t, K_ACC);
            for (k = 0; k < NSOLO_PAGES; k++)
                cv_round(xd + k * 8, 3, 5, 5, 2, k == ui.solo_page ? K_ACC : K_LINE);
        }
        for (i = 0; i < 4; i++) {
            int p = SOLO_PAGE_KNOBS[ui.solo_page][i];
            int lo, hi, v, hot = now_touch && ui.solo_touched == (int)i;
            int32_t x = (int32_t)i * 60, cx = x + 30;
            char t[16];
            uint16_t bg = hot ? K_PANEL : K_BG, fill = hot ? K_ACC : K_PAD;
            if (i)
                cv_rect(x, 16, 1, 46, K_LINE);
            if (hot)
                cv_round(x + 3, 11, 54, 57, 6, K_PANEL);
            solo_param_range(p, &lo, &hi);
            v = solo_get(p);
            caps(t, solo_param_name(p), sizeof t);
            text_c(cx, 12, &FONT_XS, t, hot ? K_TEXT : K_DIM);
            if (hi - lo < 6) {
                int n = hi - lo + 1, j, pw = n > 4 ? 6 : 9, gap = 3, w0 = n * (pw + gap) - gap;
                for (j = 0; j < n; j++)
                    cv_round(cx - w0 / 2 + j * (pw + gap), 33, pw, 8, 2, j == v - lo ? fill : K_LINE);
            } else {
                arc(cx, 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
            }
            solo_param_text(p, v, t);
            uint16_t col = (p == SP_ARP_MODE && solo_arp_hold_get()) ? K_PULSE : (hot ? K_ACC : K_TEXT);
            text_c(cx, 50, &FONT_B, t, col);
        }
        cv_blit(0, KNB_Y);
        return;
    }
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8 | (uint32_t)proj.piece << 16), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(VIEW_KNOB[ui.view][i]));
    h = hash(h, (uint32_t)now_touch);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    {   /* the rule; at its right end, where you are: the page, then the dots */
        char t[12];
        int k;
        int32_t xd = 237 - NVIEWS * 6, xt;
        caps(t, VIEW_NAME[ui.view], sizeof t);
        xt = xd - 5 - text_w(&FONT_XS, t);
        cv_rect(4, 5, xt - 9, 1, K_LINE);
        cv_text(xt, -1, &FONT_XS, t, K_ACC);
        for (k = 0; k < NVIEWS; k++) {
            int cur = (k == ui.view);
            int same = (group_of(k) >= 0 && group_of(k) == group_of(ui.view));
            cv_round(xd + k * 6, 3, 4, 4, 2, cur ? K_ACC : (same ? K_DIM : K_LINE));
        }
    }
    for (i = 0; i < 4; i++) {
        int k = VIEW_KNOB[ui.view][i], lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        uint16_t bg = hot ? K_PANEL : K_BG, fill = hot ? K_ACC : K_PAD;
        if (k == K_NONE)
            continue;
        if (i)
            cv_rect(x, 16, 1, 46, K_LINE);
        if (hot)
            cv_round(x + 3, 11, 54, 57, 6, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        caps(t, KNOB_LABEL[ui.view][i], sizeof t);
        text_c(cx, 12, &FONT_XS, t, hot ? K_TEXT : K_DIM);
        if (k != K_CPU && hi - lo < 6) {         /* a switch: its positions as pips */
            int n = hi - lo + 1, j, pw = n > 4 ? 6 : 9, gap = 3, w0 = n * (pw + gap) - gap;
            for (j = 0; j < n; j++)
                cv_round(cx - w0 / 2 + j * (pw + gap), 33, pw, 8, 2, j == v - lo ? fill : K_LINE);
        } else if (k != K_CPU) {
            arc(cx, 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
        }
        knob_text(k, t);
        text_c(cx, 50, &FONT_B, t, hot ? K_ACC : K_TEXT);
    }
    cv_blit(0, KNB_Y);
}

static void leds(void)
{
    uint32_t b = 0, k = 0, dk = 0, i, ap = pool_abs();
    if (ui.solo_mode) {
        b |= 1u << B_SEL;
        if (solo_arp_hold_get()) {
            b |= 1u << B_ARP;
            k = solo_arp_latched_keys();
        } else {
            k = ui.solo_held_keys;
        }
        if (proj.leds) {
            for (i = 0; i < NKEYS; i++) {
                if (ap >> ((5u + i) % 12u) & 1u) {
                    if (!(k & (1u << i)))
                        dk |= 1u << i;
                }
            }
        }
    } else {
        int g = group_of(ui.view);
        if (g >= 0)
            b |= 1u << GROUP[g].btn;
        if (solo_arp_hold_get())
            b |= 1u << B_SEL; /* Lit while solo arp runs in the background */
        if (proj.leds)
            for (i = 0; i < NKEYS; i++)
                if (ap >> ((5u + i) % 12u) & 1u)
                    k |= 1u << i;
    }
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_frozen)
        b |= 1u << B_REC;
    if (ui.cosmic_mode)
        b |= 1u << B_HOME;
    plat_glow(proj.leds == 1);
    plat_lights(b, k, proj.leds == 1 ? ~b : 0u, dk, (proj.leds == 1 || dk) ? 1 : 0);
}

/* AUTOSAVE: a few seconds after a change, while resting and quiet, nothing touched (a flash erase
 * silences the audio for a moment: never while the garden grows; SAVE saves then) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || en_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || en_pad_level > 0.01f)
        return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}

/* ------------------------------------------------------ screen reader --- */
/* The screen as text for a screen reader on the computer (announce.c; fm1-reader): a page once it settles,
 * with its knobs; a knob's value as it turns; a help card as it comes up; the chord as it moves (queued: the
 * garden moves it by itself, every 20 s at the quickest); growing and resting from MIDI, and a MIDI clock
 * arriving or going; what a button did (say). WHERE (the reader asks, or it has just started listening) is
 * the whole screen in two: the page, its knobs and the header, then the garden (the chord, the scale).
 * Nothing is built while no reader listens. */
static struct {
    uint8_t view, help, ext, started;
    uint8_t solo_mode, solo_page;
    int16_t val[4];
    uint32_t chord;                            /* the chord's name, hashed */
} sp;
static char sp_buf[ANN_MAX + 1];
static uint32_t sp_n;
/* the chord names (en_chord_name's, after the root) in words: "m" and "M7" differ only in capitals, which
 * the words file ignores */
static const char *const CHORD_Q[][2] = {
    {"", "major"}, {"m", "minor"}, {"7", "7"}, {"M7", "major 7"}, {"m7", "minor 7"}, {"6", "6"},
    {"m6", "minor 6"}, {"add9", "add 9"}, {"madd9", "minor add 9"}, {"aug", "augmented"}, {"dim", "diminished"},
    {"dim7", "diminished 7"}, {"m7b5", "half diminished"}, {"mM7", "minor major 7"}, {"sus2", "suspended 2"},
    {"sus4", "suspended 4"}, {"7sus4", "7 suspended 4"}, {"5", "5"},
};
#define NCHORD_Q (sizeof CHORD_Q / sizeof CHORD_Q[0])

static void sp_cat(const char *s)
{
    while (*s && sp_n < ANN_MAX)
        sp_buf[sp_n++] = *s++;
    sp_buf[sp_n] = 0;
}
static void sp_int(uint32_t v)
{
    char t[12];
    itoa_u(v, t);
    sp_cat(t);
}
static int sp_same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* a value as it is said: Drift in minutes and seconds (the screen's "1:05", "45s"), the rest as shown */
static void sp_value(int k)
{
    char t[16];
    knob_text(k, t);
    if (k == P_DRIFT) {
        uint32_t m = 0, s = 0, i;
        for (i = 0; t[i] >= '0' && t[i] <= '9'; i++)
            s = s * 10u + (uint32_t)(t[i] - '0');
        if (t[i] == ':') {
            m = s;
            s = 0;
            for (i++; t[i] >= '0' && t[i] <= '9'; i++)
                s = s * 10u + (uint32_t)(t[i] - '0');
        }
        if (m) {
            sp_int(m);
            sp_cat(m == 1u ? " minute" : " minutes");
            if (!s)
                return;
            sp_cat(" ");
        }
        sp_int(s);
        sp_cat(s == 1u ? " second" : " seconds");
        return;
    }
    sp_cat(t);
}
static void sp_knob(int view, int i)           /* "Swell 60": the name the page gives it, then its value */
{
    sp_cat(KNOB_LABEL[view][i]);
    sp_cat(" ");
    sp_value(VIEW_KNOB[view][i]);
}

static void sp_solo_value(int param, int val)
{
    char t[32];
    switch (param) {
    case SP_PAN:
        if (val == 0)
            sp_cat("Center");
        else if (val < 0) {
            sp_cat("Left ");
            sp_int((uint32_t)-val);
        } else {
            sp_cat("Right ");
            sp_int((uint32_t)val);
        }
        break;
    case SP_ARP_MODE:
        if (val == 3) {
            sp_cat("Up Down");
        } else {
            solo_param_text(param, val, t);
            sp_cat(t);
        }
        break;
    case SP_ARP_OCT:
        sp_int((uint32_t)val);
        sp_cat(val == 1 ? " octave" : " octaves");
        break;
    default:
        solo_param_text(param, val, t);
        sp_cat(t);
        break;
    }
}

static void sp_solo_knob(int page, int i)
{
    int p = SOLO_PAGE_KNOBS[page][i];
    int v = solo_get(p);
    sp_cat(solo_param_name(p));
    sp_cat(" ");
    sp_solo_value(p, v);
}

static void sp_chord_q(int root, const char *q)   /* "E minor 7" */
{
    uint32_t i;
    sp_cat(EN_NOTE_NAME[root % 12]);
    sp_cat(" ");
    for (i = 0; i < NCHORD_Q; i++)
        if (sp_same(q, CHORD_Q[i][0])) {
            sp_cat(CHORD_Q[i][1]);
            return;
        }
    sp_cat(q);
}
static void sp_chord(void)                     /* the chord in the middle of the garden */
{
    char nm[16];
    const char *q = chord_now(nm), *r = EN_NOTE_NAME[en_chord_root % 12u];
    while (*r && *q == *r)
        q++, r++;
    sp_chord_q(en_chord_root % 12u, q);
}

/* the page: its name and its knobs; all: the header too (the piece, its key, the tempo, growing or resting) */
static void sp_page(int all)
{
    int i;
    sp_n = 0;
    sp_buf[0] = 0;
    if (ui.solo_mode) {
        sp_cat("Solo ");
        sp_cat(solo_page_name(ui.solo_page));
        sp_cat(" page");
        for (i = 0; i < 4; i++) {
            sp_cat(i ? ", " : ": ");
            sp_solo_knob(ui.solo_page, i);
        }
        if (!all)
            return;
        sp_cat(". Page ");
        sp_int((uint32_t)(ui.solo_page + 1));
        sp_cat(" of 6");
        if (solo_arp_hold_get())
            sp_cat(", arp hold");
        if (ui.dirty)
            sp_cat(", not saved yet");
        return;
    }
    sp_cat(VIEW_NAME[ui.view]);
    sp_cat(" page");
    for (i = 0; i < 4; i++) {
        sp_cat(i ? ", " : ": ");
        sp_knob(ui.view, i);
    }
    if (!all)
        return;
    sp_cat(". Piece ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    if (en_ext) {
        sp_cat(", tempo EXT");
    } else if (ui.view != V_GARDEN) {          /* (the Garden's first knob says it) */
        sp_cat(", tempo ");
        sp_int((uint32_t)proj.par[P_TEMPO]);
    }
    sp_cat(sp_tp.playing ? ", growing" : ", resting");
    if (ui.dirty)
        sp_cat(", not saved yet");
    if (ui.view == V_SETUP) {
        sp_cat(". CPU ");
        sp_int(plat_cpu_pct());
        sp_cat("%");
    }
    if (batt_words(batt())) {
        sp_cat(". ");
        sp_cat(batt_words(batt()));
    }
}

/* the garden: the chord and how far to the next, the scale from the root up ("F and F sharp": one letter's
 * two notes joined, so they are heard as two), the pulse sitting out */
static void sp_garden(void)
{
    uint32_t ap = pool_abs();
    int i, last = -1;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("Chord ");
    sp_chord();
    if (en_frozen) {
        sp_cat(", frozen");
    } else {
        sp_cat(", next chord ");
        sp_int((uint32_t)(fm_clampf(en_drift, 0.0f, 1.0f) * 100.0f + 0.5f));
        sp_cat("% of the way");
    }
    sp_cat(". Scale");
    for (i = 0; i < 12; i++) {
        int pc = (proj.par[P_ROOT] + i) % 12;
        if (ap >> pc & 1u) {
            sp_cat(last < 0 ? " " : EN_NOTE_NAME[last][0] == EN_NOTE_NAME[pc][0] ? " and " : ", ");
            sp_cat(EN_NOTE_NAME[pc]);
            last = pc;
        }
    }
    if (en_pulse_rest && proj.par[P_PULSE])
        sp_cat(". Pulse resting");
}

/* a help card: its button, then its lines ("ENV button. THE PAD. LEVEL, SWELL, BRIGHT, COLOUR") */
static void sp_line(const char *s)
{
    char p = sp_n ? sp_buf[sp_n - 1] : 0;
    if (*s == ' ')                             /* it goes on from the line before */
        s++;
    else if (p != ',' && p != ':')
        sp_cat(".");
    sp_cat(" ");
    sp_cat(s);
}
static void sp_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(BTN_LABEL[ui.help - 1]);
    sp_cat(" button");
    for (k = 0; k < n; k++)
        sp_line(l[k]);
}

static void sp_piece_text(void)                /* "PIECE Departures, C# Ionian, tempo 60" */
{
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("PIECE ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    sp_cat(", tempo ");
    sp_int((uint32_t)proj.par[P_TEMPO]);
}
static void sp_piece(void)
{
    if (!ann_on())
        return;
    sp_piece_text();
    ann_say(sp_buf, 0, ANN_NOW);
    sp_said = 1;
}

static uint32_t sp_chord_sig(void)
{
    char nm[16];
    const char *s = chord_now(nm);
    uint32_t h = 2166136261u;
    while (*s)
        h = hash(h, (uint8_t)*s++);
    return h;
}
static void sp_snap(void)
{
    int i;
    if (ui.solo_mode) {
        for (i = 0; i < 4; i++)
            sp.val[i] = (int16_t)solo_get(SOLO_PAGE_KNOBS[ui.solo_page][i]);
    } else {
        for (i = 0; i < 4; i++)
            sp.val[i] = (int16_t)knob_get(VIEW_KNOB[ui.view][i]);
    }
    sp.view = ui.view;
    sp.help = ui.help;
    sp.ext = en_ext;
    sp.solo_mode = ui.solo_mode;
    sp.solo_page = ui.solo_page;
    sp.chord = sp_chord_sig();
}

static void speak(void)
{
    uint32_t now = plat_ms(), c;
    int where = ann_where(), i, changed = -1;
    if (!ann_on()) {
        sp.started = 0;
        sp_said = 0;
        return;
    }
    if (!sp.started || where) {                       /* everything on the screen, in two */
        sp.started = 1;
        sp_snap();
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            sp_page(1);
        } else {
            sp_page(1);
            ann_say(sp_buf, 0, ANN_NOW);
            if (!ui.solo_mode)
                sp_garden();
        }
        if (!ui.solo_mode)
            ann_say(sp_buf, 0, 0);
        sp_said = 0;
        return;
    }
    if (sp_said) {                                    /* a button said what it did (and what it changed) */
        sp_said = 0;
        int mode_switched = (ui.solo_mode != sp.solo_mode);
        int solo_page_switched = (ui.solo_mode && ui.solo_page != sp.solo_page);
        sp_snap();
        if (mode_switched || solo_page_switched) {
            sp_page(0);
            ann_later(sp_buf, 0, 500u, now);
        }
        return;
    }
    if (en_playing != sp_tp.playing && now - sp_tp.play_t > 150u) {   /* MIDI start / stop */
        sp_tp.playing = en_playing;
        ann_say(en_playing ? "GROWING" : "RESTING", 0, 0);
    }
    if (en_ext != sp.ext) {                           /* a MIDI clock arrived, or stopped */
        sp.ext = en_ext;
        sp_n = 0;
        sp_cat("Tempo ");
        if (en_ext)
            sp_cat("EXT");
        else
            sp_int((uint32_t)proj.par[P_TEMPO]);
        ann_say(sp_buf, 0, 0);
    }
    if (ui.help != sp.help) {                         /* a help card: at once */
        sp.help = ui.help;
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            return;
        }
    }
    int page_changed = 0;
    if (ui.solo_mode != sp.solo_mode) {
        page_changed = 1;
    } else if (ui.solo_mode) {
        if (ui.solo_page != sp.solo_page)
            page_changed = 1;
    } else {
        if (ui.view != sp.view)
            page_changed = 1;
    }
    if (page_changed) {                               /* a new page: said when it settles */
        sp_snap();
        sp_page(0);
        ann_later(sp_buf, ANN_NOW, 200u, now);
        return;
    }
    if (ui.solo_mode) {
        for (i = 0; i < 4; i++) {
            int p = SOLO_PAGE_KNOBS[ui.solo_page][i];
            int v = solo_get(p);
            if (v != sp.val[i]) {
                sp.val[i] = (int16_t)v;
                if (changed < 0)
                    changed = i;
            }
        }
        if (changed >= 0) {
            sp_n = 0;
            sp_solo_knob(ui.solo_page, changed);
            ann_later(sp_buf, ANN_NOW, 120u, now);
        }
    } else {
        for (i = 0; i < 4; i++) {                         /* a knob turning */
            int v = knob_get(VIEW_KNOB[ui.view][i]);
            if (v != sp.val[i]) {
                sp.val[i] = (int16_t)v;
                if (changed < 0)
                    changed = i;
            }
        }
        if (changed >= 0) {
            sp_n = 0;
            sp_knob(ui.view, changed);
            ann_later(sp_buf, ANN_NOW, 120u, now);
        }
    }
    c = sp_chord_sig();
    if (c != sp.chord) {                              /* the chord moved: queued, after what is being said */
        sp.chord = c;
        if (!ui.solo_mode) {
            sp_n = 0;
            sp_cat("Chord ");
            sp_chord();
            ann_say(sp_buf, 0, 0);
        }
    }
}

#ifdef PLAT_HOST
/* every page, knob value, help card, chord and message as a screen reader gets them (the "speakdump"
 * command): KIND <tab> the text sent (fm1-reader/tools/audit.mjs puts it in words with web/words/ and checks it) */
static void sp_print(const char *kind, const char *t) { printf("%s\t%s\n", kind, t); }
static void speak_dump(void)
{
    static const char *const MSG[][2] = {{"UPDATE MODE IN", "3.."}, {"UPDATE MODE", "CANCELLED"},
                                         {"NEXT CHORD", "ON THE BAR"}, {"NEXT CHORD", 0}, {"RESTING", 0},
                                         {"GROWING", 0}, {"FREEZE", "ON"}, {"FREEZE", "OFF"}, {"SAVED", 0}};
    static const char *const WARN[][2] = {{"THE ROOT STAYS", "(SETUP)"}, {"3 NOTES", "AT LEAST"},
                                          {"NOT SAVED", "FLASH ERROR"}};
    project_t keep = proj;
    uint8_t keep_view = ui.view, keep_help = ui.help, keep_dirty = ui.dirty, keep_play = sp_tp.playing;
    uint8_t keep_ext = en_ext, keep_frozen = en_frozen, keep_rest = en_pulse_rest;
    int v, i, x, lo, hi;
    char t[48];
    for (v = 0; v < NVIEWS; v++) {
        ui.view = (uint8_t)v;
        sp_page(0);
        sp_print("SCREEN", sp_buf);
        for (i = 0; i < 4; i++) {
            int k = VIEW_KNOB[v][i];
            knob_range(k, &lo, &hi);
            for (x = lo; x <= hi; x++) {
                if (k < P_NPARAMS)
                    proj.par[k] = (int16_t)x;
                else
                    proj.leds = (uint8_t)x;
                sp_n = 0;
                sp_knob(v, i);
                sp_print("KNOB", sp_buf);
            }
            proj = keep;
        }
    }
    for (x = 0; x < 8; x++) {                         /* WHERE: growing, resting, unsaved, a clock, frozen, custom */
        ui.view = (uint8_t)(x * 3 % NVIEWS);
        sp_tp.playing = (uint8_t)(x & 1);
        ui.dirty = (uint8_t)(x >> 1 & 1);
        en_ext = (uint8_t)(x == 3);
        en_frozen = (uint8_t)(x == 5);
        en_pulse_rest = (uint8_t)(x == 2);
        proj.pool = x == 6 ? 0xFFFu : x == 7 ? 0x91u : keep.pool;
        proj.par[P_ROOT] = (int16_t)(x * 5 % 12);
        sp_page(1);
        sp_print("WHERE", sp_buf);
        sp_garden();
        sp_print("WHERE", sp_buf);
    }
    proj = keep;
    ui.dirty = keep_dirty;
    sp_tp.playing = keep_play;
    en_ext = keep_ext;
    en_frozen = keep_frozen;
    en_pulse_rest = keep_rest;
    ui.view = keep_view;
    for (i = 0; i < NB; i++) {
        const char *l[HELP_LINES];
        if (!help_lines(i, l))
            continue;
        ui.help = (uint8_t)(i + 1);
        sp_help();
        sp_print("HELP", sp_buf);
    }
    ui.help = keep_help;
    for (x = 0; x < 12; x++)
        for (i = 0; i < (int)NCHORD_Q; i++) {
            sp_n = 0;
            sp_cat("Chord ");
            sp_chord_q(x, CHORD_Q[i][0]);
            sp_print("CHORD", sp_buf);
        }
    for (i = 0; i < (int)(sizeof MSG / sizeof MSG[0]); i++) {
        msg_text(MSG[i][0], MSG[i][1], t);
        sp_print("MSG", t);
    }
    for (i = 0; i < (int)(sizeof WARN / sizeof WARN[0]); i++) {
        msg_text(WARN[i][0], WARN[i][1], t);
        sp_print("WARN", t);
    }
    for (x = 0; x < 12; x++) {
        msg_text("ADDED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
        msg_text("REMOVED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = 0; x < EN_NMODES; x++) {
        msg_text("MODE", EN_MODE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = -2; x <= 2; x++) {
        char o[8];
        en_param_text(P_BOCT, x, o);
        msg_text("BELL OCTAVE", o, t);
        sp_print("MSG", t);
    }
    for (x = 30; x <= 120; x += 45) {
        char o[8];
        itoa_u((uint32_t)x, o);
        msg_text("TEMPO", o, t);
        sp_print("MSG", t);
    }
    for (x = 0; x < NPIECES; x++) {
        project_piece(x);
        sp_piece_text();
        sp_print("MSG", sp_buf);
        msg_text("RESET", piece_name(x), t);
        sp_print("MSG", t);
    }
    proj = keep;
    sp_print("MSG", "Tempo EXT");
    sp_print("MSG", "Tempo 72");
    for (x = 0; x <= 4; x++)                          /* the battery, at the end of WHERE's first part */
        sp_print("WHERE", batt_words(x));
}
#endif

/* ------------------------------------------------ cosmic visualizer --- */
typedef struct {
    uint8_t x, y;
    uint8_t speed, phase, size;
} cosmic_star_t;

#define COSMIC_NSTARS 64
static cosmic_star_t cosmic_stars[COSMIC_NSTARS];
static uint8_t star_glow[COSMIC_NSTARS];
static uint8_t star_hue[COSMIC_NSTARS];

static void cosmic_seed_stars(void)
{
    uint32_t seed = plat_ms();
    if (seed == 0) seed = 0x12345678u;
    int i;
    for (i = 0; i < COSMIC_NSTARS; i++) {
        seed = seed * 1664525u + 1013904223u;
        cosmic_stars[i].x = (uint8_t)((seed >> 16) % 240u);
        seed = seed * 1664525u + 1013904223u;
        cosmic_stars[i].y = (uint8_t)((seed >> 16) % 240u);
        seed = seed * 1664525u + 1013904223u;
        cosmic_stars[i].speed = (uint8_t)(1u + ((seed >> 16) % 4u));
        cosmic_stars[i].phase = (uint8_t)((seed >> 20) % 32u);
        seed = seed * 1664525u + 1013904223u;
        cosmic_stars[i].size = (uint8_t)(((seed >> 16) % 100u < 18u) ? 1u : 0u);
        star_glow[i] = 0;
        star_hue[i] = 0;
    }
}

static void cosmic_activate_star(int idx, uint8_t glow, uint8_t hue)
{
    if (idx < 0 || idx >= COSMIC_NSTARS) return;
    if (star_glow[idx] < glow) {
        star_glow[idx] = glow;
        star_hue[idx] = hue;
    }
}

static void cosmic_trigger_stars_note(uint8_t note)
{
    uint8_t base = (uint8_t)((note * 7u + 3u) % COSMIC_NSTARS);
    uint8_t hue = (note / 4u) % 3u;
    cosmic_activate_star(base, 28, hue);
    cosmic_activate_star((base + 11) % COSMIC_NSTARS, 24, hue);
    cosmic_activate_star((base + 23) % COSMIC_NSTARS, 20, (hue + 1) % 3);
    cosmic_activate_star((base + 37) % COSMIC_NSTARS, 16, hue);
}

static void cosmic_update(void)
{
    uint8_t s_note = solo_get_and_clear_triggered_note();
    if (s_note > 0) {
        cosmic_trigger_stars_note(s_note);
    }
    int l;
    for (l = 0; l < EN_NLOOP; l++) {
        if (en_loop_glow[l] > 0.85f && (ui.frame % 4u == 0)) {
            uint8_t base = (uint8_t)((l * 12u + ui.frame / 8u) % COSMIC_NSTARS);
            cosmic_activate_star(base, 22, 1);
            cosmic_activate_star((base + 17) % COSMIC_NSTARS, 18, 0);
            break;
        }
    }
    if (ui.frame % 32u == 0) {
        uint8_t rnd_star = (uint8_t)((ui.frame * 13u + 7u) % COSMIC_NSTARS);
        cosmic_activate_star(rnd_star, 18, 0);
    }
    for (l = 0; l < COSMIC_NSTARS; l++) {
        if (star_glow[l] > 0)
            star_glow[l]--;
    }
}

static void draw_cosmic_band(int band_idx, int32_t y_start, int32_t h)
{
    /* Deep space cosmic void background */
    cv_begin(240, (uint32_t)h, RGB(6, 8, 16));

    /* 1. Starfield with Dynamic Star Activation */
    float audio_boost = en_pad_level * 0.35f + en_bowls_level * 0.25f;
    int i;
    for (i = 0; i < COSMIC_NSTARS; i++) {
        int32_t gy = (int32_t)cosmic_stars[i].y;
        if (gy >= y_start && gy < y_start + h) {
            int32_t lx = (int32_t)cosmic_stars[i].x;
            int32_t ly = gy - y_start;
            float t = (float)(ui.frame + cosmic_stars[i].phase * 4u) * (0.04f + cosmic_stars[i].speed * 0.015f);
            float s = fm_sinf(t);
            float b = 0.30f + 0.30f * s + audio_boost;

            uint8_t glow = star_glow[i];
            if (glow > 0) {
                float flare = (float)glow / 28.0f;
                b += flare * 0.85f;
            }
            if (b > 1.0f) b = 1.0f;
            if (b < 0.08f) b = 0.08f;
            uint32_t alpha = (uint32_t)(b * 16.0f);

            uint16_t col;
            if (glow > 0) {
                if (star_hue[i] == 1)
                    col = RGB(255, 225, 120); /* Celestial Gold */
                else if (star_hue[i] == 2)
                    col = RGB(120, 235, 255); /* Bright Cyan */
                else
                    col = RGB(255, 255, 255); /* Diamond White */
            } else {
                col = (cosmic_stars[i].size == 1) ? RGB(225, 240, 255) : RGB(140, 175, 215);
            }

            /* Core star pixel */
            px_blend(lx, ly, (glow > 12) ? RGB(255, 255, 255) : col, alpha);

            /* Cross diffraction spikes on active or large stars */
            if (glow > 10 || (cosmic_stars[i].size == 1 && alpha >= 12u)) {
                uint32_t as = alpha / 3u;
                if (as > 0) {
                    if (lx > 0) px_blend(lx - 1, ly, col, as);
                    if (lx < 239) px_blend(lx + 1, ly, col, as);
                    if (ly > 0) px_blend(lx, ly - 1, col, as);
                    if (ly < h - 1) px_blend(lx, ly + 1, col, as);
                }
            }

            /* Extended brilliant flare rays for intensely excited stars */
            if (glow > 18) {
                uint32_t as2 = alpha / 6u;
                if (as2 > 0) {
                    if (lx > 1) px_blend(lx - 2, ly, col, as2);
                    if (lx < 238) px_blend(lx + 2, ly, col, as2);
                    if (ly > 1) px_blend(lx, ly - 2, col, as2);
                    if (ly < h - 2) px_blend(lx, ly + 2, col, as2);
                }
            }
        }
    }

    /* 2. Minimalist chord display at bottom */
    if (band_idx == 2) {
        char cnm[16];
        const char *c = chord_now(cnm);
        if (c && c[0]) {
            text_c(120, 32, &FONT_B, c, RGB(175, 205, 240));
        }
    }

    cv_blit(0, (uint32_t)y_start);
}

static void draw_cosmic(void)
{
    cosmic_update();
    draw_cosmic_band(0, 0, HDR_H);
    draw_cosmic_band(1, MAIN_Y, MAIN_H);
    draw_cosmic_band(2, KNB_Y, KNB_H);
}

/* ------------------------------------------------ aura visualizer --- */
#define AURA_GW 31
#define AURA_GH 31
static uint8_t aura_r[AURA_GH][AURA_GW];
static uint8_t aura_g[AURA_GH][AURA_GW];
static uint8_t aura_b[AURA_GH][AURA_GW];
static float aura_spark = 0.0f;

static void aura_trigger_note(uint8_t note)
{
    (void)note;
    aura_spark = 1.0f;
}

static void aura_update(void)
{
    uint8_t s_note = solo_get_and_clear_triggered_note();
    if (s_note > 0) {
        aura_spark = 1.0f;
    }
    int l;
    for (l = 0; l < EN_NLOOP; l++) {
        if (en_loop_glow[l] > 0.82f) {
            aura_spark = 0.85f;
            break;
        }
    }
    aura_spark *= 0.94f;

    /* Slower, majestic, serene drift (~3x slower than before) */
    float t = (float)ui.frame * 0.012f;
    float bass = en_bass_level * 0.5f + en_bowls_level * 0.5f;
    if (en_drum_glow[0] > 0.1f)
        bass += en_drum_glow[0] * 0.35f;
    if (bass > 1.0f) bass = 1.0f;

    float mids = en_pad_level;
    if (mids > 1.0f) mids = 1.0f;

    float highs = aura_spark;

    /* 4 floating aura centroids with smooth, graceful Lissajous drift */
    float c1_x = fm_sinf(t * 0.35f) * 0.38f;
    float c1_y = fm_cosf(t * 0.28f) * 0.26f;

    float c2_x = fm_cosf(t * 0.32f + 1.8f) * 0.40f;
    float c2_y = fm_sinf(t * 0.38f + 2.2f) * 0.28f;

    float c3_x = fm_sinf(t * 0.25f - 2.0f) * 0.45f;
    float c3_y = fm_cosf(t * 0.34f - 1.2f) * 0.32f;

    float c4_x = fm_cosf(t * 0.20f) * 0.22f;
    float c4_y = fm_sinf(t * 0.32f) * 0.18f - 0.04f;

    /* Lower decay constants = wider, more diffused aura filling the screen softly */
    float k1 = 2.6f - bass * 0.8f;
    float k2 = 3.0f;
    float k3 = 2.5f;
    float k4 = 3.4f;

    float amp1 = 0.85f + bass * 0.50f;
    float amp2 = 0.75f + mids * 0.50f;
    float amp4 = 0.55f + highs * 0.60f;

    int gy, gx;
    for (gy = 0; gy < AURA_GH; gy++) {
        float sy = (float)(gy * 8 - 120) * (1.0f / 240.0f);
        for (gx = 0; gx < AURA_GW; gx++) {
            float sx = (float)(gx * 8 - 120) * (1.0f / 240.0f);
            float warp = fm_sinf(sx * 1.2f + t * 0.4f) * fm_cosf(sy * 1.2f - t * 0.3f) * (0.04f + bass * 0.04f);
            float px = sx + warp;
            float py = sy - warp;

            float d1 = (px - c1_x) * (px - c1_x) + (py - c1_y) * (py - c1_y);
            float glow1 = fm_expf(-d1 * k1) * amp1;

            float d2 = (px - c2_x) * (px - c2_x) + (py - c2_y) * (py - c2_y);
            float glow2 = fm_expf(-d2 * k2) * amp2;

            float d3 = (px - c3_x) * (px - c3_x) + (py - c3_y) * (py - c3_y);
            float glow3 = fm_expf(-d3 * k3) * 0.65f;

            float d4 = (px - c4_x) * (px - c4_x) + (py - c4_y) * (py - c4_y);
            float glow4 = fm_expf(-d4 * k4) * amp4;

            /* Deep luminous aura blending across celestial midnight blue (no stark pitch-black voids) */
            float r = 0.009f + 0.12f * glow1 * 0.60f + 0.52f * glow2 * 0.50f + 0.08f * glow3 * 0.45f + 0.78f * glow4 * 0.35f;
            float g = 0.014f + 0.62f * glow1 * 0.60f + 0.18f * glow2 * 0.50f + 0.24f * glow3 * 0.45f + 0.44f * glow4 * 0.35f;
            float b = 0.028f + 0.92f * glow1 * 0.60f + 0.88f * glow2 * 0.50f + 0.70f * glow3 * 0.45f + 0.12f * glow4 * 0.35f;

            /* Gentle contrast shaping maintaining shadow luminescence */
            r = fm_clampf(r, 0.0f, 1.0f);
            g = fm_clampf(g, 0.0f, 1.0f);
            b = fm_clampf(b, 0.0f, 1.0f);
            r = r * (1.25f - 0.25f * r);
            g = g * (1.25f - 0.25f * g);
            b = b * (1.25f - 0.25f * b);

            aura_r[gy][gx] = (uint8_t)(r * 255.0f);
            aura_g[gy][gx] = (uint8_t)(g * 255.0f);
            aura_b[gy][gx] = (uint8_t)(b * 255.0f);
        }
    }
}

static void draw_aura_band(int band_idx, int32_t y_start, int32_t h)
{
    cv_begin(240, (uint32_t)h, C_BLACK);

    int r_line[AURA_GW];
    int g_line[AURA_GW];
    int b_line[AURA_GW];

    int ly;
    for (ly = 0; ly < h; ly++) {
        int gy_scr = (int)y_start + ly;
        int row = gy_scr >> 3;
        int fy = gy_scr & 7;
        int row1 = (row < AURA_GH - 1) ? row + 1 : row;
        int wy0 = 8 - fy;
        int wy1 = fy;

        int gx;
        for (gx = 0; gx < AURA_GW; gx++) {
            r_line[gx] = ((int)aura_r[row][gx] * wy0 + (int)aura_r[row1][gx] * wy1) >> 3;
            g_line[gx] = ((int)aura_g[row][gx] * wy0 + (int)aura_g[row1][gx] * wy1) >> 3;
            b_line[gx] = ((int)aura_b[row][gx] * wy0 + (int)aura_b[row1][gx] * wy1) >> 3;
        }

        uint16_t *dst = &cv_px[(uint32_t)ly * 240u];
        for (gx = 0; gx < AURA_GW - 1; gx++) {
            int r = r_line[gx] << 8;
            int g = g_line[gx] << 8;
            int b = b_line[gx] << 8;
            int dr = ((r_line[gx + 1] - r_line[gx]) << 8) >> 3;
            int dg = ((g_line[gx + 1] - g_line[gx]) << 8) >> 3;
            int db = ((b_line[gx + 1] - b_line[gx]) << 8) >> 3;

            int k;
            for (k = 0; k < 8; k++) {
                uint8_t ir = (uint8_t)(r >> 8);
                uint8_t ig = (uint8_t)(g >> 8);
                uint8_t ib = (uint8_t)(b >> 8);
                *dst++ = swap16(RGB(ir, ig, ib));
                r += dr;
                g += dg;
                b += db;
            }
        }
    }

    /* Minimalist chord display at bottom (in band 2) */
    if (band_idx == 2) {
        char cnm[16];
        const char *c = chord_now(cnm);
        if (c && c[0]) {
            text_c(120, 32, &FONT_B, c, RGB(220, 240, 255));
        }
    }

    cv_blit(0, (uint32_t)y_start);
}

static void draw_aura(void)
{
    aura_update();
    draw_aura_band(0, 0, HDR_H);
    draw_aura_band(1, MAIN_Y, MAIN_H);
    draw_aura_band(2, KNB_Y, KNB_H);
}

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.touched = -1;
    cur_piece_theme = -1;
    update_theme();
    lcd_fill(0, 0, 240, 240, K_BG);
    cosmic_seed_stars();
    if (!en_playing)
        en_play(1);                              /* it grows from the start */
    sp_tp.playing = 1;
    sp_tp.play_t = plat_ms();
}

void ui_frame(void)
{
    input();
    update_theme();
    autosave();
    if (ui.cosmic_mode) {
        if (ui.cosmic_mode == 1)
            draw_aura();
        else
            draw_cosmic();
    } else {
        draw_header();
        draw_main();
        draw_knobs();
    }
    leds();
    speak();
    ann_tick(plat_ms());
    ui.frame++;
}

void ui_input_only(void) { input(); }
