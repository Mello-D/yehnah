/* SPDX-License-Identifier: GPL-3.0-only */
/* HOLD (core.h CP_HOLD, seq.c seq_to_hold / seq_unhold), on the real sequencer and UI sources (with the stubs of
 * tests/ui_test.c): a HOLD step's notes ring over the steps after it (their gates and releases leave them) until the
 * pattern comes round to step 1, the next HOLD step or STOP; a step without it as before; a ratchet or a kit: nothing;
 * NOTE's FLAG lists set it (ui.c nf_set) with ECHO beside it. Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("hold: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint32_t sounding(const track_t *t, uint32_t note)   /* a gated voice of that note */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].gate && t->v[i].note == note;
    return n;
}
static uint32_t age_of(const track_t *t, uint32_t note)   /* the age (strike) of its gated voice */
{
    uint32_t i, a = 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note) a = t->v[i].age;
    return a;
}
/* a 4-step loop: 0 NOTE 48 (comp2 HOLD if hold), 1 NOTE 60, 2 NOTE 64, 3 REST; the transport started */
static track_t *loop4(int hold)
{
    track_t *t;
    ui_power_on();
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 4;
    t->p[P_SGATE] = 64;
    t->p[P_SSWING] = 0;
    song.g[G_SWING] = 0;
    t->p[P_VOICE] = V_POLY;
    t->step[0] = (step_t){{48}, 1, ST_NOTE, 0, 100};
    if (hold) t->step[0].comp2 = CP_HOLD;
    t->step[1] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    t->step[2] = (step_t){{64}, 1, ST_NOTE, 0, 100};
    t->step[3] = (step_t){{0}, 0, ST_REST};
    seq_start();
    return t;
}
static uint32_t slen(track_t *t, uint32_t i) { return step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), i); }
static void run_to(track_t *t, uint32_t samples)          /* ticks for that many samples */
{
    uint32_t b;
    for (b = 0; b < samples / CTL; b++) seq_tick(t, CTL);
}
int main(void)
{
    track_t *t;
    uint32_t L, held = 0, other = 0;
    t = loop4(1);
    L = slen(t, 0);
    run_to(t, L / 2);                                    /* in step 0 */
    ck("step 0's note sounds", sounding(t, 48) == 1u);
    run_to(t, L);                                        /* in step 1, past step 0's gate */
    ck("  step 1 plays over it: 48 rings, 60 sounds", sounding(t, 48) == 1u && sounding(t, 60) == 1u && t->hold_n == 1u);
    run_to(t, L);                                        /* in step 2 */
    held = sounding(t, 48); other = sounding(t, 64);
    ck("  step 2 plays over it: 48 rings, 60 has ended, 64 sounds", held == 1u && other == 1u && sounding(t, 60) == 0u);
    run_to(t, L * 3 / 4);                                /* the REST step, long past every gate */
    ck("  a REST step: 48 still rings, nothing else", sounding(t, 48) == 1u && sounding(t, 64) == 0u);
    run_to(t, L);                                        /* the pass wraps: step 0 again */
    ck("  the pattern comes round: 48 struck again, held again", sounding(t, 48) == 1u && t->hold_n == 1u);
    seq_stop();
    ck("  STOP ends it", t->hold_n == 0u && sounding(t, 48) == 0u);

    t = loop4(0);
    L = slen(t, 0);
    run_to(t, L * 3 / 2);
    ck("without HOLD: step 0's note ended with its gate (as before)", sounding(t, 48) == 0u && t->hold_n == 0u);
    seq_stop();

    t = loop4(1);                                         /* the next HOLD step takes over */
    t->step[2].comp2 = CP_HOLD;
    L = slen(t, 0);
    run_to(t, L * 5 / 2);
    ck("the next HOLD step lets the first go: 64 held, 48 ended", sounding(t, 64) == 1u && sounding(t, 48) == 0u && t->hold_n == 1u);
    seq_stop();

    t = loop4(0);                                         /* a ratchet without HOLD: as before, no hold */
    step_set_ratchet(&t->step[0], 2);
    L = slen(t, 0);
    run_to(t, L * 3 / 2);
    ck("a ratcheted step without HOLD does not hold", t->hold_n == 0u && sounding(t, 48) == 0u);
    seq_stop();

    t = loop4(1);                                         /* carg: the steps it rings */
    t->step[0].carg2 = 2;
    L = slen(t, 0);
    run_to(t, L * 3 / 2);
    ck("HOLD of 2 steps: 48 rings in step 1", sounding(t, 48) == 1u);
    run_to(t, L);                                        /* in step 2 */
    ck("  and has ended by step 2 (its steps are over); 64 sounds", sounding(t, 48) == 0u && t->hold_n == 0u && sounding(t, 64) == 1u);
    seq_stop();

    ui_power_on();                                        /* STEP's LENGTH past another note */
    t = &trk[0];
    t->p[P_SLEN] = 8;
    memset(t->step, 0, sizeof t->step);
    { uint32_t i; for (i = 0; i < NSTEP; i++) t->step[i].time = ST_REST; }
    t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    t->step[2] = (step_t){{64}, 1, ST_NOTE, 0, 100};
    go_title("STEP"); frame();
    cursor_set(0);
    turn(EN_K3, 1);                                     /* over the empty step 2: a TIE */
    ck("LENGTH over an empty step: a TIE as before", t->step[1].time == ST_TIE && !t->step[0].comp2);
    turn(EN_K3, 1);                                     /* step 3 holds a note: HOLD of 3 steps, the TIE a REST */
    ck("  past a note: a HOLD (3 steps), the TIE a REST, the other note whole", t->step[0].comp2 == CP_HOLD && t->step[0].carg2 == 3u &&
       t->step[1].time == ST_REST && t->step[2].time == ST_NOTE && t->step[2].note[0] == 64u);
    turn(EN_K3, -1); turn(EN_K3, -1);                   /* back to one step: no HOLD */
    ck("  shortened to one step: no HOLD", t->step[0].comp2 == CP_NONE && note_len(t, 0, 8) == 1u);
    t = loop4(1);                                         /* a RATCH on the HOLD: its parts over the whole length (2026-11-10) */
    t->step[0].carg2 = 4;
    step_set_ratchet(&t->step[0], 2);
    L = slen(t, 0);
    { uint32_t a1, a2, a3, a4;
      run_to(t, L / 2);       a1 = age_of(t, 48);
      run_to(t, L);           a2 = age_of(t, 48);          /* in step 1 */
      ck("HOLD + RATCH 2: the note rings from step 0 into step 1 (one strike so far)", sounding(t, 48) == 1u && a1 == a2);
      run_to(t, L);           a3 = age_of(t, 48);          /* in step 2: the second part, struck again */
      ck("  struck again half way through the hold (step 2), still held", sounding(t, 48) == 1u && a3 != a2);
      run_to(t, L);           a4 = age_of(t, 48);          /* in step 3 */
      ck("  and rings to the end: no third strike", sounding(t, 48) == 1u && a4 == a3);
    }
    seq_stop();
    t = loop4(1);                                         /* RATCH 4 over a HOLD of 4: a strike every step */
    t->step[0].carg2 = 4;
    step_set_ratchet(&t->step[0], 4);
    L = slen(t, 0);
    { uint32_t a[5], k, d = 1;
      for (k = 0; k < 4; k++) { run_to(t, L / 2); a[k] = age_of(t, 48); run_to(t, L / 2); }
      for (k = 1; k < 4; k++) if (a[k] == a[k - 1]) d = 0;
      ck("HOLD 4 + RATCH 4: struck again in every step", d);
    }
    seq_stop();
    t = loop4(1);                                         /* an EVEN ECHO of 4 hits (comp) over a HOLD of 4 (comp2) */
    t->step[0].carg2 = 4;
    t->step[0].comp = CP_ECHO; t->step[0].carg = CP_ECHO_EVEN | 2u;
    L = slen(t, 0);
    { uint32_t a[5], k, d = 1;
      for (k = 0; k < 4; k++) { run_to(t, L / 2); a[k] = age_of(t, 48); run_to(t, L / 2); }
      for (k = 1; k < 4; k++) if (a[k] == a[k - 1]) d = 0;
      ck("HOLD 4 + EVEN ECHO 4: struck again in every step", d);
    }
    seq_stop();

    ui_power_on();                                        /* the device's way: HOLD by LENGTH, then NOTE's FLAG RATCH 4 */
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 8; t->p[P_VOICE] = V_POLY; t->p[P_SGATE] = 64; t->p[P_SSWING] = 0; song.g[G_SWING] = 0;
    memset(t->step, 0, sizeof t->step);
    { uint32_t i; for (i = 0; i < NSTEP; i++) t->step[i].time = ST_REST; }
    t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    t->step[1] = (step_t){{64}, 1, ST_NOTE, 0, 100};
    go_title("STEP"); frame(); cursor_set(0);
    { uint32_t i; for (i = 0; i < 6; i++) turn(EN_K3, 1); }
    go_title("NOTE"); frame(); cursor_set(0);
    { uint32_t i; for (i = 0; i < 5; i++) turn(EN_K2, 1); }
    ck("UI path: HOLD 7 with RATCH 4", t->step[0].comp2 == CP_HOLD && step_ratchet(&t->step[0]) == 4u);
    seq_start();
    L = slen(t, 0);
    { uint32_t a[9], k, ch = 0;
      for (k = 0; k < 8; k++) { run_to(t, L / 4); a[k] = age_of(t, 60); run_to(t, L * 3 / 4); }
      for (k = 1; k < 7; k++) if (a[k] != a[k - 1]) ch++;
      ck("  and it is struck 3 more times over its 7 steps", ch == 3u);
    }
    seq_stop();

    ui_power_on();                                        /* SLD from NOTE's FLAG: the next note glides on a MONO voice */
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 4; t->p[P_VOICE] = V_MONO; t->p[P_SGATE] = 64; t->p[P_SSWING] = 0; song.g[G_SWING] = 0; t->p[P_GLIDE] = 0;
    memset(t->step, 0, sizeof t->step);
    { uint32_t i; for (i = 0; i < NSTEP; i++) t->step[i].time = ST_REST; }
    t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    t->step[1] = (step_t){{64}, 1, ST_NOTE, 0, 100};
    go_title("NOTE"); frame(); cursor_set(0);
    { uint32_t i; for (i = 0; i < 2; i++) turn(EN_K2, 1); }
    ck("SLD: the flag set on the step", (t->step[0].flags & SF_SLIDE) != 0);
    seq_start();
    L = slen(t, 0);
    { uint32_t g0, g1;
      run_to(t, L / 2); g0 = t->slide_glide;
      run_to(t, L);     g1 = t->slide_glide;
      printf("slide_glide step0 %u step1 %u\n", g0, g1);
      ck("SLD: the second step is played as a slide (glide)", g1 == 1u);
    }
    seq_stop();

    ui_power_on();                                        /* SLD on a POLY voice: the first voice glides on, no second strike */
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 4; t->p[P_VOICE] = V_POLY; t->p[P_SGATE] = 64; t->p[P_SSWING] = 0; song.g[G_SWING] = 0; t->p[P_GLIDE] = 0;
    memset(t->step, 0, sizeof t->step);
    { uint32_t i; for (i = 0; i < NSTEP; i++) t->step[i].time = ST_REST; }
    t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    t->step[0].flags |= SF_SLIDE;
    t->step[1] = (step_t){{64}, 1, ST_NOTE, 0, 100};
    seq_start();
    L = slen(t, 0);
    { uint32_t a, b;
      run_to(t, L / 2); a = age_of(t, 60);
      run_to(t, L * 3 / 4); b = age_of(t, 64);
      ck("SLD on POLY: the same voice glides into the next note (no new strike)", a && a == b);
    }
    seq_stop();

    ui_power_on();                                        /* a TIED note's RATCH 4 over its 4 steps (2026-11-10) */
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 8; t->p[P_VOICE] = V_POLY; t->p[P_SGATE] = 64; t->p[P_SSWING] = 0; song.g[G_SWING] = 0;
    memset(t->step, 0, sizeof t->step);
    { uint32_t i; for (i = 0; i < NSTEP; i++) t->step[i].time = ST_REST; }
    t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    go_title("STEP"); frame(); cursor_set(0);
    { uint32_t i; for (i = 0; i < 3; i++) turn(EN_K3, 1); }
    go_title("NOTE"); frame(); cursor_set(0);
    { uint32_t i; for (i = 0; i < 5; i++) turn(EN_K2, 1); }
    ck("tied note: LEN 4 with RATCH 4", step_ratchet(&t->step[0]) == 4u && t->step[1].time == ST_TIE && t->step[3].time == ST_TIE);
    seq_start();
    L = slen(t, 0);
    { uint32_t a[4], k, d = 1;
      for (k = 0; k < 4; k++) { run_to(t, L / 2); a[k] = age_of(t, 60); run_to(t, L / 2); }
      for (k = 1; k < 4; k++) if (a[k] == a[k - 1]) d = 0;
      ck("  struck once in every step it lasts (not four times in the first)", d);
    }
    seq_stop();

    ui_power_on();                                        /* a HOLD on a mono voice says so (2026-11-10) */
    t = &trk[0];
    t->p[P_SLEN] = 8; t->p[P_VOICE] = V_MONO;
    memset(t->step, 0, sizeof t->step);
    { uint32_t i; for (i = 0; i < NSTEP; i++) t->step[i].time = ST_REST; }
    t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    t->step[1] = (step_t){{64}, 1, ST_NOTE, 0, 100};
    go_title("STEP"); frame();
    cursor_set(0);
    turn(EN_K3, 1);
    ck("HOLD on a MONO voice: the message says MONO", str_eq(ui.msg, "HOLD 2 MONO") && t->step[0].comp2 == CP_HOLD);
    frame();
    ck("  and the LENGTH card's unit", strstr(ui.col[2], "|MONO") != 0);
    t->p[P_VOICE] = V_POLY;
    frame();
    ck("  POLY: HOLD", strstr(ui.col[2], "|HOLD") != 0);

    ui_power_on();                                        /* RATCH 5 .. 8: an EVEN ECHO (2026-11-10) */
    t = &trk[0];
    memset(t->step, 0, sizeof t->step);
    t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    go_title("NOTE"); frame();
    cursor_set(0);
    { uint32_t i; for (i = 0; i < 4u; i++) turn(EN_K2, 1); }             /* FLAG 1: ACC SLD RATCH 2 RATCH 3 */
    ck("NOTE FLAG 1: RATCH 3 is the step's ratchet", step_ratchet(&t->step[0]) == 3u && !t->step[0].comp);
    { uint32_t i; for (i = 0; i < 3u; i++) turn(EN_K2, 1); }             /* .. RATCH 4, 5, 6 */
    ck("  RATCH 6 is an EVEN ECHO of 6 hits (the ratchet back to x1)", t->step[0].comp == CP_ECHO && t->step[0].carg == (CP_ECHO_EVEN | 4u) &&
       step_ratchet(&t->step[0]) == 1u);
    turn(EN_K2, 1); turn(EN_K2, 1);                                       /* .. 7, 8 */
    ck("  RATCH 8: hits 8", t->step[0].carg == (CP_ECHO_EVEN | 6u));
    turn(EN_K2, -1); turn(EN_K2, -1); turn(EN_K2, -1); turn(EN_K2, -1);   /* back to RATCH 4: the ECHO gone */
    ck("  back to RATCH 4: the step's ratchet again, no ECHO", step_ratchet(&t->step[0]) == 4u && !t->step[0].comp);
    { uint32_t hits = 0, b, v0;                           /* RATCH 8 plays 8 strikes a step */
      track_t *u = &trk[0];
      track_defaults_steps(u);
      u->p[P_SLEN] = 1; u->p[P_SGATE] = 64; u->p[P_SSWING] = 0; song.g[G_SWING] = 0;
      u->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100, .comp = CP_ECHO, .carg = CP_ECHO_EVEN | 6u};
      seq_start();
      for (b = 0; b < slen(u, 0) / CTL * 2u - 2u; b++) { v0 = vage; seq_tick(u, CTL); hits += vage - v0; }
      ck("  and plays 8 strikes a pass of the step (two passes: 16)", hits == 16u);
      seq_stop();
    }

    ui_power_on();                                        /* STEP's MOVE: each note on its own (2026-11-10) */
    t = &trk[0];
    t->p[P_SLEN] = 8;
    memset(t->step, 0, sizeof t->step);
    { uint32_t i; for (i = 0; i < NSTEP; i++) t->step[i].time = ST_REST; }
    t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100, .comp2 = CP_HOLD, .carg2 = 4};   /* a HOLD over steps 1 .. 3 */
    t->step[1] = (step_t){{64}, 1, ST_NOTE, 0, 90};
    go_title("STEP"); frame();
    cursor_set(0);
    turn(EN_K4, 1);                                     /* MOVE right: step 2 (the note on step 1 passed, not swapped) */
    ck("MOVE: the held note passes the note on the next step (no swap)", t->step[1].time == ST_NOTE && t->step[1].note[0] == 64u &&
       t->step[2].time == ST_NOTE && t->step[2].note[0] == 60u && t->step[2].comp2 == CP_HOLD && t->step[2].carg2 == 4u &&
       t->step[0].time == ST_REST && !t->step[0].n && !t->step[0].comp2 && ui.cursor == 2u);
    turn(EN_K4, 1);                                     /* on into the free step 3 */
    ck("  on to the next free step", t->step[3].note[0] == 60u && t->step[2].time == ST_REST && t->step[1].note[0] == 64u && ui.cursor == 3u);
    turn(EN_K4, -1); turn(EN_K4, -1);                   /* back: step 2, then past 64 to step 0 */
    ck("  and back, passing the other note again", t->step[0].note[0] == 60u && t->step[1].note[0] == 64u && ui.cursor == 0u);
    t->step[0].carg2 = 0;                               /* a HOLD to the pass's end: kept as it moves */
    t->step[7] = (step_t){{67}, 1, ST_NOTE, 0, 100};
    turn(EN_K4, -1);                                    /* left from 0: wraps past 7 (a note) to 6 */
    ck("  wraps round the pattern's end, passing notes there too", t->step[6].note[0] == 60u && t->step[7].note[0] == 67u && ui.cursor == 6u);
    printf("%s\n", bad ? "HOLD TEST FAILED" : "hold tests passed");
    return bad != 0;
}
