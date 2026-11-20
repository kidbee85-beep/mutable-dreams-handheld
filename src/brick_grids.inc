/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: GRIDS, Mutable Instruments' topographic drum sequencer.
 *
 * Grids' map (Emilie Gillet, GPL-3.0-or-later; build/gen/grids_map.h from third_party/mi/grids/resources.cc by
 * tools/gen_grids.py): 25 drum patterns laid out on a 5 x 5 map. MAP X and MAP Y choose a place on it; the four
 * patterns around it blend (as grids/pattern_generator.cc's ReadDrumMap does), each step of KICK, SNARE and HAT has a
 * level, and it plays when its level is above the instrument's threshold (DENSITY), with an accent above 192.
 * CHAOS lifts each part's levels at random at the start of every pattern (EvaluateDrums). 32 steps, at RATE, while
 * the transport runs. Each part goes to a track, the kit's pads, POLY or SYNTH. Included by brick_lab.inc. */
#include "grids_map.h"

static const char *const GR_TARGETS[10] = {"OFF", "TRACK 1", "TRACK 2", "TRACK 3", "TRACK 4", "TRACK 5", "TRACK 6", "KIT", "POLY", "SYNTH"};
static const char *const GR_RATES[3] = {"1/8", "1/16", "1/16 T"};
static const float GR_RATE_BEATS[3] = {0.5f, 0.25f, 1.0f / 6.0f};
static double gr_clock;
static uint32_t gr_step, gr_rng = 0x1234567u;
static int gr_was_playing;
static uint8_t gr_perturb[3];
static uint32_t gr_hits[3], gr_flash_at[3];      /* (the picture) */
static struct { int note, target; uint32_t off; int on; } gr_on[6];
static uint32_t gr_frames;

static uint8_t gr_mix(uint8_t a, uint8_t b, uint8_t balance) { return (uint8_t)((a * (255 - balance) + b * balance) >> 8); }

/* the level of instrument's step at (x, y) on the map: the four nodes around it, blended */
static uint8_t gr_level(int step, int instrument, uint8_t x, uint8_t y)
{
    uint8_t i = x >> 6, j = y >> 6;
    const uint8_t *a = GRIDS_NODES[GRIDS_MAP[i][j]], *b = GRIDS_NODES[GRIDS_MAP[i + 1][j]];
    const uint8_t *c = GRIDS_NODES[GRIDS_MAP[i][j + 1]], *d = GRIDS_NODES[GRIDS_MAP[i + 1][j + 1]];
    int o = instrument * 32 + step;
    return gr_mix(gr_mix(a[o], b[o], (uint8_t)(x << 2)), gr_mix(c[o], d[o], (uint8_t)(x << 2)), (uint8_t)(y << 2));
}
static uint8_t gr_xy(float pct) { float v = pct / 100.0f * 255.0f; return (uint8_t)(v > 255 ? 255 : v < 0 ? 0 : v); }

static void gr_play(int part, int accent)
{
    int target = (int)lab_v[P_GR_KICK_TO + part], root = (int)(md_state()->root % 12), vel = accent ? 120 : 80;
    static const int pitch[3] = {36, 50, 66};    /* kick low, snare in the middle, hat high (root added) */
    int note = pitch[part] + root;
    uint32_t gate = 2200;
    gr_hits[part]++;
    gr_flash_at[part] = gr_frames;
    if (target <= 0)
        return;
    if (target == 7) {
        if (md_state()->kit.enabled)
            dream_cmd(MD_KIT_TRIGGER, 3, part);
        return;
    }
    if (target == 9) {
        if (lab_v[P_SY_LEVEL] > 0)
            sy_note_on(note + 12, vel);
        return;
    }
    for (int i = 0; i < 6; i++)
        if (!gr_on[i].on) {
            if (target == 8) {
                if (lab_v[P_PO_LEVEL] <= 0)
                    return;
                po_note_on_vel(note, 0, vel);
            } else
                dream_cmd(MD_TRIGGER, target - 1, note);
            gr_on[i].note = note, gr_on[i].target = target, gr_on[i].off = gr_frames + gate, gr_on[i].on = 1;
            return;
        }
}

static void gr_release(int all)
{
    for (int i = 0; i < 6; i++)
        if (gr_on[i].on && (all || (int32_t)(gr_frames - gr_on[i].off) >= 0)) {
            if (gr_on[i].target == 8)
                po_note_off(gr_on[i].note);
            else
                dream_cmd(MD_NOTE_OFF, gr_on[i].target - 1, gr_on[i].note);
            gr_on[i].on = 0;
        }
}

static void gr_run(uint32_t n)
{
    int playing = md_playing() && lab_v[P_GR_ON] > 0.5f;
    double step = 44100.0 * 60.0 / (double)(md_tempo_now() > 20 ? md_tempo_now() : 20) * GR_RATE_BEATS[(int)lab_v[P_GR_RATE] % 3];
    gr_frames += n;
    gr_release(0);
    if (!playing) {
        if (gr_was_playing)
            gr_release(1);
        gr_was_playing = 0;
        gr_clock = 0;
        gr_step = 0;
        return;
    }
    gr_was_playing = 1;
    gr_clock -= (double)n;
    while (gr_clock <= 0) {
        uint8_t x = gr_xy(lab_v[P_GR_X]), y = gr_xy(lab_v[P_GR_Y]);
        int s = (int)(gr_step % 32u);
        gr_clock += step;
        if (s == 0)                              /* a new pattern: how far each part strays (CHAOS) */
            for (int i = 0; i < 3; i++) {
                gr_rng = gr_rng * 1664525u + 1013904223u;
                gr_perturb[i] = (uint8_t)(((gr_rng >> 24) * (uint32_t)(lab_v[P_GR_CHAOS] / 100.0f * 63.0f)) >> 8);
            }
        for (int i = 0; i < 3; i++) {
            int level = gr_level(s, i, x, y);
            uint8_t threshold = (uint8_t)~(uint8_t)(lab_v[P_GR_KICK_DENSITY + i] / 100.0f * 255.0f);
            level = level < 255 - gr_perturb[i] ? level + gr_perturb[i] : 255;
            if (level > threshold)
                gr_play(i, level > 192 + (int)((1.0f - lab_v[P_GR_ACCENT] / 100.0f) * 50.0f));
        }
        gr_step++;
    }
}
