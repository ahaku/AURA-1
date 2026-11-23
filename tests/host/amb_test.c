/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The instrument on the host: the pure parts (Euclid, scales, chords, voice leading, names) and the
 * render (sound, no NaN, bounded, the loops evolve, the chord drifts, freeze holds). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "amb.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

static uint32_t midi_count;
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)st;
    (void)d1;
    (void)d2;
    midi_count++;
}

static uint32_t pat(const char *s)
{
    uint32_t m = 0;
    int i;
    for (i = 0; s[i]; i++)
        if (s[i] == 'x')
            m |= 1u << i;
    return m;
}

static int popcount(uint32_t m)
{
    int c = 0;
    while (m) {
        c += (int)(m & 1u);
        m >>= 1;
    }
    return c;
}

static int32_t buf[2 * 256];
static float peak, sum2;
static int bad;
static void run(float secs)
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 512; j++) {
            float x = (float)buf[j] / 8388608.0f;
            if (x != x)
                bad++;
            if (fabsf(x) > peak)
                peak = fabsf(x);
            sum2 += x * x;
        }
    }
}

static void measure_partials(const float hz[3], float amp[3])
{
    float re[3] = {0}, im[3] = {0};
    const int blocks = 64;
    for (int block = 0; block < blocks; block++) {
        en_render(buf, 256, 2048);
        for (int j = 0; j < 256; j++) {
            float mono = (float)(buf[2 * j] + buf[2 * j + 1]) * (1.0f / 16777216.0f);
            float time = (float)(block * 256 + j) * (6.283185307f / EN_SR);
            for (int k = 0; k < 3; k++) {
                re[k] += mono * cosf(time * hz[k]);
                im[k] += mono * sinf(time * hz[k]);
            }
        }
    }
    for (int k = 0; k < 3; k++)
        amp[k] = 2.0f * sqrtf(re[k] * re[k] + im[k] * im[k]) / (float)(blocks * 256);
}

int main(void)
{
    int pcs[4], n, i;
    char name[16];

    /* Euclid: the classic patterns */
    CHECK(en_euclid(3, 8, 0) == pat("x..x..x."), "E(3,8) %x", en_euclid(3, 8, 0));
    CHECK(en_euclid(5, 8, 0) == pat("x.xx.xx."), "E(5,8) %x", en_euclid(5, 8, 0));
    CHECK(en_euclid(4, 16, 0) == pat("x...x...x...x..."), "E(4,16)");
    CHECK(en_euclid(0, 16, 0) == 0, "E(0,16)");
    CHECK(en_euclid(7, 7, 0) == 0x7Fu, "E(7,7)");
    for (i = 0; i <= 12; i++)
        CHECK(popcount(en_euclid(i, 12, 5)) == i, "E(%d,12) has %d hits", i, popcount(en_euclid(i, 12, 5)));
    CHECK(en_euclid(1, 7, 3) == 1u << 3, "rotation");

    /* scales and chords */
    {
        int st[12];
        CHECK(en_scale(EN_MODE_POOL[MODE_DORIAN], st) == 7 && st[2] == 3 && st[6] == 10, "Dorian steps");
        CHECK(en_scale(EN_MODE_POOL[MODE_PENT_MIN], st) == 5, "pentatonic minor has 5");
        CHECK(en_scale(EN_MODE_POOL[MODE_HIRAJOSHI], st) == 5, "Hirajoshi has 5 steps");
        CHECK(en_scale(EN_MODE_POOL[MODE_INSEN], st) == 5, "Insen has 5 steps");
        CHECK(en_scale(EN_MODE_POOL[MODE_WHOLE_TONE], st) == 6, "Whole Tone has 6 steps");
        CHECK(en_scale(EN_MODE_POOL[MODE_LYDIAN_DOM], st) == 7, "Lydian Dom has 7 steps");
        CHECK(!strcmp(EN_MODE_NAME[MODE_LYDIAN_DOM], "Lydian Dominant"), "Lydian Dominant name");
        CHECK(en_scale(0, st) == 1, "the root is always in");
    }
    n = en_chord_tones(EN_MODE_POOL[MODE_IONIAN], 0, 0, pcs);
    CHECK(n == 4 && pcs[0] == 0 && pcs[1] == 4 && pcs[2] == 7 && pcs[3] == 11, "C Ionian I = Cmaj7");
    CHECK(!strcmp(en_chord_name(pcs, n, name), "CM7"), "name %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 0, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "Dm7"), "D Dorian i: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 3, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "G7"), "D Dorian IV: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_PENT_MAJ], 0, 0, pcs);
    CHECK(n == 3, "pentatonic chords have three tones (%d)", n);
    n = en_chord_tones(1u | 1u << 4 | 1u << 7 | 1u << 9, 0, 0, pcs);   /* four notes: no doubled tones */
    CHECK(n == 3 && pcs[0] != pcs[1] && pcs[1] != pcs[2] && pcs[0] != pcs[2], "4-note pool: distinct");
    n = en_chord_tones(1u | 1u << 4 | 1u << 7, 0, 1, pcs);
    CHECK(n == 3, "3-note pool: all three (%d)", n);

    /* voice leading: small moves, in range, no unisons */
    {
        int from[4] = {50, 57, 62, 66}, to[4], cm7[4] = {0, 4, 7, 11}, f7[4] = {5, 9, 0, 4}, tot = 0;
        en_voice_lead(from, cm7, 4, to, 48, 77);
        for (i = 0; i < 4; i++) {
            CHECK(to[i] >= 48 && to[i] <= 77, "range %d", to[i]);
            tot += abs(to[i] - from[i]);
        }
        CHECK(tot <= 8, "Cmaj7 from D-ish: moved %d", tot);
        memcpy(from, to, sizeof from);
        en_voice_lead(from, f7, 4, to, 48, 77);
        tot = 0;
        for (i = 0; i < 4; i++)
            tot += abs(to[i] - from[i]);
        CHECK(tot <= 6, "CM7 -> FM7 moved %d", tot);
        CHECK(to[0] != to[1] && to[1] != to[2] && to[2] != to[3] && to[0] != to[3], "no unisons");
    }

    /* texts */
    {
        char t[12];
        en_param_text(P_DRIFT, 0, t);
        CHECK(!strcmp(t, "20s"), "drift 0: %s", t);
        en_param_text(P_DRIFT, 100, t);
        CHECK(!strcmp(t, "5:00"), "drift 100: %s", t);
        en_param_text(P_ETIME, 3, t);
        CHECK(!strcmp(t, "1 bar"), "etime: %s", t);
        en_param_text(P_BOCT, 1, t);
        CHECK(!strcmp(t, "+1"), "boct: %s", t);
    }

    /* the render: silent at rest, sound when growing, never NaN, bounded */
    en_init();
    run(1.0f);
    CHECK(peak == 0.0f, "silent before play (%f)", (double)peak);
    en_play(1);
    peak = sum2 = 0.0f;
    run(20.0f);
    CHECK(!bad, "NaN in the output");
    CHECK(peak > 0.05f && peak < 0.99f, "peak %f", (double)peak);
    printf("  20 s playing: peak %.3f, rms %.4f, bars %u\n", (double)peak,
           (double)sqrtf(sum2 / (20.0f * 44100.0f * 2.0f)), en_bars / 16u);
    CHECK(en_bars / 16u >= 5u, "the grid ran (%u sixteenths)", en_bars);
    CHECK(en_pad_level > 0.5f, "the pad swelled (%f)", (double)en_pad_level);

    /* the chord drifts: with Drift at its shortest, a few changes in two minutes */
    {
        int changes = 0, last = en_chord_root * 16 + en_chord[0] % 16, k;
        en_set(P_DRIFT, 0);
        for (k = 0; k < 120; k++) {
            int now;
            run(1.0f);
            now = en_chord_root * 16 + en_chord[0] % 16;
            changes += now != last;
            last = now;
        }
        CHECK(changes >= 3, "chord changes in 2 min at Drift 0: %d", changes);
        printf("  chord changes in 2 min: %d\n", changes);
    }

    /* a chord change is heard: within 2 s no sounding voice sings a note outside the new chord */
    {
        int k, i, bad_s = 0, changes = 0, last = -1;
        en_set(P_DRIFT, 0);
        for (k = 0; k < 90 * 4; k++) {
            int sig = en_chord[0] | en_chord[1] << 7 | en_chord[2] << 14 | en_chord[3] << 21;
            static int since = -1;
            run(0.25f);
            if (sig != last) {
                if (last >= 0) {
                    changes++;
                    since = 0;
                }
                last = sig;
            } else if (since >= 0 && ++since == 8) {   /* 2 s after a change */
                uint32_t pcs = 0;
                for (i = 0; i < EN_NPAD; i++)
                    pcs |= 1u << (en_chord[i] % 12);
                for (i = 0; i < EN_NPAD; i++)
                    if (en_pad_amp[i] > 0.05f && !(pcs >> (en_pad_note[i] % 12) & 1u))
                        bad_s++;
                since = -1;
            }
        }
        CHECK(changes >= 3, "chord changes in 90 s at Drift 0: %d", changes);
        CHECK(!bad_s, "%d voices still off the new chord 2 s after a change", bad_s);
    }

    /* the loops evolve: with Evolve at 100, the phrases change */
    {
        int8_t before[EN_NLOOP][EN_LOOP_MAX];
        int diff = 0, l;
        en_set(P_EVOLVE, 100);
        en_set(P_DENSITY, 80);
        memcpy(before, (const void *)en_loop_note, sizeof before);
        run(90.0f);
        for (l = 0; l < EN_NLOOP; l++)
            for (i = 0; i < EN_LOOP_MAX; i++)
                diff += before[l][i] != en_loop_note[l][i];
        CHECK(diff > 0, "the loops evolved");
        for (l = 0; l < EN_NLOOP; l++)
            for (i = EN_LOOP_LEN[l]; i < EN_LOOP_MAX; i++)
                CHECK(en_loop_note[l][i] == -1, "loop %d slot %d past its end", l, i);
    }

    /* freeze: the chord holds */
    {
        int held = en_chord[0] + 128 * en_chord[1], k;
        en_freeze(1);
        for (k = 0; k < 60; k++) {
            run(1.0f);
            CHECK(en_chord[0] + 128 * en_chord[1] == held, "the chord moved while frozen");
        }
        en_freeze(0);
    }

    /* a new key while playing (a piece, the root, the scale): within 3 s no voice still sings the old
     * chord (a voice only takes its note at its next breath otherwise, up to a minute later) */
    {
        int k, stale = 0;
        en_freeze(0);
        en_set(P_ROOT, 6);                       /* a tritone away: nothing in common to hide behind */
        en_pool(EN_MODE_POOL[MODE_LYDIAN]);
        en_regrow();
        run(3.0f);
        for (k = 0; k < EN_NPAD; k++)
            if (en_pad_amp[k] > 0.02f && en_pad_note[k] != en_chord[k]) {
                printf("  voice %d sings %d, the chord has %d (amp %.2f)\n", k, en_pad_note[k], en_chord[k],
                       (double)en_pad_amp[k]);
                stale++;
            }
        CHECK(!stale, "%d voices still in the old key after 3 s", stale);
        en_set(P_ROOT, 2);
    }

    /* a pool change: the chord's tones are in it */
    {
        uint32_t pent = EN_MODE_POOL[MODE_PENT_MIN];
        en_pool(pent);
        run(0.1f);
        for (i = 0; i < 4; i++)
            CHECK(pent >> ((en_chord[i] - 2 + 120) % 12) & 1u, "chord note %d not in the pool", en_chord[i]);
    }

    /* every setting at its extremes: still bounded */
    {
        int p;
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->hi);
        en_set(P_MIDI, 1);
        peak = 0.0f;
        run(30.0f);
        CHECK(!bad && peak < 1.0f, "all at max: peak %f", (double)peak);
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->lo);
        en_set(P_TEMPO, 30);
        run(10.0f);
        CHECK(!bad, "all at min: NaN");
    }

    /* Bell tone and Bell oct are heard: brighter (zero crossings) and higher with them, bells alone */
    {
        float zc[3];
        int k;
        static const int TONE[3] = {0, 100, 0}, OCT[3] = {0, 0, 2};
        for (k = 0; k < 3; k++) {
            int b2, j;
            long z = 0;
            float prev = 0.0f;
            en_init();
            en_set(P_PAD, 0);
            en_set(P_BASS, 0);
            en_set(P_PULSE, 0);
            en_set(P_ECHO, 0);
            en_set(P_REVERB, 0);
            en_set(P_DENSITY, 100);
            en_set(P_BTONE, TONE[k]);
            en_set(P_BOCT, OCT[k]);
            en_play(1);
            for (b2 = 0; b2 < 20 * 44100 / 256; b2++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 256; j++) {
                    float x = (float)buf[2 * j];
                    z += (x > 0.0f) != (prev > 0.0f);
                    prev = x;
                }
            }
            zc[k] = (float)z;
        }
        CHECK(zc[1] > 2.5f * zc[0], "Bell tone 100 not brighter: %.0f vs %.0f", (double)zc[1], (double)zc[0]);
        CHECK(zc[2] > 1.8f * zc[0], "Bell oct +2 not higher: %.0f vs %.0f", (double)zc[2], (double)zc[0]);
        en_init();
        en_play(1);
    }

    /* stop: the tails die away */
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, en_param_info(i)->def);
    en_play(0);
    run(40.0f);
    peak = 0.0f;
    run(1.0f);
    CHECK(peak < 0.01f, "quiet after stopping (%f)", (double)peak);
    CHECK(midi_count > 50u, "MIDI out (%u)", midi_count);

    /* The isolated bowl keeps its inharmonic ring at maximum Blur. */
    {
        const float root = 146.83238f;   /* D3, octave +1 */
        const float hz[3] = {root, 2.72f * root, 4.84f * root};
        const float next_hz[3] = {root, 220.0f, 2.72f * 220.0f}; /* D3 and A3 */
        float early[3], late[3], renewed[3], retuned[3];
        en_init();
        en_set(P_PAD, 0);
        en_set(P_BELLS, 0);
        en_set(P_BASS, 0);
        en_set(P_PULSE, 0);
        en_set(P_TEXT_VOL, 0);
        en_set(P_ECHO, 0);
        en_set(P_REVERB, 0);
        en_set(P_ROOT, 2);
        en_set(P_BOWLS_OCT, 1);
        en_set(P_BOWLS_VOL, 100);
        en_set(P_BOWLS_BLUR, 100);
        en_play(1);
        run(0.25f);
        measure_partials(hz, early);
        run(4.0f);
        measure_partials(hz, late);
        CHECK(early[1] > 0.02f, "bowl first overtone at maximum Blur: %.4f", (double)early[1]);
        CHECK(early[2] > 0.005f && late[2] < 0.2f * early[2] && late[0] > 0.014f,
              "bowl loses its bright overtone first: %.4f to %.4f, fundamental %.4f",
              (double)early[2], (double)late[2], (double)late[0]);
        run(3.4f);
        measure_partials(hz, renewed);
        CHECK(renewed[1] > 2.0f * late[1], "bowl is struck again: %.4f to %.4f",
              (double)late[1], (double)renewed[1]);
        en_set(P_ROOT, 9);
        run(0.25f);
        measure_partials(next_hz, retuned);
        CHECK(retuned[0] > 0.02f && retuned[1] > 0.035f, "bowl pitch handoff: old %.4f, new %.4f",
              (double)retuned[0], (double)retuned[1]);
    }

    /* Solo engine: 13 presets, Saw/Square waves, and Arpeggiator */
    {
        solo_init();
        CHECK(!strcmp(solo_preset_name(0), "Pure"), "preset 0 Pure");
        CHECK(!strcmp(solo_preset_name(2), "Chime"), "preset 2 Chime");
        CHECK(!strcmp(solo_preset_name(9), "Vangelis"), "preset 9 Vangelis");
        CHECK(!strcmp(solo_preset_name(10), "Astral"), "preset 10 Astral");
        CHECK(!strcmp(solo_preset_name(12), "User"), "preset 12 User");
        solo_load_preset(10);
        CHECK(solo_get(SP_ARP_MODE) == 3, "Astral has Arp Up/Dn enabled");
        solo_note(60, 100, 1);
        solo_note(64, 100, 1);
        solo_note(67, 100, 1);
        float l[64], r[64], es[64], rs[64];
        solo_render(l, r, es, rs, 64);
        solo_note(60, 0, 0);
        solo_note(64, 0, 0);
        solo_note(67, 0, 0);
        solo_render(l, r, es, rs, 64);
        solo_panic();
        char sbuf[16];
        solo_param_text(SP_GLIDE, 0, sbuf);
        CHECK(!strcmp(sbuf, "Off"), "Glide is Off at zero");

        /* Test Arp Hold (Latch) */
        solo_load_preset(10); /* Astral, Arp Up/Dn */
        solo_arp_hold_set(1);
        CHECK(solo_arp_hold_get() == 1, "Arp Hold is active");
        solo_note(60, 100, 1);
        solo_note(64, 100, 1);
        solo_note(67, 100, 1);
        solo_render(l, r, es, rs, 64);
        /* Release physical keys */
        solo_note(60, 0, 0);
        solo_note(64, 0, 0);
        solo_note(67, 0, 0);
        /* Arp should remain latched and cycling */
        CHECK((solo_arp_latched_keys() & (1u << (60 - 53))) != 0, "Latched note 60");
        CHECK((solo_arp_latched_keys() & (1u << (64 - 53))) != 0, "Latched note 64");
        CHECK((solo_arp_latched_keys() & (1u << (67 - 53))) != 0, "Latched note 67");
        solo_render(l, r, es, rs, 64);
        /* Play new note -> clears old chord and latches new one */
        solo_note(72, 100, 1);
        solo_note(72, 0, 0);
        CHECK(solo_arp_latched_keys() == (1u << (72 - 53)), "Latched replaced by note 72");
        solo_arp_hold_set(0);
        CHECK(solo_arp_hold_get() == 0, "Arp Hold disabled");
        CHECK(solo_arp_latched_keys() == 0, "No latched keys after hold off");

        /* Test mode memory and capture */
        solo_set(SP_ARP_MODE, 4); /* Random */
        CHECK(solo_arp_last_mode_get() == 4, "remembers Random mode");
        solo_set(SP_ARP_MODE, 0); /* Off */
        CHECK(solo_arp_last_mode_get() == 4, "still remembers Random mode after Off");
        /* Capture 3 keys */
        uint32_t mask3 = (1u << (60 - 53)) | (1u << (64 - 53)) | (1u << (67 - 53));
        solo_arp_capture_keys(mask3);
        solo_arp_hold_set(1);
        CHECK((solo_arp_latched_keys() & (1u << (60 - 53))) != 0, "Captured note 60");
        CHECK((solo_arp_latched_keys() & (1u << (64 - 53))) != 0, "Captured note 64");
        CHECK((solo_arp_latched_keys() & (1u << (67 - 53))) != 0, "Captured note 67");
        solo_panic();

        /* Test interactive toggle editing */
        solo_load_preset(10);
        solo_arp_hold_set(1);
        solo_arp_toggle_note(60);
        CHECK((solo_arp_latched_keys() & (1u << (60 - 53))) != 0, "Toggle on note 60");
        solo_arp_toggle_note(64);
        CHECK((solo_arp_latched_keys() & (1u << (64 - 53))) != 0, "Toggle on note 64");
        solo_arp_toggle_note(67);
        CHECK((solo_arp_latched_keys() & (1u << (67 - 53))) != 0, "Toggle on note 67");
        /* Remove note 64 */
        solo_arp_toggle_note(64);
        CHECK((solo_arp_latched_keys() & (1u << (64 - 53))) == 0, "Toggle off note 64");
        CHECK((solo_arp_latched_keys() & (1u << (60 - 53))) != 0, "Note 60 still active");
        CHECK((solo_arp_latched_keys() & (1u << (67 - 53))) != 0, "Note 67 still active");
        /* Remove remaining notes */
        solo_arp_toggle_note(60);
        solo_arp_toggle_note(67);
        CHECK(solo_arp_latched_keys() == 0, "All notes toggled off");
        /* Test upper octave (C5..G5, keys 19..26) */
        solo_load_preset(10);
        solo_arp_hold_set(1);
        solo_arp_toggle_note(72); /* C5, key 19 */
        solo_arp_toggle_note(79); /* G5, key 26 */
        CHECK((solo_arp_latched_keys() & (1u << (72 - 53))) != 0, "Upper octave C5 (key 19) is lit");
        CHECK((solo_arp_latched_keys() & (1u << (79 - 53))) != 0, "Upper octave G5 (key 26) is lit");
        /* Verify background rendering continues while hold is on */
        solo_render(l, r, es, rs, 64);
        CHECK(solo_arp_hold_get() == 1, "Hold persists for background playback");
        CHECK(solo_arp_latched_keys() != 0, "Latched keys persist in background");
        solo_arp_hold_set(0);
        CHECK(solo_arp_latched_keys() == 0, "Cleared when hold turned off");

        /* Test solo_arp_stop */
        solo_load_preset(10);
        solo_arp_hold_set(1);
        solo_arp_toggle_note(60);
        solo_arp_toggle_note(64);
        solo_arp_toggle_note(67);
        CHECK(solo_get(SP_ARP_MODE) > 0, "Arp active");
        CHECK(solo_arp_latched_keys() != 0, "Latched keys active");
        solo_set(SP_ARP_MODE, 0);
        solo_arp_stop();
        CHECK(solo_get(SP_ARP_MODE) == 0, "Arp mode is 0 (off)");
        CHECK(solo_arp_latched_keys() == 0, "No latched keys after stop");
        CHECK(solo_arp_hold_get() == 0, "Hold is off after stop");

        /* Test turn K1 to Up then press ARP */
        solo_set(SP_ARP_MODE, 1);
        solo_arp_hold_set(0);
        CHECK(solo_arp_hold_get() == 0, "Hold is 0 after selecting mode on K1");
        /* Press ARP when hold is 0 -> turns ON */
        solo_arp_hold_set(1);
        CHECK(solo_arp_hold_get() == 1, "Hold engaged after ARP press");
        /* Press ARP when hold is 1 -> turns OFF */
        solo_set(SP_ARP_MODE, 0);
        solo_arp_stop();
        CHECK(solo_get(SP_ARP_MODE) == 0, "Arp stopped after second ARP press");
        CHECK(solo_arp_hold_get() == 0, "Hold off after second ARP press");

        /* Test preset switching while Arp Hold is active */
        solo_load_preset(10); /* Astral */
        solo_arp_hold_set(1);
        solo_arp_toggle_note(60);
        solo_arp_toggle_note(64);
        CHECK(solo_arp_hold_get() == 1, "Hold active before preset switch");
        CHECK(solo_arp_latched_keys() != 0, "Latched keys active before preset switch");
        solo_load_preset(0); /* Switch to Pure */
        CHECK(solo_arp_hold_get() == 1, "Hold remains active after switching to Pure");
        CHECK(solo_get(SP_ARP_MODE) > 0, "Arp mode preserved when switching to Pure");
        CHECK(solo_arp_latched_keys() != 0, "Latched keys preserved when switching to Pure");
        /* Switch preset when Hold is OFF -> Arp stopped */
        solo_arp_hold_set(0);
        solo_load_preset(1); /* Switch to Mallet */
        CHECK(solo_get(SP_ARP_MODE) == 0, "Arp stopped when loading Mallet without hold");
        CHECK(solo_arp_latched_keys() == 0, "Latched keys cleared without hold");

        /* Test solo_get_and_clear_triggered_note */
        solo_panic();
        CHECK(solo_get_and_clear_triggered_note() == 0, "Triggered note 0 after panic");
        solo_note(65, 100, 1);
        CHECK(solo_get_and_clear_triggered_note() == 65, "Note 65 triggered");
        CHECK(solo_get_and_clear_triggered_note() == 0, "Cleared after read");
        solo_note(65, 0, 0);

        solo_panic();
    }

    printf(fails ? "amb: %d FAILED\n" : "amb: ok\n", fails);
    return fails != 0;
}
