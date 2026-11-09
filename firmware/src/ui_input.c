/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI input: LEDs, knobs and buttons, SEQ step entry, panel setup. */
#include "ui_name.c"                                    /* NAME: naming user presets and projects */
/* ----------------------------------------------------------- LEDs --- */
/* The LED picture is built off-line and copied one byte per column: clearing
 * and relighting would let the 10 kHz scan catch the dark gap and flicker. */
static uint8_t led_pos[41];                        /* (col << 3) | row bit, 0xFF = none */

static void led_pos_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < 41u; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)((p << 3) | r);
    }
}

static void led_put(uint8_t *nl, uint32_t id, int on)
{
    uint8_t q = led_pos[id];
    if (q != 0xFF && on)
        nl[q >> 3] |= (uint8_t)(1u << (q & 7u));
}

/* PLAY's second, green LED: not in the key matrix (no key there), found on the hardware at column 8, row PA9 (bit 1) */
#define LED_PLAY_GREEN ((8u << 3) | 1u)
static void led_clear(uint8_t *nl, uint32_t id)
{
    uint8_t q = led_pos[id];
    if (q != 0xFF)
        nl[q >> 3] &= (uint8_t)~(1u << (q & 7u));
}

static const uint8_t FAM_BTN[FAM_COUNT] = {B_HOME, B_ENV, B_LFO, B_FX, B_SCL, B_EDIT, B_GLO, B_SAVE,
                                           B_ARP, B_SEQ, B_GLO};   /* GLO: mixer + global settings; REC is transport */

/* the section shown (2026-11-04, the user's: the redesign moved pages between sections, MIXER SENDS PRESETS MORPH to
 * HOME: the button lit is the section's whose page row it is, ui_lens.c nav_sec, not the page's family's) */
static uint32_t cur_fam(void) { return SEC_FAM[nav_sec()]; }

/* (0.9.2) a section's button held + SELECT or PRESETS (2026-11-02: nothing but GLO's tempo): the button let go stays
 * there (its tap, its layer: not now) */
static uint8_t hm_btn;                                   /* the held menu's button (ui_hmenu.c) */
static uint8_t ly_used;                                  /* the held layer's knobs or keys used: PRESETS stays its */
static int song_open(uint32_t n);                         /* (song_main.c: SAVE > PROJECT's KNOB 1) */
static uint8_t pj_btn = 0xFFu;                           /* the button a SELECT combo was made with: no layer while held */
static uint32_t held_family(void)
{
    uint32_t f;
    for (f = FAM_ENV; f < FAM_COUNT; f++)
        if (f != FAM_TRK && (fm1_in.buttons >> panel.btn[FAM_BTN[f]]) & 1u)
            return f;
    return FAM_COUNT;
}
/* (0.6.67) a button whose SELECT combo was used: no next page when let go, no tap, its layer shut until let go */
static void btn_used(uint32_t b)
{
    ui.pg_down &= (uint16_t)~(1u << panel.btn[b]);
    if (b == B_SEQ) ui.seq_t0 |= 2u;
    if (b == B_SAVE) ui.save_t0 |= 2u;
    pj_btn = (uint8_t)b;
}
/* every pass: while the button of a page jump is held its layer stays shut (no map at HOLD, no tap when let go), its
 * page taps too; let go: free again */
static void pj_hold(void)
{
    if (pj_btn == 0xFFu)
        return;
    if (!((fm1_in.buttons >> panel.btn[pj_btn]) & 1u)) {
        pj_btn = 0xFFu;
        return;
    }
    ui.pg_down &= (uint16_t)~(1u << panel.btn[pj_btn]);
    if (pj_btn == B_SEQ) ui.seq_t0 |= 2u;
    if (pj_btn == B_SAVE) ui.save_t0 |= 2u;
    if (ui.ly)
        ui.ly_t0 = (ui.ly_t0 & ~2u) | 8u;               /* (ui_layer.c: LY_OPEN off, LY_DEAD) */
}

static int layer_set_open(void);                       /* (ui_layer.c) */
/* the OCT LEDs, bit 0 OCT- lit, bit 1 OCT+ lit, OCT_BREATH OCT+ breathing (can be pressed: dark .. ~60 %,
 * hal/fm1_input.h fm1_led_breath, #119). In the dialogs and on action pages OCT- (back) is lit and OCT+ breathes
 * while it would do something; the menu: on a value row OCT- and OCT+ both breathe while that way has a value to go to,
 * on CALIBRATION / ABOUT OCT+ breathing alone (opens), in ABOUT OCT- lit (back); elsewhere the octave shift */
#define OCT_BREATH 4u
#define OCT_BREATH_DN 8u                                /* OCT- breathing (the menu: the previous value) */
static uint32_t act_oct(void);                         /* (below) */
static uint32_t oct_leds(void)
{
    if (name_on() && !ui.confirm && !ui.menu)           /* NAME: OCT- cancels, OCT+ (breathing) writes */
        return 1u | OCT_BREATH;
    if (layer_set_open())                               /* a SET layer: OCT± do nothing (TONIC: no put back); */
        return ui.layer == LAYER_FX ? 1u : 0u;          /* FX LATCH: OCT- all off, lit */
    if (ui.menu && !ui.confirm)
        return 1u;                                      /* (ABOUT: OCT- back, lit) */
    if (ui.confirm || act_oct())
        return 1u | (ui.confirm || act_ready() ? OCT_BREATH : 0u);
    return (song.octave < 0 ? 1u : 0u) | (song.octave > 0 ? 2u : 0u);
}

/* the key LEDs on the grid, bit k = key k: the white keys show where the selected lane hits on the page
 * shown (its accents while ACC is held), the step playing inverted (a light walks over them); the black keys
 * the lane selected, ACC while held, the page keys while there is more than one page */
static uint32_t grid_leds(void)
{
    const track_t *t = TSEL;
    uint32_t k, m = 0, len = (uint32_t)t->p[P_SLEN], b = 1u << ui.lane, acc = (uint32_t)black_held(GK_ACC);
    uint32_t ph = song.playing && seq_src(t, t->seq_idx) < len && seq_src(t, t->seq_idx) / 16u == ui.bank ? seq_src(t, t->seq_idx) % 16u : 0xFFu;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), on;
        if (!key_black(k)) {
            uint32_t i = ui.bank * 16u + p;
            on = i < len && ((acc ? step_accents(&seq_steps(t)[i]) : step_lanes(&seq_steps(t)[i])) & b) != 0u;
            on ^= (uint32_t)(p == ph);
        } else {
            on = p < NLANE ? p == ui.lane : p == GK_ACC ? acc : len > 16u;
        }
        m |= on << k;
    }
    return m;
}

/* the DRUM grid's beats, bit k = key k (ui_leds: a steady brighter glow than the other steps', hal/fm1_input.h
 * fm1_led_mid): the first step of each group of four on the page shown, steps 1 5 9 13 of 16 ("5111 5111 5111 5111"),
 * within LEN, where grid_leds shows no hit (the lane's, its accents while ACC is held) and not the playhead. Always
 * groups of four from the pattern's first step, whatever LEN or DIV (a 3/4 or a DIV 1/8 pattern too): the steps are
 * counted, not the time */
static uint32_t grid_beats(void)
{
    const track_t *t = TSEL;
    uint32_t k, m = 0, len = (uint32_t)t->p[P_SLEN], b = 1u << ui.lane, acc = (uint32_t)black_held(GK_ACC);
    uint32_t ph = song.playing && seq_src(t, t->seq_idx) < len && seq_src(t, t->seq_idx) / 16u == ui.bank ? seq_src(t, t->seq_idx) % 16u : 0xFFu;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), i = ui.bank * 16u + p;
        if (key_black(k) || p % 4u || i >= len || p == ph)
            continue;
        m |= (uint32_t)(((acc ? step_accents(&seq_steps(t)[i]) : step_lanes(&seq_steps(t)[i])) & b) == 0u) << k;
    }
    return m;
}

/* the DRUM grid's keys that do something, bit k = key k (they glow with the idle LEDs, ui_leds): the page's steps
 * within LEN, the lane keys, ACC, the page keys while there is more than one page */
static uint32_t grid_glow(void)
{
    uint32_t k, m = 0, len = (uint32_t)TSEL->p[P_SLEN];
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        m |= (uint32_t)(!key_black(k) ? ui.bank * 16u + p < len : p < NLANE || p == GK_ACC || len > 16u) << k;
    }
    return m;
}

/* an ARP playing on any part flashes the ARP button on the beat, the bar's first beat longer: 1 lit, 0 dark,
 * 2 no ARP playing */
static uint32_t arp_led(void)
{
    uint32_t k, on = 0, b = beat_samples();
    for (k = 0; k < NPART; k++)
        on |= trk[k].p[P_AMODE] && trk[k].nheld;
    return !on ? 2u : beat_pos < (beat_n ? b / 6u : b / 2u);
}

/* Discussion #81: the keys of the notes MIDI IN (USB and TRS, routed by ROUT) holds on track t, bit k = key k: as
 * play_leds, where the keys play that note at the octave now, the lowest key that gives it; a note no key plays is
 * not shown. Read from midi_control.c's own state (midi_note_held: the notes each channel holds for the track, the
 * pedal's too, and the tones of MIDI chords): no state here; nothing to do while MIDI holds none of the track's */
static uint32_t midi_leds(const track_t *t)
{
    uint32_t k, note, m = 0, seen[4] = {0, 0, 0, 0};
    if (!midi_owners[trk_index(t)])
        return 0;
    for (k = 0; k < 27u; k++) {
        note = kb_map(t, k);
        if (note > 127u || ((seen[note >> 5] >> (note & 31u)) & 1u))
            continue;                                   /* (silent, or a lower key gives it) */
        seen[note >> 5] |= 1u << (note & 31u);
        m |= (uint32_t)midi_note_held(t, note) << k;
    }
    return m;
}

/* the keys of the notes the selected track's sequencer and ARP sound now (#38), bit k = key k: where the keys
 * play that note (kb_map: the octave, TRN, QNT, an engine's own map), the lowest key that gives it (QNT SNAP
 * rounds the keys above down onto it); a note no key plays is not shown. A snapshot of the ISR's seq_notes /
 * arp_note (arp_ch): no state of its own, nothing to do while nothing sounds */
static uint32_t play_leds(void)
{
    const track_t *t = TSEL;
    uint8_t s[4 + NLANE + 4];
    uint32_t n = t->seq_n < 4u + NLANE ? t->seq_n : 4u + NLANE, i, k, note, used = 0, m = 0, hit;
    for (i = 0; i < n; i++)
        s[i] = t->seq_notes[i];
    if (t->arp_note)
        s[n++] = t->arp_note;
    for (i = 0; i < 3u; i++)                            /* (the ARP's CHORD) */
        if (t->arp_ch[i])
            s[n++] = t->arp_ch[i];
    for (k = 0; n && k < 27u; k++) {
        note = kb_map(t, k);
        for (i = 0, hit = 0; i < n; i++)
            if (s[i] == note && !((used >> i) & 1u)) {
                used |= 1u << i;
                hit = 1;
            }
        m |= hit << k;
    }
    return m | midi_leds(t);
}

/* (0.6.83) the keys of every note the selected track's voices hold now, bit k = key k: a chord of SCL > CHORD, LATCH, a
 * chord gliding, MIDI, all of what sounds is shown (its released tails are not), as play_leds: the lowest key that
 * gives the note at the octave now; a kit's lanes not (its keys are its sounds). Read from the voices: no state */
static uint32_t voice_leds(void)
{
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t i, k, m = 0, seen[4] = {0, 0, 0, 0}, have[4] = {0, 0, 0, 0}, any = 0, note;
    if (e->oneshot || (e->keys && e->keys(t, 0) >= 0))
        return 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note < 128u) {
            have[t->v[i].note >> 5] |= 1u << (t->v[i].note & 31u);
            any = 1;
        }
    for (k = 0; any && k < 27u; k++) {
        note = kb_map(t, k);
        if (note > 127u || ((seen[note >> 5] >> (note & 31u)) & 1u))
            continue;                                   /* (silent, or a lower key gives it) */
        seen[note >> 5] |= 1u << (note & 31u);
        m |= ((have[note >> 5] >> (note & 31u)) & 1u) << k;
    }
    return m;
}

/* Discussion #89: on SEQ > STEP (the piano roll; the DRUM grid has its own map) while stopped, the keys of the
 * notes stored in the cursor step, bit k = key k: as play_leds, where the keys play that note now (kb_map: the
 * octave, TRN, QNT), the lowest key that gives it; a note no key plays is not shown, nor a REST step's. Playing,
 * the keys show what plays (play_leds) instead. Read from the step: no state of its own */
static uint32_t step_leds(void)
{
    const track_t *t = TSEL;
    const step_t *st;
    uint32_t i, k, note, used = 0, m = 0;
    if (ui.home || ui.menu || ui.confirm || !song.seq_mode || cur_page()->graph != GR_ROLL || song.playing ||
        chain_busy() || ui.cursor >= NSTEP)
        return 0;
    st = &t->step[ui.cursor];
    if (!st->n || st->time == ST_REST)
        return 0;
    for (k = 0; k < 27u; k++) {
        note = kb_map(t, k);
        for (i = 0; i < st->n && i < 4u; i++)
            if (st->note[i] == note && !((used >> i) & 1u)) {
                used |= 1u << i;
                m |= 1u << k;
            }
    }
    return m;
}

/* 1: the keys show a map of their own (NAME, a layer's map: SCL's scale, FX; the DRUM grid), lit or
 * dark; 0: the keys held and the notes playing, over the idle glow */
static int keys_own(void)
{
    return (name_on() && !ui.menu) || ui.layer || grid_on();
}

/* Discussion #127 (1.2): MENU > SCALE LEDS ON, the keys of the selected track's scale (ROOT, SCALE), bit k = key k, as
 * the keys play now: the note a key gives with TRN (QNT SNAP / SEQ: the key's own note; the others round down onto
 * the scale); QNT WHITE every white key (they walk the scale; the black ones are silent). *root: the keys of the
 * scale's root. None where the engine maps the keys itself (DRUM's GM map, the slices): 0 there, and OFF */
static uint32_t scale_leds(uint32_t *root)
{
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t k, m = 0, r = 0, mask = scale_mask(t), note;
    int32_t pc;
    *root = 0;
    if (!(ui_rec_prefs & (PREF_SCALE_LEDS >> 8)) && !t->p[P_QUANT])   /* (2026-11-10: a QUANT mode on shows the scale as well) */
        return 0;
    for (k = 0; k < 27u; k++) {
        if (e->keys && e->keys(t, k) >= 0)
            continue;
        if (t->p[P_QUANT] == QN_WHITE) {
            if ((note = kb_map(t, k)) > 127u)
                continue;                               /* (a black key: silent) */
            pc = (int32_t)note + t->p[P_TRANS];         /* (0.6.81: the note it sounds: TRANS added as it sounds,
                                                         * voice.c trk_trans; WHITE walks the scale before it) */
        } else {
            pc = 53 + (int32_t)k + t->p[P_TRANS];
        }
        pc = ((pc - t->p[P_ROOT]) % 12 + 12) % 12;
        m |= ((mask >> pc) & 1u) << k;
        r |= (uint32_t)(pc == 0) << k;
    }
    *root = r & m;
    return m;
}

/* (chord-harp) the selected track in CHORD HARP: its own map, in place of the keys played and SCALE LEDS: the chord
 * keys lit, the plate's a level lower (*mid: SCALE LEDS' root level, DIM HI ~37 %), each dimmed to the glow while
 * down; LATCH lit while on, the glow while off; the bend keys in the glow, lit while held; on SCL > PROG F#4 WRITE in the
 * glow, lit while picked; the silent black keys dark.
 * *glow: every key that does something (lit and in the glow: lit; never empty, so the rest dark) */
static uint32_t harp_leds(const track_t *t, uint32_t *glow, uint32_t *mid)
{
    uint32_t k, plate = 0, chords = 0, bends = 0, down = fm1_in.notes & ~kb_layer, latch = 1u << harp_lk(t), wr = 0, wave = 0;
    for (k = 0; k < 27u; k++)
        if (harp_ck(t, k))
            chords |= 1u << k;
        else if (harp_map(t, k) <= 127u)
            plate |= 1u << k;
        else if (harp_bend_k(k))                        /* (2026-10-26) the bend keys: in the glow, lit while held */
            bends |= 1u << k;
    if (harp_prog(t) && !ui.home && cur_page()->graph == GR_PROG)   /* (2026-10-27) SCL > PROG's WRITE: lit while picked */
        wr = 1u << HARP_KWRITE;
    *glow = chords | latch | bends | wr;               /* (2026-11-06: the plate keys dark; the wave alone lights them) */
    *mid = 0;
    if (plate) {                                        /* (2026-11-06) the wave: a bright head running up the plate keys from the
                                                         * lowest to the top in 0.75 s and back down (1.5 s round), with a tail of
                                                         * two keys behind it (mid, then dim; below it going up, above it going down), its place in 1/256 of a
                                                         * key so the swell slides from key to key; it stops while a plate key is
                                                         * down and starts again upward from the last one hit once all are let go */
        static uint32_t t0, prev, held, last_i;
        uint32_t n = (uint32_t)__builtin_popcount(plate), ph, j = 0, nw = down & plate, add = nw & ~prev;
        int32_t pos, d;
        prev = nw;
        if (add) {                                      /* the newest hit: its place among the plate's keys */
            for (k = 31u; !((add >> k) & 1u); k--)
                ;
            last_i = (uint32_t)__builtin_popcount(plate & ((1u << k) - 1u));
        }
        if (nw) {
            held = 1;
        } else {
            if (held) {                                 /* let go: resume going up from that note */
                held = 0;
                t0 = fm1_ms - (n > 1u ? last_i * 750u / (n - 1u) : 0u);
            }
            ph = (fm1_ms - t0) % 1500u;
            pos = (int32_t)((ph < 750u ? ph : 1500u - ph) * (n - 1u) * 256u / 750u);
            for (k = 0; k < 27u; k++) {
                if (!((plate >> k) & 1u))
                    continue;
                d = pos - (int32_t)(j++ * 256u);
                if (ph >= 750u)                         /* going down: the tail is the keys above the head */
                    d = -d;
                if (d >= -128 && d < 128)               /* the head, then the two keys behind it: three keys alight */
                    wave |= 1u << k;
                else if (d >= 128 && d < 384)
                    *mid |= 1u << k;
                else if (d >= 384 && d < 640)
                    *glow |= 1u << k;
            }
        }
    }
    {
        uint32_t pc, pp = harp_playing(t, &pc);          /* (the sequencer's chord key: as held; its plate notes lit) */
        chords &= ~pc;
        wave |= pp;
    }
    return (chords & ~down) | wave | (t->p[P_AHOLD] ? latch : 0u) | (bends & down) | (ui.act ? wr : 0u);
}

/* the key LEDs, bit k = key k: NAME's keys, the layer's map, the DRUM grid, else the keys held and the notes
 * the selected track's sequencer, ARP and MIDI IN play, on STEP while stopped the cursor step's notes (#89), and
 * (0.6.83) every note its voices hold (a CHORD's whole chord) */
static uint32_t key_leds(uint32_t *br)                 /* (*br: a layer's keys that breathe, layer_leds) */
{
    uint32_t c;
    if (br)
        *br = 0;
    c = name_on() && !ui.menu ? name_leds() : ui.layer ? layer_leds(br) : grid_on() ? grid_leds() :
                 (fm1_in.notes & ~kb_layer) | play_leds() | step_leds() | voice_leds();
    return c;
}

/* The LEDs: lit = active (the page's family, PLAY / REC running, the keys held or playing, a map's keys), the ARP
 * beat flashes, the ones that can be pressed breathe (#119: the open layer's button, OCT+ when it would act, a
 * layer's keys; dark .. ~60 % of lit and back, hal/fm1_input.h fm1_led_breath, in every mode: never a hard blink in
 * a dark room; DIM LO up to ~30 %, DIM HI OFF INV ~60 %; with LEDS OFF it is the only glow; INV leaves it as it is,
 * between the dark active ones and the lit idle ones), every other button and key glows dim (#35: the
 * buttons of the black FM-1 can be found in the dark; hal/fm1_input.h fm1_led_dim, a short pulse each frame).
 * MENU > LEDS: DIM HI (default) that glow, DIM LO a darker one (fm1_led_dim_level), OFF no glow (as 1.0); INV
 * turns it around, as the stock firmware: the idle ones fully lit, the active ones dark, no glow (the ARP beat:
 * lit / dark). The keys' own maps (keys_own) stay lit or dark in every mode: their dark keys read as dark; only the
 * DRUM grid's keys that do something (grid_glow) glow under it, so STEP on a DRUM track is never a dark
 * keyboard (the steps of an empty pattern); on it the first step of each four (grid_beats: 1 5 9 13 of a page, where
 * no hit shows) glows steadier and brighter, so a bar's beats can be counted: DIM HI / DIM LO the mid level
 * (hal/fm1_input.h fm1_led_mid: lit 1 frame in 8, DIM LO in 12, over the glow), OFF and INV the glow alone (no
 * other key glows there). 1.2, MENU > SCALE LEDS ON (scale_leds; not over a map of the keys' own, nor on a DRUM or
 * SLICE track): the keys of the selected track's scale glow and the others are dark, the root's at the mid level, in
 * DIM HI, DIM LO and OFF alike (asked for: OFF only takes the other glow away); INV turns it around as it does the
 * rest: the root's keys lit, the scale's at the mid level, the others glowing; the keys held and playing as ever
 * (lit, INV dark). Each picture is built off-line and copied one byte per column, the
 * glow first: an LED going from lit to dim never has a dark frame */
static void ui_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0}, own[FM1_NCOL] = {0}, og[FM1_NCOL] = {0}, nb[FM1_NCOL] = {0};
    uint8_t nm[FM1_NCOL] = {0}, sg[FM1_NCOL] = {0}, sm[FM1_NCOL] = {0};
    uint32_t k, c, g, br, beat, sc, sr = 0, full;
    uint32_t fam = cur_fam(), mode = settings_leds;
    int keys_map = keys_own();
    static uint8_t ready;
    if (fm1_led_anim_on())                              /* the power-on sweep has them (main.c boot_leds) */
        return;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    if (!ui.layer || FAM_BTN[fam] != layer_btn())
        led_put(nl, panel.btn[FAM_BTN[fam]], 1);
    if ((k = arp_led()) != 2u && (!ui.layer || layer_btn() != B_ARP))
        led_put(nl, panel.btn[B_ARP], FAM_BTN[fam] == B_ARP ? !k : (int)k);   /* (on ARP's page: dark flashes) */
    if (!ui.layer && (perf_latched || perf_k[0] || perf_k[1] || perf_k[2] || perf_k[3]))
        led_put(nl, panel.btn[B_FX], 1);                /* FX LATCH: lit while an effect or a macro is on */
    if (ui.layer)                                       /* the layer's button breathes while its map is up */
        led_put(nb, panel.btn[layer_btn()], 1);
    if (ui.hm)                                          /* .. and a held menu's while it is open */
        led_put(nb, panel.btn[hm_btn], 1);
    led_put(nl, panel.btn[B_REC], (song.rec != 0u || rp_state) && ui.layer != LAYER_REC);   /* (its layer: it breathes;
                                                         * chord-harp fork: a ONE PASS waiting for step 1 lit too) */
    k = oct_leds();
    led_put(nl, panel.btn[B_OCTDN], (int)(k & 1u));
    led_put(nl, panel.btn[B_OCTUP], (int)((k >> 1) & 1u));
    led_put(nb, panel.btn[B_OCTUP], (int)(k & OCT_BREATH));
    led_put(nb, panel.btn[B_OCTDN], (k & OCT_BREATH_DN) != 0u);
    c = key_leds(&br);
    g = !keys_map ? 0u : grid_on() && !ui.layer && !name_on() ? grid_glow() : 0u;   /* (key_leds: the grid's map) */
    beat = g ? grid_beats() & ~c : 0u;                  /* (the grid's empty beat starts: brighter than the glow) */
    sc = keys_map ? 0u : scale_leds(&sr);               /* SCALE LEDS: the scale's keys glow, the root's brighter */
    if (!keys_map && harp_on(TSEL)) {                   /* (chord-harp) HARP's map in their place */
        c = harp_leds(TSEL, &sc, &sr);
    }
    full = sc && !keys_map && !harp_on(TSEL) && TSEL->p[P_QUANT] && mode != LEDS_INV;
    for (k = 0; k < 27u; k++) {
        uint32_t in = (sc >> k) & 1u, rt = (sr >> k) & 1u, idle = !((c >> k) & 1u);
        led_put(nm, 14u + k, (int)((beat >> k) & 1u));
        led_put(keys_map ? own : nl, 14u + k, (int)((c >> k) & 1u));
        led_put(keys_map ? og : nd, 14u + k, keys_map ? (int)((g >> k) & 1u) : !sc || (mode == LEDS_INV && rt));
        led_put(nb, 14u + k, (int)((br >> k) & 1u));
        if (!sc)
            continue;
        if (full) {                                     /* (2026-11-10) QUANT on: the scale's keys lit in full, the root breathing */
            led_put(nl, 14u + k, (int)in);
            led_put(nb, 14u + k, (int)rt);
            continue;
        }
        led_put(sg, 14u + k, mode == LEDS_INV ? !in && idle : (int)in);   /* (INV: the active ones dark) */
        led_put(sm, 14u + k, mode == LEDS_INV ? in && !rt && idle : (int)rt);
    }
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    if (song.playing || seq_counting())                 /* playing: PLAY's green, its own LED dark in every mode */
        led_clear(nd, panel.btn[B_PLAY]);
    for (c = 0; c < FM1_NCOL; c++) {
        if (mode == LEDS_INV)                           /* INV: the active ones dark, the rest lit */
            nl[c] = (uint8_t)(nd[c] & ~nl[c]);
        nd[c] = mode == LEDS_DIM || mode == LEDS_DIM_LO ? (uint8_t)(nd[c] | og[c]) : nm[c];   /* OFF, INV: no glow but */
        if (mode != LEDS_DIM && mode != LEDS_DIM_LO)    /* the grid's beats (the one level above dark there); DIM HI */
            nm[c] = 0;                                  /* / LO: the beats at the mid level over the glow */
        nd[c] |= sg[c];                                 /* SCALE LEDS: in every mode (INV: inverted above) */
        nm[c] |= sm[c];
        nm[c] &= (uint8_t)~(nb[c] | own[c]);
        nl[c] |= own[c];
        nl[c] &= (uint8_t)~nb[c];                       /* breathing: neither lit (INV's idle) nor the glow */
        nd[c] &= (uint8_t)~nb[c];
    }
    if (song.playing || (seq_counting() && cin_pos < click_beat_len() / 4u))   /* counting in: green on each beat */
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    fm1_led_dim_level(mode == LEDS_DIM_LO);
    for (c = 0; c < FM1_NCOL; c++) {                    /* (0.6.96) DIM HI: the idle glow at the mid level (the pulse
                                                         * alone was too dim to find the keys: ~1/30 of lit; now ~15 %),
                                                         * the beats and the scale's root a level above (fm1_led_hi) */
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = nb[c];
        fm1_led_hi[c] = mode == LEDS_DIM ? nm[c] : 0u;
        fm1_led_mid[c] = mode == LEDS_DIM ? (uint8_t)((nd[c] | nm[c]) & ~(nb[c] | nl[c])) : nm[c];
    }
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = nl[c];
}

/* ---------------------------------------------------------- input --- */
/* Predictable hardware response (#23): each decoded detent is one value step; a fast turn keeps its full signed
 * detent count. KNOB ACCEL (#52; Yehnah: ON by default, MENU > KNOB ACCEL OFF turns it off) multiplies a fast turn of a
 * wide value (range > 16, not a list of names; the FX and GLO layers' knobs too, #126): the faster the turn the
 * wider the sweep, a slow turn is fine tuning, one step per detent. The main loop reads the knobs many times a frame
 * (main.c), so a read holds one detent as a rule: the speed is the time per detent, ACC_RATE / ms -> 40 ms x2,
 * 27 ms x3, 20 ms x4, 10 ms x8, 5 ms x16; capped by the range: x2 up to 32, x4 up to 64, x8 up to 100, x16 above (a
 * flick across 0..127 in a dozen detents). Only the longer of this read's and the previous read's time counts, and
 * only while the turn goes on (both under ACC_GAP ms) in one direction: a slow turn, the first two detents of a turn,
 * a single quick detent (a bounce) and a reversal are one step per detent, and the sign is always the detents'.
 * ui.enc_t[role]: bits 0..23 the ms of its last read, bit 24 its direction (+1), 25..31 its ms per detent (127 slow) */
#define ACC_GAP 60u
#define ACC_RATE 80u
static int32_t accel(uint32_t role, int32_t s, int32_t range)
{
    uint32_t now = fm1_ms & 0xFFFFFFu, st = ui.enc_t[role], up = s > 0, pi = st >> 25, a, i, m = 1;
    if ((ui_prefs & PREF_ACCEL_OFF) || range <= 16 || !s)
        return s;
    a = (uint32_t)(s < 0 ? -s : s);
    i = ((now - st) & 0xFFFFFFu) / a;                   /* ms per detent of this read */
    if (!st || ((st >> 24) & 1u) != up || i >= ACC_GAP)
        i = 127u;                                       /* a new turn, or reversed */
    else if (pi < ACC_GAP) {
        m = ACC_RATE / (i > pi ? i : pi ? pi : 1u);
        a = range > 100 ? 16u : range > 64 ? 8u : range > 32 ? 4u : 2u;   /* (the cap) */
        m = m < 1u ? 1u : m > a ? a : m;
    }
    ui.enc_t[role] = now | up << 24 | (i ? i : 1u) << 25;
    return s * (int32_t)m;
}

/* MIXER page: KNOB 1 LEVEL, 2 PAN, 3 REV send of the selected track (the track itself is ALGORITHM's, on every page),
 * (0.9.1) KNOB 4 the master DJ FILTER (G_DJF: was the track's MUTE, which the GLO layer's black keys latch, as before).
 * Pattern length stays on SEQ. */
static void tracks_edit(uint32_t slot, int32_t steps)
{
    track_t *t = TSEL;
    int16_t *vp;
    const param_desc_t *d;
    switch (slot) {
    case 3: {                                           /* (no acceleration across the middle: it stops there once) */
        int32_t v = song.g[G_DJF], to = clamp(v + accel(EN_K4, steps, 127), -64, 63);
        if ((v < 0 && to > 0) || (v > 0 && to < 0))
            to = 0;
        motion_djf_set(t, (int16_t)to);
        return;
    }
    case 0:
        vp = &t->p[P_LEVEL];
        d = &TP[P_LEVEL];
        break;
    case 2: {                                           /* (2026-11-02, NAV-LAYOUT.md) OFF / SOLO / MUTE (was REV: SENDS) */
        uint32_t m = (uint32_t)wrap_add((int32_t)mix_state(song.sel), steps > 0 ? 1 : -1, 3), b = 1u << song.sel;
        t->p[P_MUTE] = (int16_t)(m == 2u);
        ui.msolo = (uint8_t)(m == 1u ? ui.msolo | b : ui.msolo & ~b);
        ui.force = 1;
        return;
    }
    default:
        vp = &t->p[P_PAN];
        d = &TP[P_PAN];
        break;
    }
    *vp = (int16_t)clamp(*vp + accel(EN_K1 + slot, steps, d->max - d->min), d->min, d->max);
    motion_capture(t, (uint32_t)(vp - t->p), *vp);
}

/* SEQ > STEP's piano roll (not the DRUM grid, whose white keys stay its steps): where step recording writes */
static int roll_on(void)
{
    return !ui.home && !ui.menu && !ui.confirm && song.seq_mode && cur_page()->graph == GR_ROLL && !drum_track(TSEL);
}

/* REC tap on every page: arm / disarm recording on the selected track without navigating; arming while stopped
 * starts the transport too (live recording), except on STEP's piano roll (1.2, Discussion #133): there it arms step
 * recording and the transport stays stopped: the keys write the cursor step (seq_entry), PLAY then records live.
 * Armed and playing, STEP's keys record live (at the play head) instead of writing the cursor step */
/* (chord-harp fork) REC's mode (rec_loop, the REC layer's MODE: A3, KNOB 2): ONE PASS (the default: the track's pattern
 * once from step 1, then recording and the transport stop: seq.c rp_*) or LOOP (overdubbing round, Yehnah's). A REC tap
 * arms in it at once (2026-10-26: was a prompt asking each time) */
static void confirm_open(uint32_t kind, uint32_t trk);
static void rec_arm(int once)
{
    uint8_t bit = (uint8_t)(1u << song.sel);
    if (song.playing && cin_bars && !song.g[G_CLOCK] && !chain.running) {   /* (playing, a COUNT-IN: the pattern to its
                                                         * end, then the count-in, then recording from step 1; seq.c
                                                         * rec_cin_end) */
        if (rp_state && rp_trk != song.sel + 1u && rp_state >= RP_PRE && rp_state != RP_CIN)
            song.rec &= (uint8_t)~(1u << ((rp_trk - 1u) % NTRK));
        rp_trk = (uint8_t)(song.sel + 1u);
        rp_once = (uint8_t)once;
        rp_state = RP_CIN;
        ui_message("COUNT-IN AT THE END");
        return;
    }
    if (!once) {
        song.rec |= bit;
        if (!song.playing && !seq_counting())
            transport_req = 1;
        ui_message(grid_on() ? "LANE KEYS RECORD" : !ui.home && cur_page()->graph == GR_ROLL ? "KEYS RECORD LIVE" :
                   "RECORDING: LOOP");
        return;
    }
    if (rp_state && rp_trk != song.sel + 1u && rp_state >= RP_PRE)   /* (another track's pass: disarmed) */
        song.rec &= (uint8_t)~(1u << ((rp_trk - 1u) % NTRK));
    rp_trk = (uint8_t)(song.sel + 1u);
    if (song.playing || seq_counting()) {           /* playing: from the next step 1 */
        rp_state = RP_WAIT;
        ui_message("ONE PASS FROM STEP 1");
    } else {
        rp_state = RP_REC;
        song.rec |= bit;
        transport_req = 1;
        ui_message("RECORDING: ONE PASS");
    }
}
static int rec_tap(void)                                /* 1: the arming changed */
{
    uint8_t bit = (uint8_t)(1u << song.sel);
    if (song.rec || rp_state) {                       /* (chord-harp fork) REC lit (any track armed, a ONE PASS waiting
                                                         * or counting in): off, whichever track and page, a song
                                                         * running too (was: the selected track's only) */
        rp_state = RP_OFF;
        song.rec = 0;
        ui.force = 1;
        ui_message("REC OFF");
        return 1;
    }
    if (chain_busy()) {
        ui_message("STOP TO RECORD");
        return 0;
    }
    if (!ui.home && cur_page()->graph == GR_SONG) {
        ui_message("[SEQ] TO RECORD");
        return 0;
    }
    if (song.playing || seq_counting() || !roll_on()) {   /* (arming, not STEP's step entry: in the REC mode) */
        rec_arm(!rec_loop);
        ui.force = 1;
        return 1;
    }
    song.rec ^= bit;
    ui.force = 1;                                     /* also refresh the status on MENU / ABOUT */
    if ((song.rec & bit) && !song.playing && !seq_counting() && roll_on()) {
        ui_message("KEYS WRITE STEPS");               /* step recording: stopped, at the cursor */
        return 1;
    }
    if ((song.rec & bit) && !song.playing && !seq_counting())
        transport_req = 1;
    if ((song.rec & bit) && grid_on())
        ui_message("LANE KEYS RECORD");               /* (the white keys stay the steps) */
    else if ((song.rec & bit) && !ui.home && cur_page()->graph == GR_ROLL)
        ui_message("KEYS RECORD LIVE");               /* (seq_entry pauses while armed and playing) */
    return 1;
}

/* live recording into the selected track now: the STEP page's key entry pauses meanwhile */
static int live_rec_sel(void)
{
    return ((song.rec >> song.sel) & 1u) && (song.playing || transport_req == 1u || seq_counting());
}

/* the OCT- / OCT+ dialog (ui_draw.c draws it); trk: the track or the slot it is about */
static void confirm_open(uint32_t kind, uint32_t trk)
{
    ui.confirm = (uint8_t)kind;
    ui.confirm_trk = (uint8_t)trk;
    ui.act = 0;
    ui.force = 1;
}

/* KNOB 1 STEP: the cursor, with knob acceleration on long patterns (0.9.18 cd6b00b) */
static void cursor_turn(int32_t steps)
{
    cursor_set((int32_t)ui.cursor + accel(EN_K1, steps, (int32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP)));
}
/* the grid's page down (-1) / up (+1): the cursor to the same place on it (at most the last step) */
static void page_go(int32_t d)
{
    uint32_t len = (uint32_t)TSEL->p[P_SLEN], pages = (len + 15u) / 16u, b;
    if (pages < 2u)
        return;
    b = (ui.bank + pages + (uint32_t)d) % pages;
    cursor_set((int32_t)(b * 16u + ui.cursor % 16u < len ? b * 16u + ui.cursor % 16u : len - 1u));
}

/* the grid's knobs: 1 STEP (the cursor), 2 LANE, 3 HIT and 4 ACC of the lane at the cursor (2026-11-03: each detent the other; 0.8.5:
 * HIT on along its modes, ui.c GRID_HIT) */
static void grid_edit(uint32_t slot, int32_t steps)
{
    if (slot == 0u)
        cursor_turn(steps);
    else if (slot == 1u)
        ui.lane = (uint8_t)wrap_add((int32_t)ui.lane, steps > 0 ? 1 : -1, NLANE);
    else if (slot == 2u)
        grid_hit_set(TSEL, ui.cursor, ui.lane,
                     (uint32_t)wrap_add((int32_t)grid_hit_of(&TSEL->step[ui.cursor], ui.lane), steps > 0 ? 1 : -1, LM_N + 1));
    else
        grid_acc(TSEL, ui.cursor, ui.lane, 2u);
}

/* the keys on the grid (presses): a white key toggles the selected lane at its step of the page (its accent
 * while ACC is held) and puts the cursor there; a lane key selects the lane (seq.c plays it); the page keys */
static void grid_keys(uint32_t pressed)
{
    uint32_t k, len = (uint32_t)TSEL->p[P_SLEN];
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        if (!((pressed >> k) & 1u))
            continue;
        if (!key_black(k)) {
            uint32_t i = ui.bank * 16u + p;
            if (chain_busy()) { ui_message("STOP TO EDIT"); continue; }
            if (i >= len)
                continue;                               /* past LEN: no step there */
            {   /* a hit (an accent) there goes when the key is let go, unless it was held for a lock (lock_keys) */
                const step_t *st = &TSEL->step[i];
                uint64_t b = (uint64_t)1 << i;
                uint32_t acc = (uint32_t)black_held(GK_ACC);
                if (((acc ? step_accents(st) : step_lanes(st)) >> ui.lane) & 1u) {
                    ui.plk_rm |= b;
                    ui.plk_acc = acc ? ui.plk_acc | b : ui.plk_acc & ~b;
                    ui.plk_lane = ui.lane;
                    ui.plk_trk = song.sel;
                } else if (acc) {
                    grid_acc(TSEL, i, ui.lane, 1);
                } else {
                    grid_hit(TSEL, i, ui.lane, 1);
                }
            }
            cursor_set((int32_t)i);
        } else if (p < NLANE) {
            ui.lane = (uint8_t)p;
        } else if (p != GK_ACC) {
            page_go(p == GK_PGUP ? 1 : -1);
        }
    }
}

/* ------------------------------------------------- parameter locks --- */
/* 1.1 (Discussions #75, #53): on SEQ > STEP, hold a step and turn KNOB 1..4: the step gets a lock of the parameter
 * that knob edits on the sound page shown last (ui.plk_src: HOME's four, ENV, LFO, FX, EDIT 1 / 2, OP ENV, ..; the
 * cards show those four while a step is held, a locked value bright, the sound's own DIM). The step held: on the
 * DRUM grid its white keys (several at once: each gets the lock; a press on a hit takes it away only when let go,
 * and not at all when it was held for a lock), on the roll the step being entered (its note keys held). [EDIT]
 * tapped meanwhile clears the held steps' locks (EDIT alone still clears the step, its locks with it). Not while a
 * song plays (STOP TO EDIT). A lock is a motion record (motion.c): AUTOMATION's PLAY plays it, CLEAR clears it */
static uint64_t lock_held(void)
{
    const track_t *t = TSEL;
    uint32_t k, len = (uint32_t)t->p[P_SLEN], keys = fm1_in.notes & ~kb_layer;
    uint64_t m = 0;
    if (ui.home || ui.menu || ui.confirm || ui.layer || name_on() || !song.seq_mode || cur_page()->graph != GR_ROLL ||
        !keys)
        return 0;
    if (!grid_on())                                     /* the roll: the step being entered */
        return ui.entry_open && !live_rec_sel() && ui.cursor < len ? (uint64_t)1 << ui.cursor : 0u;
    for (k = 0; k < 27u; k++)
        if (((keys >> k) & 1u) && !key_black(k) && ui.bank * 16u + key_place(k) < len)
            m |= (uint64_t)1 << (ui.bank * 16u + key_place(k));
    return m;
}

/* each pass, before the grid's keys: a hit pressed on goes once its key is let go (plk_rm, grid_keys) */
static void lock_keys(void)
{
    uint64_t gone = ui.plk_rm & ~lock_held();
    uint32_t i;
    for (i = 0; gone && i < NSTEP; i++)
        if ((gone >> i) & 1u) {
            if ((ui.plk_acc >> i) & 1u)
                grid_acc(&trk[ui.plk_trk % NTRK], i, ui.plk_lane, 0);
            else
                grid_hit(&trk[ui.plk_trk % NTRK], i, ui.plk_lane, 0);
        }
    ui.plk_rm &= ~gone;
}

/* KNOB k turned s with the steps `held` held: each gets (or changes) its lock of lock_id(k), from its lock or the
 * sound's own value */
static void lock_turn(uint32_t k, int32_t s, uint64_t held)
{
    track_t *t = TSEL;
    uint32_t id = lock_id(k), i;
    const param_desc_t *d;
    if (chain_busy()) {
        ui_message("STOP TO EDIT");
        return;
    }
    if (id == 0xFFu)
        return;                                         /* (an empty card: nothing to lock) */
    ui.plk_rm &= ~held;                                 /* (held for a lock: its hit stays) */
    d = track_desc(t, id);
    s = accel(EN_K1 + k, s, d->fmt == F_ENUM ? 0 : d->max - d->min);
    for (i = 0; i < NSTEP; i++) {
        int16_t v;
        if (!((held >> i) & 1u))
            continue;
        if (!motion_lock_get(t, i, id, &v))
            v = motion_base_value(t, id);
        if (motion_set_lock(t, i, id, (int16_t)param_turn(d, v, s)) == 2)
            break;                                      /* (full: ui_notices says so) */
    }
}

/* [EDIT] with the steps `held` held: their locks go */
static void lock_clear(uint64_t held)
{
    uint32_t i, n = 0;
    ui.plk_rm &= ~held;
    if (!(motion_lock_steps(song.sel) & held)) {
        ui_message("NO LOCKS");
        return;
    }
    for (i = 0; i < NSTEP; i++)
        if ((held >> i) & 1u)
            n += motion_clear_locks(TSEL, i);
    ui_message(n ? "LOCKS CLEARED" : "NO LOCKS");
}

/* (0.6.81) SEQ > NOTE, the piano roll's edits on the cursor's step: FOCUS the chord or one of its notes (lit), PITCH
 * moves what is in focus up or down, MOVE moves it a step left or right (the chord with its length: it changes places
 * with the step beside it; a note: into the step beside, a NOTE with room or an empty one), LEN the chord's length (its
 * TIEs, over empty steps only; 0.8.4: STEP's KNOB 3, len_turn). EDIT deletes what is in focus (page_tap) */
/* (0.8.4) a note's length d steps longer or shorter, its notes kept (a tester: no way to lengthen a recorded note but
 * TIE by TIE): longer, the steps after it TIEs (over RESTs and empty steps; the next note stops it: NEXT NOTE), up to
 * 16 steps or LEN, round past LEN's end as it plays (as step recording writes them); shorter, its last TIEs RESTs. Its
 * length after */
static uint32_t len_turn(track_t *t, uint32_t c, int32_t d)
{
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, l = note_len(t, c, len), max = len < 16u ? len : 16u;
    if (c >= len)
        return 1;                                       /* (past LEN: nothing plays it) */
    for (; d > 0 && l < max; d--, l++) {
        step_t *nx = &t->step[(c + l) % len];
        if (step_on(nx)) { ui_message("NEXT NOTE"); break; }
        memset(nx, 0, sizeof *nx);
        nx->time = ST_TIE;
        nx->vel = t->step[c].vel;
    }
    for (; d < 0 && l > 1u; d++) {
        step_t *e = &t->step[(c + --l) % len];
        memset(e, 0, sizeof *e);
        e->time = ST_REST;
    }
    return l;
}
static void note_drop(step_t *st, uint32_t j)           /* note j out of the step (empty: a REST, its TIEs nothing) */
{
    uint32_t i;
    for (i = j; i + 1u < st->n; i++)
        st->note[i] = st->note[i + 1u];
    if (st->n) st->note[--st->n] = 0;
    if (!st->n && !st->hit) st->time = ST_REST;
}
/* (2026-11-03, the user's) the NOTE knobs' NONE, under the lowest note: the notes in focus (f: 0 all, else that one)
 * turned below 1 */
static int note_none(const step_t *st, uint32_t f, int32_t steps)
{
    uint32_t i;
    for (i = 0; i < st->n; i++)
        if ((!f || i == f - 1u) && (int32_t)st->note[i] + steps < 1)
            return 1;
    return 0;
}
static void step_none(void)                             /* the cursor's step empty, its locks with it */
{
    step_clear(&TSEL->step[ui.cursor]);
    (void)motion_clear_locks(TSEL, ui.cursor);
    ui_message("NOTE: NONE");
}
/* STEP's KNOB 4 MOVE: the step (a chord and its TIEs) trades places with the step beside, the cursor with it */
static void move_step(int32_t steps)
{
    track_t *t = TSEL;
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, c = ui.cursor;
    step_t *st = &t->step[c];
    int32_t dir = steps > 0 ? 1 : -1;
    uint32_t l, k;
    step_t blk[NSTEP], other;
    if (st->time != ST_NOTE || !st->n) { ui_message("NO NOTE HERE"); return; }
    l = note_len(t, c, len);
    if (l >= len) return;
    for (k = 0; k < l; k++) blk[k] = t->step[(c + k) % len];
    if (dir > 0) {
        uint32_t nx = (c + l) % len, nl = note_len(t, nx, len);
        if (t->step[nx].time == ST_NOTE && nl > 1u) { ui_message("BLOCKED: A LONG NOTE"); return; }
        other = t->step[nx];
        if (other.time == ST_TIE) { other.time = ST_REST; other.n = 0; }
        t->step[c] = other;
        for (k = 0; k < l; k++) t->step[(c + 1u + k) % len] = blk[k];
        ui.cursor = (uint8_t)((c + 1u) % len);
    } else {
        uint32_t pv = (c + len - 1u) % len;
        if (t->step[pv].time == ST_TIE) { ui_message("BLOCKED: A LONG NOTE"); return; }
        other = t->step[pv];
        for (k = 0; k < l; k++) t->step[(pv + k) % len] = blk[k];
        t->step[(pv + l) % len] = other;
        ui.cursor = (uint8_t)pv;
    }
    ui.bank = (uint8_t)(ui.cursor / 16u);
    ui.entry_open = 0;
}
/* SEQ > NOTE (2026-11-10): KNOB 1 STEP, KNOB 2 FLAG 1, KNOB 3 FLAG 2 (ui.c nf_set), KNOB 4 VELOCITY */
static void note_edit(uint32_t slot, int32_t steps)
{
    step_t *st = &TSEL->step[ui.cursor];
    int32_t dir = steps > 0 ? 1 : -1;
    if (slot == 0u) { cursor_turn(steps); return; }
    if (st->time != ST_NOTE || !st->n) { ui_message("NO NOTE HERE"); return; }
    if (slot == 3u) {                                   /* VELOCITY 1 .. 127 (0 = the default 96; an ACC plays 127) */
        st->vel = (uint8_t)clamp((int32_t)(st->vel ? st->vel : 96u) + steps, 1, 127);
        return;
    }
    {
        uint32_t a, b;
        nf_pair(&a, &b);
        nf_set(st, slot - 1u, (uint32_t)clamp((int32_t)(slot == 1u ? a : b) + dir, 0, NF_N - 1));
    }
}
static void step_edit(uint32_t slot, int32_t steps)
{
    step_t *st = &TSEL->step[ui.cursor];
    uint32_t i;
    if (cur_page()->id[0] == 4u) {                      /* (0.6.81) SEQ > NOTE; (2026-11-10) FLAG 1, FLAG 2, VELOCITY */
        if (drum_track(TSEL) && slot != 0u && slot != 3u) ui_message("A KIT: STEP + VELOCITY");
        else if (drum_track(TSEL) && slot == 3u) {
            if (!st->hit) ui_message("NO HIT HERE");
            else st->vel = (uint8_t)clamp((int32_t)(st->vel ? st->vel : 96u) + steps, 1, 127);
        } else note_edit(slot, steps);
        return;
    }
    if (drum_track(TSEL)) {
        grid_edit(slot, steps);
        return;
    }
    switch (slot) {
    case 0:                                               /* STEP: the cursor */
        cursor_turn(steps);
        break;
    case 1:                                               /* NOTE: transpose the step (2026-11-03: below the lowest,
                                                           * NONE: the step empty, as TIME's REST) */
        if (!st->n && steps < 0)
            break;
        if (note_none(st, 0, steps)) {
            step_none();
            break;
        }
        if (!st->n) {
            st->note[0] = last_note;
            st->n = 1;
            st->time = ST_NOTE;
            break;
        }
        for (i = 0; i < st->n; i++)
            st->note[i] = (uint8_t)clamp(st->note[i] + steps, 1, 127);
        st->time = ST_NOTE;
        last_note = st->note[0];
        break;
    case 2:                                               /* LENGTH (2026-11-10): the note longer or shorter, a step a
                                                           * detent (TIEs over empty steps; no REST or TIE to pick) */
        if (st->time != ST_NOTE || !st->n) ui_message("NO NOTE HERE");
        else len_turn(TSEL, ui.cursor, steps);
        break;
    default:                                              /* MOVE (2026-11-10, was NOTE's KNOB 3): the step and its TIEs
                                                           * change places with the step beside */
        move_step(steps);
        break;
    }
}

/* (0.6.39) SEQ > COMP: KNOB 1 the step, KNOB 2 its TYPE (OFF, the step's CHANCE and RATCH, the components), KNOB 3 / 4
 * the TYPE's values (CHANCE %, RATCH x1..x4; ARP: ORDER, RATE x RANGE; TRIG: the condition). CHANCE and RATCH are the
 * step's own fields; a component is one at a time: turning to OFF, CHANCE or RATCH takes it away. (0.6.82) COMP 2: the
 * step's second component the same way (no CHANCE or RATCH there); RATCH and ECHO: KNOB 4 their DIV. WHEN: KNOB 2 / 3
 * the loops each component plays on (ALWAYS, 1:2, 2:2, SKIP), KNOB 4 the step's LEN */
static void comp_knob(uint32_t slot, int32_t steps)
{
    step_t *st = &TSEL->step[ui.cursor];
    uint32_t v = comp_view(), k = comp_slot();
    int32_t d = clamp(steps, -4, 4), e = steps > 0 ? 1 : -1;
    uint8_t *cp = k == 1u ? &st->comp2 : &st->comp, *ap = k == 1u ? &st->carg2 : &st->carg;
    if (slot == 0u) { cursor_set(ui.cursor + steps); return; }
    if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
    if (slot == 3u && !((v >= COMP_ARPN && v <= COMP_ARPS && k == 0u) || ((v == COMP_RATCH || v == COMP_ECHO) && k == 0u) ||
                        v == COMP_WALK || v == COMP_CLIMB)) {   /* (0.6.98: WALK / CLIMB's KNOB 4 is their RATE on both pages) */
        uint32_t i;                                     /* (0.6.92) EVERY: the step's components' loops, in the knob's order */
        for (i = 0; i < EV_N && EV_ORD[i] != ST_EVERY(st); i++) {}
        i = (uint32_t)wrap_add((int32_t)i, e, EV_N);
        ST_EVERY_SET(st, EV_ORD[i]);
        return;
    }
    if (slot == 1u) {
        v = (uint32_t)wrap_add((int32_t)v, d, COMP_N);
        if (k && v > COMP_OFF && v < COMP_ARPN)         /* (COMP 2: no CHANCE or RATCH) */
            v = d > 0 ? COMP_ARPN : COMP_OFF;
        if (!k) ui.comp_sel = (uint8_t)v;
        if (v >= COMP_ARPN) {
            uint32_t c = CP_ARPN + v - COMP_ARPN;
            if (*cp != c) {
                int keep = cp_isarp(*cp) && cp_isarp(c);   /* (ARP to ARP: ORDER, RATE, RANGE kept) */
                *cp = (uint8_t)c;
                if (!keep) *ap = c <= CP_ARPS ? CP_ARG(0u, 2u, 0u) : CP_ADEF[c];   /* (UP x4 1 OCT; the others' own) */
            }
        } else
            *cp = *ap = 0;
        return;
    }
    if ((v == COMP_RATCH || v == COMP_ECHO) && slot == 3u)   /* (0.6.82) DIV */
        st->ctime = (uint8_t)((st->ctime & ~7u) | (uint32_t)clamp((int32_t)CT_DIV(st) + e, 0, CT_NDIV - 1));
    else if (v == COMP_CHANCE && slot == 2u)
        step_set_chance(st, (uint32_t)clamp((int32_t)step_chance(st) + steps, 0, 100));
    else if (v == COMP_RATCH && slot == 2u)
        step_set_ratchet(st, (uint32_t)clamp((int32_t)step_ratchet(st) + d, 1, 4));
    else if (*cp == CP_JUMP && slot == 2u)            /* (0.6.62) TO STEP (0.6.92: TIMES is EVERY, KNOB 4) */
        *ap = (uint8_t)((*ap & 0xC0u) | (uint32_t)clamp((int32_t)CP_JTO(*ap) + steps, 0, 63));
    else if (v >= COMP_TRIG && slot == 2u && *cp > CP_ARPS && CP_AMAX[*cp])   /* TRIG .. PULSE: their value (0.8.5: ECHO's
                                                                              * EVEN kept) */
        *ap = (uint8_t)((*cp == CP_ECHO ? *ap & CP_ECHO_EVEN : 0u) |
                        (uint32_t)clamp((int32_t)(*cp == CP_ECHO ? *ap & ~CP_ECHO_EVEN : *ap) + d, CP_AMIN[*cp], CP_AMAX[*cp]));
    else if ((v == COMP_WALK || v == COMP_CLIMB) && slot == 2u) {   /* (0.6.98) MODE x RANGE: one list, a range after another */
        int32_t i = clamp((int32_t)(CP_WMODE(*ap) * 4u + CP_WRNG(*ap)) + d, 0, 15);
        *ap = CP_WARG(CP_WRATE(*ap), (uint32_t)i / 4u, (uint32_t)i % 4u);
    } else if ((v == COMP_WALK || v == COMP_CLIMB) && slot == 3u) {   /* .. RATE: the loops a move, 1 .. 16 */
        *ap = CP_WARG((uint32_t)clamp((int32_t)CP_WRATE(*ap) + e, 0, (int32_t)CP_NWRATE - 1), CP_WMODE(*ap), CP_WRNG(*ap));
    } else if (v >= COMP_ARPN && v <= COMP_ARPS && slot == 2u) {   /* (0.6.92) ORDER x RANGE: one list, a range after another */
        int32_t i = wrap_add((int32_t)(CP_ORD(*ap) * 4u + CP_RNG(*ap)), d, 16);
        *ap = CP_ARG((uint32_t)i / 4u, CP_RATE(*ap), (uint32_t)i % 4u);
    } else if (v >= COMP_ARPN && v <= COMP_ARPS && slot == 3u) {   /* (0.6.92) RATE: x1 x2 x3 x4 x6 x8 (in the track's time) */
        uint32_t i;
        for (i = 0; i < CP_NRATE && CP_RATE_ORD[i] != CP_RATE(*ap) % CP_NRATE; i++) {}
        i = (uint32_t)clamp((int32_t)i + e, 0, CP_NRATE - 1);
        *ap = CP_ARG(CP_ORD(*ap), CP_RATE_ORD[i], CP_RNG(*ap));
    }
}
/* (0.6.85) SEQ > STRETCH: KNOB 1 STRETCH /8 .. /2, x1, x2 .. x8; KNOB 2 REPEAT OFF, x2 .. x8, TO 64; (0.6.92) done as the
 * knob turns, always from the sequence as it was before the first turn (its base: the steps, LEN and automation,
 * kept while the result stays untouched; back to the middle = back to it; UNDO: REC + D4); KNOB 3 NUDGE: the whole
 * sequence a step left / right; KNOB 4 ROTATE: played from another step (P_SOFS). ui_tools.c tl_stretch, tl_repeat,
 * tl_shift */
static uint32_t tl_len(const track_t *t);
static void tl_take(const track_t *t);
static void tl_shift(track_t *t, uint32_t len, uint32_t d);
static uint32_t tl_stretch(track_t *t, int32_t k);
static uint32_t tl_repeat(track_t *t, uint32_t k);
static struct {
    uint8_t valid, trk, which;                          /* which: the knob the base belongs to (0.6.96: the other knob
                                                         * takes the result as its base, so REPEAT then STRETCH
                                                         * stretches the repeats) */
    int16_t len;
    uint32_t sig;                                       /* the result's steps: another sig, the user edited: a new base */
    step_t step[NSTEP];
    motion_store_t mo;
} shp_base;
static void shape_apply(uint32_t which)
{
    track_t *t = TSEL;
    uint32_t was, n, f;
    char m[24];
    if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
    if (!shp_base.valid || shp_base.trk != song.sel || shp_base.sig != steps_sig(t) || shp_base.which != which) {   /* the
                                                         * base: as it is now (the other knob's result included) */
        undo_mark();
        shp_base.valid = 1; shp_base.trk = (uint8_t)song.sel; shp_base.len = t->p[P_SLEN]; shp_base.which = (uint8_t)which;
        memcpy(shp_base.step, t->step, sizeof shp_base.step);
        f = motion_guard(); memcpy(&shp_base.mo, &motion, sizeof shp_base.mo); motion_unguard(f);
        if (which == 0u) shp_r = 0; else shp_i = 7;    /* (the other knob's pick starts over) */
    } else {                                            /* back to the base, then the new pick (as the tools write:
                                                         * straight into the steps, the automation with them) */
        memcpy(t->step, shp_base.step, sizeof t->step);
        t->p[P_SLEN] = shp_base.len;
        f = motion_guard(); memcpy(&motion, &shp_base.mo, sizeof motion); RING_PUBLISH(); motion_unguard(f);
    }
    was = (uint32_t)clamp(shp_base.len, 1, NSTEP);
    n = which == 0u ? (shp_k() == 1 ? was : tl_stretch(t, shp_k())) : (shp_rep() == 1u ? was : tl_repeat(t, shp_rep()));
    shp_base.sig = steps_sig(t);
    if (!n) { ui_message("LEN 64: NO ROOM"); return; }
    str_cpy(m, "LEN ", sizeof m); fmt_int(m + 4, (int32_t)was); str_cpy(m + str_len(m), " > ", 4);
    fmt_int(m + str_len(m), (int32_t)n);
    if (which == 0u && shp_k() > 1 && was * (uint32_t)shp_k() > NSTEP) str_cpy(m + str_len(m), " CUT", 5);
    ui_message(m);
}
/* (chord-harp fork) SCL > PROG's WRITE: the track's steps the progression PROG picks, from step 1, a chord a 2 BAR, 1 BAR
 * or 1/2 BAR (rate 0..2): each a CHORD step (its root, PROG's chord id: harp.c prog_chord) held by TIEs to the next; DIV
 * the finest of 1/16 1/8 1/4 1/2 1/1 its chords fit in at that (64 steps), LEN their steps. The sound, the mix and the
 * automation stay (a pattern load's undo); its message */
static const char *prog_write(track_t *t, uint32_t rate)
{
    static const uint8_t SPB[5] = {16, 8, 4, 2, 1}, DIVI[5] = {2, 1, 0, 6, 7};   /* steps a bar; N_DIV's index */
    static char m[24];
    uint32_t p = harp_pg(t), n = RES_OK() ? PROGS[p].n : 1u, hb = rate == 0u ? 4u : rate == 1u ? 2u : 1u, d, spc = 0, c, i;
    if (chain_busy())
        return "STOP TO EDIT";
    for (d = 0; d < 5u; d++) {                          /* (steps a chord: half bars x steps a bar / 2) */
        spc = (uint32_t)SPB[d] * hb / 2u;
        if (spc && (uint32_t)SPB[d] * hb % 2u == 0u && n * spc <= NSTEP)
            break;
    }
    if (d == 5u)
        return "TOO LONG";
    undo_mark();
    load_begin(t, LOAD_PAT);
    for (i = 0; i < NSTEP; i++)
        step_clear(&t->step[i]);
    for (c = 0; c < n; c++) {
        step_t *s = &t->step[c * spc];
        int32_t rn;
        uint32_t id = prog_chord(p, c, t->p[P_ROOT], &rn);
        s->note[0] = (uint8_t)clamp(rn, 0, 127);
        s->n = 1;
        s->time = ST_NOTE;
        s->vel = 100;
        step_chord_set(s, id);
        for (i = 1; i < spc; i++) {
            t->step[c * spc + i].time = ST_TIE;
            t->step[c * spc + i].vel = 100;
        }
    }
    t->p[P_SLEN] = (int16_t)(n * spc);
    t->p[P_SDIV] = (int16_t)DIVI[d];
    load_end(t);
    str_cpy(m, "WRITTEN: ", sizeof m);
    str_cpy(m + str_len(m), PROG_NAME[p], sizeof m - str_len(m));
    return m;
}
static void shape_knob(uint32_t slot, int32_t steps)
{
    int32_t d = steps > 0 ? 1 : -1;
    if (slot == 0u) { shp_i = (uint8_t)wrap_add((int32_t)shp_i, d, 15); shape_apply(0); }
    else if (slot == 1u) { shp_r = (uint8_t)wrap_add((int32_t)shp_r, d, 9); shape_apply(1); }
    else if (slot == 2u) {
        track_t *t = TSEL;
        if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
        tl_take(t);
        tl_shift(t, tl_len(t), d > 0 ? 1u : tl_len(t) - 1u);
        ui_message(d > 0 ? "NUDGED RIGHT" : "NUDGED LEFT");
    } else {
        TSEL->p[P_SOFS] = (int16_t)clamp(TSEL->p[P_SOFS] + steps, -32, 32);
        motion_capture(TSEL, P_SOFS, TSEL->p[P_SOFS]);
    }
}
/* (0.8.5) EDIT > FOCUS: KNOB 1 the parameter (E1..E8 with a name; 2026-11-03: wraps), 2 its value, 3 the SRC of the MOD slot aimed
 * at it (from OFF: a free slot takes it, its DEST E1..E8; back to OFF: the slot free again), 4 that slot's AMT */
static void focus_knob(uint32_t slot, int32_t steps)
{
    uint32_t k = focus_k(), f = k, n;
    int32_t j = focus_slot(k);
    int16_t *p;
    const param_desc_t *d;
    if (slot == 0u) {                                   /* (a named one a detent) */
        int32_t m = steps < 0 ? -steps : steps;
        for (n = 0; n < 8u * (uint32_t)m && m > 0; n++) {
            f = (f + (steps > 0 ? 1u : 7u)) & 7u;
            if (focus_named(f)) {
                ui.focus = (uint8_t)f;
                m--;
            }
        }
        return;
    }
    if (slot == 1u) {
        d = track_desc(TSEL, P_E0 + k);
        TSEL->p[P_E0 + k] = (int16_t)param_turn(d, TSEL->p[P_E0 + k], accel(EN_K1 + 1u, steps, d->fmt == F_ENUM ? 0 : d->max - d->min));
        motion_capture(TSEL, P_E0 + k, TSEL->p[P_E0 + k]);
        return;
    }
    if (slot == 2u && j < 0) {                          /* (no slot aims at it: a free one, turned up) */
        uint32_t m;
        if (steps <= 0)
            return;
        for (m = 0; m < 4u && TSEL->p[P_M1SRC + 3u * m]; m++)
            ;
        if (m == 4u) {
            ui_message("MOD: 4 SLOTS USED");
            return;
        }
        j = (int32_t)m;
        TSEL->p[P_M1DST + 3u * m] = (int16_t)(MD_E1 + k);
    }
    if (j < 0)
        return;
    p = &TSEL->p[P_M1SRC + 3u * (uint32_t)j + (slot == 2u ? 0u : 2u)];
    d = track_desc(TSEL, P_M1SRC + 3u * (uint32_t)j + (slot == 2u ? 0u : 2u));
    *p = (int16_t)param_turn(d, *p, slot == 2u ? (steps > 0 ? 1 : -1) : accel(EN_K1 + 3u, steps, d->max - d->min));
}
static void edit_param(uint32_t slot, int32_t steps)
{
    int16_t *vp;
    const page_t *pg = cur_page();
    const param_desc_t *d;
    int32_t v;
    if (BAR_PAGE(pg) && slot == 0u) {                    /* (0.9.2) SEQ > BAR: KNOB 1 the bar (2026-11-03: wraps), the cursor at its
                                                         * place in it */
        uint32_t len = (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP), nb = (len + 15u) / 16u;
        uint32_t b = (uint32_t)wrap_add((int32_t)ui.bank, steps, (int32_t)nb), at = b * 16u + ui.cursor % 16u;
        cursor_set((int32_t)(at < len ? at : len - 1u));
        return;
    }
    if (HARP_PAGE(pg) && slot < 3u && slot) {           /* (2026-10-27) GENRE: SET to the next / last genre's first;
                                                         * SET: within its genre */
        uint32_t s = (uint32_t)clamp(TSEL->p[P_CVOI], 0, (int32_t)HARP_NALL - 1), g = harp_genre(s), f = harp_genre_set(g), n = f;
        if (slot == 1u) {
            TSEL->p[P_CVOI] = (int16_t)harp_genre_set((uint32_t)clamp((int32_t)g + steps, 0, RES_OK() ? (int32_t)J6_NGEN : 0));   /* (the
                                                         * J-6's in the resource region) */
            return;
        }
        while (n < HARP_NALL && harp_genre(n) == g) n++;
        TSEL->p[P_CVOI] = (int16_t)clamp((int32_t)s + steps, (int32_t)f, (int32_t)n - 1);
        return;
    }
    if (pg->graph == GR_COMP) {
        comp_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_SHAPE) {                        /* (0.6.85) STRETCH, REPEAT (picked, OCT+ does it), NUDGE (now) */
        shape_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_CHANCE) {
        if (slot == 0u) cursor_set(ui.cursor + steps);
        else if (slot == 1u || slot == 2u) {
            if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
            step_t *st = &TSEL->step[ui.cursor];
            if (slot == 1u)
                step_set_chance(st, (uint32_t)clamp((int32_t)step_chance(st) + steps, 0, 100));
            else                                      /* RATCH x1..x4 */
                step_set_ratchet(st, (uint32_t)clamp((int32_t)step_ratchet(st) + steps, 1, 4));
        }
        return;
    }
    if (pg->graph == GR_DXSND) {                          /* EDIT > SOUND: the DRUM-X kit (ui_dx.c) */
        dx_knob(pg, slot, steps);
        return;
    }
    if (pg->graph == GR_FOCUS) {                          /* (0.8.5) EDIT > FOCUS */
        focus_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_STAGE && slot < 2u) {             /* (0.8.5) ENV > STAGE: the stage, its value */
        if (slot == 0u)
            ui.stage = (uint8_t)wrap_add((int32_t)(ui.stage & 3u), steps > 0 ? 1 : -1, 4);
        else {
            uint32_t id = P_ATK + (ui.stage & 3u);
            TSEL->p[id] = (int16_t)param_turn(&TP[id], TSEL->p[id], accel(EN_K1 + 1u, steps, 127));
            motion_capture(TSEL, id, TSEL->p[id]);
        }
        return;
    }
    if (DX_KIT3(pg)) {                                    /* (0.6.54) KIT 3: BP, FEEDBK (0.6.58: IMAGE on STEREO) */
        dx_kit3_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_MOTION) {
        if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
        if (slot == 0u) motion_set_enabled(TSEL, steps > 0);
        else if (slot == 3u) ui.act = steps > 0 ? 4u : 0u;
        return;
    }
    if (pg->graph == GR_SEDIT) {                        /* (2026-11-06) SONG EDIT: the tracks run down the screen, each broken into the song's parts
                                                         * (the rows). K1 the track, K2 the part (on to the next, to the song's end), K3 / K4 the bars the
                                                         * track sits out of that part, from its start / its end */
        uint32_t n = chain_config.count, a, b, bars;
        uint8_t *cut;
        if (!n) return;
        if (slot == 0u) {
            ui.sed_trk = (uint8_t)clamp((int32_t)ui.sed_trk + steps, 0, (int32_t)trk_live - 1);
            return;
        }
        if (ui.sed_trk >= trk_live) ui.sed_trk = (uint8_t)(trk_live - 1u);
        if (ui.song_row >= n) ui.song_row = (uint8_t)(n - 1u);
        if (slot == 1u) {
            ui.song_row = (uint8_t)clamp((int32_t)ui.song_row + steps, 0, (int32_t)n - 1);
            return;
        }
        bars = chain_config.row[ui.song_row].bars;         /* (2026-11-11: editable while the song plays; in from the next bar) */
        row_trims(&chain_config, song_cur, ui.song_row, ui.sed_trk, &a, &b);
        cut = song_idx.scene[song_cur][ui.song_row].cut[ui.sed_trk];
        if (slot == 2u) a = (uint32_t)clamp((int32_t)a + steps, 0, (int32_t)(bars - 1u - b));
        else b = (uint32_t)clamp((int32_t)b + steps, 0, (int32_t)(bars - 1u - a));
        cut[0] = (uint8_t)a;
        cut[1] = (uint8_t)b;
        song_idx_touch();
        return;
    }
    if (pg->graph == GR_SONG) {                         /* (2026-11-06) SONG STRUCTURE: K1 PROJECT, K2 SECT, K3 TIMES (what OCT+ inserts),
                                                         * K4 ROW (n/N, then END). On END K2 sets STOP / x2..x8 / LOOP; TIMES below x1 deletes the row */
        uint32_t n = chain_config.count;
        if (slot == 0u) {                               /* the project (one of the songs; as PROJECT's KNOB 1) */
            int32_t p = wrap_add((int32_t)song_cur, steps > 0 ? 1 : -1, (int32_t)NSONG);
            if ((uint32_t)p != song_cur && song_open((uint32_t)p) == 0)
                ui.song_row = (uint8_t)(chain_config.count ? chain_config.count - 1u : 0u);
            return;
        }
        if (slot == 3u) {                               /* the row cursor: the rows, then END */
            ui.song_row = (uint8_t)clamp((int32_t)ui.song_row + steps, 0, (int32_t)n);
            return;
        }
        if (ui.song_row >= n && n) {                    /* END: K2 STOP / x2 .. x8 / LOOP */
            if (slot == 1u) {
                if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
                SONG_END(&chain_config) = (uint8_t)song_end_step(SONG_END(&chain_config), steps > 0 ? 1 : -1);
                song_idx_touch();
            }
            return;
        }
        if (slot == 1u) ui.arr_sect = (uint8_t)wrap_add((int32_t)(ui.arr_sect & 3u), steps, 4);
        else if (slot == 2u) {
            if (steps < 0 && arr_bars_get() <= 1u && ui.song_row < n) {   /* TIMES below x1: DELETE ROW? */
                if (chain_busy()) ui_message("STOP TO EDIT");
                else confirm_open(CF_DEL_ROW, ui.song_row);
                return;
            }
            ui.arr_bars = (uint8_t)clamp((int32_t)arr_bars_get() + steps, 1, (int32_t)CHAIN_BARS);
        }
        return;
    }
    if (chain_busy() && (pg->scope == SC_STEP || pg->graph == GR_STEPS ||
        0)) {
        ui_message("STOP TO EDIT"); return;
    }
    if (pg->scope == SC_STEP) {
        step_edit(slot, steps);
        return;
    }
    if (pg->scope == SC_TRK) {
        tracks_edit(slot, steps);
        return;
    }
    if (pg->graph == GR_BROWSE) {                         /* KNOB 1: the TYPE (chord-harp fork: PRESETS alone browses),
                                                         * KNOB 2: the next / previous engine */
        if (slot == 0u) {
            uint32_t ty = (uint32_t)wrap_add((int32_t)ptype_sel, steps, (int32_t)PT_USER + 1);
            if (ty != ptype_sel) {
                uint32_t total;
                ptype_sel = (uint8_t)ty;
                ui.force = 1;
                preset_pos(&total);
                if (total)
                    preset_go(0);                       /* (the user: the type's first sound, loaded) */
            }
        } else if (slot == 1u) {
            preset_engine_step(steps);
        } else if (slot == 2u) {
            preset_mark(!preset_favorite());            /* (2026-11-03: a pick of two, each detent the other) */
        } else if (slot == 3u && steps) {
            favorites.filter = !favorites.filter;
            ui.force = 1;
            settings_save();
        }
        return;
    }
    if (pg->graph == GR_SQTOOL) {                         /* (2026-11-03) SEQ > TOOLS: KNOB 1 the tool, 2 the lane */
        sqt_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_SLOTS && save_on() && save_level() == SV_PROJ) {   /* (0.8.5) SAVE > PROJECT's SECTIONS: KNOB 1
                                                         * the section, KNOB 2 its name (shown; EDIT renames it) */
        if (slot == 1u)
            return;
        if (slot == 0u)
            slot = 1u;
    }
    if (pg->graph == GR_PROG && slot == 2u) {            /* (chord-harp fork) SCL > PROG's RATE */
        prog_rate = (uint8_t)clamp((int32_t)prog_rate + (steps > 0 ? 1 : -1), 0, 2);
        return;
    }
    if (pg->graph != GR_PROG && (act_cols() >> slot) & 1u) {   /* an action's knob picks it (right) or drops it (left); */
        ui.act = steps > 0 ? (uint8_t)(slot + 1u) : ui.act == slot + 1u ? 0u : ui.act;
        return;
    }
    if ((pg->graph == GR_SLOTS || pg->graph == GR_ERASE) && slot == 0u) {   /* (0.6.12) KNOB 1 the project: its sections, the music kept */
        int32_t n = wrap_add((int32_t)song_cur, steps > 0 ? 1 : -1, (int32_t)NSONG);
        if ((uint32_t)n != song_cur) (void)song_open((uint32_t)n);
        return;
    }
    if (pg->graph == GR_USER) {                           /* KNOB 1 the slot */
        if (slot == 0u)
            ui.uslot = (uint8_t)wrap_add((int32_t)ui.uslot, steps, UP_SLOTS);
        return;
    }
    if (pg->graph == GR_SLPIT && slot == 0u) {           /* (0.6.3) SL PITCH: KNOB 1 the step, 2 its pitch */
        sl_ui_step = (uint8_t)wrap_add((int32_t)sl_ui_step, steps, 16);
        return;
    }
    if (pg->graph == GR_MSEQ && slot == 0u) {            /* (Yehnah) MSEQ: KNOB 1 the step, 2 its level */
        ms_ui_step = (uint8_t)wrap_add((int32_t)ms_ui_step, steps, 16);
        return;
    }
    if (fop_page(pg->graph)) {                           /* (0.6.10) FM6's operators: KNOB 1 OP, 2..4 its bytes (ui_fm6op.c) */
        fop_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_MOD && slot == 0u) {             /* MOD: KNOB 1 the slot, 2..4 its SRC DST AMT */
        mod_ui_slot = (uint8_t)wrap_add((int32_t)mod_ui_slot, steps > 0 ? 1 : -1, 4);
        return;
    }
    if (pg->graph == GR_SETS) {                         /* (topic C) GLO's lists: KNOB 1 the row (top to bottom, round), KNOB 2 its value
                                                         * (cur_page()'s copy has the row in slot 1); the other two knobs: nothing */
        uint32_t ls = sets_of(&PAGES[ui.page]) & 3u;
        if (slot == 0u) {
            sets_cur[ls] = (uint8_t)wrap_add((int32_t)sets_cur[ls], steps > 0 ? 1 : -1, (int32_t)sets_n(ls));
            ui.force = 1;
            return;
        }
        if (slot != 1u)
            return;
        if (pg->id[1] == G_INFO) {                      /* (2026-11-09) CPU: a turn right opens its page (graph, load figures) */
            if (steps > 0) {
                ui.menu = 3;
                ui.menu_scroll = 0;
                ui.force = 1;
            }
            return;
        }
        if (IS_MROW(pg->id[1]) && pg->id[1] - ID_MENU >= MI_VALUES && steps <= 0)
            return;                                     /* (an action: a turn right asks, left nothing) */
    }
    if (pg->fam == FAM_GLO)
        glo_k = (uint8_t)slot;                          /* (GLO: its row lit; SYSTEM: USB's choices or the CPU line) */
    if (IS_MROW(pg->id[slot])) {                        /* (2026-11-02) GLO: a MENU row, as the MENU turns it; an action
                                                         * (CALIBRATION INIT SYSTEM ABOUT) its YES / NO dialog */
        uint32_t r = pg->id[slot] - ID_MENU;
        if (r < MI_VALUES)
            menu_put(r, menu_step(r, steps)), ui.force = 1;   /* (THEME: the screen anew) */
        else
            confirm_open(r == MI_PANEL ? CF_CALIB : r == MI_INIT ? CF_INIT_SYS : CF_ABOUT, song.sel);
        return;
    }
    d = page_desc(pg, slot, &vp);
    if (!d || !vp || d->max == d->min)
        return;
    v = param_turn(d, *vp, accel(EN_K1 + slot, steps, d->fmt == F_ENUM ? 0 : d->max - d->min));
    *vp = (int16_t)v;
    if (vp == &trks_ui) {                               /* (0.6.18) GLO > SYSTEM's TRKS: the MENU's setting, saved */
        trks_put(v);
        {
            char m[24];
            fmt_int(m, (int32_t)v + 1);
            str_cpy(m + str_len(m), v ? " TRACKS" : " TRACK", 8);
            ui_message(m);
        }
        settings_save();
        return;
    }
#if FELUCCA_FXSCENE
    if (vp == &scene_ui) {                              /* (chord-harp fork) GLO > FX > SCENE: loaded as it turns */
        fx_scene((uint32_t)v);
        if (v)
            ui_message(N_SCENE[v]);
        return;
    }
#endif
    if (vp == &load_ui) {                               /* (0.6.30) GLO > SYSTEM's LOAD: the same */
        load_put(v);
        ui_message(v == 2 ? "LOAD FULL: ALL FX" : v ? "LOAD LITE: LIGHTER FX" : "LOAD ECO: NO SLICER/CHORUS");
        settings_save();
        return;
    }
    if (pg->scope != SC_GLOBAL && vp >= TSEL->p && vp < TSEL->p + P_COUNT)   /* (0.7.6: SCL's ALL TR is a global) */
        motion_capture(TSEL, (uint32_t)(vp - TSEL->p), *vp);
}

/* OCT+ on an action page: the picked action. A load stays picked (browse and load again); the others
 * are dropped once done. Flash writes only while stopped; over the user's data: the dialog */
static void act_do(void)
{
    uint32_t c = act_col(), id, k = (uint32_t)song.g[G_SLOT] - 1u;
    if (!c--)
        return;
    if (cur_page()->graph == GR_MOTION) {
        if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
        confirm_open(CF_CLEAR_MOTION, song.sel);
        return;
    }
    if (cur_page()->graph == GR_PROG) {                 /* (chord-harp fork) SCL > PROG: the progression on the track */
        ui_message(prog_write(TSEL, prog_rate));
        ui.act = 0;
        return;
    }
    if (cur_page()->graph == GR_PUNCH) {                /* (0.6.99) the punch-in lane cleared (a dialog) */
        if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
        confirm_open(CF_CLEAR_PFX, song.sel);
        return;
    }
    if (cur_page()->graph == GR_SQTOOL) {
        sqt_do();
        return;
    }
    if (cur_page()->graph == GR_TOOLS) {
        if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
        if (!act_ready()) {                               /* nothing there to clear or delete */
            ui_message(c == 1u ? "NOTHING TO DELETE" : "NOTHING TO CLEAR");
            return;
        }
        confirm_open(c == 0u ? CF_CLEAR_SEQ : c == 1u ? CF_DEL_ROW : c == 2u ? CF_CLEAR_SONG : CF_INIT_SYS,
                     c == 1u ? ui.song_row : song.sel);
        return;
    }
    if (cur_page()->graph == GR_SONG) {                 /* (2026-11-05) ARRANGE: OCT+ adds the SECT and BARS shown as a row: at the
                                                         * end (the cursor stays on NEW), or after the row picked */
        uint32_t n = chain_config.count, at, i;
        if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
        if (n >= CHAIN_ROWS) { ui_message("16 ROWS"); return; }
        at = ui.song_row < n ? ui.song_row + 1u : n;    /* (always after the selected row; on END, at the end) */
        for (i = n; i > at; i--) {
            chain_config.row[i] = chain_config.row[i - 1u];
            song_idx.scene[song_cur][i] = song_idx.scene[song_cur][i - 1u];
        }
        chain_config.row[at].slot = (uint8_t)(ui.arr_sect & 3u);
        chain_config.row[at].bars = (uint8_t)arr_bars_get();
        memset(&song_idx.scene[song_cur][at], 0, sizeof song_idx.scene[0][0]);   /* (no scene yet) */
        chain_config.count++;
        ui.song_row = (uint8_t)at;                      /* (the cursor follows the new row) */
        song_idx_touch();
        ui_message("INSERTED");
        return;
    }
    if (cur_page()->graph == GR_USER) {                   /* 1 LOAD, 2 ERASE, 3 SAVE (the NAME screen first) */
        if (c > 1u)
            ui.act = 0;
        if (c == 2u && up_used(ui.uslot) && !transport_busy())
            confirm_open(CF_ERASE_USER, ui.uslot);        /* ERASE: the dialog first */
        else if (c != 3u)
            up_ui(c - 1u, ui.uslot);
        else if (transport_busy())
            ui_message("STOP TO SAVE");
        else if (up_used(ui.uslot))
            confirm_open(CF_OVR_USER, ui.uslot);
        else
            name_open(NK_USER_SAVE, ui.uslot);
        return;
    }
    if (cur_page()->graph == GR_ERASE) {                 /* (2026-11-05) K3 the section picked, K4 the project: a dialog */
        if (transport_busy()) { ui_message("STOP TO ERASE"); return; }
        if (!act_ready()) { ui_message("NOTHING TO ERASE"); return; }
        confirm_open(c == 2u ? CF_ERASE_SEC : CF_ERASE_PRJ, c == 2u ? k : song_cur);
        return;
    }
    id = cur_page()->id[c & 3u];
    if (id != G_LOAD)
        ui.act = 0;
    switch (id) {
    case G_LOAD:
        undo_mark();                                      /* (0.6.21) UNDO: the music before the load */
        project_load(k);
        break;
    case G_SAVE:                                          /* the NAME screen writes it */
        if (transport_busy())
            ui_message("STOP TO SAVE");
        else if (project_used(k))
            confirm_open(CF_OVR_PROJ, k);
        else
            name_open(NK_PROJ_SAVE, k);
        break;
    case G_CLRSEQ:
        if (chain_busy()) { ui_message("STOP TO EDIT"); break; }
        confirm_open(CF_CLEAR_SEQ, song.sel);
        break;
    default:                                              /* G_INITSND */
        set_engine(TSEL->eng_req);                        /* engine defaults + its first preset (the steps stay) */
        ui_message("SOUND INIT");
        ui.force = 1;
        break;
    }
}

/* (chord-harp fork) the action pages OCT- / OCT+ answer on: act_cols' (SCL > PROG only while WRITE is picked, F#4: else
 * its OCT- / OCT+ are the octave, as on HARP and OMNI) */
static uint32_t act_oct(void) { return act_cols(); }

/* OCT- / OCT+ where they answer (the dialogs, the menu, action pages): on release, and only a press
 * that began there; both down together (UPDATE MODE, main.c) is no tap. Bit 0 OCT-, bit 1 OCT+ */
static uint32_t oct_taps(uint32_t pressed, int here)
{
    static uint8_t down, chord;
    uint32_t dn = panel.btn[B_OCTDN], up = panel.btn[B_OCTUP];
    uint32_t now = ((fm1_in.buttons >> dn) & 1u) | ((fm1_in.buttons >> up) & 1u) << 1, tap;
    if (here)
        down |= (uint8_t)(((pressed >> dn) & 1u) | ((pressed >> up) & 1u) << 1);
    if (now == 3u)
        chord = 1;
    tap = down & ~now;
    down &= (uint8_t)now;
    if (chord) {
        tap = 0;
        chord = now != 0u;
    }
    return tap;
}

/* SEQ step entry = step recording (1.2, Discussion #133: only with the track armed and the transport stopped), acid
 * style: the keys pressed together (POLY: up to 4 notes, MONO: the last one) become the cursor step; releasing all
 * keys moves on one step (wrapping inside LEN) */
/* (0.6.37) the entry let go: a chord held for LEN steps (TIEs after it), or arpeggiated (ARP pressed while held): its
 * notes over the octaves in the ORDER, one every RES steps; the cursor after it */
static const char *const ENT_ORD[4] = {"UP", "DOWN", "UP-DN", "RND"};
static const char *const ENT_RES[4] = {"1/16", "1/8", "1/8.", "1/4"};
static void ent_message(void)
{
    char a[24], b[24];
    if (ui.ent_arp) {
        str_cpy(a, "ARP ", sizeof a); str_cpy(a + str_len(a), ENT_ORD[ui.ent_ord & 3u], 8);
        str_cpy(b, ENT_RES[(ui.ent_res - 1u) & 3u], 8); str_cpy(b + str_len(b), " x", 4);
        fmt_int(b + str_len(b), ui.ent_oct); str_cpy(b + str_len(b), " OCT", 6);
    } else {
        str_cpy(a, "LEN ", sizeof a); fmt_int(a + str_len(a), ui.ent_len);
        str_cpy(a + str_len(a), ui.ent_len > 1u ? " STEPS" : " STEP", 8);
        str_cpy(b, "ARP: ARPEGGIATE", sizeof b);
    }
    str_cpy(ui.msg, a, sizeof ui.msg); str_cpy(ui.msg2, b, sizeof ui.msg2); ui.msg_t = 40;
}
static void ent_knob(uint32_t k, int32_t s)
{
    int32_t d = clamp(s, -4, 4);                        /* (detents, not accelerated: small ranges) */
    if (k == 0u && !ui.ent_arp) ui.ent_len = (uint8_t)clamp(ui.ent_len + d, 1, TSEL->p[P_SLEN] > 16 ? 16 : TSEL->p[P_SLEN]);
    else if (k == 0u) ui.ent_res = (uint8_t)clamp(ui.ent_res + d, 1, 4);
    else if (k == 1u) ui.ent_ord = (uint8_t)clamp(ui.ent_ord + d, 0, 3);
    else ui.ent_oct = (uint8_t)clamp(ui.ent_oct + d, 1, 3);
    ent_message();
}
static void ent_commit(void)
{
    track_t *t = TSEL;
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, cur = ui.cursor, i, j, n = 0, span;
    uint8_t vel = t->step[cur].vel, list[24];
    if (!ui.ent_arp || !ui.ent_nn) {
        span = ui.ent_len < len ? ui.ent_len : len;
        for (i = 1; i < span; i++) {
            step_t *s = &t->step[(cur + i) % len];
            memset(s, 0, sizeof *s);
            s->time = ST_TIE; s->vel = vel;
        }
        cursor_set((int32_t)(cur + (span ? span : 1u)));
        return;
    }
    for (j = 0; j < ui.ent_oct; j++)                    /* the notes over the octaves, low to high */
        for (i = 0; i < ui.ent_nn; i++)
            if (ui.ent_notes[i] + 12u * j <= 127u) list[n++] = (uint8_t)(ui.ent_notes[i] + 12u * j);
    if (ui.ent_ord == 1u)                               /* DOWN */
        for (i = 0; i < n / 2u; i++) { uint8_t x = list[i]; list[i] = list[n - 1u - i]; list[n - 1u - i] = x; }
    else if (ui.ent_ord == 2u && n > 2u)                /* UP-DN: back down without the ends twice */
        for (i = n - 2u; i > 0u && n < sizeof list; i--) list[n++] = list[i];
    else if (ui.ent_ord == 3u)                          /* RND */
        for (i = n; i > 1u; i--) { uint32_t r = rng() % i; uint8_t x = list[i - 1u]; list[i - 1u] = list[r]; list[r] = x; }
    if (n * ui.ent_res > len) n = len / ui.ent_res ? len / ui.ent_res : 1u;
    span = n * ui.ent_res;
    for (i = 0; i < span; i++) {
        step_t *s = &t->step[(cur + i) % len];
        memset(s, 0, sizeof *s);
        s->vel = vel;
        if (i % ui.ent_res == 0u) { s->time = ST_NOTE; s->n = 1; s->note[0] = list[i / ui.ent_res]; }
        else s->time = ST_TIE;
    }
    cursor_set((int32_t)(cur + span));
}
static void ent_note(track_t *t, step_t *st, uint32_t note)   /* one note into the entry (the step, the arpeggio's) */
{
    uint32_t j;
    if (!ui.entry_open) {
        ui.entry_open = 1;
        st->n = 0;
        st->time = ST_NOTE;
        ui.ent_len = ui.ent_res = ui.ent_oct = 1; ui.ent_arp = ui.ent_ord = ui.ent_nn = 0;
    }
    for (j = 0; j < ui.ent_nn && ui.ent_notes[j] < note; j++)   /* (the arpeggio's notes, in order of pitch) */
        ;
    if (ui.ent_nn < sizeof ui.ent_notes && (j == ui.ent_nn || ui.ent_notes[j] != note)) {
        uint32_t m;
        for (m = ui.ent_nn; m > j; m--) ui.ent_notes[m] = ui.ent_notes[m - 1u];
        ui.ent_notes[j] = (uint8_t)note; ui.ent_nn++;
    }
    if (t->p[P_VOICE] && !ENGINES[t->engine]->oneshot) {   /* (drums: hits stack as a chord) */
        st->note[0] = (uint8_t)note;
        st->n = 1;
    } else if (st->n < 4u) {
        for (j = 0; j < st->n && st->note[j] != note; j++)
            ;
        if (j == st->n)
            st->note[st->n++] = (uint8_t)note;
    }
}
static void seq_entry(uint32_t pressed)
{
    track_t *t = TSEL;
    step_t *st = &t->step[ui.cursor];
    uint32_t k, ti = song.sel % NTRK;
    for (k = 0; k < 27u; k++) {
        uint32_t note, c, nc;
        uint8_t cn[KB_CH];
        if (!((pressed >> k) & 1u))
            continue;
        if (harp_on(t) && harp_idk(t, k)) {             /* (chord-harp) a HARP chord key: the step its CHORD */
            st->n = 0;
            ent_note(t, st, harp_root(t, k));
            harp_step(t, st, k);
            last_note = st->note[0];
            continue;
        }
        note = harp_on(t) && harp_ck(t, k) ? harp_root(t, k) : kb_map(t, k);   /* (a J-6 chord key: its notes) */
        if (note == KB_SILENT)
            continue;
        if (kb_sn[k] && kb_trk[k] == ti) {             /* (0.6.83) SCL > CHORD: the chord the key sounds (seq.c key_on), */
            nc = kb_sn[k];                              /* all of it into the step, as live recording does */
            memcpy(cn, kb_snd[k], KB_CH);
        } else if (harp_on(t) && harp_ck(t, k)) {
            nc = harp_build(t, k, cn);
        } else if (harp_on(t)) {                        /* (a plate key: its note; CHORD's too) */
            nc = 1;
            cn[0] = (uint8_t)note;
        } else {                                        /* (the key not in yet: built here, LEAD's memory as it was) */
            uint8_t last[KB_CH], nl = chord_nlast[ti];
            memcpy(last, chord_last[ti], KB_CH);
            nc = chord_build(t, note, cn);
            memcpy(chord_last[ti], last, KB_CH);
            chord_nlast[ti] = nl;
        }
        for (c = 0; c < nc && c < KB_CH; c++)
            ent_note(t, st, cn[c]);
        last_note = (uint8_t)note;
    }
    if (ui.entry_open && !(fm1_in.notes & ~kb_layer))
        ent_commit();                                   /* (let go: LEN / the arpeggio written, the cursor on) */
}

/* (0.9.2, rule 3) PRESETS loads presets, on every page (the menu, a dialog, NAME and the layers: ui_input before this;
 * GEN: its GENRE, there): the selected track's sound, one list over every engine, then the used user presets
 * (preset_step; a sound load keeps the steps); in SEQ the patterns: the PHRASES loader over the page (KNOB 1 the pattern
 * too, OCT+ LOAD, OCT- back to the page), each detent the next one. Never a page, a track, a module, a step or a row
 * (until 0.9.1 it was the bar, the step, the row, the module, the track, ROOT ..: KNOB 1's now, rule 4) */
static void presets_turn(int32_t s)
{
    preset_step(s);
}

/* (chord-harp fork) HOME > IDEAS (ideas.c): what each of its pages throws for the selected track and what OCT- BACK
 * puts back. The sound (0.5's SELECT dice, now IDEAS > SOUND's three knobs): PRESET one of its engine's factory
 * presets as it is; VARY the sound as it was (BEFORE, or the last PRESET / WILD) with each engine parameter moved up to
 * a quarter of its range (an ENUM: another value, one time in three); WILD each anywhere in its range. DRUM-X: both
 * patches (A, B) of every sound moved from the factory kit's, up to +-40 (WILD +-64; COLOR up 12 at most: no knife
 * edges), the wave one time in five SINE or FM (WILD: any) */
enum { IX_TRACK, IX_SOUND, IX_LFO, IX_FX, IX_TEX, IX_N };
enum { SI_PRESET, SI_VARY, SI_WILD };
static int32_t dice_by(int32_t v, int32_t lo, int32_t hi, int32_t span)
{
    return clamp(v + (int32_t)(rng() % (uint32_t)(2 * span + 1)) - span, lo, hi);
}
static int16_t si_base[8];                              /* (VARY's sound: BEFORE, or the last PRESET / WILD) */
static void si_base_take(const track_t *t)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        si_base[i] = t->p[P_E0 + i];
}
static const char *sound_idea(track_t *t, uint32_t how)
{
    uint32_t e = trk_eng(t), i, l, n = ENG_NP(ENGINES[e]);
    if (e == ENGI_DRUM && drum_elsewhere(t)) {          /* (0.8.3, reported again) the section's one kit is another
                                                         * track's: its drums stay as they are */
        static char m[16] = "KIT OF T? KEPT";
        m[8] = (char)('0' + drum_elsewhere(t));
        return m;
    }
    if (how == SI_PRESET) {                             /* (another than it has, when there is one) */
        if (!n)
            return "NO PRESETS";
        l = n > 1u ? (t->preset + 1u + rng() % (n - 1u)) % n : 0u;
        apply_preset_to(t, l);
        si_base_take(t);
        return ENGINES[e]->presets[l].name;
    }
    if (e == ENGI_DRUM) {
        int32_t sp = how == SI_WILD ? 64 : 40;
        for (l = 0; l < 8u; l++) {
            const dx_lane_t *D = &DX_KIT_DEF[l];
            dx_lane_t *L = &dx_kit[l];
            for (i = 0; i < DXP_N; i++) {               /* (COLOR: the brightness, down 40 but up 12 at most) */
                int32_t up = i == DXP_COLOR ? 12 : 127;
                L->a[i] = (uint8_t)dice_by(D->a[i], 0, D->a[i] + up < 127 ? D->a[i] + up : 127, sp);
                L->b[i] = (uint8_t)dice_by(D->b[i], 0, D->b[i] + up < 127 ? D->b[i] + up : 127, sp);
            }                                            /* (a new wave: SINE or FM, the gentle two) */
            L->mode = (uint8_t)((D->mode & ~3u) | (how == SI_WILD ? rng() % 4u : rng() % 5u ? (D->mode & 3u) : rng() % 2u));
        }
        for (l = 0; l < 8u; l++) {                      /* (0.6.49) LAYER strong where a note is in the way (snare,
                                                         * hats, rim, bell), light on the kick, clap and tom; FEEDBK
                                                         * one sound in three */
            static const uint8_t LO[8] = {0, 4, 0, 6, 6, 0, 3, 8}, HI[8] = {6, 15, 8, 15, 15, 7, 12, 15};
            uint32_t la = LO[l] + rng() % (HI[l] - LO[l] + 1u), fb = rng() % 3u ? 0u : 1u + rng() % 9u;
            dx_lay[l] = (uint8_t)(la | fb << 4);
        }
        return how == SI_WILD ? "WILD KIT" : "KIT VARIED";
    }
    for (i = 0; i < 8u; i++) {
        const param_desc_t *d = track_desc(t, P_E0 + i);
        int32_t span = (d->max - d->min) / 4;
        if (!d->label || d->label[0] == '-' || d->max <= d->min)
            continue;
        if (how == SI_WILD)
            t->p[P_E0 + i] = (int16_t)(d->min + (int32_t)(rng() % (uint32_t)(d->max - d->min + 1)));
        else if (d->fmt == F_ENUM)
            t->p[P_E0 + i] = (int16_t)(rng() % 3u ? si_base[i] : d->min + (int32_t)(rng() % (uint32_t)(d->max - d->min + 1)));
        else
            t->p[P_E0 + i] = (int16_t)dice_by(si_base[i], d->min, d->max, span ? span : 1);
    }
    if (how == SI_WILD)
        si_base_take(t);
    return how == SI_WILD ? "WILD SOUND" : "SOUND VARIED";
}
static const uint8_t DK_LFO[8] = {P_LRATE, P_LWAVE, P_LPHASE, P_LFADE, P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP};
static const uint8_t DK_FXT[9] = {P_DIST, P_DTYPE, P_DTONE, P_CHOR, P_DLY, P_REV, P_APDEP, P_APRATE, P_APWAVE};
static const uint8_t DK_TMIX[3] = {P_LEVEL, P_PAN, P_REV};
static const uint8_t DK_FXG[6] = {G_RTYPE, G_RSIZE, G_RDAMP, G_DTIME, G_DFDBK, G_DPIT};
static struct {                                         /* what BACK puts back (the sound: undo_sound) */
    step_t step[NSTEP];
    int16_t slen, scale, v[9], g[6];
} dk;
static int32_t dk_rr(int32_t lo, int32_t hi) { return hi <= lo ? lo : lo + (int32_t)(rng() % (uint32_t)(hi - lo + 1)); }
static int32_t dk_frac(const param_desc_t *d, int32_t a, int32_t b)   /* a value a% .. b% up the range */
{
    int32_t span = d->max - d->min;
    return dk_rr(d->min + span * a / 100, d->min + span * b / 100);
}
/* (0.8.5) the BEFORE: the sound as it was (chord-harp fork: its engine and EDIT values, drawn as its pixel art; was its
 * being; the steps, LFO and FX before are dk's) */
static struct { int16_t pe[8]; uint8_t e; } dkv;
static void dk_vis_take(void)
{
    uint32_t i;
    dkv.e = (uint8_t)trk_eng(TSEL);
    for (i = 0; i < 8u; i++)
        dkv.pe[i] = TSEL->p[P_E0 + i];
}
static void dice_side(uint32_t kind, uint32_t now, int32_t x0)   /* one half of IDEAS' panel: BEFORE (0) or NOW */
{
    const track_t *t = TSEL;
    uint32_t i;
    cv_text_in(x0, 6 - AF_X_CAP_Y, 112, &AF_X, now ? "NOW" : "BEFORE", T_DIM, T_SURF);
    if (kind == IX_SOUND) {
        if (now)
            (void)vx_draw(t, x0 + 4, 30, 104, 72, -1);
        else {                                          /* (its engine and EDIT values then, still) */
            vx_pe = dkv.pe;
            (void)vx_draw_e(t, dkv.e, x0 + 4, 30, 104, 72, 0);
            vx_pe = 0;
        }
    } else if (kind == IX_TRACK || kind == IX_TEX) {    /* the first 16 steps' notes as dots */
        const step_t *st = now ? t->step : dk.step;
        for (i = 0; i < 16u; i++)
            if (step_on(&st[i]))
                cv_rect(x0 + 4 + (int32_t)i * 7, 96 - clamp((int32_t)st[i].note[0] - 36, 0, 40) * 2, 5, 3,
                        now ? T_ACCENT : T_MID);
    } else {                                            /* LFO / FX: their four first values as bars */
        const uint8_t *ids = kind == IX_LFO ? DK_LFO : DK_FXT;
        for (i = 0; i < 4u; i++) {
            int16_t v = now ? t->p[ids[i]] : dk.v[i];
            const param_desc_t *d = track_desc(t, ids[i]);
            int32_t w = d ? clamp(RATIO(d, enum_rank(d, v)), 0, 1000) * 96 / 1000 : 0;
            cv_text(x0 + 8, 20 + (int32_t)i * 20 - AF_X_CAP_Y, &AF_X, d ? d->label : "", T_DIM);
            cv_rect(x0 + 8, 29 + (int32_t)i * 20, 96, 3, T_LINE);
            cv_rect(x0 + 8, 29 + (int32_t)i * 20, w ? w : 1, 3, now ? T_ACCENT : T_MID);
        }
    }
}
static void dk_keep(uint32_t kind)                      /* the BEFORE of a page's first throw */
{
    track_t *t = TSEL;
    uint32_t i;
    dk_vis_take();
    if (kind == IX_TRACK || kind == IX_TEX) {           /* (TEXTURE: its mix too; its sound undo_sound's) */
        for (i = 0; i < NSTEP; i++) dk.step[i] = t->step[i];
        dk.slen = t->p[P_SLEN];
        dk.scale = t->p[P_SCALE];
        for (i = 0; i < 3u; i++) dk.v[i] = t->p[DK_TMIX[i]];
    } else if (kind == IX_SOUND) {
        si_base_take(t);
    } else if (kind == IX_LFO) {
        for (i = 0; i < 8u; i++) dk.v[i] = t->p[DK_LFO[i]];
    } else {
        for (i = 0; i < 9u; i++) dk.v[i] = t->p[DK_FXT[i]];
        for (i = 0; i < 6u; i++) dk.g[i] = song.g[DK_FXG[i]];
    }
}
static int dk_back(uint32_t kind, uint32_t k)           /* BACK: 0 done */
{
    track_t *t = &trk[k % NTRK];
    uint32_t i;
    if (kind == IX_SOUND || (kind == IX_TEX && undo_sound(k)))   /* (TEXTURE: its sound, then its mix and steps) */
        return kind == IX_SOUND ? undo_sound(k) : 1;
    if (kind == IX_TRACK || kind == IX_TEX) {
        for (i = 0; kind == IX_TEX && i < 3u; i++) t->p[DK_TMIX[i]] = dk.v[i];
        load_begin(t, LOAD_PAT);
        for (i = 0; i < NSTEP; i++) t->step[i] = dk.step[i];
        t->p[P_SLEN] = dk.slen;
        t->p[P_SCALE] = dk.scale;
        load_end(t);
        latch_drop_req |= (uint8_t)(1u << (k % NTRK));
    } else if (kind == IX_LFO) {
        for (i = 0; i < 8u; i++) t->p[DK_LFO[i]] = dk.v[i];
    } else {
        for (i = 0; i < 9u; i++) t->p[DK_FXT[i]] = dk.v[i];
        for (i = 0; i < 6u; i++) song.g[DK_FXG[i]] = dk.g[i];
    }
    return 0;
}
/* the LFO: its rate in the middle of the range, any wave and phase, a fade now and then; aimed mostly at the filter,
 * a little at the shape, seldom at the pitch or the level (a wobble, not a siren). k (IDEAS' knob): 0 all, 1 RATE
 * (rate, fade), 2 WAVE (wave, phase), 3 TARGET (the depths) */
#define DK_ON(g) (!k || k == (g))
static void lfo_dice(track_t *t, uint32_t k)
{
    const param_desc_t *d;
    if (DK_ON(1u)) { d = track_desc(t, P_LRATE); t->p[P_LRATE] = (int16_t)dk_frac(d, 25, 80); }
    if (DK_ON(2u)) { d = track_desc(t, P_LWAVE); t->p[P_LWAVE] = (int16_t)dk_frac(d, 0, 100); }
    if (DK_ON(2u)) { d = track_desc(t, P_LPHASE); t->p[P_LPHASE] = (int16_t)(rng() % 3u ? d->min : dk_frac(d, 0, 100)); }
    if (DK_ON(1u)) { d = track_desc(t, P_LFADE); t->p[P_LFADE] = (int16_t)(rng() % 3u ? d->min : dk_frac(d, 0, 50)); }
    if (!DK_ON(3u)) return;
    d = track_desc(t, P_LD_PIT); t->p[P_LD_PIT] = (int16_t)(rng() % 4u ? 0 : dk_frac(d, 44, 56));
    d = track_desc(t, P_LD_FLT); t->p[P_LD_FLT] = (int16_t)dk_frac(d, d->min < 0 ? 15 : 20, d->min < 0 ? 85 : 70);
    d = track_desc(t, P_LD_SHP); t->p[P_LD_SHP] = (int16_t)(rng() % 2u ? 0 : dk_frac(d, d->min < 0 ? 30 : 0, d->min < 0 ? 70 : 40));
    d = track_desc(t, P_LD_AMP); t->p[P_LD_AMP] = (int16_t)(rng() % 3u ? 0 : dk_frac(d, 0, 35));
}
/* the effects: the track's drive (half the time none), its sends, an AUTOPAN one time in three; the shared reverb's
 * kind and size and the delay's time and feedback, a pitched delay one time in five. k: 0 all, 1 DRIVE (the drive),
 * 2 SPACE (the sends, the shared reverb and delay: every track hears them), 3 MOVE (the chorus, the AUTOPAN) */
static void fx_dice(track_t *t, uint32_t k)
{
    const param_desc_t *d;
    if (DK_ON(1u)) {
        d = track_desc(t, P_DIST); t->p[P_DIST] = (int16_t)(rng() % 2u ? 0 : dk_frac(d, 8, 55));
        d = track_desc(t, P_DTYPE); t->p[P_DTYPE] = (int16_t)dk_frac(d, 0, 100);
        d = track_desc(t, P_DTONE); t->p[P_DTONE] = (int16_t)dk_frac(d, 30, 80);
    }
    if (DK_ON(3u)) { d = track_desc(t, P_CHOR); t->p[P_CHOR] = (int16_t)(rng() % 2u ? 0 : dk_frac(d, 15, 70)); }
    if (DK_ON(2u)) {
        d = track_desc(t, P_DLY); t->p[P_DLY] = (int16_t)dk_frac(d, 0, 65);
        d = track_desc(t, P_REV); t->p[P_REV] = (int16_t)dk_frac(d, 10, 85);
    }
    if (DK_ON(3u)) {
        d = track_desc(t, P_APDEP); t->p[P_APDEP] = (int16_t)(rng() % 3u ? 0 : dk_frac(d, 25, 80));
        d = track_desc(t, P_APRATE); t->p[P_APRATE] = (int16_t)dk_frac(d, 20, 80);
        d = track_desc(t, P_APWAVE); t->p[P_APWAVE] = (int16_t)dk_frac(d, 0, 100);
    }
    if (!DK_ON(2u)) return;
    song.g[G_RTYPE] = (int16_t)dk_rr(GP[G_RTYPE].min, GP[G_RTYPE].max);
    song.g[G_RSIZE] = (int16_t)dk_rr(GP[G_RSIZE].min + (GP[G_RSIZE].max - GP[G_RSIZE].min) / 3, GP[G_RSIZE].max);
    song.g[G_RDAMP] = (int16_t)dk_rr(GP[G_RDAMP].min, GP[G_RDAMP].max);
    song.g[G_DTIME] = (int16_t)dk_rr(GP[G_DTIME].min, GP[G_DTIME].max);
    song.g[G_DFDBK] = (int16_t)dk_rr(20, 70);
    {   static const int8_t PIT[4] = {-12, -5, 7, 12};
        song.g[G_DPIT] = (int16_t)(rng() % 5u ? 0 : clamp(PIT[rng() % 4u], GP[G_DPIT].min, GP[G_DPIT].max)); }
}
#undef DK_ON
static void macro_dice(uint32_t mode);
static int16_t sl_root = -1, sl_scale, sl_quant;      /* (scale_link: the song's scale) */
#include "song_dice.c"                             /* (0.6.26) the genres, the roles' figures (IDEAS > TRACK) */
#if YEHNAH_GEN
#include "gen.c"                                   /* (0.6.91) GEN: the song generator (the host's tests and tools) */
#endif
#include "ideas.c"                                 /* (chord-harp fork) HOME > IDEAS */

/* HOME / REC / SEQ / SAVE: a tap on release. t0 = press time | 1, bit 1 = swallowed (no tap when let go). (2026-11-03,
 * the user's: only FX, SCL and REC have a held state) held past HOLD (the layers' time, MENU > HOLD) it is swallowed:
 * let go, nothing (hold: REC's is its layer's, ui_layer.c) */
enum { BT_NONE, BT_TAP };
static uint32_t btn_hold(uint32_t *t0, uint32_t label, uint32_t now, int hold)
{
    uint32_t tap;
    if ((fm1_in.buttons >> panel.btn[label]) & 1u) {
        if (!*t0)
            *t0 = (now | 1u) & ~2u;
        else if (hold && now - (*t0 & ~3u) >= (uint32_t)HOLD_MS[settings_hold % 4u] * 1000u * FM1_TICKS_PER_US)
            *t0 |= 2u;
        return BT_NONE;
    }
    tap = *t0 && !(*t0 & 2u);
    *t0 = 0;
    return tap ? BT_TAP : BT_NONE;
}

/* the quick layers: ui_layer.c (included after this file) */
static void layer_masks(void);
static void hm_tick(void);
static void hm_close(void);
static void hm_hold(uint32_t b);
static void hm_knob(uint32_t k, int32_t s);
static uint32_t hm_btn_is(uint32_t b);
static void macro_dice(uint32_t mode);                  /* (0.4) */
static void layer_arm(uint32_t pressed, uint32_t now);
static uint32_t layer_held(void);
static int layer_knobs_quiet(void);
static uint32_t layer_gesture(uint32_t now, uint32_t combo);
static void layer_show(void);
static void layer_tap(uint32_t l);
static void layer_keys(uint32_t keys);
static void layer_knob(uint32_t k, int32_t s);
static int layer_set_open(void);
static uint32_t layer_oct(uint32_t pressed, uint32_t oct);
static void layer_oct_open(uint32_t pressed);
static int layer_allowed(void);
static uint32_t ly_bit(uint32_t l);

static uint16_t pg_ms[14];                              /* (2026-11-03) a page button's press (fm1_ms): held past HOLD,
                                                         * no tap */
/* a page button let go (they act on release; a layer's own button: layer_gesture): its section (rule 2; 0.9.2: EDIT is
 * always EDIT: a slot's rename REC + F5, ui_layer.c; 2026-11-03: a step's clear the NOTE knob's NONE). 1: it acted
 * (a step held on STEP + EDIT: its locks go, the step stays) instead of opening a page */
static int page_tap(uint32_t b)
{
    uint32_t f;
    if (b == B_EDIT && song.seq_mode && !ui.home && cur_page()->graph == GR_ROLL && lock_held()) {
        if (chain_busy()) ui_message("STOP TO EDIT");
        else lock_clear(lock_held());
        return 1;
    }
    for (f = FAM_HOME + 1u; f < FAM_COUNT; f++)
        if (FAM_BTN[f] == b) {
            open_family(f);
            return 0;
        }
    return 0;
}
/* a track whose sound's sample is missing (engines.c snd_missing: an empty user slot, a built-in set this build lacks;
 * it plays a sine): the no-file icon and NO SAMPLE (ui.c MSG_NO_SAMPLE). Once per track and source: at power-on, a sound
 * or project load, SRC / SET turned, a slot erased; again after another source or a project load. One a frame; after
 * a message already showing (LOADED) as the second message. Not on notes: nothing in the audio path */

static void hints_reset(void);                          /* (ui_layer.c) */
/* messages of things that happened elsewhere (a load, the editor, MIDI in): after this frame's own */
static void ui_notices(void)
{
    static uint32_t midi_t, midi_last;
    if (motion_full) { motion_full = 0; ui_message("AUTOMATION FULL"); }
    if (midi_hint) {                                    /* MIDI notes into a track that is not selected */
        uint32_t h = midi_hint;
        midi_hint = 0;
        if (!ui.msg_t && (h != midi_last || fm1_ms - midi_t > 4000u)) {
            char b[4] = {'T', (char)('0' + h), 0, 0};
            ui_say("MIDI IN -> ", b);
            midi_last = h;
            midi_t = fm1_ms;
        }
    }
}

/* (0.4) one scale for the whole instrument. (0.6.19) Strictly: ROOT, SCALE and QNT are the song's, held here;
 * the selected track's (a knob, the SCL layer, the editor, MIDI, automation, a project load) is the one that counts,
 * and every melodic track is kept on it each frame (any other way a track got its own scale undone: a sequence
 * never plays in another scale). A DRUM track has none to follow. Each track keeps its TRN */
static void scale_link(void)
{
    const track_t *t = TSEL;
    uint32_t k;
    for (k = 0; k < NTRK && drum_track(t); k++) t = &trk[k];   /* (0.8.3) a DRUM track selected: the first melodic
                                                         * track's counts (a section the song brings in its own key:
                                                         * its key, not the one before it) */
    if (!drum_track(t) && (t->p[P_ROOT] != sl_root || t->p[P_SCALE] != sl_scale || t->p[P_QUANT] != sl_quant)) {
        sl_root = t->p[P_ROOT];
        sl_scale = t->p[P_SCALE];
        sl_quant = t->p[P_QUANT];
    }
    if (sl_root < 0)
        return;                                         /* (a DRUM track selected from the start: nothing yet) */
    for (k = 0; k < NTRK; k++)
        if (!drum_track(&trk[k])) {
            trk[k].p[P_ROOT] = sl_root;
            trk[k].p[P_SCALE] = sl_scale;
            trk[k].p[P_QUANT] = sl_quant;
        }
}

static void ui_input_frame(void);
static void ui_input(void)
{
    ui_input_frame();
    scale_link();                                       /* (0.4) one scale for every melodic track (after the
                                                         * frame's edits) */
}
static void ui_input_frame(void)
{
    uint32_t pressed = fm1_input_edges(0), notes = fm1_input_note_edges(), now = fm1_ticks(), id, b, k;
    uint32_t home, rec, seq;
    uint32_t oct;
    uint32_t lay, combo = 0, lytap, lkeys, kq = 0;
    int32_t s, ks[4] = {0, 0, 0, 0};
    if (rp_done) {                                      /* (chord-harp fork) REC > ONE PASS: done, stopped */
        rp_done = 0;
        ui_message("ONE PASS RECORDED");
    }
    fm6_poll();                                         /* FM6: PTCH turned -> its patch */
    {   /* (0.6.5) RESO's PITCH and FOLW live in PRE's and DAMP's slots: into RESO a ROOM's values become theirs
         * (DAMP past FOLW's T6: OFF; PRE past 14: 14), out of it the reverb's own defaults back */
        uint32_t rt = (uint32_t)song.g[G_RTYPE], rt0 = rev_rt0;
        if (rt == 3u) {
            if (song.g[G_RDAMP] > 6) song.g[G_RDAMP] = 0;
            if (song.g[G_RPRE] > 14) song.g[G_RPRE] = 14;
            if (song.g[G_RPRE] < 0) song.g[G_RPRE] = 0;
        } else if (rt0 == 3u) {
            song.g[G_RDAMP] = GP[G_RDAMP].def;
            song.g[G_RPRE] = GP[G_RPRE].def;
        }   /* (0.6.12) PRE (ROOM, SPRING) and AIR (SHIMMER, CLOUD) share the slot: from one to the other it starts at 0 */
        if (rt0 != 0xFFu && rt0 != rt && (rt == 2u || rt == 4u) != (rt0 == 2u || rt0 == 4u) && rt != 3u && rt0 != 3u)
            song.g[G_RPRE] = 0;
        if (rt != 2u && rt != 4u && song.g[G_RPRE] < 0) song.g[G_RPRE] = 0;
        if ((rt == 2u || rt == 4u) && song.g[G_RPRE] > 24) song.g[G_RPRE] = 24;
        rev_rt0 = (uint8_t)rt;
    }
    for (k = 0; k < NTRK; k++)                          /* a retired engine's sound any other way (core.h): ANALOG */
        if (eng_gone(trk[k].eng_req))
            set_engine_of(&trk[k], ENGI_PHYS_TO);
    perf_latch_on = fx_latch & 1u;                      /* (MENU > FX LATCH; a settings load sets it too) */
    fx_usb_fixed = (ui_prefs & PREF_USB_FIXED) != 0u;   /* (MENU > USB LEVEL: fx.c, audio.c) */
    fx_usb_gain = USB_GAIN[fx_usb_fixed ? usb_lvl % 5u : 0u];   /* (0.7.0) .. and its attenuation (usb.c uac_tap) */
    mo_lvl = (uint8_t)(midi_out_mode & 3u);             /* (0.6.72) MENU > MIDI OUT (seq.c) */
    midi_pc_poll();                                     /* (0.6.72) PROGRAM CHANGE in: the sounds it asked for */
    rp_apply();                                        /* (MENU > CLICK, CLICK LEVEL, COUNT-IN: click.c, seq.c) */
    {   /* (0.6.55) TRACKS 6 -> 4 (MENU or GLO > SYSTEM) on T5 / T6: T4 selected (ALGORITHM stops there) */
        static uint8_t live0 = NTRK;
        if (trk_live < live0 && song.sel >= trk_live)
            track_select(trk_live - 1u);
        live0 = trk_live;
    }
    {   /* (0.6.18) TRACKS 4 LOW: T5 / T6 picked, the screen says how to wake them (once a pick). (0.6.28) Also when a
         * note is sent to them (a key, MIDI, their steps) and when PLAY starts with steps on them: they were silent and
         * people did not know why (at most every 4 s) */
        static uint8_t sel0 = 0xFFu, play0;
        static uint32_t said_ms;
        uint32_t k, said = song.sel != sel0 && song.sel >= trk_live;
        int quiet = !said_ms || fm1_ms - said_ms > 4000u;
        if (song.playing && !play0 && song.g[G_STRN]) {   /* (0.6.45) PLAY with every sequence moved: say so (it
                                                         * looked like a bug: notes lower than the roll shows) */
            char b[24];
            str_cpy(b, "ALL TR ", sizeof b);
            if (song.g[G_STRN] < 0) str_cpy(b + 7, "-", 2);
            fmt_int(b + str_len(b), song.g[G_STRN] < 0 ? -song.g[G_STRN] : song.g[G_STRN]);
            ui_say(b, " DEG");
            str_cpy(ui.msg2, "SCL > ALL TR: 0 = OFF", sizeof ui.msg2);   /* (0.7.6: SCL KNOB 4, was SCL 2) */
        }
        if (song.playing && !play0 && quiet)
            for (k = trk_live; k < NTRK; k++)
                if (!seq_is_empty(&trk[k]) && !trk[k].p[P_MUTE]) said = 1;
        if (asleep_hit) { asleep_hit = 0; said |= quiet; }
        if (said) {
            char m[20] = {'T', (char)('1' + trk_live), 0};   /* (chord-harp fork: TRACKS 1..6: "T3-T6 ASLEEP", "T6 ASLEEP") */
            if (trk_live + 1u < NTRK)
                str_cpy(m + 2, "-T6", 4);
            str_cpy(m + str_len(m), " ASLEEP: ", 10);
            ui_say(m, "");
            str_cpy(ui.msg2, "GLO>SYSTEM>TRACKS", sizeof ui.msg2);
            said_ms = fm1_ms | 1u;
        }
        sel0 = song.sel;
        play0 = song.playing;
    }
    if (!ui.menu)
        usb_serial_apply();                             /* (MENU > USB SERIAL: when the menu has closed) */
    if (scr_input(pressed, notes))                      /* MENU > SCREEN OFF: dark, or the wake gesture: swallowed */
        return;                                         /* (before the buttons' timers: no tap or hold after it) */
    home = btn_hold(&ui.home_t0, B_HOME, now, 1);       /* (2026-11-03: held, nothing; the MENU is GLO's pages) */
    rec = btn_hold(&ui.rec_t0, B_REC, now, 0);          /* (a tap; held, the REC layer: ui_layer.c) */
    seq = btn_hold(&ui.seq_t0, B_SEQ, now, 1);          /* (2026-11-03: its TOOLS a page; held, nothing) */
    layer_arm(pressed, now);
    if (((pressed >> panel.btn[B_REC]) & 1u) && ui.ly == LAYER_REC)
        ui.rec_t0 |= 2u;                                /* (REC is its layer's button now: its tap is layer_tap's) */
    layer_oct_open(pressed);                            /* (OCT± with a SET layer's button down: it opens now) */
    hm_tick();
    oct = oct_taps(pressed, ui.menu || ui.confirm || act_oct() || name_on() || layer_set_open() || ui.hm);
    if (ui.hm && !dlg_on() && !name_on()) {              /* a held menu: OCT- closes it, OCT+ nothing */
        if (oct & 1u)
            hm_close();
        oct = 0;
    }
    oct = layer_oct(pressed, oct);                      /* (a SET layer's OCT-: put back) */
    layer_masks();                                      /* seq.c: keys pressed with a layer's button are its own */
    {   /* (0.6.67) a button held + SELECT: (2026-11-02, the redesign) nothing: its chain (nav_jump) and HOME + SELECT's
         * IDEAS went with the tabs (NAV-LAYOUT.md), GLO + SELECT's tempo with GLO's layer. The turn, the button's tap and layer
         * swallowed, and the turn's last detents for a moment after it is let go (no page moved by them). Not in the
         * menu, a dialog, NAME. (0.9.2) A button held + PRESETS: nothing too (rule 3; until 0.9.1 its pages) */
        static uint32_t pj_sel_ms;
        uint32_t f = held_family(), hb = (fm1_in.buttons >> panel.btn[B_HOME]) & 1u;
        if (!ui.menu && !dlg_on() && !name_on() && (f != FAM_COUNT || hb) && panel_enc(EN_SELECT) != 0) {
            if (f == FAM_COUNT)
                ui.home_t0 |= 2u;
            else
                btn_used(FAM_BTN[f]);
            pj_sel_ms = fm1_ms | 1u;
        } else if (f != FAM_COUNT && !ui.menu && !dlg_on() && !name_on() && !ly_used && panel_enc(EN_PRESET) != 0) {
            btn_used(FAM_BTN[f]);                       /* (no tap after, no map while held. A layer already used:
                                                         * PRESETS stays its, dropped) */
        } else if (pj_sel_ms && fm1_ms - pj_sel_ms < 400u) {
            if (panel_enc(EN_SELECT))
                pj_sel_ms = fm1_ms | 1u;                /* (dropped: still the turn made with the button) */
        } else {
            pj_sel_ms = 0;
        }
        pj_hold();
    }
    lay = layer_held();
    if (!layer_allowed()) {
        perf_kill = 1;                                  /* (effects off until their keys are let go) */
        perf_latched = 0;                               /* (FX LATCH: the latched ones and the macros off) */
        perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
    }
    else if (!kb_layer)
        perf_kill = 0;
    {   /* a key pressed with the button: a combo (its edge, or the ISR already took it); then not the grid's or a step's */
        static uint32_t kb_seen;
        lkeys = kb_layer & ~kb_seen;
        combo = lay && (notes || lkeys);
        kb_seen = kb_layer;
    }
    if (lay)
        lkeys |= notes & ~fm1_in.notes;                 /* (tapped and let go already) */
    notes &= ~kb_layer;
    if (lay) {                                          /* a layer's button held: keys, knobs and buttons are combos */
        combo |= (pressed & ~ly_bit(ui.ly) &            /* (REC + PLAY: PLAY plays and REC let go soon is still */
                  ~(ui.ly == LAYER_REC ? 1u << panel.btn[B_PLAY] : 0u)) != 0u;   /* a tap, it arms: as before) */
        notes = 0;                                      /* (the keys are the layer's, not the grid's or a step's) */
        if (pressed & (1u << panel.btn[B_SAVE]))       /* no UNDO, no page */
            ui.save_t0 |= 2u;
        if (pressed & (1u << panel.btn[B_HOME]))       /* no menu, no HOME */
            ui.home_t0 |= 2u;
        if (pressed & (1u << panel.btn[B_SEQ]))
            ui.seq_t0 |= 2u;
        for (k = 0; k < 4u; k++)                        /* KNOB 1..4: the layer's (ui_layer.c layer_knob) */
            if ((ks[k] = panel_enc(EN_K1 + k)) != 0)
                combo = 1;
        panel_enc(EN_PRESET);                           /* (a stray turn would load another sound) */
        {   /* (0.8.5) ALGORITHM: the layer's lens, MAP <-> STATE (2026-11-02; no wrap; was dropped: another track there,
             * OCT- put back the layer's track only) */
            int32_t a = panel_enc(EN_ALGO);
            if (a && ui.layer) {
                ui.lylv = (uint8_t)clamp((int32_t)ui.lylv + a, 0, 1);
                ui.force = 1;
                combo = 1;
            }
        }
    } else if (fx_lock_on) {                            /* (0.6.44) the FX layer locked open: KNOB 1..4 its macros */
        for (k = 0; k < 4u; k++)
            ks[k] = panel_enc(EN_K1 + k);
        panel_enc(EN_PRESET);
        kq = 1;
    } else if ((kq = (uint32_t)layer_knobs_quiet()) != 0) {   /* a layer letting go: KNOB 1..4 are nobody's (#39) */
        for (k = 0; k < 4u; k++)
            if (panel_enc(EN_K1 + k) != 0)
                combo = 1;                              /* (with the button let go this frame: no tap) */
    }
    if (!lay)
        ly_used = 0;
    else if (combo)
        ly_used = 1;
    lytap = layer_gesture(now, combo);
    layer_show();
    layer_keys(lkeys);
    for (k = 0; k < 4u; k++)
        if (ks[k]) {
            layer_knob(k, ks[k]);
            ui.hot_col = (uint8_t)k;
            ui.hot_t = 40;
        }
    song.grid = (uint8_t)keys_mode();                 /* (the menu, a dialog: the keys play again; NAME: silent) */
    if (home == BT_TAP && ui.menu) {                    /* HOME closes ABOUT */
        menu_close();
        home = BT_NONE;                                 /* (menu_close went HOME already) */
    }
    if (rec == BT_TAP && !ui.menu && !dlg_on() && !name_on())   /* REC pressed in another layer (REC alone: */
        rec_tap();                                      /* its layer's tap); nothing in the menu, a dialog or NAME */
    if (ui.menu) {                                      /* SAVE / REC taps do nothing here (HOME: above) */
        if (ui.save_t0)
            ui.save_t0 |= 2u;
        ui.pg_down = 0;
        if (!ui.home_t0)
            menu_input(oct);
        return;
    }
    if (name_on() && !ui.confirm) {                     /* NAME: the keys type, KNOB 1 / 2, OCT+ / OCT- (ui_name.c); */
        if (ui.save_t0)                                 /* SAVE does nothing */
            ui.save_t0 |= 2u;
        if (((pressed >> panel.btn[B_PLAY]) & 1u) && (song.playing || chain_busy() || seq_counting()))
            transport_req = 2;                          /* PLAY stops a transport started meanwhile (MIDI Start, the
                                                         * editor) so the name can be saved; it never starts one */
        ui.pg_down = 0;
        name_input(notes, oct);
        return;
    }
    if (dlg_on()) {                                     /* OCT- cancels, OCT+ does it; nothing else reacts (GEN: a page,
                                                         * below) */
        if (oct & 2u) {
            uint32_t kind = ui.confirm;
            ui.confirm = 0;
            ui.force = 1;
            if (kind != CF_OVR_PROJ && kind != CF_OVR_USER && kind != CF_ERASE_USER && kind != CF_DICE_ALL &&
                kind != CF_ERASE_SEC && kind != CF_ERASE_PRJ &&
                kind != CF_DEL_ROW && kind != CF_CLEAR_SONG && !chain_busy())
                undo_mark();                            /* (0.6.21) the music before a clear, INIT, RECALL, pattern */
            if (kind == CF_OVR_PROJ) {                  /* overwrite: the NAME screen writes it */
                name_open(NK_PROJ_SAVE, ui.confirm_trk & 3u);
            } else if (kind == CF_OVR_USER) {
                name_open(NK_USER_SAVE, ui.confirm_trk);
            } else if (kind == CF_ERASE_USER) {
                up_ui(1u, ui.confirm_trk);
            } else if (kind == CF_ERASE_SEC) {          /* (2026-11-05) SAVE > ERASE: stored music, no UNDO */
                project_erase_section(ui.confirm_trk);
            } else if (kind == CF_ERASE_PRJ) {
                project_erase_all();
            } else if (kind == CF_CLEAR_PFX) {         /* (0.6.99) FX > PUNCH > CLEAR */
                if (!chain_busy()) { memset(pfx_lane, 0, sizeof pfx_lane); pfx_ltgt = 0; ui_message("PUNCH LANE CLEARED"); }
            } else if (kind == CF_CLEAR_MOTION) {
                track_t *t = &trk[ui.confirm_trk % NTRK];
                if (!chain_busy()) { load_begin(t, LOAD_PAT); motion_clear(t); load_end(t); ui_message("AUTOMATION CLEARED"); }
            } else if (kind == CF_DEL_ROW) {
                uint32_t r = ui.confirm_trk;
                if (!chain_busy() && r < chain_config.count) {
                    for (; r + 1u < chain_config.count; r++) {
                        chain_config.row[r] = chain_config.row[r + 1u];
                        song_idx.scene[song_cur][r] = song_idx.scene[song_cur][r + 1u];   /* (its scene with it) */
                    }
                    memset(&song_idx.scene[song_cur][r], 0, sizeof(scene_t));
                    song_idx_touch();
                    chain_config.count--;
                    if (ui.song_row >= chain_config.count) ui.song_row = (uint8_t)(chain_config.count ? chain_config.count - 1u : 0u);
                    ui_message("ROW DELETED");
                }
            } else if (kind == CF_CLEAR_SONG) {
                if (!chain_busy()) {
                    chain_defaults(&chain_config);
                    memset(song_idx.scene[song_cur], 0, sizeof song_idx.scene[song_cur]);
                    song_idx_touch();
                    ui.song_row = 0;
                    ui_message("SONG CLEARED");
                }
            } else if (kind == CF_CALIB) {              /* (2026-11-02) GLO > TOOLS: as the MENU's row (OCT+) */
                panel_setup();
                scrn.idle = fm1_ms;
            } else if (kind == CF_ABOUT) {              /* .. the MENU's document; OCT- back to the page */
                ui.menu = 2, ui.menu_scroll = 0, about_glo = 1;
            } else if (kind == CF_INIT_SYS) {           /* (0.6.40) SAVE > TOOLS 5: everything as at power-on */
                if (!chain_busy()) {
                    undo_mark();
                    if (song.playing) transport_req = 2;
                    system_init();
                    hints_reset();                      /* (0.9.5) the teaching hints start over */
#if YEHNAH_GEN
                    gp.genre = GEN_EXPLORE;             /* (0.8.6) GEN's GENRE as at power-on: EXPLORE */
#endif
                    ui_message("NEW PROJECT");
                }
            } else if (kind == CF_CLEAR_ALL) {          /* SEQ > TOOLS CLEAR ALL: every track's steps and automation, the lane */
                if (!chain_busy()) {
                    uint32_t k;
                    for (k = 0; k < NTRK; k++) {
                        load_begin(&trk[k], LOAD_PAT);
                        track_defaults_steps(&trk[k]);
                        motion_clear(&trk[k]);
                        load_end(&trk[k]);
                        latch_drop_req |= (uint8_t)(1u << k);   /* (the held / latched notes: events_block) */
                    }
                    memset(pfx_lane, 0, sizeof pfx_lane);
                    ui_message("ALL CLEARED");
                }
            } else {
                track_t *t = &trk[ui.confirm_trk % NTRK];
                load_begin(t, LOAD_PAT);
                track_defaults_steps(t);
                load_end(t);
                latch_drop_req |= (uint8_t)(1u << (t - trk));   /* and the latched chord (events_block) */
                if (kind == CF_CLEAR_TRK) {
                    char b[12] = "1 CLEARED";
                    b[0] = (char)('1' + ui.confirm_trk);
                    ui_say("TRACK ", b);
                } else {
                    {   /* (0.6.28) which track: the others play on (SEQ > TOOLS CLEAR ALL clears them all) */
                        char cb[16] = "T1 CLEARED";
                        cb[1] = (char)('1' + ui.confirm_trk % NTRK);
                        ui_say(cb, "");
                        str_cpy(ui.msg2, "SEQ>TOOLS: CLEAR ALL", sizeof ui.msg2);
                    }
                }
            }
        } else if (oct & 1u) {
            ui.confirm = 0;
            ui.force = 1;
        }
        ui.pg_down = 0;
        enc_drop();
        return;
    }
    if (ui.hm && (home == BT_TAP || seq == BT_TAP || lytap))   /* (a held menu: any of these closes it) */
        hm_close();
    if (lytap)                                          /* a layer's button acts on release (held: the layer) */
        layer_tap(lytap);
    if (seq == BT_TAP)
        open_family(FAM_SEQ);
    if (home == BT_TAP)                                 /* HOME acts on release (or held): rule 2, HOME's */
        nav_button(SEC_HOME);                           /* its first page, TRACKS */
    cursor_fix();                                       /* LEN may have changed (knob, editor, load) */
    for (id = 0; id < 14u; id++) {
        if (!((pressed >> id) & 1u))
            continue;
        b = panel_btn_of(id);
        switch (b) {
        case B_PLAY:
            if (song.playing || chain_busy() || seq_counting())   /* (a count-in: PLAY stops it) */
                transport_req = 2;
            else if (!ui.home && cur_page()->graph == GR_SLOTS && save_on())   /* SAVE > PROJECT: preview the section */
                section_preview_ui();
            else if ((!ui.home && cur_page()->graph == GR_SONG) || live.mode)   /* (the song layer's SONG mode) */
                chain_play_ui();
            else
                transport_req = 1;
            break;
        case B_REC:                                     /* (2026-11-03: SEQ + REC's CLEAR ALL is SEQ > TOOLS') */
        case B_SEQ:                                     /* tap: above (held: nothing) */
        case B_SAVE:
        case B_HOME:
            break;
        case B_FX:                                      /* the layers' buttons: ui_layer.c (SCL: a page button, its hold a menu) */
            break;
        case B_OCTDN:
        case B_OCTUP: {
            uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
            if (act_oct() || layer_set_open() || gen_on())   /* action pages: enter / back (below); SET layers: OCT-; GEN */
                break;
            if (!ui.home && !ui.menu && !ui.confirm && cur_page()->scope == SC_TRK && b == B_OCTUP) {   /* (2026-11-02)
                                                         * HOME > MIXER: every mute and solo off */
                uint32_t t;
                for (t = 0; t < NTRK; t++)
                    trk[t].p[P_MUTE] = 0;
                ui.msolo = 0;
                dx_mute_set(0);
                ui_message("ALL UNMUTED");
                break;
            }
            if ((fm1_in.buttons & both) == both)
                song.octave = 0;
            else
                song.octave += b == B_OCTDN ? (song.octave > -3 ? -1 : 0) : (song.octave < 3 ? 1 : 0);
            if (drum_track(TSEL) && !song.grid)         /* (0.8.5) the kit's octaves: what this one does */
                ui_message(drum_oct_name(song.octave));
            break;
        }
        default:                                        /* page buttons (GLO ENV LFO EDIT ARP): when let go */
            if (b == B_ARP && ui.entry_open && !ui.home && cur_page()->graph == GR_ROLL) {
                ui.ent_arp ^= 1u;                       /* (0.6.37) a chord held in step recording: arpeggiate it */
                ent_message();
                break;
            }
            if (!lay && pj_btn != b) {                  /* (with FX held: swallowed; a SELECT combo in this */
                ui.pg_down |= (uint16_t)(1u << id);     /* same pass, btn_used: no tap either) */
                pg_ms[id] = (uint16_t)fm1_ms;
            }
            break;
        }
    }
    for (id = 0; id < 14u; id++)                        /* (2026-11-03) held past HOLD: let go, nothing */
        if (((ui.pg_down & fm1_in.buttons) >> id) & 1u && (uint16_t)(fm1_ms - pg_ms[id]) >= HOLD_MS[settings_hold % 4u]) {
            ui.pg_down &= (uint16_t)~(1u << id);
            hm_hold(panel_btn_of(id));                  /* SCL / EDIT / GLO: the held menu opens */
        }
    b = ui.pg_down & ~fm1_in.buttons;
    ui.pg_down &= (uint16_t)~b;
    for (id = 0; b; id++, b >>= 1)
        if (b & 1u) {
            uint32_t pb = panel_btn_of(id);
            if (ui.hm) {                                /* a held menu open: the tap closes it (its own button: only that) */
                uint32_t same = hm_btn_is(pb);
                hm_close();
                if (same)
                    continue;
            }
            page_tap(pb);
        }
    if (gen_on() && (oct & 2u)) {                       /* (chord-harp fork) IDEAS, HOME's pages: OCT+ KEEP, OCT- BACK */
        gen_press();                                    /* (nothing thrown: back to TRACKS) */
    } else if (gen_on() && (oct & 1u)) {
        if (!gen_back())
            nav_go(SEC_HOME, 0, 0);
    } else if (act_oct() && (oct & 2u)) {               /* action pages: OCT+ does the picked action, */
        act_do();
    } else if (act_oct() && (oct & 1u)) {               /* OCT- drops it, or (none picked) goes HOME */
        if (ui.act)
            ui.act = 0;
        else
            go_home();
    }
    song.grid = (uint8_t)keys_mode();                 /* (seq.c: the keys are the grid's) */
    seq_fill = (uint8_t)((fm1_in.buttons >> panel.btn[B_FX]) & 1u);   /* (0.6.42) FILL steps: FX held */
    lock_keys();                                        /* (a grid key on a hit let go: the hit goes) */
    if (!ui.home && cur_page()->graph == GR_PROG && harp_prog(TSEL) && (notes >> HARP_KWRITE) & 1u)
        ui.act = ui.act ? 0u : 4u;                      /* (chord-harp fork) SCL > PROG: F#4 picks WRITE (OCT+ does it) */
    if (song.grid) {
        grid_keys(notes);
    } else if (!ui.hm && song.seq_mode && cur_page()->graph == GR_ROLL) {   /* STEP (not CHANCE: its knobs only) */
        if (live_rec_sel() || !((song.rec >> song.sel) & 1u))   /* armed and playing: the keys record live, not
                                                         * into the cursor step too; not armed (1.2, #133): they
                                                         * only play, being on STEP writes nothing */
            ui.entry_open = 0;
        else if (!chain_busy())
            seq_entry(notes);                           /* armed and stopped: step recording at the cursor */
        else if (notes)
            ui_message("STOP TO EDIT");
    }

    /* #102: the knobs a layer took above are not read again here. The TIMER5 scan (10 kHz) counts detents while this
     * pass runs, so one landing after the layer's read went to the page as well: KNOB 1..4 edited the layer's control
     * and the page under it (HOME's sound, motion recorded), PRESETS could load a sound. They stay for the next pass */
    kq |= lay;
    if (!lay && (s = panel_enc(EN_PRESET)) != 0) {
        if (gen_on())
            gen_knob(0, s);                             /* (GEN: its GENRE, the generator's preset) */
        else
            presets_turn(s);
    }
    if (!lay && (s = panel_enc(EN_ALGO)) != 0)     /* ALGORITHM: the selected track, on every page. (0.6.55) Only
                                                    * the tracks that play: TRACKS 4 stops at T4 (T5 / T6 asleep) */
        track_select((uint32_t)clamp((int32_t)song.sel + (s > 0 ? 1 : -1), 0, (int32_t)trk_live - 1));
    if ((s = panel_enc(EN_SELECT)) != 0 && !lay && !ui.menu) {   /* (0.9.2, rule 1) SELECT alone: the section's
                                                        * chain, a page a detent (HOME's: ECO MACROS BEING ROUTES GEN 1..3) */
        nav_select(s);
    }
    for (k = 0; k < 4u && !kq; k++) {
        const page_t *pg = cur_page();
        int16_t *hv;
        uint64_t held;
        if ((s = panel_enc(EN_K1 + k)) == 0)
            continue;
        if (ui.hm) {                                    /* a held menu: its rows and values */
            hm_knob(k, s);
            continue;
        }
        if (gen_on()) {                                 /* GEN: its settings */
            gen_knob(k, s);
            continue;
        }
        if (ui.entry_open && !ui.home && cur_page()->graph == GR_ROLL && !grid_on() && lock_held() &&
            (k == 0u || (ui.ent_arp && k < 3u))) {      /* (0.6.37) a chord being entered: KNOB 1 its LEN (ARP: RES), */
            ent_knob(k, s);                             /* 2 ORDER, 3 OCT; the others still lock */
            continue;
        }
        if ((held = lock_held()) != 0u) {               /* a step held on STEP: KNOB k locks (parameter locks) */
            lock_turn(k, s, held);
            ui.hot_col = (uint8_t)k;
            ui.hot_t = 40;
            continue;
        }
        if (ui.home || pg->scope == SC_STEP || pg->scope == SC_TRK || page_desc(pg, k, &hv) ||
            ((pg->graph == GR_USER || pg->graph == GR_MOD || pg->graph == GR_MSEQ || BAR_PAGE(pg)) &&
             k == 0u)
            || pg->graph == GR_SONG || pg->graph == GR_SEDIT || pg->graph == GR_DXSND || pg->graph == GR_FOCUS || pg->graph == GR_STAGE) {   /* (not an empty column) */
            ui.hot_col = (uint8_t)k;
            ui.hot_t = 40;
        }
        if (home_macros()) {                            /* (0.8.5) HOME's MACROS / ROUTES: M1..M4 */
            macro_v[k] = (uint8_t)clamp(macro_v[k] + accel(EN_K1 + k, s, 127), 0, 127);
        } else if (ui.home) {
            int16_t *vp;
            const param_desc_t *d = home_param(k, &vp);
            *vp = (int16_t)param_turn(d, *vp, accel(EN_K1 + k, s, d->fmt == F_ENUM ? 0 : d->max - d->min));
            motion_capture(TSEL, (uint32_t)(vp - TSEL->p), *vp);
        } else {
            edit_param(k, s);
        }
    }
    ui_notices();
}

/* ---------------------------------------------------- panel setup --- */
/* 30 s without input: give up and keep the old table (a stuck key cannot hang the boot). A piano key cancels at
 * any time: the keys are not part of what is taught, so one works whatever the buttons' table says */
#define SETUP_IDLE_MS 30000u
#define SETUP_GIVE_UP(t0) (fm1_ms - (t0) > SETUP_IDLE_MS || fm1_in.notes != 0u)
static void setup_title(void)
{
    lcd_fill(0, 0, 240, 240, T_BG);
    {   /* the title with its icon (the menu row's), centred together; M from y 8 as before */
        const char *t = "HARDWARE CALIBRATION";
        int32_t x = (240 - text_w(&AF_M, t)) / 2;       /* (0.8.5: no icons) */
        cv_begin(240, 24, T_BG);
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "calibration title centred");
        cv_text(x, HEAD_MY, &AF_M, t, T_TEXT);
        cv_blit(0, 5);
    }
    draw_text_box(0, 32, 240, &AF_S, "TEACH EACH BUTTON AND KNOB", T_MID, 1);
    lcd_fill(16, 56, 208, 1, T_LINE);
    draw_text_box(0, 206, 240, &AF_S, "ANY KEY: CANCEL", T_DIM, 1);
}
static void setup_show(const char *what, const char *name)     /* "PRESS" / "TURN RIGHT", the control */
{
    draw_text_box(0, 80, 240, &AF_S, what, T_MID, 1);
    draw_text_box(0, 100, 240, &AF_M, name, T_THEME, 1);
}
static void panel_setup(void)
{
    uint32_t i, used = 0, t0 = fm1_ms;
    const panel_t old = panel;
    setup_title();
    while (fm1_in.buttons) {                             /* wait for OCT-/OCT+ release */
        fm1_wdt_feed();
        if (SETUP_GIVE_UP(t0))
            goto timeout;
    }
    fm1_input_edges(0);
    for (i = 0; i < NB; i++) {
        uint32_t p = 0, id;
        setup_show("PRESS", B_NAME[i]);
        t0 = fm1_ms;
        while (!(p & ~used)) {
            fm1_wdt_feed();
            p |= fm1_input_edges(0);
            if (SETUP_GIVE_UP(t0))
                goto timeout;
        }
        for (id = 0; id < 14u; id++)
            if (((p & ~used) >> id) & 1u)
                break;
        panel.btn[i] = (uint8_t)id;
        used |= 1u << id;
    }
    used = 0;
    for (i = 0; i < NE; i++) {
        uint32_t e;
        int32_t st = 0;
        setup_show("TURN RIGHT", E_NAME[i]);
        for (e = 0; e < 7u; e++)
            fm1_enc_take(e);
        t0 = fm1_ms;
        for (;;) {
            fm1_wdt_feed();
            if (SETUP_GIVE_UP(t0))
                goto timeout;
            for (e = 0; e < 7u; e++)
                if (!((used >> e) & 1u) && (st = fm1_enc_take(e)) != 0)
                    break;
            if (e < 7u)
                break;
        }
        panel.enc[i] = (uint8_t)e;
        panel.dir[i] = (int8_t)(st > 0 ? 1 : -1);
        used |= 1u << e;
        fm1_delay_ms(300);
        fm1_enc_take(e);
    }
    panel.magic = PANEL_MAGIC;
    lcd_fill(0, 0, 240, 240, T_BG);
    ui.force = 1;
    return;
timeout:
    panel = old;
    lcd_fill(0, 0, 240, 240, T_BG);
    ui.force = 1;
    ui_message("SETUP CANCELLED");
}

#include "ui_dx.c"                                  /* EDIT > SOUND / SOUND 2: the DRUM-X kit */
