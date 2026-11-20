/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the Sound Lab.
 *
 * Sound design the FM-1 has no room for, after the unchanged 2.1 engine:
 *
 *   TEXTURES  forest, air, water and stardust beds (from the VIBE FM Performance Garden), a tempo pulse
 *   CLOUD     a granular cloud over a 4 s memory: grain size, density, pitch, scale-quantised pitch scatter,
 *             spread, reverse, feedback, freeze
 *   SHIMMER   an eight-line feedback delay network reverb with an octave-up shifter in its loop
 *   SENDS     each of the six tracks (the engine's own stems) into CLOUD and SHIMMER, and the whole mix
 *   COLOUR    a three-voice ensemble, tape drive, wow and flutter, age
 *   MASTER    low cut, tilt, width, gain, a peak limiter
 *
 * With every module at its neutral setting (the default) nothing is computed and the output is the 2.1 engine's,
 * sample for sample. Settings are global and kept in SAVE_DIR/lab.bin. */
#include <math.h>
#include "brick_mi.h"

enum { M_TEX, M_SAMPLES, M_VOICES, M_POLY, M_SYNTH, M_RANDOM, M_LOGIC, M_GRIDS,   /* the sources (the Lab's first row) */
       M_RACK_A, M_RACK_B, M_CLOUD, M_PARTICLES, M_SEEDS, M_SHIMMER, M_CROSSMOD, M_SENDS, M_COLOUR, M_MASTER, M_COUNT };   /* effects and the mix */
#define M_SOURCES 8
static const char *const LAB_MOD_NAMES[M_COUNT] = {"TEXTURES", "SAMPLES", "VOICES", "POLY", "SYNTH", "RANDOM", "LOGIC", "GRIDS", "RACK A", "RACK B", "CLOUD", "PARTICLES", "SEEDS", "SHIMMER", "CROSSMOD", "SENDS", "COLOUR", "MASTER"};
enum { U_PCT, U_MS, U_SEMI, U_DB, U_HZ, U_ONOFF, U_BIPCT, U_PERS, U_ENUM, U_NUM };

enum {
    P_FOREST, P_AIR, P_WATER, P_STARDUST, P_PULSE,
    P_CL_MIX, P_CL_SIZE, P_CL_DENS, P_CL_PITCH, P_CL_SCATTER, P_CL_SPREAD, P_CL_REVERSE, P_CL_FB, P_CL_FREEZE,
    P_SH_MIX, P_SH_DECAY, P_SH_SHIMMER, P_SH_DAMP, P_SH_SIZE, P_SH_MOD,
    P_SC1, P_SC2, P_SC3, P_SC4, P_SC5, P_SC6, P_SS1, P_SS2, P_SS3, P_SS4, P_SS5, P_SS6, P_MIX_CL, P_MIX_SH,
    P_ENS_MIX, P_ENS_RATE, P_ENS_DEPTH, P_TAPE_DRIVE, P_TAPE_WOW, P_TAPE_AGE,
    P_LOWCUT, P_TILT, P_WIDTH, P_GAIN, P_LIMIT, P_CEIL,
    /* 0.3: original Mutable Instruments DSP (appended: lab.bin of 0.2 keeps its meaning) */
    P_PA_MIX, P_PA_MODE, P_PA_POS, P_PA_SIZE, P_PA_PITCH, P_PA_DENS, P_PA_TEXTURE, P_PA_SPREAD, P_PA_FB, P_PA_REVERB,
    P_PA_FREEZE, P_PA_LOFI,
    P_XM_MIX, P_XM_CARRIER, P_XM_MOD, P_XM_ALGO, P_XM_TIMBRE, P_XM_DRIVE1, P_XM_DRIVE2,
    P_RN_ON, P_RN_TRACK, P_RN_RATE, P_RN_PROB, P_RN_SPREAD, P_RN_BIAS, P_RN_DEJAVU, P_RN_LENGTH, P_RN_RANGE, P_RN_GATE,
    P_SP1, P_SP2, P_SP3, P_SP4, P_SP5, P_SP6, P_MIX_PA,
    /* 0.4: Beads (appended) */
    P_BE_MIX, P_BE_MEDIA, P_BE_DENS, P_BE_TIME, P_BE_SIZE, P_BE_SHAPE, P_BE_PITCH, P_BE_TIME_RND, P_BE_SIZE_RND,
    P_BE_SHAPE_RND, P_BE_PITCH_RND, P_BE_FB, P_BE_REVERB, P_BE_FREEZE, P_BE_MONO,
    P_SB1, P_SB2, P_SB3, P_SB4, P_SB5, P_SB6, P_MIX_BE,
    /* 0.5: samples (appended) */
    P_SA_LEVEL, P_SA_FILE, P_SA_MODE, P_SA_TRACK, P_SA_PITCH, P_SA_FOLLOW, P_SA_START, P_SA_LENGTH, P_SA_REVERSE,
    P_SA_DECAY, P_SA_CUTOFF, P_SA_PAN, P_SA_TO_CL, P_SA_TO_PA, P_SA_TO_BE, P_SA_TO_SH,
    /* 0.6: the polyphonic voice (appended) */
    P_PO_LEVEL, P_PO_ENGINE, P_PO_HARM, P_PO_TIMBRE, P_PO_MORPH, P_PO_ATTACK, P_PO_DECAY, P_PO_SUSTAIN, P_PO_RELEASE,
    P_PO_COLOUR, P_PO_VOICES, P_PO_SPREAD, P_PO_OCTAVE, P_PO_PLAYMODE, P_PO_FOLLOW, P_PO_GATE,
    P_PO_TO_CL, P_PO_TO_PA, P_PO_TO_BE, P_PO_TO_SH,
    /* 0.7: the Sound Lab per Biome (appended; both global, in MASTER) */
    P_LB_OWN, P_LB_FOLLOW,
    /* 0.8: the change between two Biomes' Labs (appended, global) */
    P_LB_TIME,
    /* 0.9: MIDI notes onto POLY (appended) */
    P_PO_MIDI,
    /* 0.28: VOICES, spoken and sung voices cut at the breaths (appended) */
    P_VO_LEVEL, P_VO_FILE, P_VO_MODE, P_VO_TRIGGER, P_VO_DENSITY, P_VO_PHRASE, P_VO_PITCH, P_VO_REVERSE, P_VO_RADIO,
    P_VO_SPREAD, P_VO_TO_CL, P_VO_TO_PA, P_VO_TO_BE, P_VO_TO_SH,
    /* 0.30: LOGIC, music from musical mathematics (appended) */
    P_LG_ON, P_LG_RULE, P_LG_TARGET, P_LG_TARGET2, P_LG_RATE, P_LG_DENSITY, P_LG_SHAPE, P_LG_RANGE, P_LG_SEED,
    P_LG_VARIATION, P_LG_GATE,
    /* 0.31: SYNTH, six instruments of the garden (appended) */
    P_SY_LEVEL, P_SY_ENGINE, P_SY_PLAYED_BY, P_SY_DENSITY, P_SY_OCTAVE, P_SY_DECAY, P_SY_BRIGHT, P_SY_CHARACTER,
    P_SY_MOTION, P_SY_SPREAD, P_SY_TO_CL, P_SY_TO_PA, P_SY_TO_BE, P_SY_TO_SH,
    /* 0.34: SYNTH's MODEL, the FX RACKs A and B, GRIDS (appended) */
    P_SY_MODEL,
    P_RA_IN_VOICES, P_RA_IN_SAMPLES, P_RA_IN_SYNTH, P_RA_IN_POLY, P_RA_FILTER, P_RA_CUTOFF, P_RA_RESONANCE, P_RA_FOLLOW, P_RA_SWEEP, P_RA_SWEEP_RATE, P_RA_DRIVE, P_RA_CRUSH, P_RA_RES_MIX, P_RA_RES_MODEL, P_RA_RES_NOTE, P_RA_STRUCTURE, P_RA_BRIGHTNESS, P_RA_DAMPING, P_RA_POSITION, P_RA_ECHO_MIX, P_RA_ECHO_TIME, P_RA_ECHO_FEEDBACK, P_RA_WOW, P_RA_ECHO_TONE,
    P_RB_IN_VOICES, P_RB_IN_SAMPLES, P_RB_IN_SYNTH, P_RB_IN_POLY, P_RB_FILTER, P_RB_CUTOFF, P_RB_RESONANCE, P_RB_FOLLOW, P_RB_SWEEP, P_RB_SWEEP_RATE, P_RB_DRIVE, P_RB_CRUSH, P_RB_RES_MIX, P_RB_RES_MODEL, P_RB_RES_NOTE, P_RB_STRUCTURE, P_RB_BRIGHTNESS, P_RB_DAMPING, P_RB_POSITION, P_RB_ECHO_MIX, P_RB_ECHO_TIME, P_RB_ECHO_FEEDBACK, P_RB_WOW, P_RB_ECHO_TONE,
    P_GR_ON, P_GR_X, P_GR_Y, P_GR_CHAOS, P_GR_KICK_DENSITY, P_GR_SNARE_DENSITY, P_GR_HAT_DENSITY, P_GR_KICK_TO, P_GR_SNARE_TO, P_GR_HAT_TO, P_GR_RATE, P_GR_ACCENT,
    /* 0.43: VOICES as a granular sampler (MODE GRAINS), its tidal modulator, Marbles as a trigger (appended) */
    P_VO_POSITION, P_VO_SCAN, P_VO_SIZE, P_VO_GRAINS, P_VO_SPRAY, P_VO_SCATTER,
    P_VO_TIDE_RATE, P_VO_TIDE_SLOPE, P_VO_TIDE_SHAPE, P_VO_TIDE_SMOOTH, P_VO_TIDE_DEPTH, P_VO_TIDE_TO, P_VO_DEJAVU,
    /* 0.62: the mastering chain in MASTER (appended, global): three-band EQ, saturation, glue compressor */
    P_MS_LOW, P_MS_LOW_HZ, P_MS_MID, P_MS_MID_HZ, P_MS_HIGH, P_MS_HIGH_HZ, P_MS_SAT, P_MS_SAT_KIND,
    P_MS_COMP, P_MS_COMP_ATT, P_MS_COMP_REL, P_MS_COMP_MIX, P_MS_PRESET,
    P_COUNT
};
typedef struct { uint8_t mod, unit; const char *name; float lo, hi, def, step; } lab_param;
static const lab_param LAB_P[P_COUNT] = {
    {M_TEX, U_PCT, "FOREST", 0, 100, 0, 1}, {M_TEX, U_PCT, "AIR", 0, 100, 0, 1},
    {M_TEX, U_PCT, "WATER", 0, 100, 0, 1}, {M_TEX, U_PCT, "STARDUST", 0, 100, 0, 1},
    {M_TEX, U_PCT, "PULSE", 0, 100, 0, 1},
    {M_CLOUD, U_PCT, "MIX", 0, 100, 0, 1}, {M_CLOUD, U_MS, "GRAIN SIZE", 20, 1500, 240, 10},
    {M_CLOUD, U_PERS, "DENSITY", 1, 40, 10, 1}, {M_CLOUD, U_SEMI, "PITCH", -24, 24, 0, 1},
    {M_CLOUD, U_SEMI, "SCATTER (SCALE)", 0, 24, 0, 1}, {M_CLOUD, U_PCT, "SPREAD", 0, 100, 50, 1},
    {M_CLOUD, U_PCT, "REVERSE", 0, 100, 0, 1}, {M_CLOUD, U_PCT, "FEEDBACK", 0, 90, 20, 1},
    {M_CLOUD, U_ONOFF, "FREEZE", 0, 1, 0, 1},
    {M_SHIMMER, U_PCT, "MIX", 0, 100, 0, 1}, {M_SHIMMER, U_PCT, "DECAY", 0, 100, 60, 1},
    {M_SHIMMER, U_PCT, "SHIMMER", 0, 100, 30, 1}, {M_SHIMMER, U_PCT, "DAMPING", 0, 100, 40, 1},
    {M_SHIMMER, U_PCT, "SIZE", 0, 100, 60, 1}, {M_SHIMMER, U_PCT, "MOTION", 0, 100, 30, 1},
    {M_SENDS, U_PCT, "TRACK 1 > CLOUD", 0, 100, 0, 1}, {M_SENDS, U_PCT, "TRACK 2 > CLOUD", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "TRACK 3 > CLOUD", 0, 100, 0, 1}, {M_SENDS, U_PCT, "TRACK 4 > CLOUD", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "TRACK 5 > CLOUD", 0, 100, 0, 1}, {M_SENDS, U_PCT, "TRACK 6 > CLOUD", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "TRACK 1 > SHIMMER", 0, 100, 0, 1}, {M_SENDS, U_PCT, "TRACK 2 > SHIMMER", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "TRACK 3 > SHIMMER", 0, 100, 0, 1}, {M_SENDS, U_PCT, "TRACK 4 > SHIMMER", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "TRACK 5 > SHIMMER", 0, 100, 0, 1}, {M_SENDS, U_PCT, "TRACK 6 > SHIMMER", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "WHOLE MIX > CLOUD", 0, 100, 0, 1}, {M_SENDS, U_PCT, "WHOLE MIX > SHIMMER", 0, 100, 0, 1},
    {M_COLOUR, U_PCT, "ENSEMBLE", 0, 100, 0, 1}, {M_COLOUR, U_PCT, "ENSEMBLE RATE", 0, 100, 30, 1},
    {M_COLOUR, U_PCT, "ENSEMBLE DEPTH", 0, 100, 50, 1}, {M_COLOUR, U_PCT, "TAPE DRIVE", 0, 100, 0, 1},
    {M_COLOUR, U_PCT, "WOW + FLUTTER", 0, 100, 0, 1}, {M_COLOUR, U_PCT, "AGE", 0, 100, 0, 1},
    {M_MASTER, U_HZ, "LOW CUT", 0, 200, 0, 5}, {M_MASTER, U_BIPCT, "TILT (DARK/BRIGHT)", -100, 100, 0, 5},
    {M_MASTER, U_PCT, "WIDTH", 0, 200, 100, 5}, {M_MASTER, U_DB, "GAIN", -12, 12, 0, 0.5f},
    {M_MASTER, U_ONOFF, "LIMITER", 0, 1, 0, 1}, {M_MASTER, U_DB, "CEILING", -12, 0, -1, 0.5f},
    {M_PARTICLES, U_PCT, "MIX", 0, 100, 0, 1}, {M_PARTICLES, U_ENUM, "MODE", 0, 3, 0, 1},
    {M_PARTICLES, U_PCT, "POSITION", 0, 100, 30, 1}, {M_PARTICLES, U_PCT, "SIZE", 0, 100, 50, 1},
    {M_PARTICLES, U_SEMI, "PITCH", -24, 24, 0, 1}, {M_PARTICLES, U_PCT, "DENSITY", 0, 100, 70, 1},
    {M_PARTICLES, U_PCT, "TEXTURE", 0, 100, 50, 1}, {M_PARTICLES, U_PCT, "STEREO SPREAD", 0, 100, 60, 1},
    {M_PARTICLES, U_PCT, "FEEDBACK", 0, 100, 20, 1}, {M_PARTICLES, U_PCT, "REVERB", 0, 100, 40, 1},
    {M_PARTICLES, U_ONOFF, "FREEZE", 0, 1, 0, 1}, {M_PARTICLES, U_ONOFF, "LO-FI", 0, 1, 0, 1},
    {M_CROSSMOD, U_PCT, "MIX", 0, 100, 0, 1}, {M_CROSSMOD, U_ENUM, "CARRIER", 0, 6, 0, 1},
    {M_CROSSMOD, U_ENUM, "MODULATOR", 0, 6, 1, 1}, {M_CROSSMOD, U_PCT, "ALGORITHM", 0, 100, 30, 1},
    {M_CROSSMOD, U_PCT, "TIMBRE", 0, 100, 50, 1}, {M_CROSSMOD, U_PCT, "CARRIER LEVEL", 0, 100, 60, 1},
    {M_CROSSMOD, U_PCT, "MODULATOR LEVEL", 0, 100, 60, 1},
    {M_RANDOM, U_ONOFF, "PLAY", 0, 1, 0, 1}, {M_RANDOM, U_ENUM, "TRACK", 0, 6, 0, 1},
    {M_RANDOM, U_ENUM, "RATE", 0, 5, 2, 1}, {M_RANDOM, U_PCT, "PROBABILITY", 0, 100, 60, 1},
    {M_RANDOM, U_PCT, "SPREAD", 0, 100, 50, 1}, {M_RANDOM, U_PCT, "BIAS", 0, 100, 50, 1},
    {M_RANDOM, U_PCT, "DEJA VU", 0, 100, 50, 1}, {M_RANDOM, U_NUM, "LOOP LENGTH", 1, 16, 8, 1},
    {M_RANDOM, U_NUM, "RANGE (OCTAVES)", 1, 4, 2, 1}, {M_RANDOM, U_PCT, "GATE", 5, 100, 50, 5},
    {M_SENDS, U_PCT, "TRACK 1 > PARTICLES", 0, 100, 0, 1}, {M_SENDS, U_PCT, "TRACK 2 > PARTICLES", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "TRACK 3 > PARTICLES", 0, 100, 0, 1}, {M_SENDS, U_PCT, "TRACK 4 > PARTICLES", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "TRACK 5 > PARTICLES", 0, 100, 0, 1}, {M_SENDS, U_PCT, "TRACK 6 > PARTICLES", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "WHOLE MIX > PARTICLES", 0, 100, 0, 1},
    {M_SEEDS, U_PCT, "MIX", 0, 100, 0, 1}, {M_SEEDS, U_ENUM, "MEDIA", 0, 3, 3, 1},
    {M_SEEDS, U_PCT, "DENSITY", 0, 100, 75, 1}, {M_SEEDS, U_PCT, "TIME", 0, 100, 20, 1},
    {M_SEEDS, U_PCT, "SIZE (100 = DELAY)", 0, 100, 50, 1}, {M_SEEDS, U_PCT, "SHAPE", 0, 100, 50, 1},
    {M_SEEDS, U_SEMI, "PITCH", -24, 24, 0, 1}, {M_SEEDS, U_BIPCT, "TIME RANDOM", -100, 100, 0, 5},
    {M_SEEDS, U_BIPCT, "SIZE RANDOM", -100, 100, 0, 5}, {M_SEEDS, U_BIPCT, "SHAPE RANDOM", -100, 100, 0, 5},
    {M_SEEDS, U_BIPCT, "PITCH RANDOM", -100, 100, 0, 5}, {M_SEEDS, U_PCT, "FEEDBACK", 0, 100, 20, 1},
    {M_SEEDS, U_PCT, "REVERB", 0, 100, 30, 1}, {M_SEEDS, U_ONOFF, "FREEZE", 0, 1, 0, 1},
    {M_SEEDS, U_ONOFF, "MONO (LONGER MEMORY)", 0, 1, 0, 1},
    {M_SENDS, U_PCT, "TRACK 1 > SEEDS", 0, 100, 0, 1}, {M_SENDS, U_PCT, "TRACK 2 > SEEDS", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "TRACK 3 > SEEDS", 0, 100, 0, 1}, {M_SENDS, U_PCT, "TRACK 4 > SEEDS", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "TRACK 5 > SEEDS", 0, 100, 0, 1}, {M_SENDS, U_PCT, "TRACK 6 > SEEDS", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "WHOLE MIX > SEEDS", 0, 100, 0, 1},
    {M_SAMPLES, U_PCT, "LEVEL", 0, 100, 0, 1}, {M_SAMPLES, U_ENUM, "FILE (A: LOOK AGAIN)", 0, 63, 0, 1},
    {M_SAMPLES, U_ENUM, "MODE", 0, 1, 0, 1}, {M_SAMPLES, U_ENUM, "FOLLOW TRACK", 0, 5, 0, 1},
    {M_SAMPLES, U_SEMI, "PITCH", -24, 24, 0, 1}, {M_SAMPLES, U_ONOFF, "FOLLOW PITCH", 0, 1, 1, 1},
    {M_SAMPLES, U_PCT, "START", 0, 99, 0, 1}, {M_SAMPLES, U_PCT, "LENGTH", 1, 100, 100, 1},
    {M_SAMPLES, U_ONOFF, "REVERSE", 0, 1, 0, 1}, {M_SAMPLES, U_PCT, "DECAY (HITS)", 0, 100, 40, 1},
    {M_SAMPLES, U_PCT, "BRIGHTNESS", 0, 100, 100, 1}, {M_SAMPLES, U_BIPCT, "PAN", -100, 100, 0, 5},
    {M_SENDS, U_PCT, "SAMPLES > CLOUD", 0, 100, 0, 1}, {M_SENDS, U_PCT, "SAMPLES > PARTICLES", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "SAMPLES > SEEDS", 0, 100, 0, 1}, {M_SENDS, U_PCT, "SAMPLES > SHIMMER", 0, 100, 0, 1},
    {M_POLY, U_PCT, "LEVEL", 0, 100, 0, 1}, {M_POLY, U_ENUM, "MODEL", 0, 23, 8, 1},
    {M_POLY, U_PCT, "HARMONICS", 0, 100, 40, 1}, {M_POLY, U_PCT, "TIMBRE", 0, 100, 45, 1},
    {M_POLY, U_PCT, "MORPH", 0, 100, 40, 1}, {M_POLY, U_PCT, "ATTACK", 0, 100, 25, 1},
    {M_POLY, U_PCT, "DECAY", 0, 100, 45, 1}, {M_POLY, U_PCT, "SUSTAIN", 0, 100, 70, 1},
    {M_POLY, U_PCT, "RELEASE", 0, 100, 55, 1}, {M_POLY, U_PCT, "GATE COLOUR (LPG)", 0, 100, 50, 1},
    {M_POLY, U_NUM, "VOICES", 1, 8, 6, 1}, {M_POLY, U_PCT, "STEREO SPREAD", 0, 100, 50, 1},
    {M_POLY, U_NUM, "OCTAVE", -2, 2, 0, 1}, {M_POLY, U_ONOFF, "PLAY MODE > POLY", 0, 1, 1, 1},
    {M_POLY, U_ENUM, "FOLLOW TRACK", 0, 6, 0, 1}, {M_POLY, U_MS, "FOLLOW GATE", 50, 3000, 600, 50},
    {M_SENDS, U_PCT, "POLY > CLOUD", 0, 100, 0, 1}, {M_SENDS, U_PCT, "POLY > PARTICLES", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "POLY > SEEDS", 0, 100, 0, 1}, {M_SENDS, U_PCT, "POLY > SHIMMER", 0, 100, 0, 1},
    {M_MASTER, U_ENUM, "THIS BIOME", 0, 1, 0, 1}, {M_MASTER, U_ONOFF, "LAB FOLLOWS BIOME", 0, 1, 1, 1},
    {M_MASTER, U_MS, "LAB CHANGE TIME", 0, 10000, 3000, 250},
    {M_POLY, U_ENUM, "MIDI NOTES", 0, 17, 1, 1},
    {M_VOICES, U_PCT, "LEVEL", 0, 100, 0, 1}, {M_VOICES, U_ENUM, "FILE", 0, 95, 0, 1},
    {M_VOICES, U_ENUM, "MODE", 0, 4, 0, 1}, {M_VOICES, U_ENUM, "TRIGGER", 0, 8, 0, 1},
    {M_VOICES, U_PCT, "DENSITY", 0, 100, 40, 1}, {M_VOICES, U_NUM, "PHRASE", 1, 999, 1, 1},
    {M_VOICES, U_SEMI, "PITCH", -12, 12, 0, 1}, {M_VOICES, U_PCT, "REVERSE CHANCE", 0, 100, 0, 5},
    {M_VOICES, U_PCT, "RADIO", 0, 100, 0, 5}, {M_VOICES, U_PCT, "SPREAD", 0, 100, 30, 5},
    {M_SENDS, U_PCT, "VOICES > CLOUD", 0, 100, 0, 1}, {M_SENDS, U_PCT, "VOICES > PARTICLES", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "VOICES > SEEDS", 0, 100, 0, 1}, {M_SENDS, U_PCT, "VOICES > SHIMMER", 0, 100, 0, 1},
    {M_LOGIC, U_ONOFF, "PLAY", 0, 1, 0, 1}, {M_LOGIC, U_ENUM, "RULE", 0, 6, 0, 1},
    {M_LOGIC, U_ENUM, "VOICE 1 TO", 0, 7, 6, 1}, {M_LOGIC, U_ENUM, "VOICE 2 TO", 0, 8, 0, 1},
    {M_LOGIC, U_ENUM, "RATE", 0, 5, 1, 1}, {M_LOGIC, U_PCT, "DENSITY", 0, 100, 70, 1},
    {M_LOGIC, U_PCT, "SHAPE", 0, 100, 30, 1}, {M_LOGIC, U_NUM, "RANGE (OCTAVES)", 1, 3, 2, 1},
    {M_LOGIC, U_NUM, "SEED", 0, 999, 1, 1}, {M_LOGIC, U_PCT, "VARIATION", 0, 100, 20, 1},
    {M_LOGIC, U_PCT, "GATE", 5, 100, 80, 5},
    {M_SYNTH, U_PCT, "LEVEL", 0, 100, 0, 1}, {M_SYNTH, U_ENUM, "ENGINE", 0, 11, 0, 1},
    {M_SYNTH, U_ENUM, "PLAYED BY", 0, 7, 0, 1}, {M_SYNTH, U_PCT, "DENSITY", 0, 100, 40, 1},
    {M_SYNTH, U_NUM, "OCTAVE", -2, 2, 0, 1}, {M_SYNTH, U_PCT, "DECAY", 0, 100, 55, 1},
    {M_SYNTH, U_PCT, "BRIGHTNESS", 0, 100, 50, 1}, {M_SYNTH, U_PCT, "CHARACTER", 0, 100, 30, 1},
    {M_SYNTH, U_PCT, "MOTION", 0, 100, 40, 1}, {M_SYNTH, U_PCT, "SPREAD", 0, 100, 50, 1},
    {M_SENDS, U_PCT, "SYNTH > CLOUD", 0, 100, 0, 1}, {M_SENDS, U_PCT, "SYNTH > PARTICLES", 0, 100, 0, 1},
    {M_SENDS, U_PCT, "SYNTH > SEEDS", 0, 100, 0, 1}, {M_SENDS, U_PCT, "SYNTH > SHIMMER", 0, 100, 0, 1},
    {M_SYNTH, U_ENUM, "MODEL", 0, 5, 0, 1},
    {M_RACK_A, U_ONOFF, "VOICES INTO", 0, 1, 0, 1},
    {M_RACK_A, U_ONOFF, "SAMPLES INTO", 0, 1, 0, 1},
    {M_RACK_A, U_ONOFF, "SYNTH INTO", 0, 1, 0, 1},
    {M_RACK_A, U_ONOFF, "POLY INTO", 0, 1, 0, 1},
    {M_RACK_A, U_ENUM, "FILTER", 0, 3, 0, 1},
    {M_RACK_A, U_PCT, "CUTOFF", 0, 100, 60, 1},
    {M_RACK_A, U_PCT, "RESONANCE", 0, 100, 30, 1},
    {M_RACK_A, U_BIPCT, "FOLLOW", -100, 100, 0, 5},
    {M_RACK_A, U_PCT, "SWEEP", 0, 100, 0, 1},
    {M_RACK_A, U_PCT, "SWEEP RATE", 0, 100, 30, 1},
    {M_RACK_A, U_PCT, "DRIVE", 0, 100, 0, 1},
    {M_RACK_A, U_PCT, "CRUSH", 0, 100, 0, 1},
    {M_RACK_A, U_PCT, "RESONATOR", 0, 100, 0, 1},
    {M_RACK_A, U_ENUM, "RES MODEL", 0, 5, 0, 1},
    {M_RACK_A, U_SEMI, "RES NOTE", -24, 24, 0, 1},
    {M_RACK_A, U_PCT, "STRUCTURE", 0, 100, 40, 1},
    {M_RACK_A, U_PCT, "BRIGHTNESS", 0, 100, 60, 1},
    {M_RACK_A, U_PCT, "DAMPING", 0, 100, 60, 1},
    {M_RACK_A, U_PCT, "POSITION", 0, 100, 30, 1},
    {M_RACK_A, U_PCT, "ECHO", 0, 100, 0, 1},
    {M_RACK_A, U_ENUM, "ECHO TIME", 0, 5, 3, 1},
    {M_RACK_A, U_PCT, "ECHO FEEDBACK", 0, 100, 40, 1},
    {M_RACK_A, U_PCT, "WOW", 0, 100, 15, 1},
    {M_RACK_A, U_PCT, "ECHO TONE", 0, 100, 60, 1},
    {M_RACK_B, U_ONOFF, "VOICES INTO", 0, 1, 0, 1},
    {M_RACK_B, U_ONOFF, "SAMPLES INTO", 0, 1, 0, 1},
    {M_RACK_B, U_ONOFF, "SYNTH INTO", 0, 1, 0, 1},
    {M_RACK_B, U_ONOFF, "POLY INTO", 0, 1, 0, 1},
    {M_RACK_B, U_ENUM, "FILTER", 0, 3, 0, 1},
    {M_RACK_B, U_PCT, "CUTOFF", 0, 100, 60, 1},
    {M_RACK_B, U_PCT, "RESONANCE", 0, 100, 30, 1},
    {M_RACK_B, U_BIPCT, "FOLLOW", -100, 100, 0, 5},
    {M_RACK_B, U_PCT, "SWEEP", 0, 100, 0, 1},
    {M_RACK_B, U_PCT, "SWEEP RATE", 0, 100, 30, 1},
    {M_RACK_B, U_PCT, "DRIVE", 0, 100, 0, 1},
    {M_RACK_B, U_PCT, "CRUSH", 0, 100, 0, 1},
    {M_RACK_B, U_PCT, "RESONATOR", 0, 100, 0, 1},
    {M_RACK_B, U_ENUM, "RES MODEL", 0, 5, 0, 1},
    {M_RACK_B, U_SEMI, "RES NOTE", -24, 24, 0, 1},
    {M_RACK_B, U_PCT, "STRUCTURE", 0, 100, 40, 1},
    {M_RACK_B, U_PCT, "BRIGHTNESS", 0, 100, 60, 1},
    {M_RACK_B, U_PCT, "DAMPING", 0, 100, 60, 1},
    {M_RACK_B, U_PCT, "POSITION", 0, 100, 30, 1},
    {M_RACK_B, U_PCT, "ECHO", 0, 100, 0, 1},
    {M_RACK_B, U_ENUM, "ECHO TIME", 0, 5, 3, 1},
    {M_RACK_B, U_PCT, "ECHO FEEDBACK", 0, 100, 40, 1},
    {M_RACK_B, U_PCT, "WOW", 0, 100, 15, 1},
    {M_RACK_B, U_PCT, "ECHO TONE", 0, 100, 60, 1},
    {M_GRIDS, U_ONOFF, "PLAY", 0, 1, 0, 1},
    {M_GRIDS, U_PCT, "MAP X", 0, 100, 50, 1},
    {M_GRIDS, U_PCT, "MAP Y", 0, 100, 50, 1},
    {M_GRIDS, U_PCT, "CHAOS", 0, 100, 10, 1},
    {M_GRIDS, U_PCT, "KICK DENSITY", 0, 100, 60, 1},
    {M_GRIDS, U_PCT, "SNARE DENSITY", 0, 100, 45, 1},
    {M_GRIDS, U_PCT, "HAT DENSITY", 0, 100, 55, 1},
    {M_GRIDS, U_ENUM, "KICK TO", 0, 9, 7, 1},
    {M_GRIDS, U_ENUM, "SNARE TO", 0, 9, 7, 1},
    {M_GRIDS, U_ENUM, "HAT TO", 0, 9, 7, 1},
    {M_GRIDS, U_ENUM, "RATE", 0, 2, 1, 1},
    {M_GRIDS, U_PCT, "ACCENT", 0, 100, 60, 1},
    {M_VOICES, U_PCT, "POSITION", 0, 100, 25, 1},
    {M_VOICES, U_BIPCT, "SCAN", -200, 200, 0, 5},
    {M_VOICES, U_MS, "GRAIN SIZE", 20, 1000, 160, 10},
    {M_VOICES, U_PERS, "GRAINS", 1, 60, 14, 1},
    {M_VOICES, U_PCT, "SPRAY", 0, 100, 10, 1},
    {M_VOICES, U_SEMI, "SCATTER (SCALE)", 0, 12, 0, 1},
    {M_VOICES, U_PCT, "TIDE RATE", 0, 100, 35, 1},
    {M_VOICES, U_PCT, "TIDE SLOPE", 0, 100, 50, 1},
    {M_VOICES, U_PCT, "TIDE SHAPE", 0, 100, 50, 1},
    {M_VOICES, U_PCT, "TIDE SMOOTH", 0, 100, 50, 1},
    {M_VOICES, U_PCT, "TIDE DEPTH", 0, 100, 0, 1},
    {M_VOICES, U_ENUM, "TIDE TO", 0, 5, 0, 1},
    {M_VOICES, U_PCT, "DEJA VU (MARBLES)", 0, 100, 60, 1},
    {M_MASTER, U_DB, "EQ LOW", -12, 12, 0, 0.5f}, {M_MASTER, U_HZ, "EQ LOW AT", 30, 400, 110, 5},
    {M_MASTER, U_DB, "EQ MID", -12, 12, 0, 0.5f}, {M_MASTER, U_HZ, "EQ MID AT", 200, 6000, 1200, 50},
    {M_MASTER, U_DB, "EQ HIGH", -12, 12, 0, 0.5f}, {M_MASTER, U_HZ, "EQ HIGH AT", 2000, 16000, 9000, 250},
    {M_MASTER, U_PCT, "SATURATION", 0, 100, 0, 1}, {M_MASTER, U_ENUM, "SATURATION KIND", 0, 2, 0, 1},
    {M_MASTER, U_PCT, "GLUE COMPRESSOR", 0, 100, 0, 1}, {M_MASTER, U_MS, "COMP ATTACK", 1, 100, 10, 1},
    {M_MASTER, U_MS, "COMP RELEASE", 20, 1000, 200, 10}, {M_MASTER, U_PCT, "COMP MIX (PARALLEL)", 0, 100, 100, 5},
    {M_MASTER, U_ENUM, "SOUND", 0, 5, 0, 1},
};
/* MASTER's sounds: one turn sets the whole chain, a starting point to shape further (OWN: as it is) */
static const char *const MS_PRESETS[6] = {"OWN", "CLEAN", "GENTLE", "WARM", "LOUD", "AIRY"};
static const struct { float lowcut, low, low_hz, mid, mid_hz, high, high_hz, tilt, sat, kind, comp, att, rel, mix, width, gain, lim, ceil; }
MS_PRESET_V[6] = {
    {0},
    {0, 0, 110, 0, 1200, 0, 9000, 0, 0, 0, 0, 10, 200, 100, 100, 0, 0, -1},          /* CLEAN: all neutral */
    {20, 1, 110, 0, 1200, 1.5f, 10000, 0, 15, 0, 30, 20, 250, 100, 100, 5, 1, -1},    /* GENTLE: a little glue, safe */
    {25, 2.5f, 120, -1, 600, -1, 8000, 0, 40, 1, 40, 15, 300, 70, 100, 7, 1, -1},     /* WARM: tube, round */
    {30, 2, 100, -0.5f, 800, 2.5f, 9000, 0, 30, 2, 70, 8, 150, 100, 110, 11, 1, -0.8f},/* LOUD: dense, forward */
    {35, 0, 110, -1.5f, 400, 4, 12000, 0, 10, 0, 25, 20, 250, 100, 125, 6, 1, -1},    /* AIRY: open, wide */
};
static const char *const MS_SAT_KINDS[3] = {"TAPE", "TUBE", "TRANSISTOR"};
/* MASTER's rows in the order of the signal (the table keeps the order of the save file) */
static const int MS_ORDER[] = {P_MS_PRESET, P_LOWCUT, P_MS_LOW, P_MS_LOW_HZ, P_MS_MID, P_MS_MID_HZ, P_MS_HIGH, P_MS_HIGH_HZ, P_TILT,
                               P_MS_SAT, P_MS_SAT_KIND, P_MS_COMP, P_MS_COMP_ATT, P_MS_COMP_REL, P_MS_COMP_MIX, P_WIDTH,
                               P_GAIN, P_LIMIT, P_CEIL, P_LB_OWN, P_LB_FOLLOW, P_LB_TIME};
static const char *const LAB_BE_MEDIA[4] = {"DIRTY TAPE", "CLEAN TAPE", "VINTAGE DIGITAL", "DIGITAL"};
/* the memory in seconds at 44.1 kHz, as Beads' delay lines hold it (stereo; mono twice): 16-bit 192000 frames, 8-bit 384000,
 * 12-bit 256000, the tapes at half rate */
static const float LAB_BE_MEMORY[4] = {768000.0f / 44100.0f, 512000.0f / 44100.0f, 256000.0f / 44100.0f, 192000.0f / 44100.0f};
static const char *const LAB_PA_MODES[4] = {"GRANULAR", "STRETCH", "LOOPING DELAY", "SPECTRAL"};
static const char *const LAB_RATES[6] = {"1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32"};
static const float LAB_RATE_BEATS[6] = {1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f};

static float lab_v[P_COUNT];
static void ms_preset(int k)
{
    if (k <= 0 || k > 5)
        return;
    lab_v[P_LOWCUT] = MS_PRESET_V[k].lowcut, lab_v[P_MS_LOW] = MS_PRESET_V[k].low, lab_v[P_MS_LOW_HZ] = MS_PRESET_V[k].low_hz;
    lab_v[P_MS_MID] = MS_PRESET_V[k].mid, lab_v[P_MS_MID_HZ] = MS_PRESET_V[k].mid_hz, lab_v[P_MS_HIGH] = MS_PRESET_V[k].high;
    lab_v[P_MS_HIGH_HZ] = MS_PRESET_V[k].high_hz, lab_v[P_TILT] = MS_PRESET_V[k].tilt, lab_v[P_MS_SAT] = MS_PRESET_V[k].sat;
    lab_v[P_MS_SAT_KIND] = MS_PRESET_V[k].kind, lab_v[P_MS_COMP] = MS_PRESET_V[k].comp, lab_v[P_MS_COMP_ATT] = MS_PRESET_V[k].att;
    lab_v[P_MS_COMP_REL] = MS_PRESET_V[k].rel, lab_v[P_MS_COMP_MIX] = MS_PRESET_V[k].mix, lab_v[P_WIDTH] = MS_PRESET_V[k].width;
    lab_v[P_GAIN] = MS_PRESET_V[k].gain, lab_v[P_LIMIT] = MS_PRESET_V[k].lim, lab_v[P_CEIL] = MS_PRESET_V[k].ceil;
}
static uint32_t lab_changed_at;                  /* bk_ms of the last edit, 0 = saved */
static int lab_dirty;

/* ------------------------------------------------------------ helpers --- */
static uint32_t lab_seed = 0x2545F491u;
static inline float lab_rand(void)               /* 0..1 */
{
    lab_seed ^= lab_seed << 13;
    lab_seed ^= lab_seed >> 17;
    lab_seed ^= lab_seed << 5;
    return (float)(lab_seed >> 8) * (1.0f / 16777216.0f);
}
static inline float lab_tanh(float x)            /* a soft clip with tanh's shape, cheap */
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}
#define LAB_TAB 1024
static float lab_sin_tab[LAB_TAB + 1];
static inline float lab_sin(float turns)         /* sin(2 pi turns), turns any value */
{
    float p = (turns - floorf(turns)) * LAB_TAB;
    int i = (int)p;
    float f = p - (float)i;
    return lab_sin_tab[i] + (lab_sin_tab[i + 1] - lab_sin_tab[i]) * f;
}
static float lab_mtof(float n) { return 440.0f * powf(2.0f, (n - 69.0f) / 12.0f); }

/* ---------------------------------------------------------- textures --- */
/* the VIBE FM Performance Garden's four beds (garden-textures.c, by Kid Bunka), here without the chime */
#define TEX_SPARKS 8
typedef struct { float phase, step, glide, amp, decay, pan; } tex_spark;
static struct {
    uint32_t seed[2];
    float band[2][2], air_lp[2], air_hp[2], water_lp[2];
    float rustle, rustle_target, breath, pulse_phase;
    unsigned rustle_clock;
    tex_spark sparks[TEX_SPARKS];
    float glow[4];                                /* for the picture: how much each bed sounded lately */
} tex;
static float tex_noise(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return (float)(int32_t)x * (1.0f / 2147483648.0f);
}
static float tex_chance(uint32_t *s) { return tex_noise(s) * 0.5f + 0.5f; }
static void tex_spark_add(float hz, float glide, float decay, float amp)
{
    for (unsigned i = 0; i < TEX_SPARKS; ++i) {
        tex_spark *s = &tex.sparks[i];
        if (s->amp > 0.0001f)
            continue;
        s->phase = 0;
        s->step = hz / 44100.0f;
        s->glide = glide;
        s->decay = decay;
        s->amp = amp;
        s->pan = 0.15f + 0.7f * tex_chance(&tex.seed[0]);
        return;
    }
}
static void tex_render(float *lr, unsigned n)
{
    const float forest = lab_v[P_FOREST] / 100, air = lab_v[P_AIR] / 100, water = lab_v[P_WATER] / 100;
    const float dust = lab_v[P_STARDUST] / 100, pulse = lab_v[P_PULSE] / 100;
    const md_patch *p = md_state();
    const int beds = forest > 0.0001f || air > 0.0001f || water > 0.0001f;
    const float tempo = (float)md_tempo_now(), pulse_step = (tempo < 30.f ? 30.f : tempo) * (2.0f / 60.0f) / 44100.0f;
    for (unsigned k = 0; k < n; ++k) {
        float out[2] = {0, 0};
        if (beds) {
            if (++tex.rustle_clock >= 2205) {
                const float r = tex_chance(&tex.seed[0]);
                tex.rustle_clock = 0;
                tex.rustle_target = 0.2f + 0.8f * r * r;
            }
            tex.rustle += (tex.rustle_target - tex.rustle) * 0.0015f;
            tex.breath += 0.12f / 44100.0f;
            if (tex.breath >= 1.0f)
                tex.breath -= 1.0f;
            const float breath = 0.55f + 0.45f * lab_sin(tex.breath);
            for (unsigned c = 0; c < 2; ++c) {
                const float x = tex_noise(&tex.seed[c]);
                tex.band[c][0] += (x - tex.band[c][0]) * 0.30f;
                tex.band[c][1] += (x - tex.band[c][1]) * 0.069f;
                out[c] += (tex.band[c][0] - tex.band[c][1]) * forest * tex.rustle * 0.085f;
                tex.air_hp[c] += (x - tex.air_hp[c]) * 0.25f;
                tex.air_lp[c] += ((x - tex.air_hp[c]) - tex.air_lp[c]) * 0.63f;
                out[c] += tex.air_lp[c] * air * breath * 0.05f;
                tex.water_lp[c] += (x - tex.water_lp[c]) * 0.04f;
                out[c] += tex.water_lp[c] * water * 0.11f;
            }
        }
        if (water > 0.0001f && tex_chance(&tex.seed[1]) < water * (9.0f / 44100.0f))
            tex_spark_add(700.0f + 1700.0f * tex_chance(&tex.seed[1]), 1.00005f, 0.9991f, 0.018f + 0.026f * water);
        if (dust > 0.0001f && tex_chance(&tex.seed[1]) < dust * (3.5f / 44100.0f)) {
            const int note = md_quantize(84 + (int)(tex_chance(&tex.seed[1]) * 14.0f), p->root, p->scale);
            tex_spark_add(lab_mtof((float)note), 1.0f, 0.99993f, 0.014f + 0.018f * dust);
            tex.glow[3] = 1;
        }
        for (unsigned i = 0; i < TEX_SPARKS; ++i) {
            tex_spark *s = &tex.sparks[i];
            if (s->amp <= 0.0001f)
                continue;
            const float y = lab_sin(s->phase) * s->amp;
            out[0] += y * (1.0f - s->pan);
            out[1] += y * s->pan;
            s->phase += s->step;
            if (s->phase >= 1.0f)
                s->phase -= 1.0f;
            s->step *= s->glide;
            if (s->step > 0.45f)
                s->step = 0.45f;
            s->amp *= s->decay;
        }
        float gain = 1.0f;
        if (pulse > 0.0001f) {
            tex.pulse_phase += pulse_step;
            if (tex.pulse_phase >= 1.0f)
                tex.pulse_phase -= 1.0f;
            const float fall = 1.0f - tex.pulse_phase;
            const float rise = tex.pulse_phase < 0.03f ? tex.pulse_phase * (1.0f / 0.03f) : 1.0f;
            gain = 1.0f - 0.62f * pulse * rise * fall * fall * fall;
        }
        lr[2 * k] = (lr[2 * k] + out[0]) * gain;
        lr[2 * k + 1] = (lr[2 * k + 1] + out[1]) * gain;
    }
    tex.glow[0] = forest * tex.rustle;
    tex.glow[1] = air * (0.55f + 0.45f * lab_sin(tex.breath));
    tex.glow[2] = water;
    tex.glow[3] *= 0.9f;
}

/* -------------------------------------------------------------- cloud --- */
#define CL_N (44100 * 4)
#define CL_GRAINS 48
typedef struct { float pos, rate, t, dt, gl, gr; int on; } cl_grain;
static float cl_buf[CL_N];
static uint32_t cl_w;
static cl_grain cl_g[CL_GRAINS];
static float cl_phase, cl_fb_l, cl_fb_r, cl_level;
static int cl_cleared = 1;
#define WIN_N 512
static float lab_win[WIN_N + 1];

static void cl_spawn(void)
{
    const md_patch *p = md_state();
    float size = lab_v[P_CL_SIZE] * 44.1f, spread = lab_v[P_CL_SPREAD] / 100, semis = lab_v[P_CL_PITCH];
    int i, sc = (int)lab_v[P_CL_SCATTER];
    cl_grain *g = 0;
    for (i = 0; i < CL_GRAINS; i++)
        if (!cl_g[i].on) {
            g = &cl_g[i];
            break;
        }
    if (!g)
        return;
    if (sc > 0) {                                /* a scale tone within +-scatter of the pitch */
        int base = 60 + (int)semis, off = (int)lrintf((lab_rand() * 2 - 1) * (float)sc);
        int q = md_quantize(base + off, p->root, p->scale);
        semis = (float)(q - 60);
    }
    g->rate = powf(2.0f, semis / 12.0f);
    if (lab_rand() * 100 < lab_v[P_CL_REVERSE])
        g->rate = -g->rate;
    {
        float back = size * fabsf(g->rate) + 64 + lab_rand() * spread * 44100.0f * 2.0f;
        if (back > CL_N - 2 * size - 64)
            back = CL_N - 2 * size - 64;
        g->pos = (float)cl_w - (g->rate < 0 ? 64 + lab_rand() * spread * 44100.0f * 2.0f : back);
        while (g->pos < 0)
            g->pos += CL_N;
    }
    {
        float pan = 0.5f + (lab_rand() - 0.5f) * spread, dens = lab_v[P_CL_DENS], ov = dens * lab_v[P_CL_SIZE] / 1000;
        float norm = 1.0f / sqrtf(ov < 1 ? 1 : ov);
        g->gl = sqrtf(1 - pan) * norm;
        g->gr = sqrtf(pan) * norm;
    }
    g->t = 0;
    g->dt = 1.0f / (size < 64 ? 64 : size);
    g->on = 1;
}

static void cl_render(const float *in, float *out, unsigned n)   /* in: mono, out: stereo (added) */
{
    const int freeze = lab_v[P_CL_FREEZE] > 0.5f;
    const float fb = lab_v[P_CL_FB] / 100, dens = lab_v[P_CL_DENS] / 44100.0f;
    float lvl = 0;
    for (unsigned k = 0; k < n; k++) {
        float l = 0, r = 0;
        if (!freeze)
            cl_buf[cl_w] = in[k] + lab_tanh((cl_fb_l + cl_fb_r) * 0.5f * fb);
        cl_w = (cl_w + 1) % CL_N;
        cl_phase += dens * (0.7f + 0.6f * lab_rand());
        if (cl_phase >= 1) {
            cl_phase -= 1;
            cl_spawn();
        }
        for (int i = 0; i < CL_GRAINS; i++) {
            cl_grain *g = &cl_g[i];
            float pos, f, s, w;
            int a, b;
            if (!g->on)
                continue;
            pos = g->pos;
            a = (int)pos;
            f = pos - (float)a;
            b = a + 1 >= CL_N ? 0 : a + 1;
            s = cl_buf[a] + (cl_buf[b] - cl_buf[a]) * f;
            w = lab_win[(int)(g->t * WIN_N)];
            l += s * w * g->gl;
            r += s * w * g->gr;
            g->pos += g->rate;
            if (g->pos >= CL_N)
                g->pos -= CL_N;
            else if (g->pos < 0)
                g->pos += CL_N;
            g->t += g->dt;
            if (g->t >= 1)
                g->on = 0;
        }
        cl_fb_l = l;
        cl_fb_r = r;
        out[2 * k] += l;
        out[2 * k + 1] += r;
        lvl += l * l + r * r;
    }
    cl_level = sqrtf(lvl / (float)(2 * n));
    cl_cleared = 0;
}

/* ------------------------------------------------------------ shimmer --- */
#define SH_LINES 8
#define SH_MAX 8192
static const int SH_BASE[SH_LINES] = {1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116};
static float sh_line[SH_LINES][SH_MAX], sh_damp[SH_LINES], sh_lfo, sh_level;
static uint32_t sh_w;
#define PS_N 4096
static float ps_buf[PS_N], ps_phase, sh_hp_x, sh_hp_y, sh_in_x, sh_in_y;
static uint32_t ps_w;
static int sh_cleared = 1;

static float sh_pitch_up(float x)                /* an octave up: two read heads sliding through a 2048 window */
{
    const float W = 2048.0f;
    float d1, d2, s1, s2, p2, w1, w2;
    ps_buf[ps_w] = x;
    ps_phase -= 1.0f / W;                        /* ratio 2: the delay shrinks one sample a sample */
    if (ps_phase < 0)
        ps_phase += 1;
    p2 = ps_phase + 0.5f;
    if (p2 >= 1)
        p2 -= 1;
    d1 = ps_phase * W;
    d2 = p2 * W;
    s1 = ps_buf[(ps_w + PS_N - 1 - (uint32_t)d1) % PS_N];
    s2 = ps_buf[(ps_w + PS_N - 1 - (uint32_t)d2) % PS_N];
    w1 = lab_sin(ps_phase * 0.5f);              /* sin^2 and its partner: the two windows always sum to 1 */
    w1 *= w1;
    w2 = 1.0f - w1;
    ps_w = (ps_w + 1) % PS_N;
    return s1 * w1 + s2 * w2;
}

static void sh_render(const float *in, float *out, unsigned n)   /* in: mono, out: stereo (added) */
{
    float size = 0.45f + lab_v[P_SH_SIZE] / 100 * 0.95f;
    float rt60 = 0.6f * powf(40.0f, lab_v[P_SH_DECAY] / 100);     /* 0.6 .. 24 s */
    float damp = 0.05f + lab_v[P_SH_DAMP] / 100 * 0.75f, shim = lab_v[P_SH_SHIMMER] / 100;
    float mod = lab_v[P_SH_MOD] / 100 * 12.0f, g[SH_LINES], lvl = 0;
    int len[SH_LINES], i;
    for (i = 0; i < SH_LINES; i++) {
        len[i] = (int)((float)SH_BASE[i] * size);
        if (len[i] > SH_MAX - 32)
            len[i] = SH_MAX - 32;
        g[i] = powf(10.0f, -3.0f * (float)len[i] / (rt60 * 44100.0f));
    }
    for (unsigned k = 0; k < n; k++) {
        float o[SH_LINES], sum = 0, up;
        sh_lfo += 0.31f / 44100.0f;
        if (sh_lfo >= 1)
            sh_lfo -= 1;
        for (i = 0; i < SH_LINES; i++) {
            float d = (float)len[i] + (i < 2 ? mod * lab_sin(sh_lfo + 0.5f * (float)i) : 0.0f);
            float rp = (float)sh_w - d;
            int a;
            float f;
            while (rp < 0)
                rp += SH_MAX;
            a = (int)rp;
            f = rp - (float)a;
            o[i] = sh_line[i][a] + (sh_line[i][(a + 1) % SH_MAX] - sh_line[i][a]) * f;
            sh_damp[i] += (o[i] - sh_damp[i]) * (1.0f - damp);
            o[i] = sh_damp[i] * g[i];
            sum += o[i];
        }
        /* Hadamard 8 mixing, scaled to keep energy */
        {
            float a0 = o[0] + o[1], a1 = o[0] - o[1], a2 = o[2] + o[3], a3 = o[2] - o[3];
            float a4 = o[4] + o[5], a5 = o[4] - o[5], a6 = o[6] + o[7], a7 = o[6] - o[7];
            float b0 = a0 + a2, b1 = a1 + a3, b2 = a0 - a2, b3 = a1 - a3;
            float b4 = a4 + a6, b5 = a5 + a7, b6 = a4 - a6, b7 = a5 - a7;
            const float s = 0.35355339f;
            o[0] = (b0 + b4) * s; o[1] = (b1 + b5) * s; o[2] = (b2 + b6) * s; o[3] = (b3 + b7) * s;
            o[4] = (b0 - b4) * s; o[5] = (b1 - b5) * s; o[6] = (b2 - b6) * s; o[7] = (b3 - b7) * s;
        }
        if (shim > 0.001f) {                     /* the octave, high-passed (~40 Hz) and softly bounded: no runaway */
            float x = sh_pitch_up(sum * 0.125f);
            sh_hp_y = 0.9943f * (sh_hp_y + x - sh_hp_x);
            sh_hp_x = x;
            up = lab_tanh(sh_hp_y * shim * 0.85f * 2.0f) * 0.5f;
        } else
            up = 0.0f;
        {                                        /* the input without its DC (the stems may carry some) */
            float x = in[k];
            sh_in_y = 0.9986f * (sh_in_y + x - sh_in_x);
            sh_in_x = x;
        }
        for (i = 0; i < SH_LINES; i++)
            sh_line[i][sh_w] = o[i] + (sh_in_y + up) * ((i & 1) ? -0.5f : 0.5f);
        sh_w = (sh_w + 1) % SH_MAX;
        {
            float l = (o[0] + o[2] + o[4] + o[6]) * 0.5f, r = (o[1] + o[3] + o[5] + o[7]) * 0.5f;
            out[2 * k] += l;
            out[2 * k + 1] += r;
            lvl += l * l + r * r;
        }
    }
    sh_level = sqrtf(lvl / (float)(2 * n));
    sh_cleared = 0;
}

/* ------------------------------------------------------------- colour --- */
#define EN_N 4096
static float en_buf[2][EN_N], en_lfo, tp_buf[2][2048], tp_lp[2], tp_wow, tp_flut;
static uint32_t en_w, tp_w;
static float lab_read(const float *buf, uint32_t size, uint32_t w, float d)
{
    float rp = (float)w - d;
    int a;
    float f;
    while (rp < 0)
        rp += (float)size;
    a = (int)rp;
    f = rp - (float)a;
    return buf[a % size] + (buf[(a + 1) % size] - buf[a % size]) * f;
}
static void colour_render(float *lr, unsigned n)
{
    const float ens = lab_v[P_ENS_MIX] / 100, rate = 0.08f + lab_v[P_ENS_RATE] / 100 * 2.5f;
    const float depth = lab_v[P_ENS_DEPTH] / 100 * 6.0f * 44.1f;
    const float drive = lab_v[P_TAPE_DRIVE] / 100, wow = lab_v[P_TAPE_WOW] / 100, age = lab_v[P_TAPE_AGE] / 100;
    const float lp = 1.0f - age * 0.85f;
    for (unsigned k = 0; k < n; k++) {
        for (int c = 0; c < 2; c++) {
            float x = lr[2 * k + c];
            if (ens > 0.001f) {
                float wet = 0;
                en_buf[c][en_w] = x;
                for (int v = 0; v < 3; v++)
                    wet += lab_read(en_buf[c], EN_N, en_w,
                                    12.0f * 44.1f + depth * (0.5f + 0.5f * lab_sin(en_lfo + (float)v / 3.0f + (float)c * 0.17f)));
                x = x * (1.0f - ens * 0.5f) + wet * (ens / 3.0f) * 0.9f;
            }
            if (wow > 0.001f) {
                tp_buf[c][tp_w] = x;
                x = lab_read(tp_buf[c], 2048, tp_w,
                             220.0f + wow * (90.0f * lab_sin(tp_wow) + 12.0f * lab_sin(tp_flut + 0.3f * (float)c)));
            }
            if (drive > 0.001f) {
                float d = 1.0f + drive * 5.0f;
                x = lab_tanh(x * d) / lab_tanh(d) * (0.6f + 0.4f / d) * 1.4f;
            }
            if (age > 0.001f) {
                tp_lp[c] += (x - tp_lp[c]) * lp;
                x = tp_lp[c] + (lab_rand() - 0.5f) * age * 0.0016f;
            }
            lr[2 * k + c] = x;
        }
        en_w = (en_w + 1) % EN_N;
        tp_w = (tp_w + 1) % 2048;
        en_lfo += rate / 44100.0f;
        if (en_lfo >= 1) en_lfo -= 1;
        tp_wow += 0.55f / 44100.0f;
        if (tp_wow >= 1) tp_wow -= 1;
        tp_flut += 6.8f / 44100.0f;
        if (tp_flut >= 1) tp_flut -= 1;
    }
}

/* ------------------------------------------------------------- master --- */
/* The mastering chain (0.62), in the order of the signal:
   LOW CUT (two one-poles) > EQ: low shelf, bell, high shelf (RBJ biquads) > TILT > SATURATION (tape: tanh, tube:
   asymmetric with a DC block, transistor: harder; all antiderivative anti-aliased, level matched) > GLUE COMPRESSOR
   (stereo linked, soft knee, auto make-up, parallel mix) > WIDTH > GAIN > LIMITER (1.5 ms look-ahead: the gain
   falls before the peak arrives, then glides back; nothing is cut) > always: a soft guard above -0.5 dBFS, so the
   card never clips hard. */
typedef struct { float b0, b1, b2, a1, a2, z1[2], z2[2]; } ms_biquad;
static ms_biquad ms_eq[3];
static float ms_eq_key[3][2] = {{1e9f, 0}, {1e9f, 0}, {1e9f, 0}};
static float ms_hp1[2], ms_hp2[2], ms_hx1[2], ms_hx2[2], ms_tilt_lp[2], lim_gr_db, comp_gr_db;
static float ms_sat_x1[2], ms_sat_F1[2], ms_dc_x[2], ms_dc_y[2], ms_comp_env, ms_lim_g = 1;
static int ms_sat_k1 = -1;                       /* (the kind F1 was worked out for) */
#define MS_LA 64
static float ms_la[MS_LA][2], ms_la_need[MS_LA] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    ms_la_bmin[MS_LA / 8] = {1, 1, 1, 1, 1, 1, 1, 1};
static int ms_la_w;

static void ms_biquad_set(ms_biquad *q, int kind, float hz, float db)
{
    float A = powf(10.0f, db / 40.0f), w = 6.2831853f * hz / 44100.0f, c = cosf(w), sn = sinf(w), b0, b1, b2, a0, a1, a2;
    if (kind == 1) {                             /* bell, Q 0.8 */
        float al = sn / (2 * 0.8f);
        b0 = 1 + al * A, b1 = -2 * c, b2 = 1 - al * A, a0 = 1 + al / A, a1 = -2 * c, a2 = 1 - al / A;
    } else {                                     /* shelves, slope 1 */
        float al = sn / 2 * sqrtf(2.0f), sq = 2 * sqrtf(A) * al;
        if (kind == 0) {
            b0 = A * ((A + 1) - (A - 1) * c + sq), b1 = 2 * A * ((A - 1) - (A + 1) * c), b2 = A * ((A + 1) - (A - 1) * c - sq);
            a0 = (A + 1) + (A - 1) * c + sq, a1 = -2 * ((A - 1) + (A + 1) * c), a2 = (A + 1) + (A - 1) * c - sq;
        } else {
            b0 = A * ((A + 1) + (A - 1) * c + sq), b1 = -2 * A * ((A - 1) + (A + 1) * c), b2 = A * ((A + 1) + (A - 1) * c - sq);
            a0 = (A + 1) - (A - 1) * c + sq, a1 = 2 * ((A - 1) - (A + 1) * c), a2 = (A + 1) - (A - 1) * c - sq;
        }
    }
    q->b0 = b0 / a0, q->b1 = b1 / a0, q->b2 = b2 / a0, q->a1 = a1 / a0, q->a2 = a2 / a0;
}
/* the response of the EQ (with LOW CUT and TILT left out) at hz, in dB: for the curve in the Lab */
static float ms_eq_db(float hz)
{
    float w = 6.2831853f * hz / 44100.0f, db = 0;
    for (int i = 0; i < 3; i++) {
        const ms_biquad *q = &ms_eq[i];
        float c1 = cosf(w), s1 = sinf(w), c2 = cosf(2 * w), s2 = sinf(2 * w);
        float nr = q->b0 + q->b1 * c1 + q->b2 * c2, ni = -(q->b1 * s1 + q->b2 * s2);
        float dr = 1 + q->a1 * c1 + q->a2 * c2, di = -(q->a1 * s1 + q->a2 * s2);
        if (ms_eq_key[i][1] == 0)
            continue;
        db += 10.0f * log10f((nr * nr + ni * ni) / (dr * dr + di * di) + 1e-12f);
    }
    return db;
}
static inline float ms_logcosh(float x)
{
    float a = fabsf(x);
    return a + log1pf(expf(-2 * a)) - 0.69314718f;
}
/* the saturation's curve f and its antiderivative F, for kind k (b: the tube's bias) */
static inline float ms_sat_f(int k, float x)
{
    return k == 1 ? tanhf(x + 0.25f) - 0.24491866f : k == 2 ? x / sqrtf(1 + x * x) : tanhf(x);
}
static inline float ms_sat_F(int k, float x)
{
    return k == 1 ? ms_logcosh(x + 0.25f) - 0.24491866f * x : k == 2 ? sqrtf(1 + x * x) : ms_logcosh(x);
}
static void master_render(float *lr, unsigned n)
{
    const float cut = lab_v[P_LOWCUT], tilt = lab_v[P_TILT] / 100, width = lab_v[P_WIDTH] / 100;
    const float gain = powf(10.0f, lab_v[P_GAIN] / 20.0f), ceil = powf(10.0f, lab_v[P_CEIL] / 20.0f);
    const int limit = lab_v[P_LIMIT] > 0.5f, sk = (int)lab_v[P_MS_SAT_KIND] % 3;
    const float a = cut > 0 ? expf(-6.2831853f * cut / 44100.0f) : 0, tl = 1.0f - expf(-6.2831853f * 800.0f / 44100.0f);
    const float sat = lab_v[P_MS_SAT] / 100, drive = 1 + sat * sat * 9;          /* up to about +20 dB into the curve */
    const float comp = lab_v[P_MS_COMP] / 100, cmix = lab_v[P_MS_COMP_MIX] / 100;
    const float thr = -4.0f - 26.0f * comp, ratio = 1.5f + 2.5f * comp, knee = 6.0f;
    const float makeup = powf(10.0f, -thr * (1 - 1 / ratio) * 0.35f * (comp > 0) / 20.0f);
    /* (0.78) the compressor's level in dB is needed only above its knee: below it a peak compares to the knee's
       foot in linear terms (one powf a block, not a log10f a sample); and with nothing held down the gain is the
       make-up alone (no powf a sample). While it compresses, every sum is the same as before */
    const float knee_foot = powf(10.0f, (thr - knee / 2) / 20.0f);
    const float catt = expf(-1.0f / (lab_v[P_MS_COMP_ATT] * 0.001f * 44100.0f)), crel = expf(-1.0f / (lab_v[P_MS_COMP_REL] * 0.001f * 44100.0f));
    const float lrel = expf(-1.0f / (0.08f * 44100.0f)), latt = 1.0f - expf(-1.0f / (MS_LA * 0.25f));
    const float dcr = 1.0f - 6.2831853f * 8.0f / 44100.0f;
    float sat_norm = 1, comp_max = 0, lim_min = 1, sat_f0 = sk != 2 ? ms_sat_f(sk, 0) : 0;
    static float g_was = -1, d_was = -1, n_was = 1;
    float g_from, d_from, n_from;
    const float eq_db[3] = {lab_v[P_MS_LOW], lab_v[P_MS_MID], lab_v[P_MS_HIGH]};
    const float eq_hz[3] = {lab_v[P_MS_LOW_HZ], lab_v[P_MS_MID_HZ], lab_v[P_MS_HIGH_HZ]};
    for (int i = 0; i < 3; i++)                  /* the EQ's coefficients, only when a value moved */
        if (ms_eq_key[i][0] != eq_hz[i] || ms_eq_key[i][1] != eq_db[i]) {
            ms_biquad_set(&ms_eq[i], i, eq_hz[i], eq_db[i]);
            ms_eq_key[i][0] = eq_hz[i], ms_eq_key[i][1] = eq_db[i];
        }
    if (sat > 0) {                               /* the same level out as in, at about -12 dBFS */
        float t = 0.25f * drive;
        sat_norm = 0.25f / fabsf(ms_sat_f(sk, t) - ms_sat_f(sk, 0));
        sat_norm = sat_norm < 1 ? sat_norm : 1;
    }
    if (g_was < 0)
        g_was = gain, d_was = drive, n_was = sat_norm;
    g_from = g_was, d_from = d_was, n_from = n_was;   /* (gain and drive glide over the block: no zipper when turned) */
    g_was = gain, d_was = drive, n_was = sat_norm;
    for (unsigned k = 0; k < n; k++) {
        float l = lr[2 * k], r = lr[2 * k + 1], u = (float)(k + 1) / (float)n;
        float gk = g_from + (gain - g_from) * u, dk = d_from + (drive - d_from) * u, nk = n_from + (sat_norm - n_from) * u;
        if (cut > 0) {                           /* two one-pole high passes in a row */
            for (int c = 0; c < 2; c++) {
                float x = c ? r : l, y;
                y = a * (ms_hp1[c] + x - ms_hx1[c]);
                ms_hx1[c] = x;
                ms_hp1[c] = y;
                x = y;
                y = a * (ms_hp2[c] + x - ms_hx2[c]);
                ms_hx2[c] = x;
                ms_hp2[c] = y;
                if (c) r = y; else l = y;
            }
        }
        for (int i = 0; i < 3; i++) {            /* the EQ (transposed direct form II) */
            ms_biquad *q = &ms_eq[i];
            if (eq_db[i] == 0)
                continue;
            for (int c = 0; c < 2; c++) {
                float x = c ? r : l, y = q->b0 * x + q->z1[c];
                q->z1[c] = q->b1 * x - q->a1 * y + q->z2[c];
                q->z2[c] = q->b2 * x - q->a2 * y;
                if (c) r = y; else l = y;
            }
        }
        if (tilt != 0) {
            for (int c = 0; c < 2; c++) {
                float x = c ? r : l, lo;
                ms_tilt_lp[c] += (x - ms_tilt_lp[c]) * tl;
                lo = ms_tilt_lp[c];
                x = lo * (1.0f - 0.6f * tilt) + (x - lo) * (1.0f + 0.9f * tilt);
                if (c) r = x; else l = x;
            }
        }
        if (sat > 0) {                           /* first-order antiderivative anti-aliasing: (F(x)-F(x1))/(x-x1);
                                                    F(x1) kept from the sample before: one F a sample, not two */
            if (ms_sat_k1 != sk) {
                ms_sat_F1[0] = ms_sat_F(sk, ms_sat_x1[0]), ms_sat_F1[1] = ms_sat_F(sk, ms_sat_x1[1]);
                ms_sat_k1 = sk;
            }
            for (int c = 0; c < 2; c++) {
                float x = (c ? r : l) * dk, x1 = ms_sat_x1[c], d = x - x1, y, Fx = ms_sat_F(sk, x);
                y = fabsf(d) > 1e-4f ? (Fx - ms_sat_F1[c]) / d : ms_sat_f(sk, 0.5f * (x + x1));
                y -= sat_f0;
                ms_sat_x1[c] = x, ms_sat_F1[c] = Fx;
                y *= nk;
                if (sk == 1) {                   /* (the tube's even harmonics leave a little DC: blocked) */
                    float o = y - ms_dc_x[c] + dcr * ms_dc_y[c];
                    ms_dc_x[c] = y, ms_dc_y[c] = o, y = o;
                }
                if (c) r = y; else l = y;
            }
        }
        if (comp > 0) {                          /* glue: stereo linked, peak sensing, soft knee */
            float pk = fmaxf(fabsf(l), fabsf(r)), gr = 0, g;
            if (pk + 1e-9f > knee_foot) {        /* (above the knee's foot: the level in dB, as always) */
                float db = 20.0f * log10f(pk + 1e-9f), over = db - thr;
                gr = over <= -knee / 2 ? 0 : over >= knee / 2 ? over * (1 - 1 / ratio)
                                                           : (1 - 1 / ratio) * (over + knee / 2) * (over + knee / 2) / (2 * knee);
            }
            ms_comp_env = gr > ms_comp_env ? gr + (ms_comp_env - gr) * catt : gr + (ms_comp_env - gr) * crel;
            if (ms_comp_env < 1e-6f)             /* (nothing held down: below a millionth of a dB, gain 1 to -139 dB) */
                ms_comp_env = 0, g = makeup;
            else
                g = powf(10.0f, -ms_comp_env / 20.0f) * makeup;
            l = l * (1 - cmix) + l * g * cmix;
            r = r * (1 - cmix) + r * g * cmix;
            if (ms_comp_env > comp_max)
                comp_max = ms_comp_env;
        }
        if (width != 1.0f) {
            float m = (l + r) * 0.5f, s2 = (l - r) * 0.5f * width;
            l = m + s2;
            r = m - s2;
        }
        l *= gk;
        r *= gk;
        if (limit) {                             /* look-ahead: the sample waits MS_LA steps; the gain sees it now */
            float pk = fmaxf(fabsf(l), fabsf(r)), need = pk > ceil ? ceil / pk : 1.0f, gmin, ol, orr;
            int j = ms_la_w, blk = j >> 3;
            ms_la_need[j] = need;
            {   /* the window's minimum from eight block minima: this sample's block anew, the other seven kept
                   (was 64 looks a sample) */
                float bm = ms_la_need[blk * 8];
                for (int i = 1; i < 8; i++)
                    bm = ms_la_need[blk * 8 + i] < bm ? ms_la_need[blk * 8 + i] : bm;
                ms_la_bmin[blk] = bm;
                gmin = ms_la_bmin[0];
                for (int i = 1; i < MS_LA / 8; i++)
                    gmin = ms_la_bmin[i] < gmin ? ms_la_bmin[i] : gmin;
            }
            ms_lim_g = gmin < ms_lim_g ? ms_lim_g + (gmin - ms_lim_g) * latt   /* down within the look-ahead */
                                       : gmin + (ms_lim_g - gmin) * lrel;  /* back up, gliding (80 ms) */
            j = (j + 1) % MS_LA;
            ol = ms_la[j][0] * ms_lim_g, orr = ms_la[j][1] * ms_lim_g;   /* the oldest sample, out */
            ms_la[j][0] = l, ms_la[j][1] = r;
            ms_la_w = j;
            l = ol > ceil ? ceil : ol < -ceil ? -ceil : ol;
            r = orr > ceil ? ceil : orr < -ceil ? -ceil : orr;
            if (ms_lim_g < lim_min)
                lim_min = ms_lim_g;
        }
        lr[2 * k] = l;
        lr[2 * k + 1] = r;
    }
    lim_gr_db = lim_min < 1 ? -20.0f * log10f(lim_min) : 0;
    comp_gr_db = comp_max;
}
/* the guard at the very end (always, MASTER or not): nothing below -0.5 dBFS changes; above, a soft knee up to 0 dBFS
   instead of the card's hard clip */
static void master_guard(float *lr, unsigned n)
{
    const float t = 0.944f, room = 1.0f - 0.944f;
    for (unsigned k = 0; k < 2 * n; k++) {
        float x = lr[k], a = fabsf(x);
        if (a > t) {
            float o = t + room * tanhf((a - t) / room);
            lr[k] = x < 0 ? -o : o;
        }
    }
}

#include "brick_samples.inc"
#include "brick_poly.inc"
#include "brick_voices.inc"

/* -------------------------------------------- Mutable Instruments --- */
static float pa_level, xm_level, xm_in_level[2];
static const float LAB_PA_GAIN[4] = {2.5f, 2.5f, 2.5f, 1.4f};   /* granular, stretch, looping, spectral */
static float be_level, be_filled;                /* be_filled: seconds recorded since the start / a media change */
static int be_cleared = 1, be_last_media = -1, be_last_mono = -1;
static int pa_cleared = 1;
/* RANDOM: Marbles' deja vu sequence plays a track in the Biome's key */
#define RN_HIST 16
static double rn_clock;
static int rn_note = -1, rn_track = 0, rn_hist[RN_HIST], rn_hist_on[RN_HIST], rn_hist_w;
static uint32_t rn_off_at, rn_frames, rn_step_count;
static int rn_was_playing;
static void rn_note_off(void)
{
    if (rn_note >= 0) {
        if (rn_track == 6)
            po_note_off(rn_note);
        else
            dream_cmd(MD_NOTE_OFF, rn_track, rn_note);
    }
    rn_note = -1;
}
static void rn_run(uint32_t n)
{
    const md_patch *p = md_state();
    int playing = md_playing() && lab_v[P_RN_ON] > 0.5f;
    double step = 44100.0 * 60.0 / (double)(md_tempo_now() > 20 ? md_tempo_now() : 20) * LAB_RATE_BEATS[(int)lab_v[P_RN_RATE]];
    rn_frames += n;
    if (rn_note >= 0 && (int32_t)(rn_frames - rn_off_at) >= 0)
        rn_note_off();
    if (!playing) {
        if (rn_was_playing)
            rn_note_off();
        rn_was_playing = 0;
        rn_clock = 0;
        return;
    }
    rn_was_playing = 1;
    rn_clock -= (double)n;
    while (rn_clock <= 0) {
        mi_random_params rp = {lab_v[P_RN_DEJAVU] / 100, lab_v[P_RN_SPREAD] / 100, lab_v[P_RN_BIAS] / 100,
                               lab_v[P_RN_PROB] / 100, (int)lab_v[P_RN_LENGTH]};
        float v;
        int on = mi_random_step(&rp, &v);
        rn_clock += step;
        rn_step_count++;
        rn_hist_on[rn_hist_w % RN_HIST] = on;
        if (on) {
            int range = (int)lab_v[P_RN_RANGE] * 12, base = 48 + (int)(p->root % 12) + 12 * dream_ui.octave;
            int note = md_quantize(base + (int)lrintf(v * (float)range), p->root, p->scale);
            note = note < 24 ? 24 : note > 96 ? 96 : note;
            rn_note_off();
            rn_track = (int)lab_v[P_RN_TRACK];
            if (rn_track == 6) {                 /* RANDOM plays the POLY voice */
                if (lab_v[P_PO_LEVEL] > 0)
                    po_note_on(note, 0);
                rn_note = note;
            } else if (p->kit.enabled && rn_track == 3)
                dream_cmd(MD_KIT_TRIGGER, 3, (note - base) & 7);
            else {
                dream_cmd(MD_TRIGGER, rn_track, note);
                rn_note = note;
            }
            rn_off_at = rn_frames + (uint32_t)(step * lab_v[P_RN_GATE] / 100);
            rn_hist[rn_hist_w % RN_HIST] = note;
        } else
            rn_hist[rn_hist_w % RN_HIST] = -1;
        rn_hist_w++;
    }
}

static void sy_note_on(int note, int vel);      /* brick_synth.inc */
#include "brick_logic.inc"
#include "brick_synth.inc"
#include "brick_fxrack.inc"
#include "brick_grids.inc"

/* ------------------------------------------------------- the chain --- */
#define LAB_SCOPE 1024
static float lab_scope[LAB_SCOPE], lab_peak;
static uint32_t lab_scope_w;
static float lab_stem_level[6];
/* the stems: int16 at the engine's fixed stem level; this brings a full send to about the track's own level in the
 * mix (measured: tests/stem_calibration.txt) */
static float LAB_STEM_K = 1.6e-5f;            /* measured 1.54e-5 over 30 s of Velvet Mist */

static int lab_sample_send(int mix_send)          /* the SAMPLES send that belongs to a module's mix send */
{
    return mix_send == P_MIX_CL ? P_SA_TO_CL : mix_send == P_MIX_PA ? P_SA_TO_PA : mix_send == P_MIX_BE ? P_SA_TO_BE : P_SA_TO_SH;
}
static int lab_auto_input(int first_track_send, int mix_send)
{
    for (int t = 0; t < 6; t++)
        if (lab_v[first_track_send + t] > 0)
            return 0;
    if (lab_v[lab_sample_send(mix_send)] > 0 && lab_v[P_SA_LEVEL] > 0)
        return 0;
    if (lab_v[lab_sample_send(mix_send) - P_SA_TO_CL + P_PO_TO_CL] > 0 && lab_v[P_PO_LEVEL] > 0)
        return 0;
    if (lab_v[lab_sample_send(mix_send) - P_SA_TO_CL + P_VO_TO_CL] > 0 && lab_v[P_VO_LEVEL] > 0)
        return 0;
    if (lab_v[lab_sample_send(mix_send) - P_SA_TO_CL + P_SY_TO_CL] > 0 && lab_v[P_SY_LEVEL] > 0)
        return 0;
    return lab_v[mix_send] <= 0;
}

static int lab_active(int m)
{
    switch (m) {
    case M_TEX: return lab_v[P_FOREST] > 0 || lab_v[P_AIR] > 0 || lab_v[P_WATER] > 0 || lab_v[P_STARDUST] > 0 || lab_v[P_PULSE] > 0;
    case M_CLOUD: return lab_v[P_CL_MIX] > 0;
    case M_SHIMMER: return lab_v[P_SH_MIX] > 0;
    case M_PARTICLES: return lab_v[P_PA_MIX] > 0;
    case M_SAMPLES: return lab_v[P_SA_LEVEL] > 0;
    case M_POLY: return lab_v[P_PO_LEVEL] > 0;
    case M_VOICES: return lab_v[P_VO_LEVEL] > 0;
    case M_SYNTH: return lab_v[P_SY_LEVEL] > 0;
    case M_GRIDS: return lab_v[P_GR_ON] > 0.5f;
    case M_RACK_A: case M_RACK_B: {
        int r = m == M_RACK_B, in = 0;
        for (int s = 0; s < 4; s++)
            in |= lab_v[(r ? P_RB_IN_VOICES : P_RA_IN_VOICES) + s] > 0.5f;
        return in;
    }
    case M_SEEDS: return lab_v[P_BE_MIX] > 0;
    case M_CROSSMOD: return lab_v[P_XM_MIX] > 0;
    case M_RANDOM: return lab_v[P_RN_ON] > 0.5f;
    case M_LOGIC: return lab_v[P_LG_ON] > 0.5f;
    case M_COLOUR: return lab_v[P_ENS_MIX] > 0 || lab_v[P_TAPE_DRIVE] > 0 || lab_v[P_TAPE_WOW] > 0 || lab_v[P_TAPE_AGE] > 0;
    case M_MASTER: return lab_v[P_LOWCUT] > 0 || lab_v[P_MS_SAT] > 0 || lab_v[P_TILT] != 0 || lab_v[P_WIDTH] != 100 || lab_v[P_GAIN] != 0 || lab_v[P_LIMIT] > 0.5f ||
                          lab_v[P_MS_LOW] != 0 || lab_v[P_MS_MID] != 0 || lab_v[P_MS_HIGH] != 0 || lab_v[P_MS_SAT] > 0 || lab_v[P_MS_COMP] > 0;
    }
    return 0;
}

static void lab_process(float *lr, const int16_t (*stems)[FM1B_UAC_CH], uint32_t n)
{
    static float cin[APP_BLOCK], sin_[APP_BLOCK], wet[2 * APP_BLOCK];
    int cloud = lab_active(M_CLOUD), shim = lab_active(M_SHIMMER);
    uint32_t k;
    int t;
    for (t = 0; t < 6; t++) {                    /* the tracks' levels, for the SENDS picture */
        float pk = 0;
        for (k = 0; k < n; k += 4) {
            float v = fabsf((float)stems[k][2 + t] * LAB_STEM_K);
            if (v > pk) pk = v;
        }
        lab_stem_level[t] = pk > lab_stem_level[t] ? pk : lab_stem_level[t] * 0.9f;
    }
#ifdef BK_STEMCAL
    {   /* calibration: the mix against the sum of the six stems, as RMS */
        static double mix2, stem2;
        static uint32_t blocks;
        for (k = 0; k < n; k++) {
            float s = 0;
            for (t = 0; t < 6; t++)
                s += (float)stems[k][2 + t];
            mix2 += (double)(lr[2 * k] + lr[2 * k + 1]) * 0.5 * (lr[2 * k] + lr[2 * k + 1]) * 0.5;
            stem2 += (double)s * s;
        }
        if (++blocks % 1720u == 0)
            printf("stemcal: mix rms %.5f, stem-sum rms %.1f, k = %.3g\n", sqrt(mix2 / (blocks * n)),
                   sqrt(stem2 / (blocks * n)), sqrt(mix2 / (blocks * n)) / sqrt(stem2 / (blocks * n) + 1e-9));
    }
#endif
    if (lab_active(M_TEX))
        tex_render(lr, n);
    rn_run(n);
    lg_run(n);
    gr_run(n);
    sa_active = lab_active(M_SAMPLES) && sa_render(n);
    if (!sa_active)
        sa_level = 0;
    sy_active = lab_active(M_SYNTH) && sy_render(n);
    if (!sy_active)
        sy_level = 0;
    vo_active = lab_active(M_VOICES) && vo_render(n);
    if (!vo_active)
        vo_level = vo_env = 0;
    if (lab_active(M_POLY))
        po_active = po_render(n);
    else if (po_active) {
        memset(po_v, 0, sizeof po_v);            /* LEVEL 0: every voice quiet at once */
        po_active = 0;
    }
    if (!po_active)
        po_level = 0;
    {   /* the FX RACKs over the sources sent into them, then everything into the mix */
        float *const bufs[4] = {vo_buf, sa_buf, sy_buf, po_buf};
        const int act[4] = {vo_active, sa_active, sy_active, po_active};
        if (lab_active(M_RACK_A) || lab_active(M_RACK_B))
            fx_run(bufs, act, n);
        for (int s = 0; s < 4; s++)
            if (act[s])
                for (k = 0; k < 2 * n; k++)
                    lr[k] += bufs[s][k];
    }
    if (lab_active(M_CROSSMOD)) {               /* one source cross-modulates another (Warps), mono, centred */
        static float car[APP_BLOCK], mod[APP_BLOCK], xo[APP_BLOCK];
        int cs = (int)lab_v[P_XM_CARRIER], ms = (int)lab_v[P_XM_MOD];
        float mix = lab_v[P_XM_MIX] / 100, lv0 = 0, lv1 = 0, lvo = 0, g = LAB_STEM_K * 1.6f;
        mi_crossmod_params xp = {lab_v[P_XM_ALGO] / 100, lab_v[P_XM_TIMBRE] / 100, lab_v[P_XM_DRIVE1] / 100,
                                 lab_v[P_XM_DRIVE2] / 100};
        for (k = 0; k < n; k++) {
            float m = (lr[2 * k] + lr[2 * k + 1]) * 0.5f;
            car[k] = cs >= 6 ? m : (float)stems[k][2 + cs] * g;
            mod[k] = ms >= 6 ? m : (float)stems[k][2 + ms] * g;
            lv0 += car[k] * car[k];
            lv1 += mod[k] * mod[k];
        }
        mi_crossmod_process(&xp, car, mod, xo, n);
        for (k = 0; k < n; k++) {               /* without the DC the rectifying algorithms leave (~20 Hz high pass) */
            static float hx, hy;
            hy = 0.9972f * (hy + xo[k] - hx);
            hx = xo[k];
            xo[k] = hy;
        }
        for (k = 0; k < n; k++) {
            lr[2 * k] += xo[k] * mix * 2.2f;      /* (levels: tools/labdiff.py, 100 % near the dry mix) */
            lr[2 * k + 1] += xo[k] * mix * 2.2f;
            lvo += xo[k] * xo[k];
        }
        xm_in_level[0] = sqrtf(lv0 / (float)n);
        xm_in_level[1] = sqrtf(lv1 / (float)n);
        xm_level = sqrtf(lvo / (float)n);
    } else
        xm_level = 0;
    if (lab_active(M_PARTICLES)) {              /* Clouds' granular processor on the sends */
        static float pin[2 * APP_BLOCK], pout[2 * APP_BLOCK];
        float sp[6], mp = lab_v[P_MIX_PA] / 100, mix = lab_v[P_PA_MIX] / 100, lvl = 0;
        int any = 0;
        mi_particles_params pp = {(int)lab_v[P_PA_MODE], lab_v[P_PA_LOFI] > 0.5f, lab_v[P_PA_FREEZE] > 0.5f,
                                  lab_v[P_PA_POS] / 100, lab_v[P_PA_SIZE] / 100, lab_v[P_PA_PITCH],
                                  lab_v[P_PA_DENS] / 100, lab_v[P_PA_TEXTURE] / 100, lab_v[P_PA_SPREAD] / 100,
                                  lab_v[P_PA_FB] / 100, lab_v[P_PA_REVERB] / 100};
        for (t = 0; t < 6; t++) {
            sp[t] = lab_v[P_SP1 + t] / 100 * LAB_STEM_K;
            any |= sp[t] > 0;
        }
        if (lab_auto_input(P_SP1, P_MIX_PA))
            mp = 1;                              /* no send set: the whole mix */
        (void)any;
        for (k = 0; k < n; k++) {
            float s0 = 0, ss0 = sa_active ? lab_v[P_SA_TO_PA] / 100 : 0, ps0 = po_active ? lab_v[P_PO_TO_PA] / 100 : 0;
            float vs0 = vo_active ? lab_v[P_VO_TO_PA] / 100 : 0;
            for (t = 0; t < 6; t++)
                s0 += (float)stems[k][2 + t] * sp[t];
            s0 += (vo_buf[2 * k] + vo_buf[2 * k + 1]) * 0.5f * vs0;
            s0 += sy_active ? (sy_buf[2 * k] + sy_buf[2 * k + 1]) * 0.5f * lab_v[P_SY_TO_PA] / 100 : 0;
            pin[2 * k] = lr[2 * k] * mp + s0 + sa_buf[2 * k] * ss0 + po_buf[2 * k] * ps0;
            pin[2 * k + 1] = lr[2 * k + 1] * mp + s0 + sa_buf[2 * k + 1] * ss0 + po_buf[2 * k + 1] * ps0;
        }
        mi_particles_process(&pp, pin, pout, n);
        for (k = 0; k < 2 * n; k++) {
            pout[k] *= LAB_PA_GAIN[(int)lab_v[P_PA_MODE] & 3];   /* (levels: tools/labdiff.py) */
            lr[k] += pout[k] * mix;
            lvl += pout[k] * pout[k];
        }
        pa_level = sqrtf(lvl / (float)(2 * n));
        pa_cleared = 0;
    } else if (!pa_cleared) {
        mi_particles_init();                     /* off: the next start begins from silence */
        pa_level = 0;
        pa_cleared = 1;
    }
    if (lab_active(M_SEEDS)) {                  /* Beads' granular processor on the sends */
        static float bin[2 * APP_BLOCK], bout[2 * APP_BLOCK];
        float sb[6], mb = lab_v[P_MIX_BE] / 100, mix = lab_v[P_BE_MIX] / 100, lvl = 0;
        int any = 0, media = (int)lab_v[P_BE_MEDIA] & 3, mono = lab_v[P_BE_MONO] > 0.5f;
        mi_beads_params bp = {media, mono, lab_v[P_BE_FREEZE] > 0.5f,
                              lab_v[P_BE_DENS] / 100, lab_v[P_BE_TIME] / 100, lab_v[P_BE_SIZE] / 100,
                              lab_v[P_BE_SHAPE] / 100, lab_v[P_BE_PITCH],
                              lab_v[P_BE_TIME_RND] / 100, lab_v[P_BE_SIZE_RND] / 100, lab_v[P_BE_SHAPE_RND] / 100,
                              lab_v[P_BE_PITCH_RND] / 100, lab_v[P_BE_FB] / 100, lab_v[P_BE_REVERB] / 100};
        if (media != be_last_media || mono != be_last_mono) {
            be_filled = 0;                       /* a new medium starts its memory empty */
            be_last_media = media;
            be_last_mono = mono;
        }
        for (t = 0; t < 6; t++) {
            sb[t] = lab_v[P_SB1 + t] / 100 * LAB_STEM_K;
            any |= sb[t] > 0;
        }
        if (lab_auto_input(P_SB1, P_MIX_BE))
            mb = 1;                              /* no send set: the whole mix */
        (void)any;
        for (k = 0; k < n; k++) {
            float s0 = 0, ss0 = sa_active ? lab_v[P_SA_TO_BE] / 100 : 0, ps0 = po_active ? lab_v[P_PO_TO_BE] / 100 : 0;
            float vs0 = vo_active ? lab_v[P_VO_TO_BE] / 100 : 0;
            for (t = 0; t < 6; t++)
                s0 += (float)stems[k][2 + t] * sb[t];
            s0 += (vo_buf[2 * k] + vo_buf[2 * k + 1]) * 0.5f * vs0;
            s0 += sy_active ? (sy_buf[2 * k] + sy_buf[2 * k + 1]) * 0.5f * lab_v[P_SY_TO_BE] / 100 : 0;
            bin[2 * k] = (lr[2 * k] * mb + s0 + sa_buf[2 * k] * ss0 + po_buf[2 * k] * ps0) * 4.0f;   /* Beads: line level */
            bin[2 * k + 1] = (lr[2 * k + 1] * mb + s0 + sa_buf[2 * k + 1] * ss0 + po_buf[2 * k + 1] * ps0) * 4.0f;
        }
        mi_beads_process(&bp, bin, bout, n);
        for (k = 0; k < 2 * n; k++) {
            bout[k] *= 0.25f * (lab_v[P_BE_MONO] > 0.5f ? 4.5f : 3.0f) * (lab_v[P_BE_SIZE] >= 99.0f ? 0.3f : 1.0f);   /* (levels: tools/labdiff.py; the delay mode is far denser) */
            lr[k] += bout[k] * mix;
            lvl += bout[k] * bout[k];
        }
        be_level = sqrtf(lvl / (float)(2 * n));
        if (!bp.freeze)
            be_filled += (float)n / 44100.0f;
        be_cleared = 0;
    } else if (!be_cleared) {
        mi_beads_init();
        be_level = be_filled = 0;
        be_last_media = be_last_mono = -1;
        be_cleared = 1;
    }
    if (cloud || shim) {
        float sc[6], ss[6], mc = lab_v[P_MIX_CL] / 100, msh = lab_v[P_MIX_SH] / 100;
        for (t = 0; t < 6; t++) {
            sc[t] = lab_v[P_SC1 + t] / 100 * LAB_STEM_K;
            ss[t] = lab_v[P_SS1 + t] / 100 * LAB_STEM_K;
        }
        /* no send set at all: the module listens to the whole mix (so turning up MIX is always heard) */
        if (lab_auto_input(P_SC1, P_MIX_CL))
            mc = 1;
        if (lab_auto_input(P_SS1, P_MIX_SH))
            msh = 1;
        for (k = 0; k < n; k++) {
            float m = (lr[2 * k] + lr[2 * k + 1]) * 0.5f, c = m * mc, s = m * msh;
            for (t = 0; t < 6; t++) {
                float x = (float)stems[k][2 + t];
                c += x * sc[t];
                s += x * ss[t];
            }
            if (sa_active) {                     /* SAMPLES into the cloud and the shimmer */
                float sm = (sa_buf[2 * k] + sa_buf[2 * k + 1]) * 0.5f;
                c += sm * lab_v[P_SA_TO_CL] / 100;
                s += sm * lab_v[P_SA_TO_SH] / 100;
            }
            if (po_active) {                     /* and POLY */
                float pm = (po_buf[2 * k] + po_buf[2 * k + 1]) * 0.5f;
                c += pm * lab_v[P_PO_TO_CL] / 100;
                s += pm * lab_v[P_PO_TO_SH] / 100;
            }
            if (vo_active) {                     /* and VOICES */
                float vm = (vo_buf[2 * k] + vo_buf[2 * k + 1]) * 0.5f;
                c += vm * lab_v[P_VO_TO_CL] / 100;
                s += vm * lab_v[P_VO_TO_SH] / 100;
            }
            if (sy_active) {                     /* and SYNTH */
                float ym = (sy_buf[2 * k] + sy_buf[2 * k + 1]) * 0.5f;
                c += ym * lab_v[P_SY_TO_CL] / 100;
                s += ym * lab_v[P_SY_TO_SH] / 100;
            }
            cin[k] = c;
            sin_[k] = s;
        }
        memset(wet, 0, sizeof(float) * 2 * n);
        if (cloud) {
            float mix = lab_v[P_CL_MIX] / 100 * 2.0f;   /* (levels: tools/labdiff.py) */
            cl_render(cin, wet, n);
            for (k = 0; k < n; k++) {             /* the cloud into the shimmer too: grains that bloom */
                sin_[k] += (wet[2 * k] + wet[2 * k + 1]) * 0.25f;
                lr[2 * k] += wet[2 * k] * mix;
                lr[2 * k + 1] += wet[2 * k + 1] * mix;
            }
        } else if (!cl_cleared) {
            memset(cl_g, 0, sizeof cl_g);
            cl_level = 0;
            cl_cleared = 1;
        }
        if (shim) {
            float mix = lab_v[P_SH_MIX] / 100 * 0.25f;  /* (levels: tools/labdiff.py: the tail is long and dense) */
            memset(wet, 0, sizeof(float) * 2 * n);
            sh_render(sin_, wet, n);
            for (k = 0; k < 2 * n; k++)
                lr[k] += wet[k] * mix;
        } else if (!sh_cleared) {
            memset(sh_line, 0, sizeof sh_line);
            memset(sh_damp, 0, sizeof sh_damp);
            sh_hp_x = sh_hp_y = sh_in_x = sh_in_y = 0;
            sh_level = 0;
            sh_cleared = 1;
        }
    }
    if (lab_active(M_COLOUR))
        colour_render(lr, n);
    if (lab_active(M_MASTER))
        master_render(lr, n);
    else
        lim_gr_db = comp_gr_db = 0;
    master_guard(lr, n);
    {
        float pk = 0;
        for (k = 0; k < n; k++) {
            float v = (lr[2 * k] + lr[2 * k + 1]) * 0.5f;
            lab_scope[lab_scope_w++ % LAB_SCOPE] = v;
            pk = fmaxf(pk, fmaxf(fabsf(lr[2 * k]), fabsf(lr[2 * k + 1])));
        }
        lab_peak = pk > lab_peak ? pk : lab_peak * 0.93f;
    }
}

/* ------------------------------------------------------- settings --- */
static void lab_defaults(void)
{
    for (int i = 0; i < P_COUNT; i++)
        lab_v[i] = LAB_P[i].def;
}
static void lab_init(void)
{
    int i;
    for (i = 0; i <= LAB_TAB; i++)
        lab_sin_tab[i] = sinf(6.2831853f * (float)i / LAB_TAB);
    for (i = 0; i <= WIN_N; i++)
        lab_win[i] = 0.5f - 0.5f * cosf(6.2831853f * (float)i / WIN_N);
    tex.seed[0] = 0x6d2b79f5u;
    tex.seed[1] = 0x1b873593u;
    tex.rustle = tex.rustle_target = 0.4f;
    lab_defaults();
    mi_particles_init();
    mi_crossmod_init();
    mi_beads_init();
    mi_plaits_init();
    mi_random_init(0x4D445242u);
}
#define LAB_MAGIC 0x424C444Du                   /* "MDLB" */
static void lab_path(char *out, size_t n, const char *suffix) { snprintf(out, n, "%s/lab.bin%s", bk_dir, suffix); }
/* what stays the same for every Biome: the output (MASTER) and the per-Biome switches themselves */
static int lab_is_global(int p) { return LAB_P[p].mod == M_MASTER; }

/* a settings file into lab_v: everything, or (biome_file) all but the global parameters; and its sample name */
static int lab_read_file(const char *path, int biome_file, char *sample, size_t sample_n)
{
    FILE *f;
    uint32_t hdr[2];
    int ok = 0;
    if (sample && sample_n)
        sample[0] = 0;
    if (!(f = fopen(path, "rb")))
        return 0;
    if (fread(hdr, 4, 2, f) == 2 && hdr[0] == LAB_MAGIC && hdr[1] <= P_COUNT) {
        float v[P_COUNT];
        if (fread(v, 4, hdr[1], f) == hdr[1]) {
            for (uint32_t i = 0; i < hdr[1]; i++)
                if (v[i] >= LAB_P[i].lo && v[i] <= LAB_P[i].hi && !(biome_file && lab_is_global((int)i)))
                    lab_v[i] = v[i];
            ok = 1;
            if (sample && sample_n) {           /* (a Biome's file ends with its sample's name) */
                char nm[64];
                if (fread(nm, 1, 64, f) == 64) {
                    nm[63] = 0;
                    snprintf(sample, sample_n, "%s", nm);
                }
            }
        }
    }
    fclose(f);
    return ok;
}
static void lab_write_file(const char *path, const char *sample)
{
    char tmp[700];
    FILE *f;
    uint32_t hdr[2] = {LAB_MAGIC, P_COUNT};
    snprintf(tmp, sizeof tmp, "%s.new", path);
    if (!(f = fopen(tmp, "wb")))
        return;
    fwrite(hdr, 4, 2, f);
    fwrite(lab_v, 4, P_COUNT, f);
    if (sample) {
        char nm[64] = {0};
        snprintf(nm, sizeof nm, "%s", sample);
        fwrite(nm, 1, 64, f);
    }
    fflush(f);
#ifdef __linux__
    fsync(fileno(f));
#endif
    fclose(f);
    rename(tmp, path);
}
static void lab_load(void)
{
    char path[600];
    if (!bk_dir[0])
        return;
    lab_path(path, sizeof path, "");
    lab_read_file(path, 0, 0, 0);
}
static void lab_save_global(void)
{
    char path[600];
    if (!bk_dir[0])
        return;
    lab_path(path, sizeof path, "");
    lab_write_file(path, 0);
}
/* the shared file: all of lab_v when the shared Lab is in use; else only its global parameters brought up to date */
static void lab_save_global_master(void)
{
    char path[600];
    float now[P_COUNT];
    if (!bk_dir[0])
        return;
    if (!lb_own_in_use()) {
        lab_save_global();
        return;
    }
    memcpy(now, lab_v, sizeof now);
    lab_path(path, sizeof path, "");
    lab_read_file(path, 0, 0, 0);                     /* the shared values ... */
    for (int p = 0; p < P_COUNT; p++)
        if (lab_is_global(p))
            lab_v[p] = now[p];                   /* ... with today's output settings */
    lab_v[P_LB_OWN] = 0;
    lab_save_global();
    memcpy(lab_v, now, sizeof now);
}
static void lb_save_current(void);               /* (brick_labbiome.inc: the Biome's own file, or the shared one) */
static void lab_save(void)
{
    lab_dirty = 0;
    if (!bk_dir[0])
        return;
    lb_save_current();
}
static void lb_set_own(int own);
static void lb_set_follow(int on);
static void lb_finish(void);
static void lab_set(int p, float v)
{
    lb_finish();                                 /* an edit during a Biome change: the change completes first */
    v = v < LAB_P[p].lo ? LAB_P[p].lo : v > LAB_P[p].hi ? LAB_P[p].hi : v;
    if (p == P_LB_OWN) {                         /* not a value: a Biome's own file made or given up */
        lb_set_own(v > 0.5f);
        return;
    }
    if (p == P_LB_FOLLOW) {
        lb_set_follow(v > 0.5f);
        return;
    }
    if (p == P_VO_FILE) {                        /* only files that are there */
        if (vo_nfiles < 0)
            vo_scan();
        v = vo_nfiles <= 0 ? 0 : v > (float)(vo_nfiles - 1) ? (float)(vo_nfiles - 1) : v;
    }
    if (p == P_SA_FILE) {                        /* only files that are there */
        if (sa_nfiles < 0)
            sa_scan();
        v = sa_nfiles <= 0 ? 0 : v > (float)(sa_nfiles - 1) ? (float)(sa_nfiles - 1) : v;
    }
    if (v == lab_v[p])
        return;
    lab_v[p] = v;
    if (p == P_MS_PRESET)                        /* MASTER's sound: the chain set at once */
        ms_preset((int)v);
    else if (LAB_P[p].mod == M_MASTER && p != P_LB_TIME)
        lab_v[P_MS_PRESET] = 0;                  /* (shaped by hand: OWN) */
    lab_dirty = 1;
    lab_changed_at = bk_ms();
}
static void lb_check(void);
static void lab_keep(void)                       /* saved 1.5 s after the last edit; a Biome change followed */
{
    lb_check();
    if (lab_dirty && bk_ms() - lab_changed_at > 1500u)
        lab_save();
}
static void lab_format(int p, char *buf, size_t n)
{
    float v = lab_v[p];
    switch (LAB_P[p].unit) {
    case U_PCT:
        if (p == P_RA_CUTOFF || p == P_RB_CUTOFF) {   /* (the filter's cutoff in Hz) */
            float hz = fx_cutoff_hz(v);
            hz >= 1000 ? snprintf(buf, n, "%.1f kHz", hz / 1000.0f) : snprintf(buf, n, "%d Hz", (int)hz);
        } else
            snprintf(buf, n, "%d %%", (int)lrintf(v));
        break;
    case U_BIPCT: snprintf(buf, n, "%+d", (int)lrintf(v)); break;
    case U_MS: snprintf(buf, n, "%d ms", (int)lrintf(v)); break;
    case U_SEMI: snprintf(buf, n, "%+d st", (int)lrintf(v)); break;
    case U_DB: snprintf(buf, n, "%+.1f dB", v); break;
    case U_HZ: v > 0 ? snprintf(buf, n, "%d Hz", (int)lrintf(v)) : snprintf(buf, n, "OFF"); break;
    case U_ONOFF: snprintf(buf, n, "%s", v > 0.5f ? "ON" : "OFF"); break;
    case U_PERS: snprintf(buf, n, "%d / s", (int)lrintf(v)); break;
    case U_NUM: snprintf(buf, n, LAB_P[p].lo < 0 ? "%+d" : "%d", (int)lrintf(v)); break;
    case U_ENUM:
        if (p == P_MS_PRESET)
            snprintf(buf, n, "%s", MS_PRESETS[(int)v % 6]);
        else if (p == P_MS_SAT_KIND)
            snprintf(buf, n, "%s", MS_SAT_KINDS[(int)v % 3]);
        else if (p == P_PA_MODE)
            snprintf(buf, n, "%s", LAB_PA_MODES[(int)v & 3]);
        else if (p == P_BE_MEDIA)
            snprintf(buf, n, "%s", LAB_BE_MEDIA[(int)v & 3]);
        else if (p == P_PO_ENGINE)
            snprintf(buf, n, "%s", PO_ENGINES[(int)v % 24]);
        else if (p == P_PO_FOLLOW)
            (int)v ? snprintf(buf, n, "%d %s", (int)v, md_model_name(md_state()->track[(int)v - 1].model)) : snprintf(buf, n, "OFF");
        else if (p == P_PO_MIDI)
            (int)v == 0 ? snprintf(buf, n, "OFF") : (int)v == 1 ? snprintf(buf, n, "ALL CHANNELS") : snprintf(buf, n, "CHANNEL %d", (int)v - 1);
        else if (p == P_LB_OWN)
            snprintf(buf, n, "%s", (int)v ? "OWN SOUND LAB" : "SHARED");
        else if (p == P_RN_TRACK && (int)v == 6)
            snprintf(buf, n, "POLY");
        else if (p == P_SA_MODE)
            snprintf(buf, n, "%s", (int)v ? "FOLLOW TRACK" : "LOOP");
        else if (p == P_VO_MODE)
            snprintf(buf, n, "%s", VC_MODES[clamp((int)v, 0, 4)]);
        else if (p == P_VO_TRIGGER)
            (int)v == 0 ? snprintf(buf, n, "FREE") : (int)v == 7 ? snprintf(buf, n, "EVERY BAR") : (int)v == 8 ? snprintf(buf, n, "MARBLES") : snprintf(buf, n, "TRACK %d HITS", (int)v);
        else if (p == P_VO_TIDE_TO)
            snprintf(buf, n, "%s", VO_TIDE_TO[(int)v % 6]);
        else if (p == P_VO_FILE) {
            if (vo_nfiles < 0)
                vo_scan();
            if (vo_nfiles <= 0)
                snprintf(buf, n, "NO FILES");
            else {
                char nm[128];
                vo_display_name((int)v < vo_nfiles ? (int)v : 0, nm, sizeof nm);
                if (strlen(nm) > 22)
                    nm[20] = '.', nm[21] = '.', nm[22] = 0;
                snprintf(buf, n, "%s", nm);
            }
        }
        else if (p == P_SA_FILE) {
            if (sa_nfiles < 0)
                sa_scan();
            if (sa_nfiles <= 0)
                snprintf(buf, n, "NO FILES");
            else {
                char nm[64];
                size_t l;
                snprintf(nm, sizeof nm, "%s", sa_names[(int)v < sa_nfiles ? (int)v : 0]);
                l = strlen(nm);
                if (l > 4)
                    nm[l - 4] = 0;               /* without .wav */
                if (strlen(nm) > 22) {
                    nm[20] = '.';
                    nm[21] = '.';
                    nm[22] = 0;
                }
                for (char *q = nm; *q; q++)
                    *q = (char)toupper((unsigned char)*q);
                snprintf(buf, n, "%s", nm);
            }
        }
        else if (p == P_RN_RATE)
            snprintf(buf, n, "%s", LAB_RATES[(int)v % 6]);
        else if (p == P_RA_FILTER || p == P_RB_FILTER)
            snprintf(buf, n, "%s", FX_FILTERS[(int)v & 3]);
        else if (p == P_RA_RES_MODEL || p == P_RB_RES_MODEL)
            snprintf(buf, n, "%s", FX_RES_MODELS[(int)v % 6]);
        else if (p == P_RA_ECHO_TIME || p == P_RB_ECHO_TIME)
            snprintf(buf, n, "%s", FX_ECHO_TIMES[(int)v % 6]);
        else if (p >= P_GR_KICK_TO && p <= P_GR_HAT_TO)
            snprintf(buf, n, "%s", GR_TARGETS[(int)v % 10]);
        else if (p == P_GR_RATE)
            snprintf(buf, n, "%s", GR_RATES[(int)v % 3]);
        else if (p == P_SY_MODEL) {
            int e = (int)lab_v[P_SY_ENGINE];
            static const char *const em[3] = {"MODAL", "STRING", "STRINGS"};
            snprintf(buf, n, "%s", e == SY_RINGS ? FX_RES_MODELS[(int)v % 6] : e == SY_ELEMENTS ? em[(int)v % 3] : "(RINGS / ELEMENTS)");
        }
        else if (p == P_SY_ENGINE)
            snprintf(buf, n, "%s", SY_NAMES[(int)v % SY_ENGINES]);
        else if (p == P_SY_PLAYED_BY)
            (int)v == 0 ? snprintf(buf, n, "ITSELF") : (int)v == 1 ? snprintf(buf, n, "LOGIC") : snprintf(buf, n, "TRACK %d HITS", (int)v - 1);
        else if (p == P_LG_RATE)
            snprintf(buf, n, "%s", LAB_RATES[(int)v % 6]);
        else if (p == P_LG_RULE)
            snprintf(buf, n, "%s", LG_RULE_NAMES[(int)v % LG_RULES]);
        else if (p == P_LG_TARGET || p == P_LG_TARGET2) {
            int tv = p == P_LG_TARGET2 ? (int)v - 1 : (int)v;
            if (p == P_LG_TARGET2 && (int)v == 0)
                snprintf(buf, n, "SAME AS VOICE 1");
            else if (tv == 6)
                snprintf(buf, n, "POLY");
            else if (tv == 7)
                snprintf(buf, n, "SYNTH");
            else
                snprintf(buf, n, "TRACK %d %s", tv + 1, md_model_name(md_state()->track[tv].model));
        }
        else if ((int)v >= 6)
            snprintf(buf, n, "WHOLE MIX");
        else
            snprintf(buf, n, "%d %s", (int)v + 1, md_model_name(md_state()->track[(int)v].model));
        break;
    }
}

#include "brick_labbiome.inc"
