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

    t = loop4(1);                                         /* a ratchet: no hold */
    step_set_ratchet(&t->step[0], 2);
    L = slen(t, 0);
    run_to(t, L * 3 / 2);
    ck("a ratcheted step does not hold", t->hold_n == 0u && sounding(t, 48) == 0u);
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
    printf("%s\n", bad ? "HOLD TEST FAILED" : "hold tests passed");
    return bad != 0;
}
