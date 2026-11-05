/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (chord-harp fork) the engines as this fork's firmware builds them: ANIMAL and NOISE again (core.h YEHNAH_ANIMAL,
 * YEHNAH_NOISE 1, 2026-10-25), DIGITAL again (2026-10-30: Clipper's, a plain engine; its sounds play as themselves);
 * T3 at power-on is PHASE's CZ PLUCK (was ANIMAL's CMOS BIRD, then PHASE's HARP); GEN never puts a part on an engine
 * not built, in any genre or EXPLORE. Built without ENGS, as the firmware */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(void)
{
    int bad = 0, ok;
    uint32_t i, k, g, s, n;
    ok = YEHNAH_ANIMAL && YEHNAH_NOISE && !eng_gone(ENGI_ANIMAL) && !eng_gone(ENGI_NOISE) && eng_ok(ENGI_ANIMAL) &&
         eng_ok(ENGI_NOISE) && eng_live(ENGI_ANIMAL) == ENGI_ANIMAL && eng_live(ENGI_NOISE) == ENGI_NOISE;
    for (i = 0; i < NENG_SHOWN; i++)                    /* (the list one picks from: all there) */
        ok &= eng_ok(eng_vis(i));
    for (i = 0, n = 0; i < NENGINES; i++)
        n += (uint32_t)eng_ok(i);
    ok &= n == NENG_SHOWN && NENG_SHOWN == 24u;         /* (ANALOG DIGITAL PHASE LOFI VOICE WHEEL DRUM FM6 ANIMAL JUNO OMNI
                                                         * KALIMBA GUITAR ACID DRIFT PLAITS STRINGS VPS MODAL WSEQ TERRAIN
                                                         * VECTOR HYBRID NOISE) */
    bad += check("ANIMAL, NOISE and DIGITAL built: the twenty-four engines offered (LCD-4 retired), their sounds play as themselves", ok);

    ui_power_on();
    power_on_parts();                                   /* (main.c's power-on parts; ui_power_on keeps the old ones) */
    ok = trk[2].eng_req == ENGI_ANALOG && !strcmp(trk_engp(&trk[2])->presets[trk[2].preset].name, "PROPH LEAD");
    for (k = 0; k < NTRK; k++)
        ok &= eng_ok(trk[k].eng_req);
    bad += check("power-on: T3 ANALOG's PROPH LEAD (2026-11-02, was PHASE's CZ PLUCK), every part on an engine that is built", ok);

    set_engine_of(&trk[0], ENGI_ANIMAL);                /* (a project's, a user preset's, the editor's) */
    ok = trk[0].eng_req == ENGI_ANIMAL;
    set_engine_of(&trk[1], ENGI_NOISE);
    ok &= trk[1].eng_req == ENGI_NOISE;
    bad += check("a sound on ANIMAL or NOISE that arrives plays as itself", ok);

    ok = !YEHNAH_FM4CONV && !eng_gone(ENGI_DIGITAL) && eng_live(ENGI_DIGITAL) == ENGI_DIGITAL;
    set_engine_of(&trk[0], ENGI_DIGITAL);              /* a DIGITAL sound: DIGITAL's (Clipper's), no conversion */
    ok &= trk[0].eng_req == ENGI_DIGITAL;
    trk[1].eng_req = ENGI_DIGITAL;                      /* (a preset of engine 1: a favourite) */
    apply_preset_to(&trk[1], 3);
    ok &= trk[1].eng_req == ENGI_DIGITAL && !strcmp(trk_engp(&trk[1])->presets[trk[1].preset].name, "CLAV");
    bad += check("DIGITAL (Clipper's) again: its sound plays as itself, no conversion to FM6", ok);

    ok = 1;
    for (g = 0; g <= NGENRE; g++)                       /* (every genre, EXPLORE: GEN_EXPLORE is NGENRE + 1) */
        for (s = 0; s < 12u; s++) {
            ui_power_on();
            gp.genre = (uint8_t)(g < NGENRE ? g + 1u : GEN_EXPLORE);
            gp.seed = (uint16_t)(500u + 37u * s + g);
            gp.energy = (uint8_t)(15u + (s * 23u) % 80u);
            gp.form = 0;
            for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
            gp.complex = (uint8_t)(10u + (s * 31u) % 85u); gp.swing = GA_AUTO; gp.human = 30;
            gen_go();
            for (k = 0; k < NTRK; k++)
                if (!eng_ok(trk[k].eng_req)) {
                    if (ok) printf("  genre %u seed %u: T%u on engine %u\n", g + 1u, s, k + 1u, trk[k].eng_req);
                    ok = 0;
                }
        }
    bad += check("GEN: every genre and EXPLORE, 12 seeds each: every track on an engine that is built", ok);
    if (bad) {
        printf("retire: %d FAILED\n", bad);
        return 1;
    }
    puts("retire: all ok");
    return 0;
}
