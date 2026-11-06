/* SPDX-License-Identifier: GPL-3.0-only */
/* (chord-harp fork, 2026-10-30) A listening archive of our presets before the Clipper hybrid replaces engines: every
 * factory preset of the engines named on the command line (all of them if none) plays a phrase that fits it, 4.75 s
 * each through the whole mix (sends, master), one WAV a preset: OUTDIR/<ENGINE>/<NN>_<NAME>.wav. The phrases and the
 * roles are Clipper's tests/preset_level.c (isod89/clipper-fm1 fbd8434, GPL-3.0-only), so a preset of ours and one of
 * Clipper's are heard the same way.
 *   cc -O2 -w -DYEHNAH_ANIMAL=1 -DYEHNAH_NOISE=1 -Ibuild/gen -Ifirmware/src tests/preset_archive.c -lm -o build/host/preset_archive
 *   build/host/preset_archive OUTDIR [ENGINE ...]      (prints engine preset role name, one line a preset)
 * (2026-10-30, the Clipper port's check) PA_RAW=FILE also writes every render as mono int32 (Clipper's preset_level.c
 * raw), PA_CLIP=1 TRANS 0 (Clipper's is its keyboard's: its preset_level.c's notes go to the voices as they are), as
 * its build/meas/clip_presets.raw was rendered, PA_NOWAV=1 no WAVs */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/stat.h>

enum { R_BASS, R_HELD, R_COMP, R_MELODY, R_ONE };
static int has(const char *n, const char *w) { return strstr(n, w) != 0; }
static int role_of(const char *n)                    /* Clipper's preset_level.c role_of */
{
    if (has(n, "BASS") || has(n, "808") || has(n, "TAURUS") || has(n, "ACID") || has(n, "REESE") || has(n, "WOBBLE") || has(n, "SUB") || has(n, "BOOM"))
        return R_BASS;
    if (has(n, "PAD") || has(n, "STR") || has(n, "CHOIR") || has(n, "OOH") || has(n, "ORGAN") || has(n, "ORGN") || has(n, "GOSPEL") ||
        has(n, "B3") || has(n, "CLOUD") || has(n, "HAZE") || has(n, "ATMOS") || has(n, "ENSEMBLE") || has(n, "DRONE") ||
        has(n, "SHIMMER") || has(n, "OCEAN") || has(n, "RISER") || has(n, "WIND") || has(n, "HISS") || has(n, "VINYL") ||
        has(n, "RAIN") || has(n, "BOWL") || has(n, "BOW") || has(n, "TANPURA") || has(n, "HALO") || has(n, "SWELL") ||
        has(n, "VOWEL") || has(n, "GLASS HARP"))
        return R_HELD;
    if ((has(n, "STAB") && !has(n, "HORN") && !has(n, "STRING")) || has(n, "CHORD"))
        return R_ONE;
    if (has(n, "RHODES") || has(n, "WURLI") || has(n, "CLAV") || has(n, "KEYS") || has(n, "STAB") || has(n, "PNO") || has(n, "PIANO") ||
        has(n, "VIBES") || has(n, "BRASS") || has(n, "HORN") || has(n, "ACCORDION") || has(n, "POLY"))
        return R_COMP;
    return R_MELODY;
}

typedef struct { uint8_t step, note, len; } ev_t;   /* 1/16 steps at 100 BPM */
static const ev_t BASS[] = {{0, 36, 3}, {4, 36, 1}, {6, 43, 2}, {8, 41, 3}, {12, 39, 2}, {14, 38, 2},
                            {16, 36, 3}, {20, 36, 1}, {22, 43, 2}, {24, 46, 3}, {28, 43, 4}};
static const ev_t MELODY[] = {{0, 72, 2}, {2, 75, 2}, {4, 77, 3}, {8, 79, 2}, {10, 77, 2}, {12, 75, 4},
                              {16, 72, 2}, {18, 70, 2}, {20, 72, 3}, {24, 67, 4}, {28, 70, 2}, {30, 72, 2}};
static const uint8_t CHORD[2][4] = {{60, 63, 67, 70}, {58, 62, 65, 68}};
static const uint8_t COMP[] = {0, 3, 6, 10, 12, 16, 19, 22, 26, 28};

static uint8_t held[128];
static uint32_t off_at[128];
static void play(uint32_t note, uint32_t vel, uint32_t off)
{
    trk_note_on(&trk[0], note, vel);
    held[note] = 1;
    off_at[note] = off;
}

static void file_name(char *d, const char *s)       /* a preset name as a file name: A-Z 0-9, the rest '_' */
{
    for (; *s; s++)
        *d++ = (char)((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '-' ? *s : '_');
    *d = 0;
}

int main(int argc, char **argv)
{
    const uint32_t step = FS * 60u / 100u / 4u, nb = (FS * 9u / 2u) / CTL, tail = (FS / 4u) / CTL;
    uint32_t e, pi, b, n, i;
    int32_t o[2 * CTL];
    FILE *raw = getenv("PA_RAW") ? fopen(getenv("PA_RAW"), "wb") : 0;
    int clip = getenv("PA_CLIP") != 0, nowav = getenv("PA_NOWAV") != 0;
    if (argc < 2)
        return 1;
    mkdir(argv[1], 0755);
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        int want = argc == 2;
        char dir[512];
        if (!en || !eng_ok(e))
            continue;
        for (i = 2; i < (uint32_t)argc; i++)
            want |= !strcmp(argv[i], en->name);
        if (!want)
            continue;
        snprintf(dir, sizeof dir, "%s/%s", argv[1], en->name);
        mkdir(dir, 0755);
        for (pi = 0; pi < en->npresets; pi++) {
            const char *name = en->presets[pi].name;
            int role = role_of(name);
            char fn[64], path[600];
            FILE *w;
            uint32_t frames = 0;
            if (e == ENGI_ACID)                                /* as Clipper's: by what the engine is */
                role = has(name, "LEAD") ? R_MELODY : R_BASS;
            else if (e == ENGI_STRINGS)
                role = R_HELD;
            file_name(fn, name);
            snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, pi, fn);
            if (!(w = fopen(nowav ? "/dev/null" : path, "wb")))
                return 1;
            wav_hdr(w, 0);
            host_tracks_init();
            song.g[G_BPM] = 100;
            host_preset(&trk[0], e, pi);
            if (clip)                                      /* (Clipper's TRANS is its keyboard's, seq.c kb_map: */
                trk[0].p[P_TRANS] = 0;                     /* not in its renders' notes) */
            song.sel = 0;
            memset(held, 0, sizeof held);
            for (b = 0; b < FS / CTL; b++)                    /* settle (the last preset's sends die out) */
                mix_block(o, CTL);
            printf("%s %u %d %s\n", en->name, pi, role, name);
            for (b = 0; b < nb + tail; b++) {
                uint32_t s0 = b * CTL, s1 = s0 + CTL, st;
                for (n = 0; n < 128u; n++)
                    if (held[n] && (off_at[n] < s1 || b >= nb)) {
                        trk_note_off(&trk[0], n);
                        held[n] = 0;
                    }
                for (st = 0; st < 32u && b < nb; st++) {
                    uint32_t at = st * step;
                    if (at < s0 || at >= s1)
                        continue;
                    if (role == R_BASS || role == R_MELODY) {
                        const ev_t *ev = role == R_BASS ? BASS : MELODY;
                        uint32_t ne = role == R_BASS ? sizeof BASS / sizeof BASS[0] : sizeof MELODY / sizeof MELODY[0];
                        for (i = 0; i < ne; i++)
                            if (ev[i].step == st)
                                play(ev[i].note, 100, at + ev[i].len * step - step / 4u);
                    } else if (role == R_HELD) {
                        if (st == 0 || st == 16)
                            for (i = 0; i < 4u; i++)
                                play(CHORD[st / 16u][i], 90, at + 15u * step);
                    } else {
                        for (n = 0; n < sizeof COMP; n++)
                            if (COMP[n] == st)
                                for (i = 0; i < (role == R_ONE ? 1u : 4u); i++)
                                    play(CHORD[st / 16u][i], 95, at + 2u * step - step / 3u);
                    }
                }
                mix_block(o, CTL);
                for (i = 0; i < CTL; i++, frames++) {
                    int32_t m = (o[2 * i] + o[2 * i + 1]) / 2;
                    wav_put(w, o[2 * i], o[2 * i + 1]);
                    if (raw)
                        fwrite(&m, 4, 1, raw);
                }
            }
            fseek(w, 0, SEEK_SET);
            wav_hdr(w, frames);
            fclose(w);
        }
    }
    if (raw)
        fclose(raw);
    return 0;
}
