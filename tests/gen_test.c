/* SPDX-License-Identifier: GPL-3.0-only
 * Yehnah (0.6.91) the song generator (firmware/src/gen.c) measured: every genre at several seeds and settings, on the
 * host build of the UI tests. What a piece must always have: the same piece from the same settings (deterministic);
 * one scale on every melodic track, every note in it (the bass's chromatic approaches excepted) and in its register;
 * drums on one track with the kick anchored (four-on-the-floor genres: step 1), its density growing with DRUMS;
 * the bass's first note at a chord change the root (most of the time: FiloBass' 68 %); the lead's strong steps on chord
 * tones more often than not; chords of three or four, voiced in C3..G5, led closely; a storable project; a form in
 * ARRANGE when asked; the parts' knobs only touching their own part. (0.8.6) EXPLORE, the default: no genre, every
 * rule broken, every piece still playing in its key, very different from the next.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/gen_test tests/gen_test.c -lm */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int gcheck(const char *what, int ok)
{
    printf("gen: %-74s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}
static void gen_reset(void)
{
    ui_power_on();
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();          /* TRACKS 6 */
    settings.palette = UI_DEFAULT_INDEX; palette_set(UI_DEFAULT_INDEX);
}
static void gen_with(uint32_t genre, uint32_t seed, uint32_t energy, uint32_t form)
{
    uint32_t k;
    gp.genre = (uint8_t)genre; gp.seed = (uint16_t)seed; gp.energy = (uint8_t)energy; gp.form = (uint8_t)form;
    for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
    gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
    gen_go();
}
static uint32_t all_sig(void)
{
    uint32_t k, h = 2166136261u ^ (uint32_t)song.g[G_BPM];
    for (k = 0; k < NTRK; k++) h = (h ^ steps_sig(&trk[k]) ^ trk[k].eng_req ^ (uint32_t)trk[k].p[P_E4] * 7u) * 16777619u;
    return h;
}
static int in_scale(const track_t *t, int32_t n)
{
    uint32_t mask = scale_mask(t);
    return (mask >> (uint32_t)((n - t->p[P_ROOT] + 120) % 12)) & 1u;
}
static uint32_t hits_of(const track_t *t, uint32_t lane)
{
    uint32_t i, c = 0;
    for (i = 0; i < (uint32_t)t->p[P_SLEN]; i++) c += t->step[i].time == ST_NOTE && ((t->step[i].hit >> lane) & 1u);
    return c;
}
/* (0.8.6) the piece heard for a while: its RMS and peak (the output before the DAC's clamp), the blocks at full scale,
 * the most voices sounding at once over the synth parts */
static void gx_listen(uint32_t blocks, double *rms, int32_t *peak, uint32_t *clip, uint32_t *vmax)
{
    uint32_t b, i, p;
    double acc = 0;
    int32_t pk = 0;
    uint32_t cl = 0, vm = 0;
    song.master_q12 = 4096;
    transport_req = 1;
    for (b = 0; b < blocks; b++) {
        int32_t o[2 * CTL];
        uint32_t nv = 0;
        if (b % 32u == 0u) { ui_input(); song_poll(); host_ticks += 20000u; fm1_ms += 20u; }
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            int32_t v = o[i] < 0 ? -o[i] : o[i];
            if (v > pk) pk = v;
            cl += v >= 32767;
            acc += (double)o[i] * (double)o[i];
        }
        for (p = 0; p < NPART; p++)                     /* (as voice.c voices_busy counts them: not the fading) */
            for (i = 0; i < NVOICE; i++) nv += trk[p].v[i].active && trk[p].v[i].stage != 4u;
        if (nv > vm) vm = nv;
    }
    transport_req = 2;
    { int32_t o[2 * CTL]; ui_input(); mix_block(o, CTL); }
    *rms = sqrt(acc / (double)(blocks * 2u * CTL)); *peak = pk; *clip = cl; *vmax = vm;
}
static uint32_t hits_all(const track_t *t)
{
    uint32_t i, c = 0, l;
    for (i = 0; i < (uint32_t)t->p[P_SLEN]; i++)
        if (t->step[i].time == ST_NOTE) for (l = 0; l < 8u; l++) c += (t->step[i].hit >> l) & 1u;
    return c;
}

int main(void)
{
    int bad = 0;
    uint32_t g, s, k, i;
    uint32_t notes_out = 0, notes_all = 0, chords_bad = 0, lead_strong = 0, lead_ct = 0, bass_changes = 0, bass_root = 0;
    uint32_t drums_many = 0, pieces = 0, kick_anchor_bad = 0, store_bad = 0, range_bad = 0, motion_bad = 0;
    uint32_t genre0 = gp.genre;                         /* (0.8.6: the power-on's GENRE, before any test turns it) */
    gen_reset();
    /* ---- deterministic: the same settings twice, the same piece; another seed, another piece */
    {
        uint32_t h1, h2, h3;
        gen_with(1, 421, 50, 0); h1 = all_sig();
        gen_with(5, 7, 80, 0);
        gen_with(1, 421, 50, 0); h2 = all_sig();
        gen_with(1, 422, 50, 0); h3 = all_sig();
        bad += gcheck("deterministic: HOUSE 0421 twice the same piece, 0422 another", h1 == h2 && h1 != h3);
    }
    /* ---- every genre, three seeds, three energies: the guard rails */
    for (g = 0; g < NGENRE; g++) {
        for (s = 0; s < 3u; s++) {
            uint32_t dr = NTRK, bs = NTRK, ld = NTRK, pd = NTRK, drums = 0;
            gen_with(g + 1u, 100u * g + 17u * s + 3u, s == 0u ? 20u : s == 1u ? 50u : 85u, 0);
            pieces++;
            for (k = 0; k < NTRK; k++) {
                if (drum_track(&trk[k])) { drums++; dr = k; }
                else if (gs.role[k] == RL_BASS && bs == NTRK) bs = k;
                else if (gs.role[k] == RL_LEAD && ld == NTRK) ld = k;
                else if (gs.role[k] == RL_PAD && pd == NTRK) pd = k;
            }
            if (drums != 1u) drums_many++;
            for (k = 0; k < NTRK; k++) {
                const track_t *t = &trk[k];
                uint32_t len = (uint32_t)t->p[P_SLEN];
                if (k == dr || len > NSTEP) continue;
                if (t->p[P_ROOT] != trk[0].p[P_ROOT] || t->p[P_SCALE] != trk[0].p[P_SCALE]) range_bad++;
                for (i = 0; i < len; i++) {
                    const step_t *st = &t->step[i];
                    uint32_t j;
                    if (st->time != ST_NOTE) continue;
                    if (st->n < 1u || st->n > 4u) chords_bad++;
                    for (j = 0; j < st->n; j++) {
                        notes_all++;
                        if (!in_scale(t, st->note[j])) notes_out++;
                        if (st->note[j] > 84u || st->note[j] < 20u) range_bad++;
                    }
                    if (k == pd && st->n >= 3u && (st->note[0] < 46u || st->note[st->n - 1u] > 81u)) { chords_bad++; printf("  chord range: %s seed %u step %u %u..%u\n", gs.g->name, s, i, st->note[0], st->note[st->n - 1u]); }
                }
            }
            if (dr < NTRK && gs.act[RL_DRUM]) {         /* the kick anchored on the one where the genre has it */
                uint32_t bd = DRUM_MAP_I[gs.g->drum][0];   /* (0.6.95: the map as this throw plays it; 0.9.7 packed) */
                if (bd && gm_level(bd, 0) == 255u && !(trk[dr].step[0].time == ST_NOTE && (trk[dr].step[0].hit & 1u))) { kick_anchor_bad++; printf("  kick anchor: %s seed %u map %u\n", gs.g->name, s, gs.g->drum); }
            }
            if (bs < NTRK && gs.act[RL_BASS] && gs.ba != BA_ACID) {   /* the root at the chord changes */
                const track_t *b = &trk[bs];
                uint32_t len = (uint32_t)b->p[P_SLEN], at;
                for (at = 0; at < len; at++) {
                    uint32_t j;
                    int32_t root = (int32_t)gchord_root_pc(at);
                    if (!gchord_starts(at)) continue;
                    for (j = at; j < at + 4u && j < len; j++)
                        if (b->step[j].time == ST_NOTE) {
                            bass_changes++;
                            if (b->step[j].note[0] % 12u == (uint32_t)root) bass_root++;
                            break;
                        }
                }
            }
            if (ld < NTRK && gs.act[RL_LEAD]) {         /* the lead's strong steps on chord tones */
                const track_t *l = &trk[ld];
                uint32_t len = (uint32_t)l->p[P_SLEN];
                for (i = 0; i < len; i += 4u) {
                    const step_t *st = &l->step[i];
                    if (st->time != ST_NOTE) continue;
                    lead_strong++;
                    if ((gchord_pcs(i) >> (st->note[0] % 12u)) & 1u) lead_ct++;
                }
            }
            project_capture(&proj_scratch);
            if (!proj_pack(&proj_wire, &proj_scratch)) store_bad++;
            if (motion.count > MOTION_MAX) motion_bad++;
            for (i = 0; i < 16u; i++) events_block(CTL);   /* (it plays: no crash) */
        }
    }
    printf("gen: %u pieces, %u notes, %u off the scale (%u %%), bass root at %u of %u changes, lead chord tones %u of %u strong\n",
           pieces, notes_all, notes_out, notes_all ? notes_out * 100u / notes_all : 0u, bass_root, bass_changes, lead_ct, lead_strong);
    bad += gcheck("every genre x 3: one DRUM track, one scale, every note in it but the approaches (< 4 %), in its register",
                  !drums_many && !range_bad && notes_out * 100u < notes_all * 4u);
    bad += gcheck("  chords of 1..4 notes, the pads' in C3..G5; the kick on the one where the map anchors it", !chords_bad && !kick_anchor_bad);
    bad += gcheck("  the bass's first note at a chord change the root, 70 % at least (FiloBass: 68 %)", bass_root * 10u >= bass_changes * 7u);
    bad += gcheck("  the lead's strong steps on chord tones, 55 % at least", lead_ct * 100u >= lead_strong * 55u);
    bad += gcheck("  every piece storable, the automation inside its 64", !store_bad && !motion_bad);
    /* ---- (0.8.3) the music: twelve seeds of every genre at varied ENERGY and COMPLEX, measured as a musician would
     * listen: every note in the key the SCALE page shows, but the chromaticism the rules allow (a semitone approach off
     * beats 1 and 3 that resolves, a borrowed / secondary / raised chord's own tones where the genre colours) and few
     * of those; the SCALE page a mode of seven, QUANT on, the CHORD page on the chord track (an in-scale chord type,
     * VOICING LEAD) and every chord it builds in the key; the lead's beats on chord tones, its motion mostly by steps,
     * never past an octave, a leap past a fourth turned back by a step, its phrases closing on a stable degree, its
     * first bar's rhythm coming back; the chords led closely; the bass on the root at every chord change */
    {
        uint32_t nn = 0, out_appr = 0, out_ct = 0, out_bad = 0, page_bad = 0, chord_bad = 0;
        uint32_t ls = 0, lct = 0, liv = 0, lrep = 0, lstep = 0, loct = 0, l5 = 0, l5r = 0, lend = 0, lend_ok = 0, lbars = 0, lbars_r = 0;
        uint32_t pmov = 0, pmot = 0, bch = 0, broot = 0;
        for (g = 0; g < NGENRE; g++)
            for (s = 0; s < 12u; s++) {
                uint32_t pd;
                gp.genre = (uint8_t)(g + 1u); gp.seed = (uint16_t)(2000u + 41u * s + g); gp.energy = (uint8_t)(15u + (s * 23u) % 80u); gp.form = 0;
                for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
                gp.complex = (uint8_t)(10u + (s * 31u) % 85u); gp.swing = GA_AUTO; gp.human = 30;
                gen_go();
                for (k = 0; k < NTRK; k++) {            /* the SCALE page */
                    const track_t *t = &trk[k];
                    uint32_t c = 0, b;
                    for (b = 0; b < 12u; b++) c += (scale_mask(t) >> b) & 1u;
                    if (t->p[P_ROOT] != gs.root || t->p[P_SCALE] != gs.scale || c != 7u || t->p[P_QUANT] == QN_OFF) page_bad++;
                }
                pd = gs.trk_of[RL_PAD];
                if (pd < NTRK && gs.act[RL_PAD]) {      /* the CHORD page: an in-scale type, LEAD, every chord in the key */
                    track_t *t = &trk[pd];
                    uint8_t o[KB_CH];
                    uint32_t key, c, j;
                    if (t->p[P_CHRD] < CD_TRI || t->p[P_CHRD] > CD_SUS4 || t->p[P_CVOI] != CV_LEAD) page_bad++;
                    for (key = 53; key < 80u; key++)
                        for (c = chord_build(t, key, o), j = 0; j < c; j++) if (!in_scale(t, o[j])) chord_bad++;
                }
                for (k = 0; k < NTRK; k++) {
                    const track_t *t = &trk[k];
                    uint32_t r = gs.role[k], len = dlen(t);
                    if (drum_track(t) || !gs.act[r]) continue;
                    for (i = 0; i < len; i++) {         /* every note in the key, or chromatic by a rule */
                        const step_t *st = &t->step[i];
                        uint32_t j;
                        if (st->time != ST_NOTE) continue;
                        for (j = 0; j < st->n; j++) {
                            int32_t n = st->note[j], nx = -1;
                            uint32_t q;
                            const gch_t *c = gchord(i);
                            nn++;
                            if (in_scale(t, n)) continue;
                            for (q = 1; q <= len && nx < 0; q++) if (t->step[(i + q) % len].time == ST_NOTE) nx = t->step[(i + q) % len].note[0];
                            if (c->alt && ((gchord_pcs(i) >> (uint32_t)(n % 12)) & 1u) &&
                                (gs.hx & (HX_RAISE | HX_SEC | HX_BOR) || HXF(gs.hx) == HF_JAZZ)) out_ct++;
                            else if (st->n == 1u && i % 8u && nx >= 0 && (nx - n == 1 || n - nx == 1)) out_appr++;
                            else { out_bad++; if (out_bad < 6u) printf("  off the key: %s seed %u T%u step %u note %d\n", gs.g->name, gp.seed, k + 1u, i, n); }
                        }
                    }
                    if (r == RL_LEAD) {
                        int32_t prev = -1, piv = 0, last = -1;
                        uint32_t pend = 0;
                        uint16_t bar0 = 0;
                        for (i = 0; i < len; i++) {
                            const step_t *st = &t->step[i];
                            int32_t n, iv, a2;
                            if (st->time != ST_NOTE || !st->n) continue;
                            n = st->note[0];
                            if (i % 4u == 0u) { ls++; lct += (gchord_pcs(i) >> (uint32_t)(n % 12)) & 1u; }
                            if (i < 16u) bar0 |= (uint16_t)(1u << i);
                            if (prev >= 0) {
                                iv = n - prev; a2 = iv < 0 ? -iv : iv;
                                liv++; lrep += !a2; lstep += a2 && a2 <= 2; loct += a2 > 12;
                                if (pend) { l5++; l5r += a2 && a2 <= 2 && (iv > 0) != (piv > 0); pend = 0; }
                                if (a2 > 5) { pend = 1; piv = iv; }
                            }
                            prev = n; last = (int32_t)i;
                        }
                        if (last >= 0 && gs.g->lead != LD_OSTINATO && gs.g->lead != LD_MECH) {   /* (the phrase's close) */
                            int32_t n = t->step[last].note[0], d = ((n - gs.root) % 12 + 12) % 12;
                            lend++;
                            lend_ok += ((gchord_pcs((uint32_t)last) >> (uint32_t)(n % 12)) & 1u) && (d == 0 || d == 3 || d == 4 || d == 7);
                        }
                        if (gs.g->lead == LD_MOTIF || gs.g->lead == LD_HOOK)
                            for (i = 16; i + 16u <= len; i += 16u) {   /* (the motif's rhythm back: two onsets moved at most) */
                                uint16_t bb = 0;
                                uint32_t q, d = 0;
                                for (q = 0; q < 16u; q++) if (t->step[i + q].time == ST_NOTE && t->step[i + q].n) bb |= (uint16_t)(1u << q);
                                if (!bb) continue;
                                for (q = 0; q < 16u; q++) d += ((bb ^ bar0) >> q) & 1u;
                                lbars++; lbars_r += d <= 2u;
                            }
                    }
                    if (r == RL_PAD && gs.vm != VM_PLANE) {   /* (the chords led: semitones moved, voice by voice) */
                        int32_t pv[4];
                        uint32_t pn = 0, c, j;
                        for (i = 0; i < len; i++) {
                            const step_t *st = &t->step[i];
                            int same = 1, mv = 0;
                            if (st->time != ST_NOTE || st->n < 2u) continue;
                            c = st->n;
                            for (j = 0; j < c && j < pn; j++) if (st->note[j] != pv[j]) same = 0;
                            if (pn && (!same || c != pn)) {
                                for (j = 0; j < c && j < pn; j++) mv += st->note[j] > pv[j] ? st->note[j] - pv[j] : pv[j] - st->note[j];
                                pmov++; pmot += (uint32_t)mv;
                            }
                            for (j = 0; j < c; j++) pv[j] = st->note[j];
                            pn = c;
                        }
                    }
                    if (r == RL_BASS)                   /* (the root at each chord change, within its beat) */
                        for (i = 0; i < len; i++) {
                            uint32_t j;
                            if (!gchord_starts(i)) continue;
                            for (j = i; j < i + 4u && j < len; j++)
                                if (t->step[j].time == ST_NOTE && t->step[j].n) {
                                    bch++; broot += t->step[j].note[0] % 12u == gchord_root_pc(i);
                                    break;
                                }
                        }
                }
            }
        printf("gen: music: %u notes: %u approaches, %u borrowed/secondary/raised chord tones, %u off the rules\n", nn, out_appr, out_ct, out_bad);
        printf("gen: music: lead beats on chord tones %u of %u, steps %u of %u moving intervals, %u past an octave, leaps back %u of %u\n",
               lct, ls, lstep, liv - lrep, loct, l5r, l5);
        printf("gen: music: phrases closing stable %u of %u, the motif's rhythm back in %u of %u bars, chords moved %u.%u semitones, bass roots %u of %u\n",
               lend_ok, lend, lbars_r, lbars, pmov ? pmot / pmov : 0u, pmov ? pmot * 10u / pmov % 10u : 0u, broot, bch);
        bad += gcheck("music: every note in the key but the rules' chromaticism (approaches < 2 %, coloured chords' tones < 3 %)",
                      !out_bad && out_appr * 50u < nn && out_ct * 33u < nn);
        bad += gcheck("  the SCALE page a mode of seven on every track, QUANT on; the CHORD page in-scale, LEAD, its chords in the key", !page_bad && !chord_bad);
        bad += gcheck("  the lead's beats on chord tones (75 % at least)", lct * 100u >= ls * 75u);
        bad += gcheck("  the lead moves mostly by steps (over half its moves), never past an octave", lstep * 2u > liv - lrep && !loct);
        bad += gcheck("  a leap past a fourth turned back by a step (70 % at least)", l5r * 100u >= l5 * 70u);
        bad += gcheck("  phrases close on a stable chord tone (70 %), the motif's rhythm comes back (70 % of its bars)",
                      lend_ok * 100u >= lend * 70u && lbars_r * 100u >= lbars * 70u);
        bad += gcheck("  the chords led closely (5.2 semitones a change at most, voice by voice); the bass's root at every change (95 %)",
                      pmot * 10u <= pmov * 52u && broot * 100u >= bch * 95u);
    }
    /* ---- (0.8.3) the sections relate: a pop genre's B on its predominant (the chords turned), its last section a whole
     * step up (a real modulation: the section's SCALE page moved with it, its notes in that key); a loop genre's
     * sections in the one key */
    {
        static project_t pa, pb, pc;
        uint32_t seeds, lift_ok = 0, rot_ok = 0, loop_ok = 0, keyc_bad = 0, n_c = 0;
        for (seeds = 1; seeds <= 6u; seeds++) {
            uint32_t bs, a0, b0;
            gen_reset();
            gen_with(15, seeds, 50, 1);                 /* SYNTH POP, PARTS: A B A C */
            bs = gs.trk_of[RL_BASS];
            if (!proj_import(&pa, &proj_slot[0], sizeof(project_store_t)) || !proj_import(&pb, &proj_slot[1], sizeof(project_store_t)) ||
                !proj_import(&pc, &proj_slot[2], sizeof(project_store_t))) continue;
            lift_ok += pc.t[bs].p[P_ROOT] == (pa.t[bs].p[P_ROOT] + 2) % 12 && pc.t[0].p[P_ROOT] == pc.t[bs].p[P_ROOT];
            for (k = 0; k < NTRK; k++) {                /* C's notes in C's key */
                track_t tt;
                if (gs.role[k] == RL_DRUM) continue;
                memcpy(tt.p, pc.t[k].p, sizeof tt.p);
                memcpy(tt.step, pc.t[k].step, sizeof tt.step);
                proj_steps(tt.step);
                for (i = 0; i < NSTEP; i++) {
                    uint32_t j;
                    if (tt.step[i].time != ST_NOTE) continue;
                    for (j = 0; j < tt.step[i].n; j++) { n_c++; keyc_bad += !in_scale(&tt, tt.step[i].note[j]); }
                }
            }
            {   /* B: the chords turned (its first bass note another root than A's) */
                step_t sa[NSTEP], sb[NSTEP];
                memcpy(sa, pa.t[bs].step, sizeof sa); proj_steps(sa);
                memcpy(sb, pb.t[bs].step, sizeof sb); proj_steps(sb);
                for (i = 0, a0 = b0 = 0; i < NSTEP && (!a0 || !b0); i++) {
                    if (!a0 && sa[i].time == ST_NOTE) a0 = sa[i].note[0] % 12u + 1u;
                    if (!b0 && sb[i].time == ST_NOTE) b0 = sb[i].note[0] % 12u + 1u;
                }
                rot_ok += a0 && b0 && a0 != b0;
            }
            gen_reset();
            gen_with(6, seeds, 50, 1);                  /* TECHNO: one key, the loop kept */
            if (proj_import(&pa, &proj_slot[0], sizeof(project_store_t)) && proj_import(&pc, &proj_slot[2], sizeof(project_store_t)))
                loop_ok += pa.t[gs.trk_of[RL_BASS]].p[P_ROOT] == pc.t[gs.trk_of[RL_BASS]].p[P_ROOT];
        }
        printf("gen: sections: SYNTH POP lifted %u of 6, its C notes off C's key %u of %u, B turned %u of 6; TECHNO one key %u of 6\n",
               lift_ok, keyc_bad, n_c, rot_ok, loop_ok);
        bad += gcheck("FORM PARTS (0.8.3): SYNTH POP's C a whole step up (its SCALE page with it), B on another chord; TECHNO's one key",
                      lift_ok == 6u && keyc_bad * 33u < n_c && rot_ok >= 5u && loop_ok == 6u);
    }
    /* ---- the DRUMS knob: the density grows with it, the kick stays */
    {
        uint32_t h0, h1, h2, k0, k2;
        gen_reset();
        gp.genre = 1; gp.seed = 77; gp.energy = 50; gp.form = 0; gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
        for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
        gp.amt[GP_DRUMS] = 20; gen_go(); h0 = hits_all(&trk[gs.trk_of[RL_DRUM]]); k0 = hits_of(&trk[gs.trk_of[RL_DRUM]], 0);
        gp.amt[GP_DRUMS] = 60; gen_go(); h1 = hits_all(&trk[gs.trk_of[RL_DRUM]]);
        gp.amt[GP_DRUMS] = 100; gen_go(); h2 = hits_all(&trk[gs.trk_of[RL_DRUM]]); k2 = hits_of(&trk[gs.trk_of[RL_DRUM]], 0);
        printf("gen: HOUSE drums at 20 / 60 / 100 %%: %u %u %u hits, the kick %u -> %u\n", h0, h1, h2, k0, k2);
        bad += gcheck("DRUMS 20 < 60 < 100 %: more hits each time, the kick's anchors from the start", h0 < h1 && h1 < h2 && k0 >= 4u && k2 >= k0);
    }
    /* ---- a part's knob touches its part only: BASS turned, the drums and the lead the same */
    {
        uint32_t d0, l0, b0, d1, l1, b1;
        gen_reset();
        gen_with(7, 500, 50, 0);
        d0 = steps_sig(&trk[gs.trk_of[RL_DRUM]]); l0 = steps_sig(&trk[gs.trk_of[RL_LEAD]]); b0 = steps_sig(&trk[gs.trk_of[RL_BASS]]);
        gp.amt[GP_BASS] = 30; gen_go();
        d1 = steps_sig(&trk[gs.trk_of[RL_DRUM]]); l1 = steps_sig(&trk[gs.trk_of[RL_LEAD]]); b1 = steps_sig(&trk[gs.trk_of[RL_BASS]]);
        bad += gcheck("BASS turned: the bass changes, the drums and the lead stay (a stream a part)", d0 == d1 && l0 == l1 && b0 != b1);
    }
    /* ---- a part OFF: its track silent; FORM: PARTS writes sections and ARRANGE */
    {
        gen_reset();
        gp.genre = 2; gp.seed = 9; gp.energy = 50; gp.form = 0;
        for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
        gp.amt[GP_LEAD] = 0; gp.amt[GP_ARP] = 0; gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
        gen_go();
        {
            uint32_t on = 0;
            for (k = 0; k < NTRK; k++)
                if (gs.role[k] == RL_LEAD || gs.role[k] == RL_ARP)
                    for (i = 0; i < (uint32_t)trk[k].p[P_SLEN]; i++) on += trk[k].step[i].time == ST_NOTE;
            bad += gcheck("LEAD and ARP OFF: their tracks silent (the sound kept)", on == 0u);
        }
        gen_reset();
        gen_with(1, 31, 60, 1);
        bad += gcheck("FORM PARTS: sections written, ARRANGE filled, PLAY the song",
                      live.mode == 1 && chain_config.count >= 2u && (dice_secs & 3u) == 3u && SONG_END(&chain_config) == SONG_FOREVER);
        for (k = 0; k < chain_config.count; k++) if (chain_config.row[k].slot > 3u || !chain_config.row[k].bars) bad += gcheck("  a row's section and bars", 0);
        gen_reset();
        gen_with(1, 31, 60, 2);
        bad += gcheck("FORM SONG (HOUSE): four sections (intro, build, drop, break), six rows, the drop longest",
                      live.mode == 1 && chain_config.count == 6u && (dice_secs & 15u) == 15u &&
                      chain_config.row[2].slot == 0u && chain_config.row[2].bars > chain_config.row[0].bars);
        {   /* the BUILD section holds automation on the chords (the filter opening), the BREAK no drums */
            project_t p;
            int ok = proj_import(&p, &proj_slot[2], sizeof(project_store_t)) && p.motion.count >= 4u;
            ok &= proj_import(&p, &proj_slot[3], sizeof(project_store_t)) && p.t[gs.trk_of[RL_DRUM]].p[P_MUTE] == 1;
            bad += gcheck("  the BUILD carries the filter's automation, the BREAK mutes the drums", ok);
        }
        {   /* the live music after: section A, nothing muted */
            uint32_t m = 0;
            for (k = 0; k < NTRK; k++) m += trk[k].p[P_MUTE] != 0;
            bad += gcheck("  after: the whole piece live (A), nothing muted", m == 0u && live.cur == 0);
        }
    }
    /* ---- (0.6.97) the kit's space, the top end's variety, Hook's register, the lead's envelope */
    {
        uint32_t seeds, rows = 0, bs, lo = 127, hi = 0, i2, sus_lt = 0, leads = 0;
        uint32_t seen[64]; uint32_t nseen = 0;
        gen_reset();
        for (seeds = 1; seeds <= 8u; seeds++) {         /* HOUSE: the closed hats' row over eight seeds */
            uint32_t h = 2166136261u, d;
            gen_with(1, seeds, 50, 0);
            d = gs.trk_of[RL_DRUM];
            for (i2 = 0; i2 < 16u; i2++) h = (h ^ (uint32_t)(trk[d].step[i2].time == ST_NOTE ? (trk[d].step[i2].hit >> 3) & 3u : 0u)) * 16777619u;
            for (i2 = 0; i2 < nseen && seen[i2] != h; i2++) {}
            if (i2 == nseen) seen[nseen++] = h;
        }
        rows = nseen;
        bad += gcheck("HOUSE x 8 seeds: three hat rows at least (the pools)", rows >= 3u);
        gen_with(1, 3, 50, 0);
        bad += gcheck("the kit's snare and clap into the reverb (SEND bus 3, 6..63)", DXS_BUS(dx_snd[DV_SNARE]) == 3u && DXS_AMT(dx_snd[DV_SNARE]) >= 6 &&
                      DXS_BUS(dx_snd[DV_CLAP]) == 3u);
        for (seeds = 1; seeds <= 4u; seeds++) {         /* POST PUNK: the bass high on the neck, never below E2 nor above A3 */
            gen_with(17, seeds, 50, 0);
            bs = gs.trk_of[RL_BASS];
            for (i2 = 0; i2 < dlen(&trk[bs]); i2++)
                if (trk[bs].step[i2].time == ST_NOTE) { if (trk[bs].step[i2].note[0] < lo) lo = trk[bs].step[i2].note[0]; if (trk[bs].step[i2].note[0] > hi) hi = trk[bs].step[i2].note[0]; }
            if (gs.act[RL_LEAD]) { leads++; sus_lt += trk[gs.trk_of[RL_LEAD]].p[P_SUS] < 95; }
        }
        if (lo < 40u || hi > 70u) printf("  post punk bass %u..%u\n", lo, hi);
        bad += gcheck("POST PUNK x 4: the bass between E2 and A#4 (Hook's neck), its tempo 118..148", lo >= 40u && hi <= 70u && song.g[G_BPM] >= 118 && song.g[G_BPM] <= 148);
        bad += gcheck("  the leads not all at full sustain", !leads || sus_lt > 0u);
    }
    /* ---- (0.6.96) a new piece: MUTATE off on every track (a reroll kept the lanes), the anchor the piece */
    {
        uint32_t on = 0;
        gen_reset();
        gen_with(1, 5, 50, 0);
        trk[3].p[P_MSND] = 3; trk[0].p[P_MPAT] = 2;
        gen_go();
        for (k = 0; k < NTRK; k++) on += trk[k].p[P_MSND] || trk[k].p[P_MPAT];
        bad += gcheck("a new piece: MUTATE SOUND / PATTERN OFF on every track, the anchor kept", !on && mut.ok);
    }
    /* ---- (0.6.95) a throw its own: HOUSE over twelve seeds takes more than one drum map and more than one chord
     * play; RANDOM over forty seeds throws mutants ("A+B") a third of the time or so, each playable and storable */
    {
        uint32_t seeds, maps = 0, plays = 0, mut = 0, bad_mut = 0;
        uint8_t seen_m[64] = {0}, seen_p[16] = {0};
        gen_reset();
        for (seeds = 1; seeds <= 12u; seeds++) {
            gen_with(1, seeds, 50, 0);
            if (!seen_m[gs.g->drum]) { seen_m[gs.g->drum] = 1; maps++; }
            if (!seen_p[gs.g->pads]) { seen_p[gs.g->pads] = 1; plays++; }
        }
        bad += gcheck("HOUSE x 12 seeds: two drum maps at least, two chord plays at least", maps >= 2u && plays >= 2u);
        for (seeds = 1; seeds <= 40u; seeds++) {
            gen_with(0, seeds, 50, 0);
            if (gs.mutant) {
                mut++;
                if (!strchr(gs.g->name, '+') || gs.g->bpm_hi < gs.g->bpm_lo || strncmp(ui.msg, "GEN: ", 5)) bad_mut++;
                project_capture(&proj_scratch);
                if (!proj_pack(&proj_wire, &proj_scratch)) bad_mut++;
                for (i = 0; i < 16u; i++) events_block(CTL);
            }
        }
        if (mut < 8u || mut > 28u || bad_mut) printf("  mutants: %u of 40, %u bad\n", mut, bad_mut);
        bad += gcheck("RANDOM x 40 seeds: 8..28 mutants, named A+B, a tempo range, storable, playing", mut >= 8u && mut <= 28u && !bad_mut);
    }
    /* ---- (0.6.94) the snare a snare: mostly noise, shorter than the factory kit's, its own FEEDBK */
    {
        gen_reset();
        gen_with(1, 5, 50, 0);                          /* HOUSE (a drum track) */
        if (dx_kit[DV_SNARE].a[DXP_NOISE] < 100u) printf("  snare: noise %u / %u, decay %u, lay %02x\n", dx_kit[DV_SNARE].a[DXP_NOISE], dx_kit[DV_SNARE].b[DXP_NOISE], dx_kit[DV_SNARE].a[DXP_DECAY], dx_lay[DV_SNARE]);
        bad += gcheck("the generated snare: NOISE >= 100 both ends, DECAY under the factory kit's, FEEDBK 3..6, LAYER kept",
                      dx_kit[DV_SNARE].a[DXP_NOISE] >= 100u && dx_kit[DV_SNARE].b[DXP_NOISE] >= 100u &&
                      dx_kit[DV_SNARE].a[DXP_DECAY] < DX_KIT_DEF[DV_SNARE].a[DXP_DECAY] && DX_LAY_FB(dx_lay[DV_SNARE]) >= 3u &&
                      DX_LAY_FB(dx_lay[DV_SNARE]) <= 6u && DX_LAY_L(dx_lay[DV_SNARE]) == DX_LAY_L(DX_LAY_DEF[DV_SNARE]));
        bad += gcheck("the generated hats: a band-pass (not the factory HP), COLOR >= 120, NOISE >= 112, the kick and clap as the kit's",
                      ((dx_kit[DV_HATC].mode >> 2) & 3u) == DXF_BP && ((dx_kit[DV_HATO].mode >> 2) & 3u) == DXF_BP &&
                      dx_kit[DV_HATC].a[DXP_COLOR] >= 120u && dx_kit[DV_HATO].b[DXP_COLOR] >= 120u &&
                      dx_kit[DV_HATC].a[DXP_NOISE] >= 112u && dx_kit[DV_HATO].a[DXP_NOISE] >= 112u &&
                      dx_kit[DV_KICK].mode == DX_KIT_DEF[DV_KICK].mode && dx_kit[DV_CLAP].mode == DX_KIT_DEF[DV_CLAP].mode);
    }
    /* ---- (0.6.93) the ARP amount is its density: a fifth of the steps at 20 %, most of them at 100 % */
    {
        uint32_t n20 = 0, n100 = 0, i, ar;
        gen_reset();
        gen_with(1, 5, 50, 0);                          /* HOUSE */
        ar = gs.trk_of[RL_ARP];
        gp.amt[GP_ARP] = 20; gen_go();
        for (i = 0; i < dlen(&trk[ar]); i++) n20 += trk[ar].step[i].time == ST_NOTE;
        gp.amt[GP_ARP] = 100; gen_go();
        for (i = 0; i < dlen(&trk[ar]); i++) n100 += trk[ar].step[i].time == ST_NOTE;
        if (n20 * 2u > dlen(&trk[ar]) || n100 < n20 * 2u) printf("  arp: %u notes at 20 %%, %u at 100 %% (LEN %u)\n", n20, n100, dlen(&trk[ar]));
        bad += gcheck("ARP 20 %: eighths, under half the steps; 100 %: at least twice as many notes", ar < NTRK && n20 * 2u <= dlen(&trk[ar]) && n100 >= n20 * 2u);
    }
    /* ---- (0.8.6) EXPLORE, the dice's default: at power-on, on NEW PROJECT, where the screen opens; sixty seeds, each
     * a piece that plays (two melodic parts at least, sound at a safe level, the voice and engine budgets), its SCALE and
     * CHORD pages the key it plays in (every note in it but the coloured chords' and the approaches), storable; and
     * very different from the next: the tempos, the keys, the loops' lengths and DIVs, the drum maps, the engines
     * (ANIMAL and VOICE among them), the components, PLAYHEADS. A genre's piece the same before and after one */
    {
        static const uint32_t SEEDS = 60u;
        uint32_t sd, nmel_bad = 0, page_bad = 0, chord_bad = 0, key_bad = 0, key_col = 0, key_appr = 0, nn = 0;
        uint32_t store_bad = 0, budget_bad = 0, quiet = 0, loud = 0, odd = 0, poly = 0, divs = 0, comps = 0, lms = 0, heads = 0;
        uint32_t nbpm = 0, nsc = 0, nlen = 0, ndm = 0, neng = 0, same = 0, bpm_lo = 999, bpm_hi = 0, name_bad = 0;
        uint8_t seen_bpm[256] = {0}, seen_sc[16] = {0}, seen_len[NSTEP + 1] = {0}, seen_dm[64] = {0}, seen_eng[NENGINES] = {0};
        uint32_t sig[60];
        double rms_min = 1e9;
        int32_t peak_max = 0;
        uint32_t vmax_all = 0;
        uint32_t vref = 0;
        bad += gcheck("EXPLORE (0.8.6): GENRE EXPLORE at power-on", genre0 == GEN_EXPLORE);
        gen_reset();
        for (g = 0; g < 3u * NGENRE; g++) {             /* (the voices the genres' pieces reach at once, the budget's
                                                         * own overshoot included: the reference) */
            double rms;
            int32_t peak;
            uint32_t clip, vmax;
            gen_with(g % NGENRE + 1u, g * 31u + 5u, 20u + (g / NGENRE) * 30u, 0);
            gx_listen(1200u, &rms, &peak, &clip, &vmax);
            if (vmax > vref) vref = vmax;
        }
        for (sd = 0; sd < SEEDS; sd++) {
            uint32_t mel = 0, fm = 0, an = 0, vo = 0, dk = 0, l0 = 0, lens_differ = 0, clip, vmax, kk;
            double rms;
            int32_t peak;
            uint32_t h = 2166136261u;
            if (sd % 12u == 0u) gen_reset();
            gp.genre = (uint8_t)GEN_EXPLORE; gp.seed = (uint16_t)(sd * 163u + 7u); gp.energy = (uint8_t)(10u + (sd * 37u) % 85u);
            gp.form = 0;
            for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
            gp.complex = (uint8_t)((sd * 29u) % 100u); gp.swing = GA_AUTO; gp.human = 30;
            gen_go();
            if (strncmp(ui.msg, "GEN: EXPLORE ", 13) || strcmp(gs.g->name, "EXPLORE")) name_bad++;
            for (k = 0; k < NTRK; k++) {
                const track_t *t = &trk[k];
                uint32_t r = gs.role[k], len = dlen(t), on = 0, c = 0, b;
                if (drum_track(t)) { dk++; if (gs.act[RL_DRUM]) for (i = 0; i < len; i++) lms += step_lm_any(&t->step[i]) != 0; continue; }
                fm += t->eng_req == ENGI_FM6; an += t->eng_req == ENGI_ANIMAL; vo += t->eng_req == EG_VO;
                if (!seen_eng[t->eng_req % NENGINES]) { seen_eng[t->eng_req % NENGINES] = 1; neng++; }
                for (b = 0; b < 12u; b++) c += (scale_mask(t) >> b) & 1u;
                if (t->p[P_ROOT] != gs.root || t->p[P_SCALE] != gs.scale || c != 7u || t->p[P_QUANT] == QN_OFF) page_bad++;
                if (!gs.act[r] || (r == RL_TEX && k != gs.trk_of[RL_TEX])) continue;
                if (!l0) l0 = len; else if (len != l0) lens_differ = 1;
                if (!seen_len[len]) { seen_len[len] = 1; nlen++; }
                odd += len % 4u != 0u;
                divs += t->p[P_SDIV] != 2;
                for (i = 0; i < len; i++) {
                    const step_t *st = &t->step[i];
                    uint32_t j;
                    if (st->time != ST_NOTE) continue;
                    on++;
                    comps += st->comp != 0;
                    for (j = 0; j < st->n; j++) {
                        int32_t n = st->note[j], nx = -1;
                        uint32_t q;
                        nn++;
                        if (n < 20 || n > 96) { key_bad++; printf("  EXPLORE out of range: seed %u T%u step %u note %d\n", gp.seed, k + 1u, i, n); }
                        if (in_scale(t, n)) continue;
                        for (q = 1; q <= len && nx < 0; q++) if (t->step[(i + q) % len].time == ST_NOTE) nx = t->step[(i + q) % len].note[0];
                        if (gchord(i)->alt && ((gchord_pcs(i) >> (uint32_t)(n % 12)) & 1u)) key_col++;
                        else if (st->n == 1u && nx >= 0 && (nx - n == 1 || n - nx == 1)) key_appr++;
                        else { key_bad++; if (key_bad < 6u) printf("  EXPLORE off the key: seed %u T%u step %u note %d\n", gp.seed, k + 1u, i, n); }
                    }
                }
                mel += on > 0u;
                for (kk = 0; kk < FG_HEADS; kk++) heads += fg_set[k][kk].rate != 0;
            }
            poly += lens_differ;
            if (mel < 2u) { nmel_bad++; printf("  EXPLORE seed %u: %u melodic parts playing\n", gp.seed, mel); }
            if (fm > 2u || an > 2u || vo > 2u || dk > 1u) budget_bad++;
            {   /* the CHORD page: an in-scale type on the chord track, its chords in the key */
                uint32_t pd = gs.trk_of[RL_PAD];
                if (pd < NTRK && gs.act[RL_PAD]) {
                    track_t *t = &trk[pd];
                    uint8_t o[KB_CH];
                    uint32_t key, c, j;
                    if (t->p[P_CHRD] < CD_TRI || t->p[P_CHRD] > CD_SUS4 || t->p[P_CVOI] != CV_LEAD) page_bad++;
                    for (key = 53; key < 80u; key++)
                        for (c = chord_build(t, key, o), j = 0; j < c; j++) if (!in_scale(t, o[j])) chord_bad++;
                }
            }
            if (!seen_bpm[song.g[G_BPM] & 255]) { seen_bpm[song.g[G_BPM] & 255] = 1; nbpm++; }
            if ((uint32_t)song.g[G_BPM] < bpm_lo) bpm_lo = (uint32_t)song.g[G_BPM];
            if ((uint32_t)song.g[G_BPM] > bpm_hi) bpm_hi = (uint32_t)song.g[G_BPM];
            if (!seen_sc[gs.scale & 15]) { seen_sc[gs.scale & 15] = 1; nsc++; }
            if (!seen_dm[gs.g->drum & 63]) { seen_dm[gs.g->drum & 63] = 1; ndm++; }
            project_capture(&proj_scratch);
            if (!proj_pack(&proj_wire, &proj_scratch) || motion.count > MOTION_MAX) store_bad++;
            h = all_sig();
            for (i = 0; i < sd; i++) same += sig[i] == h;
            sig[sd] = h;
            gx_listen(sd < 20u ? 2400u : 800u, &rms, &peak, &clip, &vmax);   /* (~1.6 s, then ~0.5 s) */
            if (rms < rms_min) rms_min = rms;
            if (peak > peak_max) peak_max = peak;
            if (vmax > vmax_all) vmax_all = vmax;
            if (rms < 150.0) { quiet++; printf("  EXPLORE seed %u: RMS %.0f\n", gp.seed, rms); }
            if (clip * 100u > 800u * 2u * CTL) { loud++; printf("  EXPLORE seed %u: %u samples at full scale\n", gp.seed, clip); }
        }
        printf("gen: EXPLORE x %u: %u tempos (%u..%u BPM), %u keys, %u loop lengths (%u odd), %u drum maps, %u engines, polyrhythm in %u,\n"
               "gen:   %u DIVs off 1/16, %u components, %u DRUM-X octave steps, %u playheads; %u notes: %u coloured, %u approaches;\n"
               "gen:   RMS %.0f at least, peak %d, %u voices at most (the genres: %u)\n",
               SEEDS, nbpm, bpm_lo, bpm_hi, nsc, nlen, odd, ndm, neng, poly, divs, comps, lms, heads, nn, key_col, key_appr, rms_min,
               peak_max, vmax_all, vref);
        bad += gcheck("  60 seeds: each named EXPLORE nnnn, each another piece (no two the same)", !name_bad && !same);
        bad += gcheck("  each plays: two melodic parts at least, sound (RMS), never stuck at full scale", !nmel_bad && !quiet && !loud);
        bad += gcheck("  the budgets: FM6 / ANIMAL / VOICE on two tracks at most, one DRUM track, no more voices than a genre's",
                      !budget_bad && vmax_all <= vref);
        bad += gcheck("  the SCALE page its key on every track (a mode of seven, QUANT on); the CHORD page in-scale, its chords in the key",
                      !page_bad && !chord_bad);
        bad += gcheck("  every note in the key but the coloured chords' tones and the approaches (< 12 %), in range",
                      !key_bad && (key_col + key_appr) * 100u < nn * 12u);
        bad += gcheck("  varied: 20 tempos from under 90 to over 150 BPM, 6 keys, 12 loop lengths (odd ones), 12 drum maps",
                      nbpm >= 20u && bpm_lo < 90u && bpm_hi > 150u && nsc >= 6u && nlen >= 12u && odd > 0u && ndm >= 12u);
        bad += gcheck("  the rules broken: polyrhythm (a third of the pieces), DIVs, components, DRUM-X octaves, PLAYHEADS",
                      poly * 3u >= SEEDS && divs && comps && lms && heads);
        bad += gcheck("  any engine: six at least, ANIMAL and VOICE among them",
                      neng >= 6u && seen_eng[ENGI_ANIMAL] && seen_eng[EG_VO]);
        bad += gcheck("  every piece storable, the automation inside its 64", !store_bad);
    }
    {   /* EXPLORE deterministic; its FORMs; a genre's piece the same before and after it (DIV 1/16, no PLAYHEADS) */
        uint32_t h1, h2, h3, h4, divs = 0, fg = 0, sec_bad = 0, f;
        static project_t ps;
        gen_reset();
        gen_with(GEN_EXPLORE, 77, 50, 0); h1 = all_sig();
        gen_with(GEN_EXPLORE, 77, 50, 0); h2 = all_sig();
        bad += gcheck("EXPLORE deterministic: the same seed twice the same piece", h1 == h2);
        gen_reset();
        gen_with(1, 421, 50, 0); h3 = all_sig();
        for (f = 0; f < 6u; f++) gen_with(GEN_EXPLORE, 300u + f, 50, f % 3u);
        gen_with(1, 421, 50, 0); h4 = all_sig();
        for (k = 0; k < NTRK; k++) { divs += trk[k].p[P_SDIV] != 2; for (i = 0; i < FG_HEADS; i++) fg += fg_set[k][i].rate != 0; }
        bad += gcheck("  HOUSE 0421 the same piece before and after six EXPLORE pieces (DIV 1/16, no PLAYHEADS)", h3 == h4 && !divs && !fg);
        for (f = 1; f <= 12u; f++) {                    /* PARTS and SONG: each section in its own key, playing, storable */
            uint32_t s2;
            gen_reset();
            gen_with(GEN_EXPLORE, 900u + f, 60, 1u + f % 2u);
            if (!live.mode || chain_config.count < 2u) { sec_bad++; continue; }
            for (s2 = 0; s2 < 4u; s2++) {
                if (!((dice_secs >> s2) & 1u)) continue;
                if (!proj_import(&ps, &proj_slot[s2], sizeof(project_store_t))) { sec_bad++; continue; }
                for (k = 0; k < NTRK; k++) {
                    track_t tt;
                    uint32_t j;
                    if (gs.role[k] == RL_DRUM) continue;
                    memcpy(tt.p, ps.t[k].p, sizeof tt.p);
                    memcpy(tt.step, ps.t[k].step, sizeof tt.step);
                    proj_steps(tt.step);
                    if (tt.p[P_SCALE] != gs.scale) sec_bad++;
                    for (i = 0; i < NSTEP; i++) {
                        if (tt.step[i].time != ST_NOTE) continue;
                        for (j = 0; j < tt.step[i].n; j++) if (tt.step[i].note[j] < 12u || tt.step[i].note[j] > 108u) sec_bad++;
                    }
                }
            }
            for (i = 0; i < 16u; i++) events_block(CTL);
        }
        bad += gcheck("  EXPLORE PARTS / SONG x 12: sections stored, ARRANGE filled, each on the piece's SCALE", !sec_bad);
    }
    {   /* NEW PROJECT (SAVE held, E4): GENRE back to EXPLORE */
        gen_reset();
        gp.genre = 5;
        sv_tap(6); press(B_OCTUP);
        bad += gcheck("NEW PROJECT: GENRE back to EXPLORE", gp.genre == GEN_EXPLORE && msg_is("NEW PROJECT"));
    }
    /* (chord-harp fork) GEN's screen is gone: HOME's last group is IDEAS (ideas.c; tests/ui_test.c) */
    printf(bad ? "gen: %d FAILED\n" : "gen: all passed\n", bad);
    return bad ? 1 : 0;
}
