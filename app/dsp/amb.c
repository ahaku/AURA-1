/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The instrument (amb.h). One compilation unit, -O2. */
#include "amb.h"
#include "fastmath.h"

#if defined EN_HOST || defined PLAT_HOST
#define EN_POOL
typedef float delay_sample_t;
static inline float delay_read(delay_sample_t x) { return x; }
static inline delay_sample_t delay_write(float x) { return x; }
#else
#define EN_POOL __attribute__((section(".pool")))
/* The FM-1 stores the cloud and plate delay lines as signed Q14 samples.
 * The filters still run in float, with room for peaks up to twice full scale. */
typedef int16_t delay_sample_t;
static inline float delay_read(delay_sample_t x) { return (float)x * (1.0f / 16384.0f); }
static inline delay_sample_t delay_write(float x)
{
    float scaled = x * 16384.0f;
    return (delay_sample_t)(scaled > 32767.0f ? 32767.0f : scaled < -32768.0f ? -32768.0f : scaled);
}
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

const char *const EN_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
const char *const EN_MODE_NAME[EN_NMODES] = {"Ionian", "Dorian", "Phrygian", "Lydian", "Mixolyd.", "Aeolian",
                                             "Pent maj", "Pent min", "Hirajoshi", "Insen", "WholeTone", "Lydian Dominant"};
#define B(a, b, c, d, e, f, g) (1u << (a) | 1u << (b) | 1u << (c) | 1u << (d) | 1u << (e) | 1u << (f) | 1u << (g))
const uint16_t EN_MODE_POOL[EN_NMODES] = {
    B(0, 2, 4, 5, 7, 9, 11), B(0, 2, 3, 5, 7, 9, 10), B(0, 1, 3, 5, 7, 8, 10), B(0, 2, 4, 6, 7, 9, 11),
    B(0, 2, 4, 5, 7, 9, 10), B(0, 2, 3, 5, 7, 8, 10), B(0, 2, 4, 7, 9, 9, 9),  B(0, 3, 5, 7, 10, 10, 10),
    B(0, 2, 3, 7, 8, 8, 8),  B(0, 1, 5, 7, 10, 10, 10), B(0, 2, 4, 6, 8, 10, 10), B(0, 2, 4, 6, 7, 9, 10)};
#undef B
const uint8_t EN_LOOP_LEN[EN_NLOOP] = {13, 17, 23, 29};
const uint8_t EN_DRUM_N[EN_NDRUM] = {16, 12, 7};

static const en_param_t PARAMS[P_NPARAMS] = {
    {"Pad", 0, 100, 60},      {"Bells", 0, 100, 65},    {"Bass", 0, 100, 45},     {"Pulse", 0, 100, 40},
    {"Density", 0, 100, 40},  {"Evolve", 0, 100, 35},   {"Drift", 0, 100, 25},    {"Hits", 0, 100, 35},
    {"Echo", 0, 100, 45},     {"Echo time", 0, 5, 3},   {"Reverb", 0, 100, 65},   {"Size", 0, 100, 80},
    {"Bright", 0, 100, 40},   {"Swell", 0, 100, 60},    {"Bell tone", 0, 100, 50}, {"Motion", 0, 100, 50},
    {"Tune", -50, 50, 0},     {"MIDI out", 0, 1, 1},    {"Tempo", 30, 120, 72},   {"Root", 0, 11, 2},
    {"Mode", 0, EN_NMODES - 1, 1}, {"Bell oct", -2, 2, 0}, {"Pad tone", 0, 1, 0},
    {"Bass oct", -1, 1, 0},   {"Glide", 0, 100, 20},    {"Fifth", 0, 100, 35},
    {"Swing", 0, 100, 0},     {"Hats", 0, 100, 50},
    {"Text vol", 0, 100, 0},  {"Text type", 0, 4, 0},   {"Text tone", 0, 100, 50}, {"Text dens", 0, 100, 50},
    {"Bowls vol", 0, 100, 0}, {"Bowls mode", 0, 3, 0}, {"Bowls oct", -1, 1, 0}, {"Bowls blur", 0, 100, 35},
};
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }

/* the pieces: eight recipes for the whole garden (the FM-1's PRESETS, the web app's cards). Tune and
 * MIDI out are taken from here by neither: they belong to the player, not the piece */
/*                     Pad Bel Bas Pul  Den Evo Dri Hit  Ech ET Rev Siz  Bri Swe Ton Mot  Tun MI Tem Rt Md Oc  PT  BO Gli Fif  Swi Hat
 * (PT: the pad's colour, 0 Rhodes, 1 Voices; BO: the bass's octave) */
static const struct {
    const char *name;
    int8_t v[P_NPARAMS];
} PIECES[EN_NPIECES] = {
    {"Garden",       {60, 65, 45, 40, 40, 35, 25, 35, 45, 3, 65, 80, 40, 60, 50, 50, 0, 1, 72, 2, 1, 0, 0, 0, 20, 35, 0, 50, 0, 0, 50, 50,  0, 0, 0, 35}},
    {"Departures",   {35, 75, 0, 0, 35, 30, 40, 0, 30, 3, 70, 85, 20, 55, 5, 40, 0, 1, 60, 1, 0, -1, 0, 0, 30, 25, 0, 40, 0, 0, 50, 50,  0, 0, 0, 35}},
    {"Voices",       {85, 0, 25, 0, 30, 30, 50, 0, 20, 5, 80, 90, 67, 70, 30, 55, 0, 1, 56, 5, 5, 0, 1, -1, 45, 30, 0, 40, 0, 0, 50, 50,  20, 2, 0, 45}},
    {"Afternoon",    {50, 65, 35, 25, 55, 45, 20, 30, 50, 2, 60, 75, 60, 50, 60, 55, 0, 1, 84, 7, 3, 0, 0, 0, 15, 40, 15, 65, 0, 0, 50, 50,  0, 0, 0, 35}},
    {"Night Bus",    {45, 55, 55, 60, 45, 50, 15, 55, 55, 1, 55, 70, 35, 40, 75, 70, 0, 1, 96, 9, 5, 0, 0, 0, 10, 45, 30, 70, 0, 0, 50, 50,  0, 0, 0, 35}},
    {"Shoreline",    {40, 60, 30, 0, 30, 40, 40, 0, 85, 4, 55, 95, 30, 80, 0, 85, 0, 1, 64, 4, 6, 0, 0, 0, 40, 20, 0, 30, 0, 0, 50, 50,  0, 0, 0, 35}},
    {"Glass",        {35, 80, 30, 20, 60, 55, 25, 25, 45, 2, 65, 80, 55, 45, 90, 50, 0, 1, 76, 3, 7, 1, 0, 1, 10, 30, 10, 85, 0, 0, 50, 50,  0, 0, 0, 35}},
    {"Tides",        {55, 60, 40, 35, 45, 75, 10, 40, 50, 3, 65, 85, 60, 60, 55, 95, 0, 1, 70, 10, 4, 0, 1, 0, 25, 55, 10, 50, 0, 0, 50, 50,  0, 0, 0, 35}},
    {"Dreamscape",   {65, 75, 50, 0, 45, 40, 40, 0, 70, 2, 75, 85, 50, 60, 12, 50, 0, 1, 68, 4, 1, 0, 0, 0, 30, 35, 0, 50, 0, 0, 50, 50,  0, 0, 0, 35}},
    {"Solaris",      {70, 65, 40, 0, 40, 50, 50, 0, 60, 3, 80, 90, 60, 70, 40, 65, 0, 1, 58, 5, 3, 1, 1, 0, 40, 45, 0, 50, 0, 0, 50, 50,  30, 2, -1, 60}},
    {"Cassette",     {65, 55, 55, 30, 35, 45, 35, 25, 65, 1, 60, 65, 35, 55, 20, 85, 0, 1, 62, 6, 5, 0, 0, 0, 20, 40, 40, 25, 0, 0, 50, 50,  0, 0, 0, 35}},
    {"Rain",         {60, 70, 35, 0, 30, 45, 40, 0, 60, 3, 80, 92, 35, 75, 25, 45, 0, 1, 52, 2, 1, 1, 0, 0, 35, 40, 0, 30, 0, 0, 50, 50,  0, 0, 0, 35}},
    {"Pulsar",       {50, 65, 60, 60, 55, 35, 25, 45, 50, 2, 55, 70, 55, 45, 65, 50, 0, 1, 104, 0, 5, 0, 0, 0, 15, 50, 0, 70, 0, 0, 50, 50,  0, 0, 0, 35}},
    {"Bamboo",       {45, 75, 0, 0, 25, 40, 60, 0, 65, 4, 85, 95, 40, 85, 10, 40, 0, 1, 48, 9, 7, 0, 1, 0, 30, 0, 0, 0, 0, 0, 50, 50,  0, 0, 0, 35}},
    {"Aurora",       {65, 70, 40, 0, 35, 45, 45, 0, 60, 3, 85, 95, 70, 70, 40, 70, 0, 1, 54, 4, 2, 2, 1, -1, 40, 40, 0, 0, 0, 0, 50, 50,  25, 1, -1, 60}},
    {"Dusk",         {70, 50, 65, 20, 30, 35, 35, 25, 45, 2, 70, 75, 20, 65, 20, 40, 0, 1, 46, 10, 5, -1, 0, -1, 30, 35, 40, 40, 0, 0, 50, 50,  0, 0, 0, 35}},
};

const char *en_piece_name(int i) { return PIECES[(unsigned)i < EN_NPIECES ? i : 0].name; }
void en_piece(int i, int16_t *par, uint16_t *pool)
{
    int p;
    i = (unsigned)i < EN_NPIECES ? i : 0;
    for (p = 0; p < P_NPARAMS; p++)
        par[p] = PIECES[i].v[p];
    *pool = EN_MODE_POOL[PIECES[i].v[P_MODE]];
}

static const char *const ETIME_NAME[6] = {"1/4.", "1/2", "1/2.", "1 bar", "2 bars", "4 bars"};
static const float ETIME_BEATS[6] = {1.5f, 2.0f, 3.0f, 4.0f, 8.0f, 16.0f};

static char *itoa_s(int v, char *b)
{
    char t[8];
    int n = 0, neg = v < 0;
    if (neg)
        v = -v;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 6);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}

static float drift_secs(int v) { return 20.0f * fm_exp2f((float)v * 0.039069f); }   /* 20 s .. 5 min */

void en_param_text(int i, int v, char *b)
{
    static const char *const ONOFF[2] = {"Off", "On"};
    const char *s = 0;
    if (i == P_ETIME)
        s = ETIME_NAME[(unsigned)v < 6u ? v : 0];
    else if (i == P_MIDI)
        s = ONOFF[v ? 1 : 0];
    else if (i == P_PADTONE)
        s = v ? "Voices" : "Rhodes";
    else if (i == P_TEXT_TYPE) {
        static const char *const TEXT_TYPES[5] = {"Rain", "Wind", "Vinyl", "Ocean", "Train"};
        s = TEXT_TYPES[(unsigned)v < 5u ? v : 0];
    }
    else if (i == P_MODE)
        s = EN_MODE_NAME[(unsigned)v < EN_NMODES ? v : 0];
    else if (i == P_ROOT)
        s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0];
    else if (i == P_BOWLS_MODE) {
        static const char *const DMODES[] = {"Root", "Sub", "Fifth", "Chord"};
        s = (unsigned)v < 4 ? DMODES[v] : "Root";
    }
    if (s) {
        int k = 0;
        int max_len = (i == P_MODE) ? 15 : 9;
        while (s[k] && k < max_len) {
            b[k] = s[k];
            k++;
        }
        b[k] = 0;
        return;
    }
    if (i == P_DRIFT) {
        int sec = (int)(drift_secs(v) + 0.5f);
        if (sec < 60) {
            b = itoa_s(sec, b);
            *b++ = 's';
            *b = 0;
        } else {
            b = itoa_s(sec / 60, b);
            *b++ = ':';
            *b++ = (char)('0' + sec % 60 / 10);
            *b++ = (char)('0' + sec % 10);
            *b = 0;
        }
        return;
    }
    if (i == P_SWING) {                          /* as a swing percentage: 50% (straight) .. 75% */
        b = itoa_s(50 + v / 4, b);
        *b++ = '%';
        *b = 0;
        return;
    }
    if ((i == P_TUNE || i == P_BOCT || i == P_BASSOCT || i == P_BOWLS_OCT) && v > 0)
        *b++ = '+';
    itoa_s(v, b);
}

/* ------------------------------------------------------- the pure parts --- */
uint32_t en_euclid(int k, int n, int rot)
{
    /* Bjorklund's algorithm (as Toussaint's "The Euclidean Algorithm Generates Traditional Musical
     * Rhythms"): k groups "x" and n - k groups ".", the remainders folded onto the heads until at most
     * one remains. Groups are bit strings, LSB first: (bits, length). */
    uint32_t gb[32], m = 0;
    uint8_t gl[32];
    int ng, heads, tails, i, pos = 0;
    if (n <= 0 || n > 32 || k <= 0)
        return 0;
    if (k > n)
        k = n;
    for (i = 0; i < n; i++) {
        gb[i] = i < k ? 1u : 0u;
        gl[i] = 1;
    }
    ng = n;
    heads = k;
    tails = n - k;
    while (tails > 1 && heads > 0) {
        int pairs = heads < tails ? heads : tails;
        for (i = 0; i < pairs; i++) {           /* a tail onto each head */
            gb[i] |= gb[ng - pairs + i] << gl[i];
            gl[i] = (uint8_t)(gl[i] + gl[ng - pairs + i]);
        }
        if (heads > tails) {                    /* the unpaired heads become the new tails */
            tails = heads - pairs;
            heads = pairs;
        } else {
            tails -= pairs;
            heads = pairs;
        }
        ng = heads + tails;
    }
    for (i = 0; i < ng; i++) {
        uint32_t j;
        for (j = 0; j < gl[i]; j++)
            if (gb[i] >> j & 1u)
                m |= 1u << (((pos + (int)j + rot) % n + n) % n);
        pos += gl[i];
    }
    return m;
}

int en_scale(uint32_t pool, int *steps)
{
    int i, m = 0;
    pool |= 1u;
    for (i = 0; i < 12; i++)
        if (pool >> i & 1u)
            steps[m++] = i;
    return m;
}

int en_chord_tones(uint32_t pool, int root, int deg, int *pcs)
{
    static const uint8_t OFF[7] = {0, 2, 4, 6, 1, 3, 5};   /* stacked thirds, then what's left */
    int st[12], m = en_scale(pool, st), want, n = 0, i, j;
    want = m >= 6 ? 4 : 3;
    deg = ((deg % m) + m) % m;
    for (i = 0; i < 7 && n < want; i++) {
        int pc = (root + st[(deg + OFF[i]) % m]) % 12, dup = 0;
        for (j = 0; j < n; j++)
            dup |= pcs[j] == pc;
        if (!dup)
            pcs[n++] = pc;
    }
    return n;
}

static int nearest(int from, int pc, int lo, int hi)    /* the note of class pc nearest from, in lo..hi */
{
    int n = from + ((pc - from) % 12 + 12) % 12;          /* at or above */
    if (n - from > 6)
        n -= 12;
    while (n < lo)
        n += 12;
    while (n > hi)
        n -= 12;
    return n;
}

void en_voice_lead(const int *from, const int *pcs, int nt, int *to, int lo, int hi)
{
    static const uint8_t PERM[24][4] = {
        {0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 2, 3, 1}, {0, 3, 1, 2}, {0, 3, 2, 1},
        {1, 0, 2, 3}, {1, 0, 3, 2}, {1, 2, 0, 3}, {1, 2, 3, 0}, {1, 3, 0, 2}, {1, 3, 2, 0},
        {2, 0, 1, 3}, {2, 0, 3, 1}, {2, 1, 0, 3}, {2, 1, 3, 0}, {2, 3, 0, 1}, {2, 3, 1, 0},
        {3, 0, 1, 2}, {3, 0, 2, 1}, {3, 1, 0, 2}, {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 2, 1, 0}};
    int t[4], p, v, best = 1 << 30;
    for (v = 0; v < 4; v++)
        t[v] = pcs[v < nt ? v : 0];                        /* three tones: the root doubled */
    for (p = 0; p < 24; p++) {
        int c = 0, n[4], a, b;
        for (v = 0; v < 4; v++) {
            n[v] = nearest(from[v], t[PERM[p][v]], lo, hi);
            c += n[v] > from[v] ? n[v] - from[v] : from[v] - n[v];
        }
        for (a = 0; a < 4; a++)                            /* no two voices on one note */
            for (b = a + 1; b < 4; b++)
                if (n[a] == n[b])
                    c += 40;
        if (c < best) {
            best = c;
            for (v = 0; v < 4; v++)
                to[v] = n[v];
        }
    }
}

const char *en_chord_name(const int *pcs, int n, char *buf)
{
    uint32_t iv = 0;
    int i, k = 0, third, fifth;
    const char *q = "", *r = EN_NOTE_NAME[pcs[0] % 12];
    for (i = 1; i < n; i++)
        iv |= 1u << ((pcs[i] - pcs[0] + 12) % 12);
    third = iv >> 4 & 1u ? 4 : iv >> 3 & 1u ? 3 : 0;
    fifth = iv >> 7 & 1u ? 7 : iv >> 6 & 1u ? 6 : iv >> 8 & 1u ? 8 : 0;
    if (third == 4) {
        q = fifth == 8 ? "aug" : iv >> 11 & 1u ? "M7" : iv >> 10 & 1u ? "7" : iv >> 9 & 1u ? "6"
            : iv >> 2 & 1u ? "add9" : "";
    } else if (third == 3) {
        if (fifth == 6)
            q = iv >> 10 & 1u ? "m7b5" : iv >> 9 & 1u ? "dim7" : "dim";
        else
            q = iv >> 10 & 1u ? "m7" : iv >> 11 & 1u ? "mM7" : iv >> 9 & 1u ? "m6" : iv >> 2 & 1u ? "madd9" : "m";
    } else {
        q = iv >> 5 & 1u ? (iv >> 10 & 1u ? "7sus4" : "sus4") : iv >> 2 & 1u ? "sus2" : "5";
    }
    while (*r)
        buf[k++] = *r++;
    while (*q)
        buf[k++] = *q++;
    buf[k] = 0;
    return buf;
}

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_POOL, C_PLAY, C_NEXT, C_FREEZE, C_SEED, C_PANIC, C_CLOCK, C_REGROW,
       C_SOLO_NOTE, C_SOLO_SET, C_SOLO_PANIC, C_SOLO_RESET };
#define QN 64u
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;                                   /* full: dropped (never, at the UI's rate) */
    q[w % QN] = c << 24 | ((uint32_t)a & 0xFFu) << 16 | ((uint32_t)b & 0xFFFFu);
    BARRIER();
    q_w = w + 1;
}
void en_set(int p, int v) { post(C_SET, p, v); }
void en_pool(uint32_t pool) { post(C_POOL, 0, (int)(pool & 0xFFFu)); }
void en_play(int on) { post(C_PLAY, on, 0); }
void en_next(void) { post(C_NEXT, 0, 0); }
void en_freeze(int on) { post(C_FREEZE, on, 0); }
void en_seed(uint32_t s) { post(C_SEED, 0, (int)(s & 0xFFFFu)); }
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_regrow(void) { post(C_REGROW, 0, 0); }
void en_solo_note(uint8_t note, uint8_t vel, uint8_t on) { post(C_SOLO_NOTE, note, (int)((vel << 8) | on)); }
void en_solo_set(int p, int v) { post(C_SOLO_SET, p, v); }
void en_solo_panic(void) { post(C_SOLO_PANIC, 0, 0); }
void en_solo_reset(void) { post(C_SOLO_RESET, 0, 0); }

/* -------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_frozen, en_ext;
volatile uint8_t en_chord[EN_NPAD], en_chord_root, en_chord_n;
volatile float en_drift, en_pad_level, en_bass_level, en_bowls_level;
volatile uint8_t en_loop_pos[EN_NLOOP];
volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];
volatile float en_loop_glow[EN_NLOOP];
volatile uint8_t en_drum_pos[EN_NDRUM];
volatile uint32_t en_drum_pat[EN_NDRUM];
volatile float en_drum_glow[EN_NDRUM];
volatile uint32_t en_bars;
volatile float en_arc;
volatile uint8_t en_pulse_rest;
volatile uint8_t en_pad_note[EN_NPAD];
volatile float en_pad_amp[EN_NPAD];

static int16_t par[P_NPARAMS];
static uint32_t pool = 0xAADu;                  /* Ionian until told */
static int nsteps, steps[12];                   /* the scale */
static int deg;                                 /* the chord's scale degree */
static int chord_pcs[4], chord_nt;
static float tunefac = 1.0f;
static uint32_t rng = 0x9E3779B9u;

static uint32_t rnd(void)                       /* Marsaglia's xorshift (Xorshift RNGs, 2003): shifts 13, 17, 5 */
{
    uint32_t x = rng;
    x = x ^ (x << 13);
    x = x ^ (x >> 17);
    rng = x ^ (x << 5);
    return rng;
}
static float frand(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static int irand(int n) { return n > 0 ? (int)((rnd() >> 8) % (uint32_t)n) : 0; }

static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (par[P_MIDI])
        en_midi_out(st, d1, d2);
}

static float mtof(float n) { return 440.0f * tunefac * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }

/* a sine table: 1024 points a turn, plus one */
#define SN 1024
static float sint[SN + 1];
static inline float wave(const float *table, float ph)  /* ph in turns, any value >= 0 */
{
    float x = (ph - (float)(int)ph) * (float)SN;
    int i = (int)x;
    float f = x - (float)i;
    return table[i] + (table[i + 1] - table[i]) * f;
}
static inline float sine(float ph) { return wave(sint, ph); }

/* ------------------------------------------------- slow random motion --- */
#define NMOD 9                                  /* 0..5 the pads', 6..8 the echo's */
#define NMOD_PAD 6
static struct {
    float v, tgt, rate;
    uint32_t left;                              /* samples until a new target */
} mod[NMOD];

static void mods_step(uint32_t n)
{
    int i;
    for (i = 0; i < NMOD; i++) {
        if (mod[i].left <= n) {
            float secs = 7.0f + 33.0f * frand();
            mod[i].tgt = frand() * 2.0f - 1.0f;
            mod[i].left = (uint32_t)(secs * EN_SR);
            mod[i].rate = (float)n / (secs * EN_SR * 0.5f);
        } else {
            mod[i].left -= n;
        }
        mod[i].v += (mod[i].tgt - mod[i].v) * fm_minf(mod[i].rate, 1.0f);
    }
}
static float motion;                            /* 0..1: Motion */

/* ---------------------------------------------------------------- pad --- */
/* Breathing voices, as the sung tape loops of Music for Airports' "2/1": each voice swells in, hangs
 * and fades on its own cycle (unequal lengths, set by Swell), taking its note of the chord as it
 * starts, so the chord forms and dissolves rather than sitting there. The timbre: two saws a few
 * cents apart through two formant band-passes, a vowel that Bright moves from "oo" through "ah" to
 * "eh", with a little of the first formant's low-pass for body. */
typedef struct {
    float ph[2], inc[2];
    float note, target;                         /* MIDI: the note of this breath, the next one's */
    float t, rate, peak, amp;                   /* t 0..1 through the breath, per 32 samples */
    float ic[4];                                /* the two filters' states */
    float c1[4], c2[4];                         /* their coefficients: a1, a2, a3, k */
    float vib, pan;
    float hf;                                   /* a handoff's fade: 1 normal */
    uint8_t midi_note, hand;                    /* hand: 1 fading out of the old key, 2 into the new */
    uint8_t voices;                             /* this breath's colour: 0 Rhodes, 1 Voices */
} pad_t;
static pad_t pad[EN_NPAD];
static float pad_gate, pad_gain, pad_vol, bright, swell;
static float text_vol;
static int text_type;
static float text_tone = 0.5f, text_dens = 0.5f;
static const float PAD_PAN[EN_NPAD] = {-0.55f, -0.18f, 0.18f, 0.55f};
static const float BREATH[EN_NPAD] = {1.0f, 1.21f, 1.43f, 1.67f};    /* unequal cycles, as 2/1's 12..20 s loops */
static const float START_T[EN_NPAD] = {0.0f, 0.9f, 0.82f, 0.95f};    /* the first breaths overlap */
static const float VOWEL[4][2] = {{320.0f, 800.0f}, {480.0f, 900.0f}, {730.0f, 1090.0f}, {530.0f, 1840.0f}};
static float arcm;                              /* the slow arc, -1 sparse .. +1 full, times Motion */

static float breath(float t)                    /* rise, hang, fall, a short rest */
{
    if (t < 0.4f)
        return 0.5f - 0.5f * fm_cosf(FM_PI * t * 2.5f);
    if (t < 0.55f)
        return 1.0f;
    if (t < 0.9f)
        return 0.5f + 0.5f * fm_cosf(FM_PI * (t - 0.55f) * (1.0f / 0.35f));
    return 0.0f;
}

static void svf_coefs(float *c, float f, float k)
{
    float g = fm_tanf(FM_PI * fm_clampf(f, 40.0f, 16000.0f) * (1.0f / EN_SR));
    c[0] = 1.0f / (1.0f + g * (g + k));
    c[1] = g * c[0];
    c[2] = g * c[1];
    c[3] = k;
}

static void midi(uint32_t st, uint32_t d1, uint32_t d2);
static void pad_breathe(pad_t *v, int i)        /* a new breath: its length, its strength, its note */
{
    float secs = (4.0f + 0.12f * swell) * BREATH[i] * (0.85f + 0.3f * frand());
    v->rate = 32.0f / (secs * EN_SR);
    v->peak = 0.6f + 0.4f * frand();
    if (en_playing && v->midi_note)
        midi(0x80, v->midi_note, 0);
    v->note = v->target;
    v->voices = (uint8_t)(par[P_PADTONE] != 0);
    v->midi_note = (uint8_t)v->target;
    if (en_playing)
        midi(0x90, v->midi_note, (uint32_t)(50.0f + 40.0f * v->peak));
}

/* a new key (piece, root, scale): the voices still singing the old one fade out in about a second and
 * breathe again on the new chord; a voice at rest just takes its new note. Chord drift doesn't do
 * this: there the old notes are meant to hang over */
#define HAND_STEP (32.0f / (1.0f * EN_SR))
static void pad_handoff(void)
{
    int i;
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        if (v->amp > 0.002f && ((float)v->midi_note != v->target || v->voices != (par[P_PADTONE] != 0)))
            v->hand = 1;
        else if (!v->hand)
            v->note = v->target;
    }
}

/* a chord change: a voice singing a note the new chord doesn't have fades out in about a second and
 * breathes in on its new note; a voice on a shared note holds it to the end of its breath */
static void pad_leave(void)
{
    int i, k;
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        int pc = v->midi_note % 12, shared = 0;
        for (k = 0; k < chord_nt; k++)
            shared |= chord_pcs[k] == pc;
        if (v->amp > 0.002f && !shared && !v->hand)
            v->hand = 1;
        else if (v->amp <= 0.002f && !v->hand)
            v->note = v->target;
    }
}

static void pad_coefs(pad_t *v, int i)          /* every 32 samples: the breath, pitch, the vowel */
{
    float f, vw, f1, f2;
    int a;
    if (v->hand == 1) {                         /* out of the old key */
        v->hf -= HAND_STEP;
        if (v->hf <= 0.0f) {
            v->hf = 0.0f;
            v->hand = 2;
            v->t = 0.15f;                       /* a breath already rising, on the new chord */
            pad_breathe(v, i);
        }
    } else if (v->hand == 2) {                  /* into the new */
        v->hf += HAND_STEP * 0.6f;
        if (v->hf >= 1.0f) {
            v->hf = 1.0f;
            v->hand = 0;
        }
    }
    v->t += v->rate;
    if (v->t >= 1.0f) {
        v->t -= 1.0f;
        pad_breathe(v, i);
    }
    v->vib += (4.6f + 0.4f * (float)i) * 32.0f / EN_SR;
    if (v->vib >= 1.0f)
        v->vib -= 1.0f;
    f = mtof(v->note + 0.08f * motion * mod[i].v);                          /* a slow drift, as tape */
    v->inc[0] = f * 0.99856f * (1.0f / EN_SR);  /* -2.5 and +2.5 cents */
    v->inc[1] = f * 1.00144f * (1.0f / EN_SR);
    vw = fm_clampf((float)bright * 0.01f + 0.25f * arcm + 0.2f * motion * mod[(i + 2) % NMOD_PAD].v, 0.0f, 1.0f) * 2.999f;
    a = (int)vw;
    vw -= (float)a;
    f1 = VOWEL[a][0] + (VOWEL[a + 1][0] - VOWEL[a][0]) * vw;
    f2 = VOWEL[a][1] + (VOWEL[a + 1][1] - VOWEL[a][1]) * vw;
    svf_coefs(v->c1, f1 * (1.0f + 0.03f * (float)i), 0.45f);
    svf_coefs(v->c2, f2 * (1.0f - 0.02f * (float)i), 0.3f);
}

static inline float blep_saw(float t, float dt)
{
    float y = 2.0f * t - 1.0f;
    if (t < dt) {
        float x = t / dt;
        y -= x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        y -= x * x + x + x + 1.0f;
    }
    return y;
}

/* ------------------------------------------------------------------ bass --- */
static struct {
    float ph, inc, note, target, amp;
    uint8_t midi_note, rest;                    /* rest: sitting this chord out */
} bass;
static float bass_vol, glide_k = 1.0f / (0.25f * EN_SR);      /* glide_k: 1 / the glide's time in samples */

/* ----------------------------------------------------------------- bowls --- */
#define BOWL_MODES 5
#define BOWLS_STRIKE_INTERVAL ((uint32_t)(8.0f * EN_SR))

/* A split fundamental gives slow beating. The inharmonic modes of the rim
 * lose energy faster than the fundamental. Decays are e-folding times. */
static const struct {
    float ratio, weight, decay, side;
} bowl_modes[BOWL_MODES] = {
    {1.000f, 0.59f, 3.8f, -0.14f},
    {1.006f, 0.32f, 3.5f,  0.19f},
    {2.720f, 0.32f, 2.8f,  0.10f},
    {4.840f, 0.19f, 1.2f, -0.19f},
    {7.520f, 0.10f, 0.5f,  0.22f},
};

typedef struct {
    uint32_t phase, step;
    float amp, side;
} bowl_partial_t;

typedef struct {
    bowl_partial_t partial[2 * BOWL_MODES];
    float attack, fade, fade_step;
    int count;
} bowl_ring_t;

static struct {
    bowl_ring_t ring[2];                         /* the new strike and its predecessor */
    uint32_t until_strike, cloud_left;
    int current, root, interval, tune;
    float level, blur;
} bowls;
static float bowls_vol;
static float bowl_decay[BOWL_MODES];

static void bowl_strike(bowl_ring_t *ring, int root, int interval)
{
    int bowls = interval ? 2 : 1;
    ring->count = bowls * BOWL_MODES;
    ring->attack = 0.0f;
    ring->fade = 1.0f;
    ring->fade_step = 0.0f;
    for (int b = 0; b < bowls; b++) {
        float step = mtof((float)(root + b * interval)) * (4294967296.0f / EN_SR);
        float level = bowls == 1 ? 1.0f : b == 0 ? 0.76f : 0.46f;
        float pan = bowls == 1 ? 0.0f : b == 0 ? -0.23f : 0.27f;
        for (int k = 0; k < BOWL_MODES; k++) {
            bowl_partial_t *p = &ring->partial[b * BOWL_MODES + k];
            p->phase = k == 1 ? (1u << 30) : 0u;
            p->step = (uint32_t)(step * bowl_modes[k].ratio);
            p->amp = level * bowl_modes[k].weight;
            p->side = pan + bowl_modes[k].side;
        }
    }
}

/* Stereo diffusion for the bowls' Blur control. */
#define CLOUD_STAGES 3
static const int CLOUD_LEN_L[CLOUD_STAGES] = {2237, 4447, 7187}; /* 50.7ms, 100.8ms, 163.0ms */
static const int CLOUD_LEN_R[CLOUD_STAGES] = {2503, 4813, 7829}; /* 56.8ms, 109.1ms, 177.5ms */
#define CLOUD_TOTAL_L (2237 + 4447 + 7187)
#define CLOUD_TOTAL_R (2503 + 4813 + 7829)

static delay_sample_t cloud_buf_l[CLOUD_TOTAL_L];
static delay_sample_t cloud_buf_r[CLOUD_TOTAL_R];
static delay_sample_t *cloud_ptrs_l[CLOUD_STAGES];
static delay_sample_t *cloud_ptrs_r[CLOUD_STAGES];
static int cloud_idx_l[CLOUD_STAGES];
static int cloud_idx_r[CLOUD_STAGES];
static float cloud_fb_l = 0.0f, cloud_fb_r = 0.0f;
static float cloud_damp_l = 0.0f, cloud_damp_r = 0.0f;
static float cloud_hp_l = 0.0f, cloud_hp_r = 0.0f;

/* 2-pole acoustic smoothing lowpass filter state */
static float bowls_lp1_l = 0.0f, bowls_lp2_l = 0.0f;
static float bowls_lp1_r = 0.0f, bowls_lp2_r = 0.0f;

static void bowls_cloud_init(void)
{
    delay_sample_t *pl = cloud_buf_l;
    delay_sample_t *pr = cloud_buf_r;
    for (int k = 0; k < CLOUD_STAGES; k++) {
        cloud_ptrs_l[k] = pl;
        cloud_idx_l[k] = 0;
        pl += CLOUD_LEN_L[k];

        cloud_ptrs_r[k] = pr;
        cloud_idx_r[k] = 0;
        pr += CLOUD_LEN_R[k];
    }
    for (int i = 0; i < CLOUD_TOTAL_L; i++) cloud_buf_l[i] = 0;
    for (int i = 0; i < CLOUD_TOTAL_R; i++) cloud_buf_r[i] = 0;
    cloud_fb_l = cloud_fb_r = 0.0f;
    cloud_damp_l = cloud_damp_r = 0.0f;
    cloud_hp_l = cloud_hp_r = 0.0f;
    bowls_lp1_l = bowls_lp2_l = 0.0f;
    bowls_lp1_r = bowls_lp2_r = 0.0f;
}

static inline float cloud_ap_step(delay_sample_t *buf, int *idx, int len, float x, float g)
{
    int i = *idx;
    float z = delay_read(buf[i]);
    float v = x - g * z;
    buf[i] = delay_write(fm_flush(v));
    if (++i >= len) i = 0;
    *idx = i;
    return z + g * v;
}

static inline float cloud_dl_step(delay_sample_t *buf, int *idx, int len, float x)
{
    int i = *idx;
    float z = delay_read(buf[i]);
    buf[i] = delay_write(fm_flush(x));
    if (++i >= len) i = 0;
    *idx = i;
    return z;
}

static void bowls_reset(void)
{
    bowls.ring[0].count = bowls.ring[1].count = 0;
    bowls.current = 0;
    bowls.until_strike = bowls.cloud_left = 0;
    bowls.level = bowls.blur = en_bowls_level = 0.0f;
    bowls_cloud_init();
}


/* ----------------------------------------------------------------- bells --- */
#define NBELL 6
typedef struct {
    float cph, mph, cinc, minc, amp, adec, idx, idec, att, gl, gr;
    int32_t wait;
    uint8_t on, note, loop;
} bell_t;
static bell_t bell[NBELL];
static float bell_vol, btone;
static int8_t loop_note[EN_NLOOP][EN_LOOP_MAX];
static uint8_t loop_vel[EN_NLOOP][EN_LOOP_MAX], loop_pos[EN_NLOOP];
static const uint8_t LOOP_BASE[EN_NLOOP] = {72, 67, 79, 62};     /* each loop's register */
static const float LOOP_PAN[EN_NLOOP] = {-0.6f, 0.35f, 0.7f, -0.25f};
static const float RATIO[5] = {1.0f, 2.0f, 3.0f, 3.5f, 4.77f};

/* ----------------------------------------------------------------- pulse --- */
typedef struct {
    float ph, inc, inc_end, amp, adec, idx, x1, x2, att, mph[4], pend;
    int32_t wait;                               /* swing: samples until a hit that waits (pend: its velocity) */
    uint8_t on, note;
} drum_t;
static drum_t drum[EN_NDRUM];
static uint8_t drum_pos[EN_NDRUM];
static uint32_t drum_pat[EN_NDRUM];
static float pulse_vol, swing, hat_f = 0.9f, hat_open = 0.42f;
static float step_len, pulse_left;               /* a sixteenth, in samples; to the next MIDI clock pulse */
static uint32_t noise = 22222u;
static const uint8_t DRUM_KMAX[EN_NDRUM] = {5, 7, 3};
static const uint8_t DRUM_GM[EN_NDRUM] = {36, 42, 46};          /* kick, closed hat, open hat */
/* the hats' metal: four square waves at the 808's inharmonic ratios (Hz / EN_SR) */
static const float HAT_INC[4] = {304.4f / EN_SR, 369.6f / EN_SR, 522.7f / EN_SR, 800.0f / EN_SR};

/* ------------------------------------------------------------------ space --- */
static float echo_send, rev_send, echo_len, echo_len_t, echo_fb = 0.55f;
static float rev_in_gain = 1.0f;                /* falls to 0 while frozen */

/* -------------------------------------------------------------- harmony --- */
static void scale_set(void)
{
    nsteps = en_scale(pool, steps);
}

static int bell_note(int l, int off)            /* a loop slot's note, from the chord's degree */
{
    int d = deg + off, oct = d / nsteps, s = steps[d % nsteps], base, root = par[P_ROOT], n;
    base = LOOP_BASE[l] - ((LOOP_BASE[l] - root) % 12 + 12) % 12;   /* the root at or below the register */
    n = base + s + 12 * oct + 12 * par[P_BOCT];
    while (n > 103)
        n -= 12;
    while (n < 36)
        n += 12;
    return n;
}

static void pad_retarget(void)                  /* the chord's tones to the four voices */
{
    int from[4], to[4], i;
    chord_nt = en_chord_tones(pool, par[P_ROOT], deg, chord_pcs);
    for (i = 0; i < EN_NPAD; i++)
        from[i] = (int)(pad[i].target + 0.5f);
    en_voice_lead(from, chord_pcs, chord_nt, to, 48, 77);
    for (i = 0; i < EN_NPAD; i++) {             /* each voice takes its note at its next breath */
        pad[i].target = (float)to[i];
        en_chord[i] = (uint8_t)to[i];
    }
    en_chord_root = (uint8_t)chord_pcs[0];
    en_chord_n = (uint8_t)chord_nt;
}

static void bass_to(int pc)
{
    int n = 36 + pc % 12 + 12 * par[P_BASSOCT];     /* C2..B2, an octave down or up */
    bass.target = (float)n;
    if (en_playing && bass.midi_note != n) {
        midi(0x82, bass.midi_note, 0);
        midi(0x92, (uint32_t)n, 80);
    }
    bass.midi_note = (uint8_t)n;
}

static float drift_left, drift_total, evolve;   /* evolve: Evolve, 0..1 */
static uint8_t change_pending, fifth_done;

static void drift_reset(void)
{
    drift_total = drift_secs(par[P_DRIFT]) * (0.7f + 0.6f * frand()) * EN_SR;
    drift_left = drift_total;
    fifth_done = 0;
}

static void chord_change(void)                  /* to another degree: the near, strong moves first */
{
    static const float W7[7] = {4.0f, 2.0f, 1.0f, 3.0f, 3.0f, 3.0f, 0.4f};
    float w[12], sum = 0.0f, r;
    int i;
    for (i = 0; i < nsteps; i++) {
        w[i] = i == deg ? 0.0f : nsteps == 7 ? W7[i] : i == 0 ? 3.0f : 1.5f;
        sum += w[i];
    }
    r = frand() * sum;
    for (i = 0; i < nsteps; i++) {
        r -= w[i];
        if (r <= 0.0f && w[i] > 0.0f)
            break;
    }
    if (i >= nsteps)
        i = deg == 0 ? (nsteps > 1 ? 1 : 0) : 0;
    deg = i;
    pad_retarget();
    pad_leave();
    bass_to(chord_pcs[0]);
    /* Evolve: the bass sometimes sits a chord out, more often when the arc is sparse */
    bass.rest = frand() < evolve * 0.3f + 0.25f * fm_maxf(-arcm, 0.0f);
    drift_reset();
}

/* ------------------------------------------------------------- the loops --- */
static int rand_offset(void)                    /* scale steps above the chord's degree */
{
    if (frand() < 0.72f) {                      /* a chord tone, sometimes an octave up */
        int o = 2 * irand(chord_nt);
        return o + (frand() < 0.3f ? nsteps : 0);
    }
    return 1 + 2 * irand(3);                    /* a passing note: the 2nd, 4th or 6th */
}

static int loop_count(int l)
{
    int i, c = 0;
    for (i = 0; i < EN_LOOP_LEN[l]; i++)
        c += loop_note[l][i] >= 0;
    return c;
}

static int loop_target(void)                    /* 0..5 notes: Density, swelled and thinned by the arc */
{
    float d = (float)par[P_DENSITY] * 0.05f * (1.0f + 0.8f * arcm);
    return (int)(fm_clampf(d, 0.0f, 5.0f) + frand());
}
static uint8_t loop_rest[EN_NLOOP];             /* this pass: silent (Evolve) */

static void loop_add(int l)
{
    int len = EN_LOOP_LEN[l], i, s;
    for (i = 0; i < 8; i++) {
        s = irand(len);
        if (loop_note[l][s] < 0) {
            loop_note[l][s] = (int8_t)rand_offset();
            loop_vel[l][s] = (uint8_t)(50 + irand(50));
            return;
        }
    }
}

static int loop_pick(int l)                     /* a random sounding slot, -1 none */
{
    int len = EN_LOOP_LEN[l], i, s = irand(len);
    for (i = 0; i < len; i++, s = (s + 1) % len)
        if (loop_note[l][s] >= 0)
            return s;
    return -1;
}

static void loop_publish(int l)
{
    int i;
    for (i = 0; i < EN_LOOP_MAX; i++)
        en_loop_note[l][i] = i < EN_LOOP_LEN[l] ? loop_note[l][i] : -1;
}

static void loop_regrow(int l)
{
    int i, n = loop_target();
    for (i = 0; i < EN_LOOP_MAX; i++)
        loop_note[l][i] = -1;
    for (i = 0; i < n; i++)
        loop_add(l);
    loop_publish(l);
}

static void loop_pass(int l)                    /* the end of a pass: grow, prune, evolve */
{
    int c = loop_count(l), t = loop_target(), s;
    for (; c < t; c++)                          /* Density: to its count by the end of a pass */
        loop_add(l);
    for (; c > t && (s = loop_pick(l)) >= 0; c--)
        loop_note[l][s] = -1;
    loop_rest[l] = frand() < evolve * 0.15f + 0.15f * fm_maxf(-arcm, 0.0f);
    if (frand() < evolve * 0.7f && (s = loop_pick(l)) >= 0) {
        if (frand() < 0.5f) {
            loop_note[l][s] = (int8_t)rand_offset();             /* another note */
        } else {
            int d = irand(EN_LOOP_LEN[l]);                       /* the same note, elsewhere */
            if (loop_note[l][d] < 0) {
                loop_note[l][d] = loop_note[l][s];
                loop_vel[l][d] = loop_vel[l][s];
                loop_note[l][s] = -1;
            }
        }
    }
    loop_publish(l);
}

/* each loop its own bell: index, decay and ratio a little apart, so the four can be told apart */
static const float LOOP_IDX[EN_NLOOP] = {1.0f, 0.55f, 1.5f, 0.8f}, LOOP_DEC[EN_NLOOP] = {1.0f, 1.35f, 0.75f, 1.6f};
static const int8_t LOOP_RAT[EN_NLOOP] = {0, 0, 1, -1};

static void bell_voice(int l, int n, float vel, int32_t wait)
{
    int i, best = 0, ri;
    float lo = 9.0f, t60, p;
    bell_t *v;
    for (i = 0; i < NBELL; i++) {               /* a free voice, or the quietest */
        float a = bell[i].on ? bell[i].amp : -1.0f;
        if (a < lo) {
            lo = a;
            best = i;
        }
    }
    v = &bell[best];
    if (v->on)
        midi(0x81, v->note, 0);
    v->on = 1;
    v->note = (uint8_t)n;
    v->loop = (uint8_t)l;
    v->cph = v->mph = 0.0f;
    v->cinc = mtof((float)n) * (1.0f / EN_SR);
    ri = par[P_BTONE] * 5 / 101 + LOOP_RAT[l];
    v->minc = v->cinc * RATIO[ri < 0 ? 0 : ri > 4 ? 4 : ri];
    v->amp = vel;
    t60 = 4.2f - (float)(n - 60) * 0.06f;       /* the low bells ring longer */
    t60 = fm_clampf(t60, 1.6f, 5.0f) * LOOP_DEC[l];
    v->adec = fm_expf(-6.9f / (t60 * EN_SR));
    /* Bell tone: 0 a round, nearly pure tone; 100 a long metallic clang (index in radians, as turns) */
    v->idx = (0.15f + 6.5f * btone * btone) * LOOP_IDX[l] * (0.6f + 0.4f * vel) * (1.0f / FM_TWO_PI);
    v->idec = fm_expf(-6.9f / ((0.35f + 2.6f * btone) * EN_SR));
    v->att = 0.0f;
    p = (LOOP_PAN[l] + 1.0f) * (FM_PI / 4.0f);
    v->gl = fm_cosf(p);
    v->gr = fm_sinf(p);
    v->wait = wait;
    en_loop_glow[l] = 1.0f;
    midi(0x91, (uint32_t)n, (uint32_t)(vel * 110.0f));
}

static void bell_ring(int l, int slot)
{
    int n = bell_note(l, loop_note[l][slot]);
    float vel = (float)loop_vel[l][slot] * 0.01f;
    int32_t wait = irand(2600);                 /* a human hand: up to 60 ms late */
    bell_voice(l, n, vel, wait);
    if (n + 12 <= 103 && frand() < 0.08f + 0.12f * fm_maxf(arcm, 0.0f))
        bell_voice(l, n + 12, vel * 0.4f, wait + 300 + irand(2000));   /* now and then, its octave */
}

/* ---------------------------------------------------------------- pulse --- */
static const uint8_t DRUM_ROT[EN_NDRUM] = {0, 2, 3};
static int drum_k[EN_NDRUM], drum_kb[EN_NDRUM], drum_rot[EN_NDRUM], pulse_rest;   /* rest: bars left */

static void drum_publish(int d)
{
    drum_pat[d] = en_euclid(drum_k[d], EN_DRUM_N[d], drum_rot[d]);
    en_drum_pat[d] = drum_pat[d];
}

static void drums_pattern(void)                 /* Hits: the patterns' home */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++) {
        drum_kb[d] = drum_k[d] = (par[P_HITS] * DRUM_KMAX[d] + 50) / 100;
        drum_rot[d] = DRUM_ROT[d];
        drum_publish(d);
    }
}

static void drums_evolve(void)                  /* a bar: a pattern turns, or gains or loses a hit */
{
    int d = irand(EN_NDRUM), n = EN_DRUM_N[d];
    if (!drum_kb[d])
        return;
    if (frand() < 0.5f) {
        drum_rot[d] = (drum_rot[d] + (frand() < 0.5f ? 1 : n - 1)) % n;
    } else {
        int k = drum_k[d] + (frand() < 0.5f ? 1 : -1);
        if (k >= drum_kb[d] - 1 && k <= drum_kb[d] + 1 && k >= 1 && k <= n)
            drum_k[d] = k;
    }
    drum_publish(d);
}

static void drum_fire(int d, float vel)
{
    drum_t *v = &drum[d];
    v->on = 1;
    v->amp = vel;
    v->att = 0.0f;
    switch (d) {
    case 0: {                                   /* thump: the chord's root, low, falling into place */
        float f = mtof((float)(36 + chord_pcs[0]));
        v->note = DRUM_GM[0];
        v->inc = f * 2.2f * (1.0f / EN_SR);
        v->inc_end = f * (1.0f / EN_SR);
        v->adec = fm_expf(-6.9f / (0.45f * EN_SR));
        break;
    }
    case 1:                                     /* closed hat: short; it chokes the open one */
        v->note = DRUM_GM[1];
        v->adec = fm_expf(-6.9f / (0.06f * EN_SR));
        drum[2].adec = fm_expf(-6.9f / (0.02f * EN_SR));
        break;
    default:                                    /* open hat: a long, soft "tss" */
        v->note = DRUM_GM[2];
        v->adec = fm_expf(-6.9f / (hat_open * EN_SR));
        break;
    }
    en_drum_glow[d] = 1.0f;
    midi(0x99, v->note, (uint32_t)(vel * 100.0f));
    midi(0x89, v->note, 0);
}

static void drum_hit(int d, float vel, int off)  /* off: an off sixteenth, which Swing delays */
{
    int32_t w = off ? (int32_t)(swing * step_len) : 0;
    if (w > 0) {
        drum[d].wait = w;
        drum[d].pend = vel;
    } else {
        drum_fire(d, vel);
    }
}

static int32_t drums_wait(void)                 /* samples to the next hit that waits; 0: none */
{
    int32_t m = 0;
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (!m || drum[d].wait < m))
            m = drum[d].wait;
    return m;
}

static void drums_wait_step(int32_t k)          /* k samples went by: hits whose wait is over */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (drum[d].wait -= k) <= 0) {
            drum[d].wait = 0;
            drum_fire(d, drum[d].pend);
        }
}

/* the grid: one call a sixteenth (MIDI clock: 6 ticks) */
static uint32_t sixteenth;
static void tick(void)
{
    int d, l;
    const float pa = fm_clampf(1.0f + 1.3f * arcm, 0.3f, 1.15f);   /* the arc: the pulse fades (never out) */
    if ((sixteenth & 15u) == 0u) {              /* a bar: Evolve's rests and turns */
        if (pulse_rest > 0)
            pulse_rest--;
        else if (frand() < evolve * 0.015f)
            pulse_rest = 2 + irand(3);           /* a short breath: 2 to 4 bars */
        en_pulse_rest = (uint8_t)(pulse_rest > 0);
        if (frand() < evolve * 0.25f)
            drums_evolve();
    }
    for (d = 0; d < EN_NDRUM; d++) {
        if (drum_pat[d] >> drum_pos[d] & 1u && !pulse_rest)
            drum_hit(d, ((drum_pos[d] == 0 ? 0.95f : 0.6f) + 0.25f * frand()) * pa, (int)(sixteenth & 1u));
        en_drum_pos[d] = drum_pos[d];
        if (++drum_pos[d] >= EN_DRUM_N[d])
            drum_pos[d] = 0;
    }
    if ((sixteenth & 3u) == 0u) {               /* the loops step in quarter notes */
        for (l = 0; l < EN_NLOOP; l++) {
            int s = loop_pos[l];
            if (loop_note[l][s] >= 0 && !loop_rest[l])
                bell_ring(l, s);
            en_loop_pos[l] = (uint8_t)s;
            if (++loop_pos[l] >= EN_LOOP_LEN[l]) {
                loop_pos[l] = 0;
                loop_pass(l);
            }
        }
    }
    if ((sixteenth & 15u) == 0u && change_pending && !en_frozen) {   /* chords move on the bar */
        change_pending = 0;
        chord_change();
    }
    sixteenth++;
    en_bars = sixteenth;
}

/* ------------------------------------------------- reverb: Dattorro's plate --- */
/* J. Dattorro, "Effect Design, Part 1", JAES 1997 (as in FoMni): a predelay, four input diffusers,
 * then a figure-eight tank of two halves, each a modulated allpass, a delay, a damping lowpass, an
 * allpass and a delay, feeding the other half. Stereo out from seven taps a side. The paper's lengths
 * are for 29761 Hz; DS() scales them to 44.1 kHz. */
#define DS(n) (((n) * 14818 + 5000) / 10000)          /* 44100 / 29761 */
#define PRE_N 1323                                   /* 30 ms */
#define EXC DS(16)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC + 2))
static delay_sample_t pl_buf[DL_TOTAL] EN_POOL;
typedef struct {
    delay_sample_t *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.5f, pl_decay_t = 0.5f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;

static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return delay_read(d->b[j < 0 ? j + d->n : j]);
}
static inline void push(dl_t *d, float x)
{
    d->b[d->i] = delay_write(x);
    if (++d->i >= d->n)
        d->i = 0;
}
static inline float allpass(dl_t *d, int len, float x, float g)
{
    float z = tap(d, len), v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline float allpass_mod(dl_t *d, float len, float x, float g)
{
    int k = (int)len;
    float f = len - (float)k, z = tap(d, k) + (tap(d, k + 1) - tap(d, k)) * f, v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}

static inline void reverb(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(&dl[L_D2R], DS(3163)), fb_r = tap(&dl[L_D2L], DS(3720));
    push(&dl[L_PRE], in);
    x = tap(&dl[L_PRE], PRE_N);
    pl_bw += 0.6f * (x - pl_bw);
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {
        float s = lfo_s + 0.0001f * lfo_c, c = lfo_c - 0.0001f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += 0.45f * (b - pl_damp_l);
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, pl_dd2);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += 0.45f * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_decay, pl_dd2);
    push(&dl[L_D2R], b);
    *ol = 0.6f * (tap(&dl[L_D1R], DS(266)) + tap(&dl[L_D1R], DS(2974)) - tap(&dl[L_AP2R], DS(1913)) +
                  tap(&dl[L_D2R], DS(1996)) - tap(&dl[L_D1L], DS(1990)) - tap(&dl[L_AP2L], DS(187)) -
                  tap(&dl[L_D2L], DS(1066)));
    *or_ = 0.6f * (tap(&dl[L_D1L], DS(353)) + tap(&dl[L_D1L], DS(3627)) - tap(&dl[L_AP2L], DS(1228)) +
                   tap(&dl[L_D2L], DS(2673)) - tap(&dl[L_D1R], DS(2111)) - tap(&dl[L_AP2R], DS(335)) -
                   tap(&dl[L_D2R], DS(121)));
}

static void plate_init(void)
{
    delay_sample_t *p = pl_buf;
    int k, i;
    for (k = 0; k < L_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0;
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* ------------------------------------------------- echo: tape and Echoplex --- */
/* After Discreet Music's chain: an Echoplex (a short, smearing echo, ~160 ms, fed back) and a tape loop
 * (long: up to ~5.6 s, synced to the tempo), each repeat a little darker and thinner, with a little
 * wow. The tape runs at a quarter rate (11 kHz, 16-bit, mono): it loses its highs anyway. Two heads:
 * the first plays the loop's length, the second a part of it (3/4 at first). With Motion the echo
 * evolves: each head wanders across the field, the feedback leans now on one head, now on the other
 * (the repeats fall in changing patterns, left and right), and now and then the second head slides to
 * another part of the loop, as a head moved by hand (the repeats bend a little in pitch on the way).
 * The Echoplex has a second tap, for the right. */
#define TAPE_N 62000                             /* 5.6 s at 11.025 kHz */
#define TAPE_SR 11025.0f
static int16_t tape[TAPE_N] EN_POOL;
static int tape_w, tape_ph;
static float tape_acc, tape_lp, tape_hp, tape_p1, tape_p2, tape_c1, tape_c2;
static const float HEAD2[5] = {0.75f, 0.6667f, 0.625f, 0.5f, 0.375f};   /* the second head's parts */
static float head2 = 0.75f, head2_t = 0.75f, head2_v;   /* its place (of the loop), target, speed */
static float fb_lean;                            /* 0..1: how much of the feedback is the second head's */
static float hg[4] = {1.0f, 0.0f, 0.0f, 1.0f};   /* the heads' pan gains: 1 left, 1 right, 2 left, 2 right */
static uint32_t head_left;                       /* samples until the second head may move */
#define PLEX_N 7100                              /* 160 ms at 44.1 kHz */
#define PLEX_TAP 2700                            /* the right's tap: ~100 ms */
static int16_t plex[PLEX_N];
static int plex_w;
static float plex_lp, plex_lp2;

static inline float tape_read(float d)           /* d quarter-rate samples ago */
{
    int k = (int)d, i0, i1;
    float f = d - (float)k, a, c;
    i0 = tape_w - k;
    i1 = i0 - 1;
    if (i0 < 0)
        i0 += TAPE_N;
    if (i1 < 0)
        i1 += TAPE_N;
    a = (float)tape[i0];
    c = (float)tape[i1];
    return (a + (c - a) * f) * (1.0f / 12000.0f);
}
static inline int16_t s16(float x)
{
    x *= 12000.0f;
    return (int16_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
}

static void echo_time(void)
{
    int i = par[P_ETIME];
    float beat, t;
    if (par[P_TEMPO] <= 0)
        return;                                  /* not yet (en_init applies in order) */
    beat = 60.0f / (float)par[P_TEMPO];
    t = ETIME_BEATS[i] * beat;
    while (t * TAPE_SR > (float)(TAPE_N - 8) && i > 0)
        t = ETIME_BEATS[--i] * beat;             /* too long for the tape: the next shorter */
    if (t * TAPE_SR > (float)(TAPE_N - 8))
        t = (float)(TAPE_N - 8) / TAPE_SR;
    echo_len_t = t * TAPE_SR;
    if (echo_len <= 0.0f)
        echo_len = echo_len_t;
}

/* once a chunk: where the heads sit in the field, how the feedback leans, the second head's slide */
static void heads_step(uint32_t n)
{
    float p, want, vmax;
    int i;
    for (i = 0; i < 2; i++) {
        p = fm_clampf((i ? 0.7f : -0.7f) + 0.9f * motion * mod[6 + i].v, -1.0f, 1.0f);
        p = (p + 1.0f) * (FM_PI / 4.0f);
        hg[2 * i] = fm_cosf(p);
        hg[2 * i + 1] = fm_sinf(p);
    }
    fb_lean = 0.25f * motion * (1.0f + mod[8].v);
    if (head_left <= n) {
        head_left = (uint32_t)((20.0f + 40.0f * frand()) * EN_SR);
        if (en_playing && !en_frozen && frand() < motion)
            head2_t = HEAD2[(int)(frand() * 4.999f)];
    } else {
        head_left -= n;
    }
    /* its speed (a part of the loop per tape sample): at most a bend of ~14 cents, easing in and out */
    vmax = 0.008f / fm_maxf(echo_len, 100.0f);
    want = fm_clampf((head2_t - head2) * (1.0f / (2.0f * TAPE_SR)), -vmax, vmax);
    head2_v += (want - head2_v) * fm_minf((float)n * (1.0f / EN_SR), 1.0f);
}

/* one sample of both: the input (a mono send) to the tape's heads (el, er) and the Echoplex (xl, xr) */
static inline void echoes(float in, float *el, float *er, float *xl, float *xr)
{
    float y = (float)plex[plex_w] * (1.0f / 12000.0f);         /* the Echoplex: 160 ms, fed back, darker */
    int t = plex_w - PLEX_TAP;
    plex_lp += 0.35f * (y - plex_lp);
    plex_lp2 += 0.35f * ((float)plex[t < 0 ? t + PLEX_N : t] * (1.0f / 12000.0f) - plex_lp2);
    plex[plex_w] = s16(in + 0.55f * plex_lp);
    if (++plex_w >= PLEX_N)
        plex_w = 0;
    *xl = plex_lp;
    *xr = plex_lp2;
    tape_acc += in + 0.3f * plex_lp;
    if (++tape_ph >= 4) {                        /* the tape, a quarter of the time */
        float d, h1, h2, fb;
        tape_ph = 0;
        echo_len += (echo_len_t - echo_len) * 0.001f;
        head2 += head2_v;
        d = echo_len * (1.0f + 0.003f * motion * mod[5].v);   /* wow */
        if (d > (float)(TAPE_N - 4))
            d = (float)(TAPE_N - 4);
        h1 = tape_read(d);
        h2 = tape_read(d * head2);
        tape_lp += 0.55f * (h1 + fb_lean * (h2 - h1) - tape_lp);   /* each time round: darker, thinner */
        tape_hp += 0.03f * (tape_lp - tape_hp);
        fb = tape_lp - tape_hp;
        if (++tape_w >= TAPE_N)
            tape_w = 0;
        tape[tape_w] = s16(tape_acc * 0.25f + echo_fb * fb);
        tape_acc = 0.0f;
        tape_p1 = tape_c1;
        tape_p2 = tape_c2;
        tape_c1 = h1;
        tape_c2 = h2;
    }
    {   /* between the tape's samples: a line from the last to the next; then to the field */
        float f = (float)tape_ph * 0.25f;
        float h1 = tape_p1 + (tape_c1 - tape_p1) * f, h2 = tape_p2 + (tape_c2 - tape_p2) * f;
        *el = hg[0] * h1 + hg[2] * h2;
        *er = hg[1] * h1 + hg[3] * h2;
    }
}

/* ------------------------------------------------------------ parameters --- */
static int pulse;
static uint32_t ext_age;

static void tempo_set(void) { step_len = EN_SR * 60.0f / ((float)par[P_TEMPO] * 4.0f); }

static void apply(int p, int v)
{
    const en_param_t *pi = &PARAMS[p];
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    par[p] = (int16_t)v;
    switch (p) {
    case P_PAD: pad_vol = (float)(v * v) * 0.0001f; break;
    case P_BELLS: bell_vol = (float)(v * v) * 0.0001f; break;
    case P_BASS: bass_vol = (float)(v * v) * 0.0001f; break;
    case P_PULSE: pulse_vol = (float)(v * v) * 0.0001f; break;
    case P_HITS: drums_pattern(); break;
    case P_BASSOCT:
        if (nsteps)
            bass_to(bass.midi_note % 12);
        break;
    case P_GLIDE: glide_k = 1.0f / ((0.02f + 0.012f * (float)v) * EN_SR); break;   /* 20 ms .. 1.2 s */
    case P_SWING: swing = (float)v * 0.005f; break;      /* the off sixteenths up to half a step late */
    case P_HATS:
        hat_f = 0.55f + 0.0070f * (float)v;               /* the high-pass: ~4 kHz dark .. ~10 kHz bright */
        hat_open = 0.6f - 0.0035f * (float)v;             /* the open hat: long and soft .. short and crisp */
        break;
    case P_TEXT_VOL: text_vol = (float)(v * v) * 0.0001f; break;
    case P_TEXT_TYPE: text_type = v; break;
    case P_TEXT_TONE: text_tone = (float)v * 0.01f; break;
    case P_TEXT_DENS: text_dens = (float)v * 0.01f; break;
    case P_BOWLS_VOL: {
        float level = (float)v * 0.01f;
        bowls_vol = level * (0.5f + 0.5f * level);
        break;
    }
    case P_DRIFT:
        if (drift_total > 0.0f) {                /* keep the progress, at the new pace */
            float frac = drift_left / drift_total;
            drift_total = drift_secs(v) * EN_SR;
            drift_left = frac * drift_total;
        }
        break;
    case P_ECHO: echo_send = (float)(v * v) * 0.0001f; break;
    case P_ETIME: echo_time(); break;
    case P_REVERB: rev_send = (float)(v * v) * 0.0001f; break;
    case P_SIZE:
        pl_decay_t = 0.6f + 0.375f * (float)v * 0.01f;     /* to 0.975: a long hall */
        echo_fb = 0.5f + 0.32f * (float)v * 0.01f;
        break;
    case P_BRIGHT: bright = (float)v; break;
    case P_SWELL: swell = (float)v; break;
    case P_EVOLVE: evolve = (float)v * 0.01f; break;
    case P_BTONE: btone = (float)v * 0.01f; break;
    case P_PADTONE: pad_handoff(); break;        /* the voices cross over as they did for a new key */
    case P_MOTION: motion = (float)v * 0.01f; break;
    case P_BOCT:                                 /* let it be heard now: a bell from the first loop */
        if (en_playing && nsteps) {
            int l, k;
            for (l = 0; l < EN_NLOOP; l++)
                for (k = 0; k < EN_LOOP_LEN[l]; k++)
                    if (loop_note[l][k] >= 0) {
                        bell_ring(l, k);
                        return;
                    }
        }
        break;
    case P_TUNE: tunefac = fm_exp2f((float)v * (1.0f / 1200.0f)); break;
    case P_TEMPO:
        tempo_set();
        echo_time();
        break;
    case P_ROOT:
        if (nsteps) {
            pad_retarget();
            pad_handoff();
            bass_to(chord_pcs[0]);
        }
        break;
    case P_MIDI:
        if (!v)
            en_midi_out(0xB0, 123, 0);
        break;
    default: break;
    }
}

static void pool_set(uint32_t p)
{
    pool = (p | 1u) & 0xFFFu;
    scale_set();
    if (deg >= nsteps)
        deg = 0;
    pad_retarget();
    pad_handoff();
    bass_to(chord_pcs[0]);
}

static void regrow(void)                        /* a new piece: fresh loops, home chord, patterns home */
{
    int l;
    for (l = 0; l < EN_NLOOP; l++) {
        loop_regrow(l);
        loop_rest[l] = 0;
    }
    deg = 0;
    pad_retarget();
    pad_handoff();
    bass_to(chord_pcs[0]);
    bass.rest = 0;
    pulse_rest = 0;
    en_pulse_rest = 0;
    drums_pattern();
    drift_reset();
    change_pending = 0;
}

static void notes_off(void)
{
    int i;
    for (i = 0; i < 4; i++)
        en_midi_out(0xB0 | (uint32_t)i, 123, 0);
    en_midi_out(0xB9, 123, 0);
}

static void quiet(void)
{
    int i;
    for (i = 0; i < NBELL; i++)
        bell[i].on = 0;
    for (i = 0; i < EN_NDRUM; i++)
        drum[i].on = 0, drum[i].wait = 0;
    for (i = 0; i < EN_NPAD; i++)
        pad[i].amp = 0.0f;
    bass.amp = 0.0f;
    bowls_reset();
    notes_off();
}

#include "solo.c"

void en_init(void)
{
    int i;
    solo_init();
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    for (i = 0; i < BOWL_MODES; i++)
        bowl_decay[i] = fm_expf(-1.0f / (bowl_modes[i].decay * EN_SR));
    plate_init();
    for (i = 0; i < TAPE_N; i++)
        tape[i] = 0;
    for (i = 0; i < PLEX_N; i++)
        plex[i] = 0;
    tape_w = plex_w = tape_ph = 0;
    head2 = head2_t = 0.75f;
    head2_v = 0.0f;
    echo_len = 0.0f;
    q_r = q_w = 0;
    for (i = 0; i < NMOD; i++) {
        mod[i].v = mod[i].tgt = 0.0f;
        mod[i].left = 0;
    }
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].note = pad[i].target = (float)(50 + 7 * i);
        pad[i].pan = PAD_PAN[i];
        pad[i].amp = 0.0f;
        pad[i].ph[0] = 0.0f;
        pad[i].ph[1] = 0.37f;
        pad[i].vib = 0.3f * (float)i;
        pad[i].midi_note = 0;
        pad[i].hf = 1.0f;
        pad[i].hand = 0;
    }
    arcm = 0.0f;
    pad_gain = 0.0f;
    for (i = 0; i < P_NPARAMS; i++)
        apply(i, PARAMS[i].def);
    pool_set(EN_MODE_POOL[par[P_MODE]]);
    for (i = 0; i < EN_NPAD; i++)
        pad[i].note = pad[i].target;
    bass.note = bass.target;
    bowls_reset();
    for (i = 0; i < EN_NLOOP; i++) {
        loop_pos[i] = 0;
        loop_regrow(i);
    }
    drums_pattern();
    drift_reset();
    pl_decay = pl_decay_t;
}

/* ------------------------------------------------------------ transport --- */
static void clock_pulse(void)
{
    if (pulse == 0)
        tick();
    if (!en_ext)
        midi(0xF8, 0, 0);
    if (++pulse >= 6)
        pulse = 0;
}

static void play_start(int from_top)
{
    int i;
    if (from_top) {
        pulse = 0;
        sixteenth = 0;
        for (i = 0; i < EN_NDRUM; i++)
            drum_pos[i] = 0;
        for (i = 0; i < EN_NLOOP; i++)
            loop_pos[i] = 0;
    }
    pulse_left = 0.0f;
    en_playing = 1;
    pad_gate = 1.0f;
    for (i = 0; i < EN_NPAD; i++)               /* from silence: the breaths start over, overlapping */
        if (pad[i].amp < 1e-4f) {
            pad[i].t = START_T[i];
            pad[i].midi_note = 0;
            pad_breathe(&pad[i], i);
        }
    if (bass.amp < 1e-4f)
        bass.note = bass.target;
    midi(0x92, bass.midi_note, 80);
    if (!en_ext)
        midi(from_top ? 0xFA : 0xFB, 0, 0);
}

static void play_stop(void)
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        drum[d].wait = 0;                        /* a swung hit still waiting stays unplayed */
    en_playing = 0;
    pad_gate = 0.0f;
    if (!en_ext)
        midi(0xFC, 0, 0);
    notes_off();
}

static void drain(void)
{
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        int a = (int)(c >> 16 & 0xFFu), b = (int)(int16_t)(c & 0xFFFFu);
        BARRIER();
        q_r++;
        switch (c >> 24) {
        case C_SET:
            if (a < P_NPARAMS)
                apply(a, b);
            break;
        case C_POOL: pool_set((uint32_t)b & 0xFFFu); break;
        case C_PLAY:
            if (a && !en_playing)
                play_start(1);
            else if (!a && en_playing)
                play_stop();
            break;
        case C_NEXT:
            if (en_playing)
                change_pending = 1;
            else
                chord_change();
            break;
        case C_FREEZE: en_frozen = (uint8_t)(a != 0); break;
        case C_SEED:
            rng ^= (uint32_t)(b & 0xFFFF) * 2654435761u;
            if (!rng)
                rng = 1u;
            break;
        case C_CLOCK:
            en_ext = 1;
            ext_age = 0;
            if (a == EN_CLK_TICK) {
                if (en_playing)
                    clock_pulse();
            } else if (a == EN_CLK_START) {
                play_start(1);
            } else if (a == EN_CLK_CONTINUE) {
                if (!en_playing)
                    play_start(0);
            } else if (en_playing) {
                play_stop();
            }
            break;
        case C_PANIC: quiet(); break;
        case C_REGROW: regrow(); break;
        case C_SOLO_NOTE: solo_note((uint8_t)a, (uint8_t)(b >> 8), (uint8_t)(b & 0xFF)); break;
        case C_SOLO_SET: solo_set(a, b); break;
        case C_SOLO_PANIC: solo_panic(); break;
        case C_SOLO_RESET: solo_reset_init(); break;
        }
    }
}

/* --------------------------------------------------------------- render --- */
#define BLK 256
static float mixl[BLK], mixr[BLK], rsend[BLK], esend[BLK], solo_rsend[BLK];

/* the pad's bus: a 12 dB low-pass, darker than it sounds on paper (Brian Eno darkened everything: the
 * Synthi's filter below half, then tape). No chorus: a modulated delay against pure tones beats (on
 * the FM-1's speaker above all), and that read as pulsing. The width comes from the voices' places,
 * the echo's two heads and the plate. */
static float padl[BLK], padr[BLK];
static float plp[4];

static void pad_bus(uint32_t n)
{
    const float fc = fm_clampf(700.0f * fm_exp2f(bright * 0.03f + 0.6f * arcm), 300.0f, 9000.0f);
    const float c = 1.0f - fm_expf(-FM_TWO_PI * fc * (1.0f / EN_SR));
    uint32_t j;
    for (j = 0; j < n; j++) {
        float l, r;
        plp[0] += c * (padl[j] - plp[0]);
        plp[1] += c * (plp[0] - plp[1]);
        plp[2] += c * (padr[j] - plp[2]);
        plp[3] += c * (plp[2] - plp[3]);
        l = plp[1];
        r = plp[3];
        mixl[j] += l * 0.8f;
        mixr[j] += r * 0.8f;
        rsend[j] += (l + r) * 0.6f;
        esend[j] += (l + r) * 0.2f;
    }
    for (j = 0; j < 4; j++)
        plp[j] = fm_flush(plp[j]);
}

static void render_pad(uint32_t n)
{
    int i;
    float lvl = 0.0f;
    uint32_t j0;
    for (j0 = 0; j0 < n; j0++)
        padl[j0] = padr[j0] = 0.0f;
    const float g = pad_vol * 0.5f;
    pad_gain += (pad_gate - pad_gain) * fm_minf((float)n / ((pad_gate > 0.0f ? 2.0f : 5.0f) * EN_SR), 1.0f);
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        float p0 = v->ph[0], p1 = v->ph[1], amp = v->amp, s0 = v->ic[0], s1 = v->ic[1], s2 = v->ic[2], s3 = v->ic[3],
              gl, gr, pn, tgt = 0.0f, idx = 0.0f;
        uint32_t j;
        if (amp < 1e-5f && pad_gain < 1e-4f) {
            v->amp = 0.0f;
            continue;
        }
        pn = fm_clampf(v->pan + 0.35f * motion * mod[(i + 3) % NMOD_PAD].v, -1.0f, 1.0f);
        gl = fm_cosf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        gr = fm_sinf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        for (j = 0; j < n; j++) {
            float x, v1, v2, v3, b1, l1, b2;
            if (!(j & 31u)) {
                float br;
                pad_coefs(v, i);
                br = breath(v->t);
                tgt = br * v->peak * pad_gain * v->hf;
                /* the Rhodes blooms: more overtones as the breath swells (index in turns) */
                idx = (0.04f + 0.1f * bright * 0.01f + 0.1f * br) * (1.0f + 0.4f * arcm);
            }
            if (!v->voices) {                    /* FM, the modulator at the carrier's pitch: a Rhodes */
                float m = sine(p0) * idx;
                x = (sine(p0 + m + 1.0f) + 0.12f * sine(2.0f * p0)) * 1.2f;   /* one carrier: no beating */
                p0 += v->inc[0];
                if (p0 >= 1.0f)
                    p0 -= 1.0f;
                amp += (tgt - amp) * 0.01f;
                x *= amp;
                padl[j] += x * gl;
                padr[j] += x * gr;
                continue;
            }
            /* as Discreet Music's Synthi: a mellow triangle (a sine's neighbour), a little saw in it */
            x = (p0 < 0.5f ? 4.0f * p0 - 1.0f : 3.0f - 4.0f * p0) * 1.4f + 0.45f * blep_saw(p0, v->inc[0]) + 0.6f * sine(p0);
            p0 += v->inc[0];
            if (p0 >= 1.0f)
                p0 -= 1.0f;
            v3 = x - s1;                         /* the first formant: TPT SVF (Zavalishin), bp and lp */
            v1 = v->c1[0] * s0 + v->c1[1] * v3;
            v2 = s1 + v->c1[1] * s0 + v->c1[2] * v3;
            s0 = 2.0f * v1 - s0;
            s1 = 2.0f * v2 - s1;
            b1 = v1 * v->c1[3];
            l1 = v2;
            v3 = x - s3;                         /* the second */
            v1 = v->c2[0] * s2 + v->c2[1] * v3;
            v2 = s3 + v->c2[1] * s2 + v->c2[2] * v3;
            s2 = 2.0f * v1 - s2;
            s3 = 2.0f * v2 - s3;
            b2 = v1 * v->c2[3];
            amp += (tgt - amp) * 0.01f;
            x = (b1 + 0.5f * b2 + 0.45f * l1) * amp;
            padl[j] += x * gl;                   /* to the pad's bus: smoothed and chorused below */
            padr[j] += x * gr;
        }
        v->ic[0] = fm_flush(s0);
        v->ic[1] = fm_flush(s1);
        v->ic[2] = fm_flush(s2);
        v->ic[3] = fm_flush(s3);
        v->amp = amp;
        v->ph[0] = p0;
        v->ph[1] = p1;
        en_pad_note[i] = (uint8_t)v->note;
        en_pad_amp[i] = amp;
        lvl = fm_maxf(lvl, amp);
    }
    en_pad_level = lvl;
    pad_bus(n);
}

static void render_bass(uint32_t n)
{
    uint32_t j;
    const float g = bass_vol * 0.42f, tgt = en_playing && !bass.rest ? 1.0f : 0.0f,
                rate = tgt > 0.0f ? 1.0f / (2.5f * EN_SR) : 1.0f / (4.0f * EN_SR);
    float ph = bass.ph, amp = bass.amp, inc;
    if (amp < 1e-5f && !en_playing) {
        bass.amp = 0.0f;
        en_bass_level = 0.0f;
        return;
    }
    bass.note += (bass.target - bass.note) * fm_minf((float)n * glide_k, 1.0f);   /* Glide */
    inc = mtof(bass.note) * (1.0f / EN_SR);
    for (j = 0; j < n; j++) {
        float x = sine(ph) * 0.75f + sine(ph * 0.5f + 0.25f) * 0.45f + sine(ph * 2.0f) * 0.12f;
        ph += inc;
        if (ph >= 2.0f)                          /* two turns: the sub-octave's period */
            ph -= 2.0f;
        amp += (tgt - amp) * rate;
        x *= amp * g;
        mixl[j] += x;
        mixr[j] += x;
        rsend[j] += x * 0.08f;
    }
    bass.ph = ph;
    bass.amp = amp;
    en_bass_level = amp;
}

static inline void bowl_step(bowl_ring_t *ring, float *left, float *right)
{
    float mid = 0.0f, side = 0.0f;
    for (int b = 0; b < ring->count; b += BOWL_MODES) {
        for (int k = 0; k < BOWL_MODES; k++) {
            bowl_partial_t *p = &ring->partial[b + k];
            uint32_t i = p->phase >> 22;         /* top ten bits index the shared sine table */
            float frac = (float)(p->phase & 0x3fffffu) * (1.0f / 4194304.0f);
            float x = (sint[i] + frac * (sint[i + 1] - sint[i])) * p->amp;
            p->phase += p->step;
            p->amp *= bowl_decay[k];
            mid += x;
            side += x * p->side;
        }
    }
    ring->attack += (1.0f - ring->attack) * (1.0f / (0.038f * EN_SR));
    ring->fade = fm_maxf(0.0f, ring->fade - ring->fade_step);
    float gain = ring->attack * ring->fade;
    *left += (mid - side) * gain;
    *right += (mid + side) * gain;
}

static void bowls_schedule(uint32_t n)
{
    if (!en_playing || bowls_vol == 0.0f) {
        bowls.until_strike = 0;
        return;
    }

    int mode = par[P_BOWLS_MODE];
    int pitch_class = mode == 3 && chord_nt > 0 ? chord_pcs[0] % 12 : par[P_ROOT];
    int root = 36 + pitch_class + 12 * par[P_BOWLS_OCT];
    int interval = mode == 1 ? 12 : mode == 2 ? 7 : 0;
    int tune = par[P_TUNE];
    int changed = root != bowls.root || interval != bowls.interval || tune != bowls.tune;
    if (bowls.until_strike == 0 || changed) {
        int next = 1 - bowls.current;
        bowl_ring_t *ring = &bowls.ring[next];
        /* Leave the last pitch ringing. Damp the older strike if its slot is needed. */
        if (ring->count != 0) {
            ring->fade_step = 1.0f / (0.04f * EN_SR);
            return;
        }
        /* Crossfade the outgoing resonance at its original pitch. */
        float fade_seconds = changed ? 2.5f : 1.2f;
        bowls.ring[bowls.current].fade_step = 1.0f / (fade_seconds * EN_SR);
        bowl_strike(ring, root, interval);
        bowls.current = next;
        bowls.root = root;
        bowls.interval = interval;
        bowls.tune = tune;
        bowls.until_strike = BOWLS_STRIKE_INTERVAL;
    }
    bowls.until_strike = bowls.until_strike > n ? bowls.until_strike - n : 0;
}

static void render_bowls(uint32_t n)
{
    bowls_schedule(n);
    int ringing = bowls.ring[0].count || bowls.ring[1].count;
    if (!ringing && bowls.cloud_left == 0) {
        en_bowls_level = 0.0f;
        return;
    }

    float blur_goal = (float)par[P_BOWLS_BLUR] * 0.01f;
    bowls.blur += (blur_goal - bowls.blur) * fm_minf((float)n / (0.04f * EN_SR), 1.0f);
    float blur = bowls.blur;
    int feed_cloud = blur > 0.001f;
    if (ringing && feed_cloud)
        bowls.cloud_left = (uint32_t)(5.0f * EN_SR);
    else
        bowls.cloud_left = bowls.cloud_left > n ? bowls.cloud_left - n : 0;
    int diffuse = bowls.cloud_left > 0;

    float fc = 11000.0f * fm_exp2f(-3.3f * blur);
    float c = fm_clampf(1.0f - fm_expf(-FM_TWO_PI * fc * (1.0f / EN_SR)), 0.01f, 0.99f);
    float dry_w = 1.0f - 0.22f * blur;
    float fb_gain = 0.15f + 0.50f * blur;
    float wet_w = 0.44f * blur;
    const float g1 = 0.55f, g2 = 0.50f;
    float level = bowls.level;

    for (uint32_t j = 0; j < n; j++) {
        float raw_l = 0.0f, raw_r = 0.0f;
        if (bowls.ring[0].count)
            bowl_step(&bowls.ring[0], &raw_l, &raw_r);
        if (bowls.ring[1].count)
            bowl_step(&bowls.ring[1], &raw_l, &raw_r);

        /* 2-pole acoustic smoothing filter */
        bowls_lp1_l = fm_flush(bowls_lp1_l + c * (raw_l - bowls_lp1_l));
        bowls_lp2_l = fm_flush(bowls_lp2_l + c * (bowls_lp1_l - bowls_lp2_l));
        bowls_lp1_r = fm_flush(bowls_lp1_r + c * (raw_r - bowls_lp1_r));
        bowls_lp2_r = fm_flush(bowls_lp2_r + c * (bowls_lp1_r - bowls_lp2_r));

        float filtered_l = bowls_lp2_l;
        float filtered_r = bowls_lp2_r;

        float out_l = filtered_l * dry_w, out_r = filtered_r * dry_w;
        if (diffuse) {
            /* Cross-feedback from opposite channel */
            float in_l = cloud_fb_r + (feed_cloud ? filtered_l : 0.0f);
            float in_r = cloud_fb_l + (feed_cloud ? filtered_r : 0.0f);

            /* Left branch: Allpass 1 (51ms) -> Allpass 2 (101ms) -> Delay (163ms) */
            float y_l = cloud_ap_step(cloud_ptrs_l[0], &cloud_idx_l[0], CLOUD_LEN_L[0], in_l, g1);
            y_l = cloud_ap_step(cloud_ptrs_l[1], &cloud_idx_l[1], CLOUD_LEN_L[1], y_l, g2);
            float d_l = cloud_dl_step(cloud_ptrs_l[2], &cloud_idx_l[2], CLOUD_LEN_L[2], y_l);

            /* Right branch: Allpass 1 (57ms) -> Allpass 2 (109ms) -> Delay (178ms) */
            float y_r = cloud_ap_step(cloud_ptrs_r[0], &cloud_idx_r[0], CLOUD_LEN_R[0], in_r, g1);
            y_r = cloud_ap_step(cloud_ptrs_r[1], &cloud_idx_r[1], CLOUD_LEN_R[1], y_r, g2);
            float d_r = cloud_dl_step(cloud_ptrs_r[2], &cloud_idx_r[2], CLOUD_LEN_R[2], y_r);

            /* Damping lowpass (~3.5 kHz) & highpass */
            cloud_damp_l = fm_flush(cloud_damp_l + 0.35f * (d_l - cloud_damp_l));
            cloud_hp_l = fm_flush(cloud_hp_l + 0.005f * (cloud_damp_l - cloud_hp_l));
            cloud_fb_l = fm_flush((cloud_damp_l - cloud_hp_l) * fb_gain);

            cloud_damp_r = fm_flush(cloud_damp_r + 0.35f * (d_r - cloud_damp_r));
            cloud_hp_r = fm_flush(cloud_hp_r + 0.005f * (cloud_damp_r - cloud_hp_r));
            cloud_fb_r = fm_flush((cloud_damp_r - cloud_hp_r) * fb_gain);

            out_l += (y_l * 0.45f + d_l * 0.55f) * wet_w;
            out_r += (y_r * 0.45f + d_r * 0.55f) * wet_w;
        }

        level += (bowls_vol - level) * (1.0f / (0.03f * EN_SR));
        float gain = level * 0.68f;
        out_l *= gain;
        out_r *= gain;
        mixl[j] += out_l;
        mixr[j] += out_r;
        rsend[j] += (out_l + out_r) * 0.06f;
    }

    bowls.level = level;
    float activity = 0.0f;
    for (int b = 0; b < 2; b++) {
        bowl_ring_t *ring = &bowls.ring[b];
        if (ring->count == 0)
            continue;
        float body = ring->partial[0].amp;
        if (body < 1e-4f || ring->fade == 0.0f || (bowls_vol == 0.0f && level < 1e-4f))
            ring->count = 0;
        else
            activity += body * ring->attack * ring->fade;
    }
    en_bowls_level = fm_minf(activity / bowl_modes[0].weight, 1.0f);
}


static void render_bells(uint32_t n)
{
    int i;
    const float g = bell_vol * 0.62f, arate = 0.0004f + 0.0056f * fm_minf(btone * 2.0f, 1.0f);
    for (i = 0; i < NBELL; i++) {
        bell_t *v = &bell[i];
        float cph = v->cph, mph = v->mph, amp = v->amp, idx = v->idx, att = v->att;
        uint32_t j = 0;
        if (!v->on)
            continue;
        if (v->wait > 0) {
            if ((uint32_t)v->wait >= n) {
                v->wait -= (int32_t)n;
                continue;
            }
            j = (uint32_t)v->wait;
            v->wait = 0;
        }
        for (; j < n; j++) {
            float m = sine(mph) * idx, x;
            float p = cph + m + 4.0f;            /* + 4 turns: positive for the table */
            x = sine(p) * amp * att * g;
            mixl[j] += x * v->gl;
            mixr[j] += x * v->gr;
            rsend[j] += x * 0.7f;
            esend[j] += x * 0.85f;
            cph += v->cinc;
            mph += v->minc;
            if (cph >= 1.0f)
                cph -= 1.0f;
            if (mph >= 1.0f)
                mph -= 1.0f;
            amp *= v->adec;
            idx *= v->idec;
            att += (1.0f - att) * arate;         /* the strike: ~4 ms, or soft as a slowed piano (~50 ms) */
        }
        v->cph = cph;
        v->mph = mph;
        v->amp = amp;
        v->idx = idx;
        v->att = att;
        if (amp < 0.0008f) {
            v->on = 0;
            midi(0x81, v->note, 0);
        }
    }
}

/* a hat: the metal and noise, high-passed (Chamberlin SVF, ~4 to 10 kHz by Hats); no pitch, little echo */
static void render_hat(drum_t *v, uint32_t n, float g, float pl, float pr)
{
    uint32_t j;
    int k;
    float x1, x2, amp, att;
    if (!v->on)
        return;
    x1 = v->x1, x2 = v->x2, amp = v->amp, att = v->att;
    for (j = 0; j < n; j++) {
        float in = 0.0f, hp, x;
        for (k = 0; k < 4; k++) {
            v->mph[k] += HAT_INC[k];
            if (v->mph[k] >= 1.0f)
                v->mph[k] -= 1.0f;
            in += v->mph[k] < 0.5f ? 0.25f : -0.25f;
        }
        noise = noise * 1664525u + 1013904223u;
        in = 0.5f * in + 0.6f * (float)(int32_t)noise * (1.0f / 2147483648.0f);
        hp = in - x2 - 0.8f * x1;
        x1 += hat_f * hp;
        x2 += hat_f * x1;
        x = hp * amp * att * g;
        mixl[j] += x * pl;
        mixr[j] += x * pr;
        rsend[j] += x * 0.2f;
        esend[j] += x * 0.08f;
        amp *= v->adec;
        att += (1.0f - att) * 0.3f;
    }
    v->x1 = fm_flush(x1);
    v->x2 = fm_flush(x2);
    v->amp = amp;
    v->att = att;
    v->on = amp > 0.0005f;
}

static void render_drums(uint32_t n)
{
    uint32_t j;
    const float g = pulse_vol * 1.1f;
    drum_t *v;
    v = &drum[0];                                /* thump: a sine falling to its note */
    if (v->on) {
        float ph = v->ph, inc = v->inc, amp = v->amp, att = v->att;
        for (j = 0; j < n; j++) {
            float x = sine(ph) * amp * att * g;
            mixl[j] += x;
            mixr[j] += x;
            rsend[j] += x * 0.1f;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            inc += (v->inc_end - inc) * 0.0012f;
            amp *= v->adec;
            att += (1.0f - att) * 0.02f;
        }
        v->ph = ph;
        v->inc = inc;
        v->amp = amp;
        v->att = att;
        v->on = amp > 0.0005f;
    }
    render_hat(&drum[1], n, g * 0.2f, 0.6f, 1.0f);    /* closed: a little right */
    render_hat(&drum[2], n, g * 0.14f, 1.0f, 0.7f);   /* open: a little left */
}

/* ------------------------------------------------------------- texture --- */
static uint32_t text_rng = 0x58A3B291u;
static inline uint32_t text_rnd(void)
{
    uint32_t x = text_rng;
    x ^= x << 13;
    x ^= x >> 17;
    text_rng = x ^ (x << 5);
    return text_rng;
}
static inline float text_frand(void)
{
    return (float)(text_rnd() >> 8) * (1.0f / 16777216.0f);
}

/* Rain: a diffuse bed under many overlapping, mid-heavy water splashes. */
#define RAIN_DROPS 12
static struct {
    float tail, env, band, low;
    float cutoff, damping, attack, decay, pan;
} rain_drop[RAIN_DROPS];
static float rain_weather = 1.0f, rain_weather_target = 1.0f;
static uint32_t rain_weather_left;
static float rain_hp_l, rain_hp_r;
static float rain_hp2_l, rain_hp2_r;
static float rain_body_band_l, rain_body_band_r, rain_body_low_l, rain_body_low_r;
static float rain_lp1_l, rain_lp1_r;
static float rain_lp2_l, rain_lp2_r;
static float rain_out_lp_l, rain_out_lp_r;

/* Wind state: dual resonant wandering bandpass with flow speed & intensity scaling */
static float wind_bp_l[2], wind_lp_l[2];
static float wind_bp_r[2], wind_lp_r[2];
static float wind_gust[2] = {0.3f, 0.3f}, wind_gust_tgt[2] = {0.3f, 0.3f};
static uint32_t wind_gust_time;

/* Vinyl state */
static float vinyl_rumble1, vinyl_rumble2;
static float vinyl_hiss_hp, vinyl_hiss_lp;
static float vinyl_click_env, vinyl_click_amp, vinyl_click_pan;

/* Ocean state: natural shoreline waves (centered mid-image, non-resonant gentle RC filters) */
static float ocean_brown_l, ocean_brown_r;
static float ocean_brown_dc_l, ocean_brown_dc_r;
static float ocean_body_lp_l, ocean_body_lp_r;
static float ocean_foam_lp_l, ocean_foam_lp_r;
static float ocean_foam_hp_l, ocean_foam_hp_r;
static float ocean_sand_lp_l, ocean_sand_lp_r;
static float ocean_phase;
static float ocean_period;
static float ocean_amp = 0.9f, ocean_amp_tgt = 0.9f;

/* Train state: clean deep night passenger train wheel clatter on rail joints */
static float train_bp0_l, train_lp0_l, train_bp0_r, train_lp0_r;
static float train_bp1_l, train_lp1_l, train_bp1_r, train_lp1_r;
static float train_bp2_l, train_lp2_l, train_bp2_r, train_lp2_r;
static float train_bp3_l, train_lp3_l, train_bp3_r, train_lp3_r;
static float train_clat_lp1_l, train_clat_lp1_r, train_clat_lp2_l, train_clat_lp2_r;
static float train_env[4];
static float train_dc_l, train_dc_r;
static float train_phase;

static void render_texture(uint32_t n)
{
    if (text_vol <= 0.00005f)
        return;

    uint32_t j;
    const float vol = text_vol;
    const float tone = text_tone;
    const float dens = text_dens;

    switch (text_type) {
    case 0: { /* Dense, irregular water patter over a quieter outdoor wash. */
        const float hp_fc = 280.0f * (FM_TWO_PI / EN_SR);
        const float body_fc = (600.0f + 400.0f * tone) * (FM_TWO_PI / EN_SR);
        float window_fc = (2200.0f + 3400.0f * tone) * (FM_TWO_PI / EN_SR);
        window_fc /= 1.0f + window_fc;
        const float p_drop = (16.0f + 95.0f * dens * dens) / EN_SR;
        const float bed_gain = (0.25f + 0.15f * dens) * vol;
        const float drop_gain = (0.75f + 0.25f * dens) * vol;
        const float air_fc = 0.44f + 0.32f * tone;

        for (j = 0; j < n; j++) {
            /* Irregular subsecond/second changes keep the wash from becoming steady hiss. */
            if (!rain_weather_left) {
                rain_weather_target = 0.60f + 0.80f * text_frand();
                rain_weather_left = (uint32_t)((0.30f + 1.80f * text_frand()) * EN_SR);
            }
            rain_weather_left--;
            rain_weather += (1.0f / (0.18f * EN_SR)) * (rain_weather_target - rain_weather);

            float wl = text_frand() * 2.0f - 1.0f;
            float wr = text_frand() * 2.0f - 1.0f;
            /* Mostly independent sides; a small shared component joins the scene. */
            float bed_l = wl + 0.15f * wr;
            float bed_r = wr + 0.15f * wl;
            rain_hp_l += hp_fc * (bed_l - rain_hp_l); bed_l -= rain_hp_l;
            rain_hp_r += hp_fc * (bed_r - rain_hp_r); bed_r -= rain_hp_r;
            rain_hp2_l += hp_fc * (bed_l - rain_hp2_l); bed_l -= rain_hp2_l;
            rain_hp2_r += hp_fc * (bed_r - rain_hp2_r); bed_r -= rain_hp2_r;
            rain_body_band_l += body_fc * (bed_l - rain_body_low_l - 1.10f * rain_body_band_l);
            rain_body_band_r += body_fc * (bed_r - rain_body_low_r - 1.10f * rain_body_band_r);
            rain_body_low_l += body_fc * rain_body_band_l;
            rain_body_low_r += body_fc * rain_body_band_r;
            rain_lp1_l += window_fc * (bed_l - rain_lp1_l);
            rain_lp1_r += window_fc * (bed_r - rain_lp1_r);
            rain_lp2_l += window_fc * (rain_lp1_l - rain_lp2_l);
            rain_lp2_r += window_fc * (rain_lp1_r - rain_lp2_r);
            bed_l = 0.79f * rain_body_band_l + 0.20f * rain_lp2_l;
            bed_r = 0.79f * rain_body_band_r + 0.20f * rain_lp2_r;

            if (text_frand() < p_drop * rain_weather) {
                int k = 0;
                for (int d = 1; d < RAIN_DROPS; d++)
                    if (rain_drop[d].tail < rain_drop[k].tail) k = d;
                float size = text_frand();
                size *= size;
                /* Noise excited, broadly damped bodies: no pitched metal oscillators. */
                rain_drop[k].cutoff = (600.0f + 600.0f * (1.0f - size)) *
                                      (0.80f + 0.40f * tone) * (FM_TWO_PI / EN_SR);
                rain_drop[k].damping = 0.75f + 0.10f * size;
                rain_drop[k].attack = 1.0f / ((0.0006f + 0.0018f * size) * EN_SR);
                rain_drop[k].decay = 1.0f - 1.0f / ((0.006f + 0.028f * size) * EN_SR);
                rain_drop[k].tail = (0.08f + 1.40f * size) * (0.80f + 0.20f * rain_weather);
                float pan = text_frand();
                rain_drop[k].pan = pan < 0.5f ? 0.05f + 0.40f * pan : 0.75f + 0.40f * (pan - 0.5f);
            }

            float patter_l = 0.0f, patter_r = 0.0f;
            for (int d = 0; d < RAIN_DROPS; d++) {
                if (rain_drop[d].tail < 0.00001f && rain_drop[d].env < 0.00001f)
                    continue;
                rain_drop[d].tail *= rain_drop[d].decay;
                rain_drop[d].env += rain_drop[d].attack * (rain_drop[d].tail - rain_drop[d].env);
                float noise = (text_frand() * 2.0f - 1.0f) * rain_drop[d].env;
                rain_drop[d].band += rain_drop[d].cutoff *
                                    (noise - rain_drop[d].low - rain_drop[d].damping * rain_drop[d].band);
                rain_drop[d].low += rain_drop[d].cutoff * rain_drop[d].band;
                float splash = rain_drop[d].band * rain_drop[d].damping;
                patter_l += splash * (1.0f - rain_drop[d].pan);
                patter_r += splash * rain_drop[d].pan;
            }

            rain_out_lp_l += air_fc * (bed_l * bed_gain * rain_weather + patter_l * drop_gain - rain_out_lp_l);
            rain_out_lp_r += air_fc * (bed_r * bed_gain * rain_weather + patter_r * drop_gain - rain_out_lp_r);
            float sl = rain_out_lp_l;
            float sr = rain_out_lp_r;
            mixl[j] += sl;
            mixr[j] += sr;
            rsend[j] += (sl + sr) * 0.025f;
        }
        break;
    }

    case 1: { /* Wind: dual resonant wandering bandpass filters with flow speed & intensity scaling */
        float base_f1 = 0.015f + 0.035f * tone;
        float base_f2 = 0.050f + 0.080f * tone;

        /* Density scales flow speed (gust frequency & rise rate) and airflow intensity */
        float min_sec = 6.0f - 4.2f * dens;  /* 6.0s down to 1.8s */
        float span_sec = 4.0f - 2.5f * dens; /* 4.0s down to 1.5s */
        float gust_speed = 0.00002f + 0.00006f * dens;
        float intensity = (0.45f + 0.55f * dens) * vol;
        float gust_scale = 0.40f + 0.60f * dens;

        for (j = 0; j < n; j++) {
            if (wind_gust_time <= 1u) {
                wind_gust_time = (uint32_t)((min_sec + span_sec * text_frand()) * EN_SR);
                wind_gust_tgt[0] = text_frand() * gust_scale;
                wind_gust_tgt[1] = text_frand() * gust_scale;
            } else {
                wind_gust_time--;
            }

            wind_gust[0] += gust_speed * (wind_gust_tgt[0] - wind_gust[0]);
            wind_gust[1] += gust_speed * (wind_gust_tgt[1] - wind_gust[1]);

            float g0 = wind_gust[0] + 0.25f * fm_maxf(arcm, 0.0f);
            float g1 = wind_gust[1] + 0.25f * fm_maxf(arcm, 0.0f);

            float f1 = base_f1 + 0.02f * g0;
            float f2 = base_f2 + 0.04f * g1;

            float w_l = text_frand() * 2.0f - 1.0f;
            float w_r = text_frand() * 2.0f - 1.0f;

            /* Resonator 1 */
            float hp1_l = w_l - wind_lp_l[0] - 0.08f * wind_bp_l[0];
            wind_bp_l[0] += f1 * hp1_l;
            wind_lp_l[0] += f1 * wind_bp_l[0];

            float hp1_r = w_r - wind_lp_r[0] - 0.08f * wind_bp_r[0];
            wind_bp_r[0] += f1 * hp1_r;
            wind_lp_r[0] += f1 * wind_bp_r[0];

            /* Resonator 2 */
            float hp2_l = w_l - wind_lp_l[1] - 0.06f * wind_bp_l[1];
            wind_bp_l[1] += f2 * hp2_l;
            wind_lp_l[1] += f2 * wind_bp_l[1];

            float hp2_r = w_r - wind_lp_r[1] - 0.06f * wind_bp_r[1];
            wind_bp_r[1] += f2 * hp2_r;
            wind_lp_r[1] += f2 * wind_bp_r[1];

            float sig_l = (wind_bp_l[0] * 0.45f * (0.3f + 0.7f * g0) + wind_bp_l[1] * 0.35f * (0.2f + 0.8f * g1)) * intensity;
            float sig_r = (wind_bp_r[0] * 0.45f * (0.3f + 0.7f * g0) + wind_bp_r[1] * 0.35f * (0.2f + 0.8f * g1)) * intensity;

            mixl[j] += sig_l;
            mixr[j] += sig_r;
            rsend[j] += (sig_l + sig_r) * 0.15f;
        }
        break;
    }

    case 2: { /* Vinyl: sub-bass turntable rumble + surface hiss + Poisson dust crackle */
        float p_click = 0.0001f + 0.0016f * dens;
        float hiss_k = 0.08f + 0.28f * tone;
        for (j = 0; j < n; j++) {
            float w = text_frand() * 2.0f - 1.0f;
            vinyl_rumble1 += 0.006f * (w - vinyl_rumble1);
            vinyl_rumble2 += 0.006f * (vinyl_rumble1 - vinyl_rumble2);
            float rumble = vinyl_rumble2 * 1.8f;

            vinyl_hiss_hp += 0.12f * (w - vinyl_hiss_hp);
            float h_in = w - vinyl_hiss_hp;
            vinyl_hiss_lp += hiss_k * (h_in - vinyl_hiss_lp);
            float hiss = vinyl_hiss_lp * (0.04f + 0.06f * tone);

            if (text_frand() < p_click && vinyl_click_env < 0.05f) {
                float r = text_frand();
                vinyl_click_amp = (r * r * r) * (text_frand() > 0.5f ? 0.6f : -0.6f) * (0.5f + 0.5f * tone);
                vinyl_click_pan = text_frand();
                vinyl_click_env = 1.0f;
            }
            float click = vinyl_click_env * vinyl_click_amp;
            vinyl_click_env *= 0.88f;

            float sl = (rumble * 0.7f + hiss * 0.8f + click * (1.0f - vinyl_click_pan)) * vol;
            float sr = (rumble * 0.7f + hiss * 0.8f + click * vinyl_click_pan) * vol;

            mixl[j] += sl;
            mixr[j] += sr;
            rsend[j] += (sl + sr) * 0.08f;
        }
        break;
    }

    case 3: { /* Ocean: rolling shoreline waves centered in front of listener (no resonant bandpasses) */
        float base_period = 11.0f - 6.5f * dens; /* dens=0 -> 11s, dens=0.5 -> 7.75s, dens=1 -> 4.5s */
        float fc_crash = 0.10f + 0.22f * tone;
        float fc_sand = 0.20f + 0.25f * tone;
        float crash_k = (0.15f + 0.25f * tone);
        float wash_k = (0.04f + 0.08f * tone);

        if (ocean_period < 1.0f)
            ocean_period = base_period;

        float phase_inc = 1.0f / (ocean_period * (float)EN_SR);

        for (j = 0; j < n; j++) {
            ocean_phase += phase_inc;
            if (ocean_phase >= 1.0f) {
                ocean_phase -= 1.0f;
                ocean_period = base_period * (0.85f + 0.30f * text_frand());
                phase_inc = 1.0f / (ocean_period * (float)EN_SR);
                ocean_amp_tgt = 0.75f + 0.45f * text_frand();
            }

            ocean_amp += 0.00008f * (ocean_amp_tgt - ocean_amp);

            /* Realistic asymmetric wave profile:
             * 0.00..0.40: Swell builds up in front (накат волны)
             * 0.40..0.52: Crest break & shore crash (удар о берег)
             * 0.52..0.82: Wash & undertow (откат по песку, шипение пены)
             * 0.82..1.00: Quiet ocean horizon lull */
            float p = ocean_phase;
            float swell_env, crash_env, wash_env;
            if (p < 0.40f) {
                float r = p * (1.0f / 0.40f);
                float r2 = r * r;
                swell_env = 0.20f + 0.60f * r2;
                crash_env = 0.02f + 0.25f * (r2 * r);
                wash_env = 0.03f;
            } else if (p < 0.52f) {
                float r = (p - 0.40f) * (1.0f / 0.12f);
                float s = r * (2.0f - r);
                swell_env = 0.80f + 0.20f * s;
                crash_env = 0.27f + 0.73f * s;
                wash_env = 0.03f + 0.40f * r;
            } else if (p < 0.82f) {
                float r = (p - 0.52f) * (1.0f / 0.30f);
                float u = 1.0f - r;
                float u2 = u * u;
                swell_env = 0.20f + 0.80f * (u2 * u);
                crash_env = u2 * u2;
                wash_env = 0.03f + 0.97f * u2;
            } else {
                swell_env = 0.20f;
                crash_env = 0.02f;
                wash_env = 0.03f;
            }

            swell_env *= ocean_amp;
            crash_env *= ocean_amp;
            wash_env *= ocean_amp;

            /* 85% mid (centered in front of listener), 15% side ambient decorrelation */
            float w_mid = text_frand() * 2.0f - 1.0f;
            float w_side_l = text_frand() * 2.0f - 1.0f;
            float w_side_r = text_frand() * 2.0f - 1.0f;

            float wl = w_mid * 0.85f + w_side_l * 0.15f;
            float wr = w_mid * 0.85f + w_side_r * 0.15f;

            /* 1. Velvety Brown body (1/f^2 integrated water mass) */
            ocean_brown_l = ocean_brown_l * 0.994f + wl * 0.065f;
            ocean_brown_r = ocean_brown_r * 0.994f + wr * 0.065f;

            ocean_brown_dc_l += 0.001f * (ocean_brown_l - ocean_brown_dc_l);
            float bl = ocean_brown_l - ocean_brown_dc_l;
            ocean_brown_dc_r += 0.001f * (ocean_brown_r - ocean_brown_dc_r);
            float br = ocean_brown_r - ocean_brown_dc_r;

            /* Smooth 1-pole lowpass (non-resonant) opening during swell */
            float fc_body = 0.020f + 0.035f * swell_env;
            ocean_body_lp_l += fc_body * (bl - ocean_body_lp_l);
            ocean_body_lp_r += fc_body * (br - ocean_body_lp_r);

            float body_sig_l = (0.5f * bl + 0.5f * ocean_body_lp_l) * swell_env * 0.70f;
            float body_sig_r = (0.5f * br + 0.5f * ocean_body_lp_r) * swell_env * 0.70f;

            /* 2. Crash / Water rush: gentle non-resonant 1-pole band (~280Hz to ~1.2-2.5kHz) */
            ocean_foam_lp_l += fc_crash * (wl - ocean_foam_lp_l);
            ocean_foam_lp_r += fc_crash * (wr - ocean_foam_lp_r);

            ocean_foam_hp_l += 0.04f * (ocean_foam_lp_l - ocean_foam_hp_l);
            ocean_foam_hp_r += 0.04f * (ocean_foam_lp_r - ocean_foam_hp_r);

            float crash_sig_l = (ocean_foam_lp_l - ocean_foam_hp_l) * (crash_k * crash_env);
            float crash_sig_r = (ocean_foam_lp_r - ocean_foam_hp_r) * (crash_k * crash_env);

            /* 3. Foam fizz & sand wash: gentle 1-pole highpass (bubbles on wet sand) */
            float w_fizz_l = w_mid * 0.65f + w_side_l * 0.35f;
            float w_fizz_r = w_mid * 0.65f + w_side_r * 0.35f;

            ocean_sand_lp_l += fc_sand * (w_fizz_l - ocean_sand_lp_l);
            ocean_sand_lp_r += fc_sand * (w_fizz_r - ocean_sand_lp_r);

            float wash_sig_l = (w_fizz_l - ocean_sand_lp_l) * (wash_k * wash_env);
            float wash_sig_r = (w_fizz_r - ocean_sand_lp_r) * (wash_k * wash_env);

            float sl = (body_sig_l + crash_sig_l + wash_sig_l) * vol;
            float sr = (body_sig_r + crash_sig_r + wash_sig_r) * vol;

            mixl[j] += sl;
            mixr[j] += sr;
            rsend[j] += (sl + sr) * 0.14f;
        }
        break;
    }

    case 4: { /* Train: clean deep rail joint wheel clatter (pure rhythmic night train, no background hiss) */
        float cycle_secs = 2.4f - 1.2f * dens;
        float train_inc = 1.0f / (cycle_secs * (float)EN_SR);

        /* Deep mass resonances (Hz) - low, heavy, dark iron */
        float fc0 = (70.0f + 25.0f * dens) * (FM_TWO_PI / (float)EN_SR);   /* Sub bogie shock 70-95 Hz */
        float fc1 = (140.0f + 50.0f * tone) * (FM_TWO_PI / (float)EN_SR);  /* Cavity/floor 140-190 Hz */
        float fc2 = (280.0f + 100.0f * tone) * (FM_TWO_PI / (float)EN_SR); /* Cast iron wheel/rail 280-380 Hz */
        float fc3 = (480.0f + 200.0f * tone) * (FM_TWO_PI / (float)EN_SR); /* Joint contact tick 480-680 Hz */

        /* Steep 2-pole lowpass (450-800 Hz) to keep hits dark, warm, and solid */
        float fc_clat_lp = (450.0f + 350.0f * tone) * (FM_TWO_PI / (float)EN_SR);

        const float decay = 1.0f - (1.0f / (0.012f * (float)EN_SR));

        for (j = 0; j < n; j++) {
            float old_p = train_phase;
            train_phase += train_inc;
            if (train_phase >= 1.0f) {
                train_phase -= 1.0f;
                train_env[0] = 1.0f;
            } else if (old_p < 0.095f && train_phase >= 0.095f) {
                train_env[1] = 1.0f;
            } else if (old_p < 0.540f && train_phase >= 0.540f) {
                train_env[2] = 1.0f;
            } else if (old_p < 0.635f && train_phase >= 0.635f) {
                train_env[3] = 1.0f;
            }

            float exc_l = 0.0f, exc_r = 0.0f;
            static const float HIT_WEIGHTS[4] = {1.00f, 0.75f, 0.90f, 0.70f};
            static const float HIT_PANS[4] = {0.38f, 0.42f, 0.58f, 0.62f};

            for (int k = 0; k < 4; k++) {
                if (train_env[k] > 0.0005f) {
                    float noise = (text_frand() * 2.0f - 1.0f) * train_env[k] * HIT_WEIGHTS[k];
                    float pan = HIT_PANS[k];
                    exc_l += noise * (1.0f - pan) * 2.0f;
                    exc_r += noise * pan * 2.0f;
                    train_env[k] *= decay;
                }
            }

            /* 1. Mode 0: Sub bogie shock (70-95 Hz, Q=2.2) */
            float hp0_l = exc_l - train_lp0_l - 0.45f * train_bp0_l;
            train_bp0_l += fc0 * hp0_l; train_lp0_l += fc0 * train_bp0_l;
            float hp0_r = exc_r - train_lp0_r - 0.45f * train_bp0_r;
            train_bp0_r += fc0 * hp0_r; train_lp0_r += fc0 * train_bp0_r;

            /* 2. Mode 1: Carriage floor hollow resonance (140-190 Hz, Q=3.0) */
            float hp1_l = exc_l - train_lp1_l - 0.33f * train_bp1_l;
            train_bp1_l += fc1 * hp1_l; train_lp1_l += fc1 * train_bp1_l;
            float hp1_r = exc_r - train_lp1_r - 0.33f * train_bp1_r;
            train_bp1_r += fc1 * hp1_r; train_lp1_r += fc1 * train_bp1_r;

            /* 3. Mode 2: Heavy cast iron wheel/rail (280-380 Hz, Q=4.5) */
            float hp2_l = exc_l - train_lp2_l - 0.22f * train_bp2_l;
            train_bp2_l += fc2 * hp2_l; train_lp2_l += fc2 * train_bp2_l;
            float hp2_r = exc_r - train_lp2_r - 0.22f * train_bp2_r;
            train_bp2_r += fc2 * hp2_r; train_lp2_r += fc2 * train_bp2_r;

            /* 4. Mode 3: Joint contact tick (480-680 Hz, Q=5.0) */
            float hp3_l = exc_l - train_lp3_l - 0.20f * train_bp3_l;
            train_bp3_l += fc3 * hp3_l; train_lp3_l += fc3 * train_bp3_l;
            float hp3_r = exc_r - train_lp3_r - 0.20f * train_bp3_r;
            train_bp3_r += fc3 * hp3_r; train_lp3_r += fc3 * train_bp3_r;

            /* Combine modes with heavy weight on deep low frequencies */
            float raw_hit_l = train_bp0_l * 1.6f + train_bp1_l * 1.3f + train_bp2_l * 0.9f + train_bp3_l * 0.4f;
            float raw_hit_r = train_bp0_r * 1.6f + train_bp1_r * 1.3f + train_bp2_r * 0.9f + train_bp3_r * 0.4f;

            /* 2-pole lowpass filter to keep sound warm, deep, and dark */
            train_clat_lp1_l += fc_clat_lp * (raw_hit_l - train_clat_lp1_l);
            train_clat_lp2_l += fc_clat_lp * (train_clat_lp1_l - train_clat_lp2_l);
            train_clat_lp1_r += fc_clat_lp * (raw_hit_r - train_clat_lp1_r);
            train_clat_lp2_r += fc_clat_lp * (train_clat_lp1_r - train_clat_lp2_r);

            float sig_l = train_clat_lp2_l * 3.8f;
            float sig_r = train_clat_lp2_r * 3.8f;

            /* 30 Hz DC block */
            train_dc_l += 0.004f * (sig_l - train_dc_l); sig_l -= train_dc_l;
            train_dc_r += 0.004f * (sig_r - train_dc_r); sig_r -= train_dc_r;

            float sl = sig_l * vol * 0.60f;
            float sr = sig_r * vol * 0.60f;

            mixl[j] += sl;
            mixr[j] += sr;
            rsend[j] += (sl + sr) * 0.08f;
        }
        break;
    }
    }
}

static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    const float rg = rev_send * rev_in_gain * (1.0f - 0.25f * arcm), eg = echo_send;
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = rsend[j] = esend[j] = solo_rsend[j] = 0.0f;
    render_pad(n);
    render_bass(n);
    render_bowls(n);
    render_bells(n);
    render_drums(n);
    solo_render(mixl, mixr, esend, solo_rsend, n);
    render_texture(n);
    float rwid = (float)solo_get(SP_REV_WID) * 0.01f;
    for (j = 0; j < n; j++) {
        float wl, wr, l, r, el, er, xl, xr;
        echoes(esend[j] * eg, &el, &er, &xl, &xr);
        reverb(((rsend[j] * rg + solo_rsend[j]) + (el + er + xl + xr) * 0.35f * rev_send) * 0.8f, &wl, &wr);
        if (rwid != 0.5f) {
            float rmid = 0.5f * (wl + wr);
            float rside = 0.5f * (wl - wr) * (0.2f + 1.6f * rwid);
            wl = rmid + rside;
            wr = rmid - rside;
        }
        l = (mixl[j] + 0.75f * el + 0.3f * xl + 1.3f * wl) * 0.36f * gain;
        r = (mixr[j] + 0.75f * er + 0.3f * xr + 1.3f * wr) * 0.36f * gain;
        out[2 * j] = (int32_t)(fm_tanhf(l) * 8300000.0f);
        out[2 * j + 1] = (int32_t)(fm_tanhf(r) * 8300000.0f);
    }
}

/* the arc: a slow tide, minutes long, between sparse (-1) and full (+1); Motion sets how far it goes */
static float arc, arc_tgt;
static uint32_t arc_left;

static void control(uint32_t n)                 /* once a chunk: the slow things */
{
    int i;
    mods_step(n);
    heads_step(n);
    if (arc_left <= n) {
        float r = frand() * 2.0f - 1.0f;
        arc_tgt = r * (0.6f + 0.4f * frand()) + (r > 0.0f ? 0.25f : -0.25f);   /* away from the middle */
        arc_left = (uint32_t)((50.0f + 100.0f * frand()) * EN_SR);
    } else {
        arc_left -= n;
    }
    if (en_playing)
        arc += (fm_clampf(arc_tgt, -1.0f, 1.0f) - arc) * (float)n * (1.0f / (35.0f * EN_SR));
    arcm = arc * motion;
    en_arc = arcm;
    pl_decay += ((en_frozen ? 0.9995f : pl_decay_t) - pl_decay) * 0.002f;
    pl_dd2 = fm_clampf(pl_decay + 0.15f, 0.25f, 0.5f);
    rev_in_gain += ((en_frozen ? 0.0f : 1.0f) - rev_in_gain) * 0.01f;
    if (en_playing && !en_frozen && !change_pending) {
        drift_left -= (float)n;
        if (!fifth_done && drift_left < 0.5f * drift_total) {
            fifth_done = 1;                      /* halfway: the bass may lean on the fifth */
            if (frand() < (float)par[P_FIFTH] * 0.01f && chord_nt >= 3)
                bass_to(chord_pcs[2]);
        }
        if (drift_left <= 0.0f)
            change_pending = 1;
    }
    en_drift = drift_total > 0.0f ? 1.0f - fm_clampf(drift_left / drift_total, 0.0f, 1.0f) : 0.0f;
    for (i = 0; i < EN_NLOOP; i++)
        en_loop_glow[i] *= 0.985f;
    for (i = 0; i < EN_NDRUM; i++)
        en_drum_glow[i] *= 0.95f;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    drain();
    if (en_ext && ++ext_age > 100u)
        en_ext = 0;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        int timed = en_playing && !en_ext;
        if (timed) {
            if (pulse_left <= 0.0f) {
                clock_pulse();
                pulse_left += step_len * (1.0f / 6.0f);
            }
            if ((float)k > pulse_left)
                k = (uint32_t)pulse_left + 1u;
        }
        if (k > n)
            k = n;
        {
            int32_t w = drums_wait();            /* a swung hit lands on its sample */
            if (w > 0 && (uint32_t)w < k)
                k = (uint32_t)w;
        }
        if (timed)
            pulse_left -= (float)k;
        control(k);
        render_chunk(out, k, gain);
        drums_wait_step((int32_t)k);
        out += 2 * k;
        n -= k;
    }
}
