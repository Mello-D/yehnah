/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (chord-harp fork, 2026-10-25) Pixel art on an 80 x 46 grid of art pixels, scaled to the rectangle it is given in whole
 * pixels, centred and clipped to it; filled shapes in the palette's colours, moving
 * with the sound (el_in: its level, its newest note, its time). (2026-10-28) The engines' pictures are robots now
 * (ui_robot.c); kept here: FM6's OPERATOR and OP ENV pages. (2026-10-29) DRUM-X's
 * kit gone: its EDIT pages show its robot.
 * Included by ui_engline.c (its el_in, el_hash, vxs). */
#define PX_W 80
#define PX_H 46
#define PX_NONE (-1)
static struct { int32_t x0, y0, ps, dv, cx0, cy0, cx1, cy1, lx, live; } PX;   /* (dv > 1: smaller than the grid, an art
/* the colours: the palette's */
#define C_TX ((int32_t)T_TEXT)
#define C_DM ((int32_t)ux_mix(T_SURF, T_TEXT, 28))
#define C_TH ((int32_t)T_THEME)
#define C_THD ((int32_t)ux_mix(T_SURF, T_THEME, 50))
#define C_AC ((int32_t)T_ACCENT)
#define C_ACD ((int32_t)ux_mix(T_SURF, T_ACCENT, 55))
#define C_K(i) ((int32_t)ux.k[i])
typedef struct { el_in_t in; uint32_t fr; int on; float e; } px_in_t;
