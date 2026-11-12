/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 3dSam (Clipper)
 * From Clipper (isod89/clipper-fm1 fbd8434, firmware/src/eng_juno.c, GPL-3.0-only). Modifications (chord-harp
 * fork, 2026-10-30): Yehnah's engine_t (named fields), 4 voices a part, the Juno-106's factory bank after its
 * presets, each preset's type and level trim (Clipper's preset_trim_clipper.h), an INIT preset (the EDIT defaults,
 * dry). */
/* JUNO (Clipper 0.8): the poly synth of the Roland Juno-6 / 60 / 106, one of the most loved sounds there is (its
 * idea: one DCO with a saw, a pulse whose width the LFO moves, a sub oscillator an octave down and noise; a high-pass
 * in four positions; a 24 dB resonant low-pass; and the BBD chorus, I, II or both). No code or sample of Roland's.
 * EDIT 1  WAVE  PW  SUB  NOIS        EDIT 2  HPF  CUT  RES  CHOR
 *   WAVE  SAW, PLS (the pulse), S+P (both, as the 106's two buttons)
 *   PW    the pulse's width, 50 % (square) .. 5 %; SHAPE (LFO DEST SHP: the PWM, MW / AT SHP, the matrix) moves it
 *   SUB   the square an octave down; NOIS white noise
 *   HPF   0 a little bass boost, 1 flat, 2 and 3 a high-pass at ~240 and ~720 Hz (the 106's four positions)
 *   CUT RES  the 24 dB ladder (dsp.c, Zavalishin's), keyboard tracking half; ENV DEST FLT its envelope
 *   CHOR  OFF, I (0.5 Hz, wide), II (0.8 Hz, deeper), I+II (8 Hz, shallow: the 60's both buttons): stereo, after the
 *         sound's INSERT and FILTER (fx.c juno_chorus: one per part; the INSERT's JUNO types give it to any sound)
 * The oscillators are band-limited (polyBLEP: dsp.c osc_saw / osc_pulse).
 * Voice state: ph[0] the DCO, ph[1] the sub, ph[2] the noise; s[0..3] the ladder, s[4] the HPF's low-pass, s[5] the
 * bass boost's
 * (chord-harp fork) 4 voices a part (Clipper: 8): a voice costs ~2.2 ANALOG voices (host); as FM6's cap */
#define JUNO_POLY 4u
enum { JW_SAW, JW_PLS, JW_SP };
static const char *const N_JWAVE[] = {"SAW", "PLS", "S+P"};
static const char *const N_JHPF[] = {"0", "1", "2", "3"};
static const char *const N_JCHOR[] = {"OFF", "I", "II", "I+II"};

static void juno_note_on(track_t *t, voice_t *v)
{
    (void)t;
    if (!v->env && !v->env_out) {                    /* from silence: the filter clear (a DCO: its phase runs on) */
        v->s[0] = v->s[1] = v->s[2] = v->s[3] = v->s[4] = v->s[5] = 0;
        if (!v->ph[2])
            v->ph[2] = 0x2545F491u + v->note * 2654435761u;
    }
}

static void juno_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t i, ph = v->ph[0], phs = v->ph[1], nz = v->ph[2], inc = m->inc, incs = inc >> 1;
    uint32_t wave = (uint32_t)clamp(p[P_E0], 0, 2), hpf = (uint32_t)clamp(p[P_E4], 0, 3);
    int32_t w = clamp(p[P_E1] * 256 + (m->shape - (64 << 8)) * 2, 0, 127 * 256);   /* 0 square .. 127 * 256 narrow */
    uint32_t pw = 0x80000000u - (uint32_t)w * 59000u;                               /* 50 % .. ~5 % */
    int32_t sub = p[P_E2] * 200, nois = p[P_E3] * 160, hp = v->s[4], bb = v->s[5];
    int32_t cut = (p[P_E5] << 8) + m->cutoff + ((v->pitch16 - 60 * 16) << 3);
    int32_t ls[4] = {v->s[0], v->s[1], v->s[2], v->s[3]};
    ladder_t lad;
    ladder_coef(&lad, cut, p[P_E6]);
    for (i = 0; i < n; i++) {
        int32_t s, y, a;
        if (wave == JW_SAW)
            s = osc_saw(ph, inc);
        else if (wave == JW_PLS)
            s = osc_pulse(ph, inc, pw);
        else
            s = (osc_saw(ph, inc) + osc_pulse(ph, inc, pw)) * 5 >> 3;
        if (sub)
            s += mulq15(osc_pulse(phs, incs, 0x80000000u), sub);
        if (nois) {
            nz = nz * 1664525u + 1013904223u;
            s += mulq15((int32_t)(nz >> 16) - 32768, nois);
        }
        ph += inc;
        phs += incs;
        s = s * 5 >> 3;                              /* (four sources: headroom) */
        if (hpf != 1u) {                             /* 0: bass boost (a low shelf ~120 Hz); 2, 3: a one-pole high-pass */
            if (!hpf) {
                bb += ((s - bb) * 560) >> 15;
                s += bb >> 1;
            } else {
                hp += ((s - hp) * (hpf == 2u ? 1120 : 3360)) >> 15;
                s -= hp;
            }
        }
        y = ladder_tick(&lad, s, ls) >> 1;
        a = y < 0 ? -y : y;
        if (a > 16000) {
            a = 16000 + (softclip((a - 16000) * 2) >> 1);
            y = y < 0 ? -a : a;
        }
        out[i] += mulq15(mulq15(y << 1, amp_at(m, i)), VOICE_FS) << 1;
    }
    v->ph[0] = ph;
    v->ph[1] = phs;
    v->ph[2] = nz;
    v->s[0] = ls[0], v->s[1] = ls[1], v->s[2] = ls[2], v->s[3] = ls[3];
    v->s[4] = hp;
    v->s[5] = bb;
}

static const preset_t JUNO_PRESETS[] YEHNAH_RES = {
    /* {WAVE, PW, SUB, NOIS, HPF, CUT, RES, CHOR}, {A D S R}, ENV -> FILTER, mono; extras: the LFO on the PW */
    {"JUNO 106 PAD", {JW_SP, 40, 40, 0, 1, 88, 20, 1}, {70, 100, 115, 95}, 10, 0, FX(0, 0, 10, 40),
     XP(P_LRATE + 1, 28, P_LD_SHP + 1, 40, P_VINT + 1, 24)},
    {"JUNO BRASS", {JW_SAW, 0, 30, 0, 1, 62, 30, 0}, {14, 82, 100, 50}, 40, 0, FX(0, 0, 10, 30)},
    {"JUNO BASS", {JW_PLS, 50, 110, 0, 0, 58, 40, 0}, {0, 70, 70, 26}, 36, 1, FX(0, 0, 0, 8), XP(P_GLIDE + 1, 10)},
    {"JUNO STRINGS", {JW_SAW, 0, 0, 0, 2, 96, 10, 2}, {80, 100, 120, 96}, 0, 0, FX(0, 0, 10, 50),
     XP(P_LRATE + 1, 34, P_LD_SHP + 1, 30)},
    {"POLY KEYS", {JW_SP, 30, 20, 0, 1, 70, 24, 1}, {0, 88, 60, 60}, 30, 0, FX(0, 0, 14, 30)},
    {"SYNTHWAVE LD", {JW_SAW, 0, 60, 0, 1, 90, 30, 1}, {2, 80, 110, 50}, 10, 1, FX(0, 0, 30, 30),
     XP(P_GLIDE + 1, 24, P_LRATE + 1, 70, P_LD_PIT + 1, 3)},
    {"80S HITS", {JW_SP, 20, 50, 0, 1, 56, 50, 1}, {0, 64, 0, 40}, 50, 0, FX(0, 0, 20, 30)},
    {"JUNO PIANO", {JW_PLS, 60, 30, 0, 2, 50, 20, 1}, {0, 90, 0, 70}, 50, 0, FX(0, 0, 10, 30)},
    {"DREAM PAD", {JW_SP, 50, 30, 10, 2, 92, 30, 3}, {90, 100, 120, 110}, 0, 0, FX(0, 0, 20, 70),
     XP(P_LRATE + 1, 20, P_LD_SHP + 1, 50, P_VINT + 1, 40)},
    {"SUB BASS 106", {JW_PLS, 0, 127, 0, 0, 40, 10, 0}, {0, 80, 110, 20}, 10, 1, FX(0, 0, 0, 4)},
    {"SYNTH BRASS 2", {JW_SP, 10, 40, 0, 1, 54, 36, 2}, {20, 86, 96, 56}, 46, 0, FX(0, 0, 12, 34)},
    {"ARP PLUCK", {JW_SAW, 0, 40, 0, 2, 44, 50, 1}, {0, 60, 0, 40}, 56, 0, FX(0, 0, 30, 30)},   /* ARP: engines.c PRESET_ARP */
    {"NOISE SWEEP", {JW_SAW, 0, 0, 127, 3, 40, 90, 3}, {60, 110, 100, 100}, 50, 0, FX(0, 0, 30, 60),
     XP(P_LRATE + 1, 10, P_LD_FLT + 1, 50)},
    {"JUNO ORGAN", {JW_PLS, 100, 90, 0, 1, 100, 0, 2}, {0, 30, 127, 20}, 0, 0, FX(0, 0, 0, 30)},
    {"SAW LEAD 106", {JW_SAW, 0, 30, 0, 1, 82, 44, 0}, {0, 80, 100, 40}, 24, 1, FX(10, 0, 26, 24),
     XP(P_GLIDE + 1, 20)},
    {"HOOVER JUNO", {JW_PLS, 80, 60, 0, 1, 76, 20, 3}, {0, 90, 110, 50}, 0, 1, FX(10, 0, 10, 30),
     XP(P_LRATE + 1, 60, P_LD_SHP + 1, 63, P_GLIDE + 1, 40)},
    /* 0.9: the arp's TURNG (a Turing machine over the held notes: hold a chord), VINTAGE */
    {"TURING JUNO", {JW_SAW, 0, 20, 0, 2, 64, 36, 1}, {0, 62, 24, 40}, 44, 0, FX(0, 0, 34, 40),   /* ARP: engines.c PRESET_ARP */
     XP(P_SPICE + 1, 16, P_SLOOP + 1, 8, P_VINT + 1, 30)},
    /* (chord-harp fork, tools/gen_juno106.py) the Juno-106's factory bank, A11 .. B88, from FM-106 (PatttF/fm-106,
     * GPL-3.0-only: decoded from the 106's SysEx); drums, effects, names twice and near copies left out */
    {"BRASS", {JW_SAW, 0, 0, 0, 1, 35, 13, 1}, {3, 49, 45, 32}, 29, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, 24)},
    {"BRASS SWELL", {JW_SAW, 0, 70, 0, 0, 43, 17, 1}, {64, 118, 38, 37}, 13, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 23)},
    {"TRUMPET", {JW_SAW, 0, 0, 0, 2, 55, 34, 0}, {5, 66, 48, 16}, 12, 0, FX(0, 0, 0, 25)},
    {"FLUTES", {JW_SAW, 0, 0, 0, 3, 55, 32, 0}, {23, 81, 0, 18}, 5, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, -11, P_LD_FLT + 1, 5, P_LFADE + 1, 43)},
    {"MOVING STR", {JW_SP, 19, 14, 0, 1, 77, 20, 2}, {13, 87, 88, 35}, 2, 0, FX(0, 0, 0, 40), XP(P_KTRK + 1, 44, P_LD_SHP + 1, 9)},
    {"BRASS+STR", {JW_PLS, 28, 23, 0, 0, 76, 17, 1}, {44, 66, 53, 44}, 2, 0, FX(0, 0, 0, 40), XP(P_TRANS + 1, -12, P_KTRK + 1, -11, P_LRATE + 1, 35, P_LD_SHP + 1, 14)},
    {"CHOIR", {JW_PLS, 25, 0, 0, 2, 59, 94, 1}, {68, 11, 127, 48}, 1, 0, FX(0, 0, 0, 40), XP(P_KTRK + 1, 5, P_LD_PIT + 1, 1, P_LFADE + 1, 14)},
    {"PIANO I", {JW_PLS, 80, 86, 0, 1, 65, 12, 0}, {0, 66, 0, 30}, 5, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, -22)},
    {"ORGAN I", {JW_PLS, 26, 23, 0, 0, 43, 76, 0}, {0, 64, 127, 8}, 7, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, 56, P_LRATE + 1, 54, P_LD_SHP + 1, 13)},
    {"ORGAN II", {JW_PLS, 26, 58, 0, 0, 53, 76, 1}, {0, 64, 127, 8}, 7, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 23, P_LRATE + 1, 44, P_LD_SHP + 1, 13, P_LFADE + 1, 15)},
    {"COMBO ORGAN", {JW_PLS, 57, 61, 0, 2, 63, 70, 0}, {0, 64, 127, 8}, 2, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, 12, P_KTRK + 1, 42)},
    {"CALLIOPE", {JW_PLS, 0, 48, 0, 2, 87, 27, 1}, {7, 127, 127, 6}, -8, 0, FX(0, 0, 0, 25)},
    {"DONALD PLK", {JW_PLS, 57, 127, 0, 3, 73, 105, 0}, {0, 64, 127, 8}, -7, 0, FX(0, 0, 0, 40), XP(P_TRANS + 1, 12, P_KTRK + 1, 18)},
    {"CELESTE", {JW_PLS, 0, 15, 0, 0, 33, 24, 0}, {0, 44, 0, 81}, 27, 0, FX(0, 0, 0, 40), XP(P_TRANS + 1, 12, P_KTRK + 1, -13)},
    {"E PIANO I", {JW_PLS, 0, 0, 0, 3, 16, 31, 0}, {1, 85, 43, 40}, 30, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, -16, P_LD_FLT + 1, 3)},
    {"E PIANO II", {JW_PLS, 71, 0, 0, 1, 50, 69, 1}, {0, 68, 0, 22}, 3, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, 19)},
    {"CLOCK CHIME", {JW_SAW, 0, 127, 22, 0, 44, 127, 1}, {0, 48, 0, 51}, 0, 0, FX(0, 0, 0, 40), XP(P_TRANS + 1, 12, P_KTRK + 1, 56)},
    {"STEEL DRUMS", {JW_PLS, 32, 33, 9, 0, 71, 46, 1}, {0, 26, 0, 37}, -13, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 56)},
    {"XYLOPHONE", {JW_PLS, 0, 15, 0, 0, 29, 24, 0}, {0, 29, 29, 38}, 27, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, 12)},
    {"BRASS III", {JW_SAW, 0, 22, 0, 0, 66, 24, 1}, {58, 100, 94, 37}, 5, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, -34)},
    {"FANFARE", {JW_SP, 35, 50, 0, 0, 44, 0, 1}, {72, 104, 75, 49}, 16, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, 9, P_LRATE + 1, 47, P_LD_SHP + 1, 17)},
    {"STRINGS III", {JW_SP, 51, 0, 0, 1, 71, 14, 2}, {63, 31, 127, 45}, 0, 0, FX(0, 0, 0, 40), XP(P_KTRK + 1, 23, P_LRATE + 1, 48, P_LD_SHP + 1, 25, P_LFADE + 1, 27)},
    {"PIZZICATO", {JW_SP, 51, 0, 0, 3, 66, 2, 1}, {0, 11, 0, 12}, 2, 0, FX(0, 0, 0, 40), XP(P_KTRK + 1, -10, P_LD_SHP + 1, 25, P_LFADE + 1, 18)},
    {"HIGH STR", {JW_PLS, 51, 0, 0, 2, 84, 8, 2}, {18, 44, 127, 40}, 1, 0, FX(0, 0, 0, 40), XP(P_TRANS + 1, 12, P_KTRK + 1, 12, P_LD_SHP + 1, 25, P_LFADE + 1, 14)},
    {"BASS CLAR", {JW_PLS, 0, 0, 0, 1, 48, 36, 0}, {11, 75, 0, 25}, 12, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_LRATE + 1, 52, P_LD_FLT + 1, 4, P_LFADE + 1, 45)},
    {"ENG HORN", {JW_PLS, 102, 0, 0, 3, 70, 48, 0}, {8, 81, 26, 16}, 3, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, -22)},
    {"BRASS ENS", {JW_SAW, 0, 31, 0, 1, 46, 44, 1}, {16, 103, 97, 34}, 14, 0, FX(0, 0, 0, 25)},
    {"GUITAR", {JW_SP, 65, 0, 0, 1, 12, 15, 0}, {0, 52, 54, 31}, 35, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, -20)},
    {"KOTO", {JW_PLS, 94, 2, 0, 3, 40, 58, 1}, {0, 56, 0, 39}, 14, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 16)},
    {"DARK PLUCK", {JW_PLS, 87, 97, 0, 0, 38, 61, 2}, {0, 52, 15, 63}, 13, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 12)},
    {"FUNKY I", {JW_SP, 73, 64, 0, 0, 31, 23, 1}, {0, 64, 127, 8}, 25, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, -11)},
    {"SYN BASS I", {JW_PLS, 57, 102, 50, 0, 32, 0, 0}, {0, 34, 0, 36}, 33, 1, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, -16)},
    {"LEAD I", {JW_PLS, 0, 0, 0, 0, 40, 97, 0}, {0, 64, 127, 8}, 18, 0, FX(0, 0, 0, 25), XP(P_LRATE + 1, 72, P_LD_PIT + 1, 1, P_LFADE + 1, 88)},
    {"LEAD II", {JW_SP, 26, 79, 0, 1, 45, 0, 0}, {0, 64, 127, 8}, 21, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_LRATE + 1, 24, P_LD_SHP + 1, 13, P_LFADE + 1, 88)},
    {"LEAD III", {JW_SAW, 0, 0, 0, 1, 60, 31, 0}, {0, 64, 127, 8}, 9, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 16, P_LRATE + 1, 86, P_LD_PIT + 1, 1, P_LFADE + 1, 71)},
    {"FUNKY II", {JW_PLS, 48, 57, 0, 1, 5, 23, 1}, {0, 64, 127, 8}, 40, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, -11)},
    {"SYN BASS II", {JW_SAW, 0, 45, 0, 0, 43, 0, 0}, {0, 64, 127, 8}, 23, 1, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, -43)},
    {"FUNKY III", {JW_SP, 9, 127, 101, 0, 38, 54, 0}, {0, 64, 127, 8}, 22, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, -35)},
    {"PIANO II", {JW_PLS, 36, 115, 0, 1, 47, 0, 0}, {0, 98, 0, 32}, 19, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, -39, P_LRATE + 1, 1, P_LD_SHP + 1, 18)},
    {"CLAV", {JW_PLS, 102, 0, 0, 1, 7, 11, 1}, {0, 64, 127, 8}, 43, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, -43, P_LRATE + 1, 66, P_LD_FLT + 1, 1)},
    {"FRONTIER", {JW_PLS, 42, 29, 0, 0, 69, 54, 0}, {0, 64, 127, 8}, 0, 0, FX(0, 0, 0, 40), XP(P_LRATE + 1, 79, P_LD_SHP + 1, 21, P_LFADE + 1, 15)},
    {"SYNTH PAD", {JW_PLS, 21, 37, 0, 0, 37, 0, 2}, {0, 85, 75, 62}, 42, 0, FX(0, 0, 0, 40), XP(P_KTRK + 1, 56, P_LD_SHP + 1, 10)},
    {"SWEEP I", {JW_PLS, 51, 42, 28, 1, 115, 40, 2}, {0, 63, 127, 82}, -33, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, 12, P_KTRK + 1, -43, P_LRATE + 1, 78, P_LD_SHP + 1, 25)},
    {"PLUCK SWEEP", {JW_SP, 34, 125, 0, 2, 60, 111, 2}, {0, 91, 0, 77}, 4, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 34, P_LRATE + 1, 68, P_LD_SHP + 1, 17, P_LD_FLT + 1, 3)},
    {"SWEEP II", {JW_PLS, 97, 48, 0, 0, 66, 102, 1}, {0, 89, 53, 78}, 34, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 12, P_LRATE + 1, 88, P_LD_PIT + 1, 1, P_LFADE + 1, 72)},
    {"PLUCK BELL", {JW_PLS, 8, 49, 0, 0, 39, 0, 2}, {0, 85, 75, 53}, 24, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, 12, P_KTRK + 1, 56, P_LRATE + 1, 51, P_LD_SHP + 1, 4)},
    {"DARK SYNTH", {JW_SP, 0, 127, 0, 0, 56, 86, 2}, {0, 47, 0, 50}, -3, 0, FX(0, 0, 0, 40), XP(P_KTRK + 1, 12)},
    {"WAH RELEASE", {JW_PLS, 25, 87, 0, 1, 72, 88, 2}, {0, 84, 0, 29}, -9, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 9)},
    {"GONG", {JW_PLS, 102, 0, 56, 1, 70, 107, 1}, {3, 48, 127, 98}, -2, 0, FX(0, 0, 0, 40), XP(P_TRANS + 1, -12, P_KTRK + 1, 13)},
    {"STRINGS", {JW_SP, 27, 0, 0, 0, 85, 0, 2}, {59, 32, 86, 40}, 0, 0, FX(0, 0, 0, 40), XP(P_KTRK + 1, 41, P_LD_SHP + 1, 13, P_LFADE + 1, 45)},
    {"VIOLIN", {JW_SAW, 0, 0, 0, 0, 77, 0, 0}, {43, 45, 57, 26}, 0, 0, FX(0, 0, 0, 40), XP(P_KTRK + 1, 51, P_LRATE + 1, 66, P_LD_PIT + 1, 1, P_LFADE + 1, 45)},
    {"CHORUS VIBE", {JW_PLS, 0, 0, 0, 0, 10, 0, 1}, {0, 89, 47, 74}, 29, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, -25)},
    {"ORGAN I", {JW_PLS, 73, 97, 0, 0, 60, 72, 2}, {0, 64, 127, 8}, 7, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 56)},
    {"HARPSICHORD", {JW_PLS, 27, 35, 0, 3, 127, 127, 0}, {0, 60, 0, 31}, 45, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, 12, P_KTRK + 1, 24, P_LRATE + 1, 23, P_LD_SHP + 1, 13)},
    {"RECORDER", {JW_PLS, 8, 0, 0, 1, 6, 0, 0}, {5, 21, 127, 30}, 23, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 56)},
    {"PERC PLUCK", {JW_PLS, 0, 0, 0, 0, 0, 0, 1}, {0, 16, 78, 112}, 45, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 14)},
    {"SPACE CHIME", {JW_PLS, 0, 0, 0, 0, 70, 127, 1}, {0, 25, 0, 60}, 0, 0, FX(0, 0, 0, 40), XP(P_TRANS + 1, 12, P_KTRK + 1, 15)},
    {"NYLON GTR", {JW_PLS, 29, 0, 0, 3, 57, 0, 0}, {0, 89, 0, 32}, 13, 0, FX(0, 0, 0, 40), XP(P_KTRK + 1, -23)},
    {"ORCHESTRAL", {JW_SP, 52, 127, 0, 0, 33, 0, 2}, {29, 88, 50, 52}, 27, 0, FX(0, 0, 0, 40), XP(P_KTRK + 1, -15, P_LRATE + 1, 22, P_LD_SHP + 1, 26, P_LFADE + 1, 45)},
    {"BRIGHT PLK", {JW_PLS, 58, 0, 0, 1, 57, 0, 0}, {0, 25, 42, 44}, 20, 0, FX(0, 0, 0, 40), XP(P_KTRK + 1, 24)},
    {"ORGAN BELL", {JW_SP, 68, 85, 0, 0, 61, 0, 1}, {0, 0, 127, 26}, 0, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 56)},
    {"ACCORDION", {JW_SP, 64, 80, 0, 3, 77, 0, 1}, {8, 11, 110, 9}, 7, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 15)},
    {"BRASS", {JW_SAW, 0, 0, 0, 0, 0, 0, 1}, {0, 64, 127, 8}, 47, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 56)},
    {"LUTE", {JW_PLS, 105, 0, 0, 0, 29, 0, 0}, {0, 48, 34, 87}, 17, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 24)},
    {"CHORUS FUNK", {JW_SP, 73, 79, 0, 0, 47, 34, 1}, {0, 64, 127, 8}, 26, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 8)},
    {"TOMITA", {JW_SAW, 0, 0, 0, 0, 49, 125, 0}, {0, 64, 127, 8}, -7, 0, FX(0, 0, 0, 40), XP(P_TRANS + 1, 12, P_KTRK + 1, 56, P_LRATE + 1, 78, P_LD_FLT + 1, 1)},
    {"SHARP REED", {JW_SP, 36, 0, 0, 0, 33, 0, 0}, {0, 64, 127, 8}, 26, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, 12, P_KTRK + 1, 8, P_LRATE + 1, 21, P_LD_SHP + 1, 18)},
    {"BASS PLUCK", {JW_SP, 30, 0, 0, 0, 37, 29, 0}, {0, 42, 35, 0}, 14, 1, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, -9, P_LRATE + 1, 0, P_LD_SHP + 1, 15)},
    {"DARK ENS", {JW_SP, 57, 75, 0, 0, 55, 0, 1}, {21, 0, 127, 35}, 0, 0, FX(0, 0, 0, 40), XP(P_KTRK + 1, 41)},
    {"CONTACT WAH", {JW_PLS, 81, 0, 0, 1, 61, 101, 0}, {0, 64, 127, 8}, -23, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 56)},
    {"GLASSY WAH", {JW_PLS, 28, 0, 0, 0, 24, 34, 0}, {6, 18, 50, 38}, 30, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 8)},
    {"PHASE ENS", {JW_PLS, 52, 0, 0, 0, 79, 0, 2}, {84, 45, 57, 49}, 0, 0, FX(0, 0, 0, 40), XP(P_KTRK + 1, 56, P_LRATE + 1, 11, P_LD_SHP + 1, 26, P_LFADE + 1, 45)},
    {"CHORUS BELL", {JW_PLS, 52, 101, 0, 0, 59, 127, 1}, {0, 62, 0, 57}, 0, 0, FX(0, 0, 0, 40), XP(P_TRANS + 1, 12, P_KTRK + 1, 56, P_LRATE + 1, 0, P_LD_SHP + 1, 26)},
    {"CLAV", {JW_PLS, 91, 7, 0, 2, 32, 0, 0}, {0, 43, 55, 0}, 31, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, -43)},
    {"ORGAN 2", {JW_SAW, 0, 0, 0, 0, 54, 106, 1}, {0, 64, 127, 8}, 3, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, 56)},
    {"BASSOON", {JW_PLS, 90, 0, 0, 3, 35, 0, 0}, {5, 127, 106, 2}, 21, 1, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, 34, P_LRATE + 1, 84, P_LD_FLT + 1, 4)},
    {"BRASS ENS 2", {JW_SP, 36, 0, 0, 0, 47, 34, 1}, {6, 68, 67, 38}, 17, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 8, P_LRATE + 1, 25, P_LD_SHP + 1, 18, P_LFADE + 1, 94)},
    {"ETHEREAL", {JW_SP, 52, 59, 0, 0, 47, 120, 2}, {92, 50, 127, 55}, 0, 0, FX(0, 0, 0, 40), XP(P_KTRK + 1, 56, P_LRATE + 1, 69, P_LD_SHP + 1, 26)},
    {"EP TREMOLO", {JW_PLS, 10, 127, 0, 1, 22, 0, 0}, {0, 65, 60, 98}, 17, 0, FX(0, 0, 0, 40), XP(P_TRANS + 1, 12, P_KTRK + 1, 41, P_LRATE + 1, 44, P_LD_SHP + 1, 5)},
    {"CLARINET", {JW_PLS, 0, 0, 0, 3, 35, 0, 0}, {5, 127, 106, 7}, 21, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 34, P_LRATE + 1, 69, P_LD_FLT + 1, 3, P_LFADE + 1, 22)},
    {"REEDY ORGAN", {JW_PLS, 36, 0, 0, 0, 33, 0, 0}, {0, 64, 127, 8}, 26, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, 8, P_LRATE + 1, 35, P_LD_SHP + 1, 18, P_LFADE + 1, 18)},
    {"FLUTE+HORN", {JW_SP, 29, 0, 0, 0, 25, 0, 2}, {21, 73, 71, 48}, 21, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, 8, P_LRATE + 1, 34, P_LD_SHP + 1, 14)},
    {"TOY RHODES", {JW_SAW, 0, 0, 0, 3, 58, 127, 0}, {0, 89, 0, 39}, 0, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, 12, P_KTRK + 1, 56, P_LRATE + 1, 30, P_LD_PIT + 1, 5)},
    {"SURFS UP", {JW_SAW, 0, 0, 127, 2, 84, 11, 0}, {33, 97, 127, 112}, 0, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, 12, P_LRATE + 1, 1, P_LD_FLT + 1, 24)},
    {"OW BASS", {JW_SP, 50, 47, 0, 0, 63, 84, 0}, {0, 64, 127, 8}, -32, 1, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, 56)},
    {"PICCOLO", {JW_SP, 14, 0, 0, 2, 4, 0, 0}, {16, 103, 106, 21}, 31, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, 12, P_KTRK + 1, 34, P_LRATE + 1, 70, P_LD_FLT + 1, 8)},
    {"MELODIC TAP", {JW_SAW, 0, 0, 127, 3, 72, 112, 1}, {0, 24, 0, 24}, 0, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, 56)},
    {"MEOW BASS", {JW_SAW, 0, 0, 0, 0, 45, 100, 0}, {0, 64, 127, 8}, 17, 1, FX(0, 0, 0, 25), XP(P_KTRK + 1, 8)},
    {"HIGH BELLS", {JW_SAW, 0, 0, 0, 3, 76, 127, 0}, {0, 22, 52, 59}, 0, 0, FX(0, 0, 0, 40), XP(P_TRANS + 1, 12, P_KTRK + 1, 56, P_LRATE + 1, 30, P_LD_PIT + 1, 5)},
    {"ROLLING WAH", {JW_PLS, 52, 0, 0, 0, 60, 9, 0}, {34, 55, 127, 105}, 0, 0, FX(0, 0, 0, 25), XP(P_KTRK + 1, -43, P_LRATE + 1, 36, P_LD_FLT + 1, 63, P_LD_SHP + 1, 26)},
    {"PING BELL", {JW_PLS, 52, 0, 0, 0, 76, 127, 1}, {0, 23, 23, 57}, 0, 0, FX(0, 0, 0, 40), XP(P_TRANS + 1, 12, P_KTRK + 1, 56, P_LRATE + 1, 0, P_LD_SHP + 1, 26)},
    {"BRASSY ORG", {JW_SP, 34, 56, 0, 0, 8, 0, 1}, {0, 36, 109, 0}, 43, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_LRATE + 1, 16, P_LD_SHP + 1, 17)},
    {"LOW DARK", {JW_SP, 40, 0, 0, 0, 71, 0, 2}, {83, 23, 109, 42}, 0, 0, FX(0, 0, 0, 40), XP(P_TRANS + 1, -12, P_KTRK + 1, 37, P_LRATE + 1, 21, P_LD_SHP + 1, 20)},
    {"CELLO", {JW_SAW, 0, 0, 0, 0, 75, 0, 0}, {48, 65, 90, 34}, 1, 0, FX(0, 0, 0, 40), XP(P_TRANS + 1, -12, P_KTRK + 1, -8, P_LD_PIT + 1, 1, P_LFADE + 1, 45)},
    {"OWGAN", {JW_SP, 45, 56, 0, 0, 38, 84, 0}, {0, 49, 55, 0}, 16, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12, P_KTRK + 1, 56)},
    /* (Yehnah, 2026-11-12) dreampop / synthwave / shoegaze: each brings an FX scene (engines.c PRESET_SCENE: the last five
     * before INIT, in this order). Written from the code, not yet heard; DIST TYPE 6 = FUZZ */
    {"DREAM HAZE", {JW_SP, 60, 10, 6, 2, 80, 6, 3}, {100, 100, 118, 120}, 4, 0, FX(0, 0, 30, 80),
     XP(P_LRATE + 1, 14, P_LD_SHP + 1, 50, P_VINT + 1, 40)},
    {"GAZE WALL", {JW_SAW, 0, 24, 8, 1, 74, 10, 3}, {40, 100, 120, 110}, 8, 0, FX(56, 0, 24, 76),
     XP(P_VOICE + 1, 3, P_DETUNE + 1, 50, P_DTYPE + 1, 6, P_DTONE + 1, -8)},
    {"GAZE SWELL", {JW_SP, 40, 30, 4, 1, 66, 14, 3}, {16, 90, 60, 70}, 20, 0, FX(30, 0, 20, 90),
     XP(P_VOICE + 1, 3, P_DETUNE + 1, 40, P_DTYPE + 1, 6)},
    {"NEON DRIVE", {JW_SAW, 0, 50, 0, 1, 88, 24, 1}, {2, 80, 110, 50}, 14, 1, FX(0, 0, 56, 40),
     XP(P_GLIDE + 1, 20, P_LRATE + 1, 64, P_LD_PIT + 1, 3, P_VOICE + 1, 5)},
    {"OUTRUN PAD", {JW_SP, 44, 40, 0, 1, 84, 16, 3}, {60, 100, 110, 100}, 10, 0, FX(0, 0, 40, 56),
     XP(P_LRATE + 1, 24, P_LD_SHP + 1, 30, P_VINT + 1, 30)},
    {"INIT", {JW_SP, 30, 40, 0, 1, 90, 20, 1}, {0, 64, 127, 30}, 0, 0, FX(0, 0, 0, 0)},   /* (chord-harp fork) the EDIT defaults, dry */
};
/* (chord-harp fork) each preset's sound type (ui.c PTYPE) and level trim (1/2 dB, P_TRIM: Clipper's
 * preset_trim_clipper.h, tools/level_presets.py over its whole mix) */
static const char JUNO_PT[] = "PRBSKLKIPBRAXOLLA"
                              "RRRWSSVIOOOOPEIIEMMRRSSSWRRGGUKBLLLKBKIKPPXUXUPKESSMOKWUEPSPOORGKSWBPKKPEKOBRPPWORIXBWMBEKERPSO"   /* the 106's bank */
                              "PPPLP"                                                                    /* dreampop .. shoegaze */
                              "N";
_Static_assert(sizeof JUNO_PT == NELEM(JUNO_PRESETS) + 1u, "a type a preset");
static const int8_t JUNO_TRIM[] = {1, 5, 10, 2, 7, 3, 14, 16, 1, 3, 7, 23, 3, 3, 5, 10, 22, 12, -1, 5, 15, -6, 13, 4, 15, -25, -7, -5, -2, 0,
    16, 21, 15, 8, 14, 18, 6, 1, -9, 30, 0, 7, 19, -8, 13, 39, 14, -5, 6, -11, -8, -5, 16, -20, -12, -10, 11, -4, 2, -1, 0,
    23, 7, 5, 12, -5, -7, 5, 42, 3, 28, 59, 30, 17, 24, 13, 14, -5, 13, 7, 46, -5, -15, -1, 4, 2, 17, 10, 9, 8, 25,
    -6, 52, -5, -5, 29, 11, -7, 23, 8, -6, -5, 33, 26, -6, 15, 1, 15, 3, -5, -3, -1,
    1, 3, 3, 3, 1,   /* DREAM HAZE .. OUTRUN PAD (the nearest old sounds' trims; not levelled) */
    0};   /* INIT; the 106 bank (ours, not Clipper's) levelled: from
 * its VCA LEVEL (tools/gen_juno106.py, with its VCF KYBD as KEY>F), its loud windows (95th percentile of 50 ms RMS) at
 * the median of Clipper's 17 through the whole mix, up to +50 dB (fx.c), then held back where that went past Clipper's
 * 17 (clicks on the device, 2026-10-30): a boost (past LEVEL 127) whose track peak (fx.c, before the mix) passes their
 * highest, or at the output's ceiling for more than ~40 samples */
_Static_assert(sizeof JUNO_TRIM == NELEM(JUNO_PRESETS), "a trim a preset");
_Static_assert(NELEM(JUNO_PRESETS) <= 128u, "JUNO's favourites");

/* (after Clipper 1.2) UNISON (voice.c unison_spread): the DCO and the sub (an octave down) moved by r (Q32) of a
 * turn of the note, the noise from its own place (the note's seed: every voice the same) */
static void juno_spread(track_t *t, voice_t *v, uint32_t r)
{
    (void)t;
    v->ph[0] += r;
    v->ph[1] += r >> 1;
    v->ph[2] ^= r;
}

/* (2026-11-11) the SOUND page's rows (core.h sound_row_t): the engine's eight settings, then params.c's generic rows */
static const sound_row_t JUNO_ROWS[] = {
    {"DCO", {P_E0, P_E1, P_E2}, {0, 0, 0}},
    {"NOISE", {P_E3, P_E4, 0xFF}, {0, 0, 0}},
    {"VCF", {P_E5, P_E6, 0xFF}, {0, 0, 0}},
    {"CHORUS", {P_E7, 0xFF, 0xFF}, {0, 0, 0}},
};

static const engine_t ENG_JUNO = {
    .rows = JUNO_ROWS,
    .nrows = (uint8_t)(sizeof JUNO_ROWS / sizeof JUNO_ROWS[0]),
    .name = "JUNO",
    .page_title = {"DCO", "VCF"},
    .edit = {
        {"WAVE", F_ENUM, 0, 2, JW_SP, N_JWAVE, 0},
        {"PW", F_PCT, 0, 127, 30, 0, 0},
        {"SUB", F_PCT, 0, 127, 40, 0, 0},
        {"NOIS", F_PCT, 0, 127, 0, 0, 0},
        {"HPF", F_ENUM, 0, 3, 1, N_JHPF, 0},
        {"CUT", F_CUTOFF, 0, 127, 90, 0, 0},
        {"RES", F_PCT, 0, 127, 20, 0, 0},
        {"CHOR", F_ENUM, 0, 3, 1, N_JCHOR, 0},
    },
    .presets = JUNO_PRESETS,
    .npresets = NELEM(JUNO_PRESETS),
    .ptype = JUNO_PT,
    .trim = JUNO_TRIM,
    .note_on = juno_note_on,
    .render = juno_render,
    .knob = {P_E5, P_E6, P_E1, P_E7},
    .poly = JUNO_POLY,
    .keep = 0x33,            /* the ladder's first two stages, the HPF and the bass boost */
};
