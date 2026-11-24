/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka.
 *
 * The whole Mutable Dreams 2.1 firmware, unchanged, through fm1-base's host simulator plat_host.c (which includes
 * app.c, which includes every page), wrapped as the browser emulator wraps it (brick_emu.inc): one translation unit,
 * as in the browser build. This file adds what the Brick needs around it: its buttons on the FM-1 panel, the saved
 * objects as files, and the stage (brick_stage.inc). Nothing in FM-1/Release-2.1/Source is changed. */
#include "brick_emu.inc"
#include "brick_core.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

/* --------------------------------------------------------------- time --- */
static uint64_t bk_frames;                       /* frames rendered since start: the core's clock */
uint32_t bk_ms(void) { return (uint32_t)(bk_frames * 1000u / BK_RATE); }
/* a moment kept as "when it happened", never 0 (0 means "not yet"). (0.76) It was written t | 1u, which at an even
   millisecond lies one ms in the future: "now - then" wrapped to four billion, and whatever waited for a while
   came at once - a chapter card gone in its first frame, the light picture after one slow frame, the sound-limit
   notice every block */
static inline uint32_t bk_stamp(uint32_t t) { return t ? t : 1u; }

/* (1.3) a notice: a title and a line, shown on a card over every screen (brick_stage.inc bk_draw_notice). Before,
   the port's messages went to the FM-1's one-line message: seen only in the garden, for 1.4 s, its second line lost
   (the Lab's, the views' and the toys' messages never appeared). The FM-1's own announcement still gets the title. */
static char bk_nt_a[48], bk_nt_b[112];
static uint32_t bk_nt_at;
static void bk_notice(const char *a, const char *b)
{
    uint32_t plat_ms(void);
    snprintf(bk_nt_a, sizeof bk_nt_a, "%s", a ? a : "");
    snprintf(bk_nt_b, sizeof bk_nt_b, "%s", b ? b : "");
    bk_nt_at = plat_ms() | 1u;
    app_say(a, b);
}

/* -------------------------------------------------------------- store --- */
static char bk_dir[512];
static int bk_lang, bk_lang_idx, bk_lang_forced;              /* (brick_stage.inc: the language; forced by MD_LANG, then the saved one is not taken) */
static int bk_lang_choose(int on);
static int bk_lang_cycle(void);
static int bk_lang_choose_idx(int idx);
static int bk_lang_by_code(const char *c);
static const char *const BK_LANG_CODE[] = {"en", "de", "fr", "es"};
static const char *const BK_LANG_NAME[] = {"ENGLISH", "DEUTSCH", "FRAN\xC3\x87" "AIS", "ESPA\xC3\x91" "OL"};
static const char *const BK_LANG_SAY[] = {"THE GARDEN SPEAKS ENGLISH", "DIE GARTENSPRACHE IST DEUTSCH", "LE JARDIN PARLE FRAN\xC3\x87" "AIS",
                                          "EL JARD\xC3\x8D" "N HABLA ESPA\xC3\x91" "OL"};
#define BK_NLANG ((int)(sizeof BK_LANG_CODE / sizeof BK_LANG_CODE[0]))

static uint32_t bk_store_seen;
static int bk_battery_pct = -1, bk_battery_warned = 100;   /* (1.3: the lowest mark already told) */
void bk_battery(int percent)
{
    bk_battery_pct = percent;
    if (percent < 0)
        return;
    if (percent > bk_battery_warned + 3)         /* (charging: the marks told again when it runs low anew) */
        bk_battery_warned = 100;
    if (percent <= 5 && bk_battery_warned > 5) {
        bk_battery_warned = 5;
        bk_notice("BATTERY NEARLY EMPTY", "SAVE NOW: R2 + SELECT HELD");
    } else if (percent <= 15 && bk_battery_warned > 15) {
        bk_battery_warned = 15;
        bk_notice("BATTERY LOW", "TIME TO SAVE: R2 + SELECT HELD");
    }
}

static void bk_store_path(char *out, size_t n, uint32_t obj, const char *suffix)
{
    snprintf(out, n, "%s/obj-%02u.bin%s", bk_dir, (unsigned)obj, suffix);
}

static void bk_store_load(void)
{
    uint32_t obj;
    if (!bk_dir[0])
        return;
    for (obj = 0; obj < web_nobj(); obj++) {
        char path[600];
        FILE *f;
        long len;
        bk_store_path(path, sizeof path, obj, "");
        if (!(f = fopen(path, "rb")))
            continue;
        fseek(f, 0, SEEK_END);
        len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len >= 0 && len <= (long)web_store_max() && fread(web_store(obj), 1, (size_t)len, f) == (size_t)len)
            web_store_set_len(obj, (int)len);
        fclose(f);
    }
}

/* every object the app has saved, each written whole beside its old file and then renamed over it: a power cut
 * leaves the old one or the new one, never half */
static void bk_store_keep(void)
{
    uint32_t obj;
    if (!bk_dir[0] || web_store_writes() == bk_store_seen)
        return;
    bk_store_seen = web_store_writes();
    for (obj = 0; obj < web_nobj(); obj++) {
        char path[600], tmp[600];
        int len = web_store_len(obj);
        FILE *f;
        bk_store_path(path, sizeof path, obj, "");
        if (len < 0) {
            remove(path);
            continue;
        }
        bk_store_path(tmp, sizeof tmp, obj, ".new");
        if (!(f = fopen(tmp, "wb")))
            continue;
        if (fwrite(web_store(obj), 1, (size_t)len, f) != (size_t)len) {
            fclose(f);
            remove(tmp);
            continue;
        }
        fflush(f);
#ifdef __linux__
        fsync(fileno(f));
#endif
        fclose(f);
        rename(tmp, path);
    }
}

/* a buffer the sound thread lets go of is freed only after the stage has finished a frame since (the stage lets
   the sound in while it draws, and may still be looking at it) */
static uint32_t bk_draw_gen;
static struct { void *p; uint32_t gen; } bk_graves[32];
static void bk_graves_collect(void)
{
    for (int i = 0; i < 32; i++)
        if (bk_graves[i].p && (int32_t)(bk_draw_gen - bk_graves[i].gen) >= 2) {
            free(bk_graves[i].p);
            bk_graves[i].p = 0;
        }
}
static void bk_defer_free(void *p)
{
    if (!p)
        return;
    bk_graves_collect();
    for (int i = 0; i < 32; i++)
        if (!bk_graves[i].p) {
            bk_graves[i].p = p, bk_graves[i].gen = bk_draw_gen;
            return;
        }
    free(p);                                     /* (no stage drawing at all: the headless runner) */
}
static int bk_light, bk_light_auto;              /* the light picture (brick_stage.inc says what it leaves out); auto: set by
                                                    itself when slow (1.5: then never kept in brick-prefs.txt) */
static int bk_calm;
static int bk_small;
static int bk_keyboard;                          /* (0.78) a PC without a game controller: the hints name the keyboard's keys */                             /* (0.76) the stage shown smaller (640 x 480 handhelds): small labels one size up */                              /* CALM MOTION (0.75): shapes still, no drifting decoration (brick_stage.inc) */
#include "brick_lab.inc"

/* ------------------------------------------------------------- input --- */
/* What each Brick control does is decided when it goes down (R2 held or not, play mode or not) and kept until it
 * comes up, so pressing R2 in the middle of a hold never turns one FM-1 button into another. */
enum { T_NONE, T_BACK, T_SELHELP, T_BUTTON, T_ENC, T_FOCUS, T_NOTE, T_SAVE, T_MENU, T_MIXER, T_LAB, T_LABMOD, T_LABFOCUS, T_LABVAL, T_LABA,
       T_VIEWKEY, T_ATLAS, T_DREAM, T_LABHUSH, T_LABNEXT, T_LABMAP };
typedef struct { uint8_t kind; int8_t arg, dir; } bk_target;
static bk_target bk_tgt[BK_NBUTTONS];
static uint32_t bk_held, bk_down_at[BK_NBUTTONS], bk_next_rep[BK_NBUTTONS];
static uint32_t bk_input_at;
static int bk_handheld;                          /* (brick_stage.inc; here for SELECT opening the menu, 1.5.8) */                     /* (1.5.8) when a button last changed: the side columns at half pace wait */
/* (1.1) how held buttons repeat: OPTIONS > REPEAT (0 calm, 1 normal, 2 quick) - the wait before the first repeat
   and how close the repeats come */
static int bk_rep_speed = 1;
static const uint32_t BK_REP_WAIT[3] = {420u, 280u, 190u}, BK_REP_GAP[3] = {140u, 100u, 70u};
static int bk_focus;                             /* K1..K4: 0..3 */
static int bk_focus_dir[4];                      /* the last turn's direction, for the stage */
static uint32_t bk_focus_at[4];
static uint32_t bk_pulse_btn, bk_pulse_until;    /* a short FM-1 tap made by the core (SAVE after its hold) */
static int bk_note_on[BK_NBUTTONS][3], bk_note_track[BK_NBUTTONS], bk_note_kit[BK_NBUTTONS], bk_note_poly[BK_NBUTTONS];
static uint32_t bk_notes_lit;                    /* play mode: which of the 8 degrees sound */
static int bk_menu_open, bk_menu_sel, bk_quit;
/* (1.1) OPTIONS (brick_options.inc) and the buttons as the player wants them: bk_map[physical] = the button it acts
   as. Only the ten below can move (the D-pad, MENU, F1, F2 and FN stay). START + SELECT held 3 s: back to the Brick's own */
static int bk_opt_open, bk_opt_capture = -1;
static uint32_t bk_phys_now;                     /* the physical buttons held, as they came */
static void bk_opt_press(int b);
static int bk_map[BK_NBUTTONS], bk_map_set;
static const int BK_REMAP[10] = {BK_A, BK_B, BK_X, BK_Y, BK_L1, BK_R1, BK_L2, BK_R2, BK_START, BK_SELECT};
static void bk_map_reset(void)
{
    for (int i = 0; i < BK_NBUTTONS; i++)
        bk_map[i] = i;
    bk_map_set = 1;
}
static int bk_map_custom(void)                  /* (anything moved?) */
{
    for (int i = 0; i < BK_NBUTTONS; i++)
        if (bk_map_set && bk_map[i] != i)
            return 1;
    return 0;
}
static int bk_map_phys(int logical)             /* the physical button that acts as logical */
{
    for (int i = 0; i < BK_NBUTTONS; i++)
        if (bk_map_set && bk_map[i] == logical)
            return i;
    return logical;
}
static void bk_map_assign(int logical, int phys)   /* phys becomes logical; the one that was logical takes phys's old part */
{
    int q = bk_map_phys(logical), old = bk_map[phys];
    bk_map[phys] = logical, bk_map[q] = old;
}
static float bk_stick[4], bk_stick_acc[4];
static int lab_open, lab_mod, lab_focus[M_COUNT];
/* (1.5) the Sound Lab's module map: every module at once, the sources in a row, the effects in two; the D-pad
   chooses, A goes there. F2 in the Lab, or L1 and R1 together (on every handheld). */
static int lab_mapview, lab_map_sel;
#define LAB_MAP_COLS 5                           /* (each row of the map: sources 8 = 5 + 3, effects 10 = 5 + 5) */
static int lab_map_rowcol(int m, int *col)       /* module > (row, column): rows 0-1 sources, 2-3 effects */
{
    int e = m >= M_SOURCES, i = e ? m - M_SOURCES : m;
    *col = i % LAB_MAP_COLS;
    return (e ? 2 : 0) + i / LAB_MAP_COLS;
}
static int lab_map_module(int row, int col)      /* (row, column) > module, -1 none */
{
    int base = row >= 2 ? M_SOURCES : 0, n = row >= 2 ? M_COUNT - M_SOURCES : M_SOURCES, i = (row & 1) * LAB_MAP_COLS + col;
    return i < n ? base + i : -1;
}
static void lab_map_press(int b)
{
    int col, row = lab_map_rowcol(lab_map_sel, &col), m;
    switch (b) {
    case BK_LEFT: case BK_RIGHT:
        for (int k = 0; k < LAB_MAP_COLS; k++) { /* (along the row, round, skipping the gaps) */
            col = (col + LAB_MAP_COLS + (b == BK_RIGHT ? 1 : -1)) % LAB_MAP_COLS;
            if ((m = lab_map_module(row, col)) >= 0) {
                lab_map_sel = m;
                break;
            }
        }
        break;
    case BK_UP: case BK_DOWN:
        for (int k = 0; k < 4; k++) {            /* (up and down through the four rows, the nearest column there) */
            row = (row + 4 + (b == BK_DOWN ? 1 : -1)) % 4;
            for (int c = col; c >= 0; c--)
                if ((m = lab_map_module(row, c)) >= 0) {
                    lab_map_sel = m;
                    return;
                }
        }
        break;
    case BK_A: case BK_F2:
        if (b == BK_A)
            lab_mod = lab_map_sel;
        lab_mapview = 0;
        break;
    case BK_L1: case BK_R1:
        lab_map_sel = (lab_map_sel + M_COUNT + (b == BK_R1 ? 1 : -1)) % M_COUNT;
        break;
    case BK_B: case BK_F1: case BK_L2: case BK_MENU:
        lab_mapview = 0;
        break;
    }
}
static void lab_map_open(void) { lab_mapview = 1, lab_map_sel = lab_mod; }
static float lab_prev[P_COUNT];

/* the parameters of module m, in order: their numbers into out, the count returned */
static int lab_params(int m, int *out)
{
    int n = 0;
    if (m == M_MASTER) {                         /* (in the order of the signal) */
        for (; n < (int)(sizeof MS_ORDER / sizeof MS_ORDER[0]); n++)
            out[n] = MS_ORDER[n];
        return n;
    }
    for (int p = 0; p < P_COUNT; p++)
        if (LAB_P[p].mod == m)
            out[n++] = p;
    return n;
}
static int lab_focused_param(void)
{
    int ps[P_COUNT], n = lab_params(lab_mod, ps);
    return ps[clamp(lab_focus[lab_mod], 0, n - 1)];
}
static uint32_t bk_turns;                        /* every turn of a dial or Lab value (the stage moves with it) */
static int bk_turn_dir;
static uint32_t lab_turned_at;                   /* (the stage shows the value large for a moment) */
static void lab_turn(int dir, int times)
{
    int p = lab_focused_param();
    lab_turned_at = bk_stamp(bk_ms());
    bk_turns++, bk_turn_dir = dir;
    lab_set(p, lab_v[p] + (float)(dir * times) * LAB_P[p].step);
}
/* (1.3) X in the Lab: the module silent and back again, to hear what it adds. Its switches and amounts are kept
   and set to nothing; X again brings them back, if nothing was turned up meanwhile (then X silences anew). */
static float lab_hush_v[M_COUNT][5];
static int lab_hushed[M_COUNT];
static int lab_hush_params(int m, int *ps)
{
    switch (m) {
    case M_TEX: ps[0] = P_FOREST, ps[1] = P_AIR, ps[2] = P_WATER, ps[3] = P_STARDUST, ps[4] = P_PULSE; return 5;
    case M_SAMPLES: ps[0] = P_SA_LEVEL; return 1;
    case M_VOICES: ps[0] = P_VO_LEVEL; return 1;
    case M_POLY: ps[0] = P_PO_LEVEL; return 1;
    case M_SYNTH: ps[0] = P_SY_LEVEL; return 1;
    case M_RANDOM: ps[0] = P_RN_ON; return 1;
    case M_LOGIC: ps[0] = P_LG_ON; return 1;
    case M_GRIDS: ps[0] = P_GR_ON; return 1;
    case M_RACK_A: case M_RACK_B:
        for (int s = 0; s < 4; s++)
            ps[s] = (m == M_RACK_B ? P_RB_IN_VOICES : P_RA_IN_VOICES) + s;
        return 4;
    case M_CLOUD: ps[0] = P_CL_MIX; return 1;
    case M_PARTICLES: ps[0] = P_PA_MIX; return 1;
    case M_SEEDS: ps[0] = P_BE_MIX; return 1;
    case M_SHIMMER: ps[0] = P_SH_MIX; return 1;
    case M_CROSSMOD: ps[0] = P_XM_MIX; return 1;
    case M_COLOUR: ps[0] = P_ENS_MIX, ps[1] = P_TAPE_DRIVE, ps[2] = P_TAPE_WOW, ps[3] = P_TAPE_AGE; return 4;
    }
    return 0;                                    /* (SENDS and MASTER: nothing to silence) */
}
static void lab_hush(int m)
{
    int ps[5], n = lab_hush_params(m, ps);
    if (!n) {
        bk_notice(LAB_MOD_NAMES[m], "NOTHING TO SILENCE HERE");
        return;
    }
    if (lab_hushed[m] && !lab_active(m)) {       /* back */
        for (int i = 0; i < n; i++)
            lab_set(ps[i], lab_hush_v[m][i]);
        lab_hushed[m] = 0;
        bk_notice(LAB_MOD_NAMES[m], "BACK AGAIN");
    } else if (lab_active(m)) {
        for (int i = 0; i < n; i++)
            lab_hush_v[m][i] = lab_v[ps[i]], lab_set(ps[i], 0);
        lab_hushed[m] = 1;
        bk_notice(LAB_MOD_NAMES[m], "SILENT   X: BACK AGAIN");
    } else
        bk_notice(LAB_MOD_NAMES[m], "SILENT ALREADY");
}
int bk_lab_hushed(void) { return lab_hushed[lab_mod] && !lab_active(lab_mod); }
const char *bk_lab_module_name(void) { return LAB_MOD_NAMES[lab_mod]; }   /* (the tests) */

/* A: an on/off switches; anything else goes to its neutral value and back to where it was */
static void lab_a(void)
{
    int p = lab_focused_param();
    if (p == P_SA_FILE) {                        /* the samples folder looked at again (a card swapped, files added) */
        char keep[64] = "";
        if (sa_loaded >= 0)
            snprintf(keep, sizeof keep, "%s", sa_names[sa_loaded]);
        sa_scan();
        for (int i = 0; i < sa_nfiles; i++)
            if (!strcmp(sa_names[i], keep))
                lab_v[P_SA_FILE] = (float)i;
        sa_loaded = -2 - sa_loaded;              /* (indices may have moved: load again) */
        return;
    }
    if (LAB_P[p].unit == U_ONOFF)
        lab_set(p, lab_v[p] > 0.5f ? 0 : 1);
    else if (lab_v[p] != LAB_P[p].def) {
        lab_prev[p] = lab_v[p];
        lab_set(p, LAB_P[p].def);
    } else if (lab_prev[p] != LAB_P[p].def)
        lab_set(p, lab_prev[p]);
}

/* (1.6) the new OPTIONS: a scale for playing, a garden that follows the clock, LISTEN MODE (the screen rests while the
   music plays on), larger letters, more contrast; and the GARDEN JOURNAL (which Biomes and worlds were visited) */
typedef struct { const char *name; int n; int iv[8]; } bk_pscale;
static const bk_pscale BK_PSCALES[] = {
    {"BIOME'S OWN", 0, {0}},
    {"MAJOR", 7, {0, 2, 4, 5, 7, 9, 11}},          {"MINOR", 7, {0, 2, 3, 5, 7, 8, 10}},
    {"PENTATONIC", 5, {0, 2, 4, 7, 9}},            {"MINOR PENTATONIC", 5, {0, 3, 5, 7, 10}},
    {"DORIAN", 7, {0, 2, 3, 5, 7, 9, 10}},         {"HIJAZ", 7, {0, 1, 4, 5, 7, 8, 10}},
    {"HIRAJOSHI", 5, {0, 2, 3, 7, 8}},             {"PELOG", 5, {0, 1, 3, 7, 8}},
    {"WHOLE TONE", 6, {0, 2, 4, 6, 8, 10}}};
#define BK_NPSCALES ((int)(sizeof BK_PSCALES / sizeof BK_PSCALES[0]))
static const int BK_LISTEN_MIN[4] = {0, 2, 5, 10};
static int bk_play_scale, bk_clock_on, bk_listen_sel, bk_big_text, bk_contrast, bk_listen_rest, bk_journal_on;
static int bk_quick_lift(void);
static int bk_tsize = 1, bk_tside, bk_tglow = 1, bk_tbuzz = 1, bk_tlift, bk_perf, bk_tpad, bk_tmode;   /* (1.6) the touch pad: size 0..2, sides swapped, glow 0..2, a buzz on a tap */
static uint32_t bk_listen_at, bk_listen_test_ms;     /* (the tests: the wait in ms instead of minutes) */
static float bk_clock_test = -1.0f;                  /* (the tests: the hour of the day, instead of the clock) */
static uint8_t bk_jr_b[(MD_BIOMES + 7) / 8], bk_jr_w[16];
static int bk_jr_dirty;
static void bk_journal_open(void) { bk_journal_on = 1; }
static int bk_jr_get(const uint8_t *a, int i) { return a[i >> 3] >> (i & 7) & 1; }
static void bk_jr_set(uint8_t *a, int i)
{
    if (!bk_jr_get(a, i))
        a[i >> 3] |= (uint8_t)(1u << (i & 7)), bk_jr_dirty = 1;
}
/* the hour of the day (0..24, with minutes); the tests may fix it */
static float bk_clock_hour(void)
{
    static uint32_t at;
    static float h = 12.0f;
    if (bk_clock_test >= 0)
        return bk_clock_test;
    if (!at || bk_ms() - at > 20000u) {
        time_t tt = time(NULL);
        struct tm lt;
        if (localtime_r(&tt, &lt))
            h = (float)lt.tm_hour + (float)lt.tm_min / 60.0f;
        at = bk_ms() | 1u;
    }
    return h;
}
/* the time of day as the garden's "dawn": 0 night .. 1 sunrise and sunset, a pale 0.45 at noon */
static float bk_clock_dawn(void)
{
    static const float pt[][2] = {{0, 0}, {5, 0}, {7, 1}, {12, 0.45f}, {17, 0.8f}, {19, 1}, {21.5f, 0}, {24, 0}};
    float h = bk_clock_hour();
    for (int i = 1; i < 8; i++)
        if (h <= pt[i][0])
            return pt[i - 1][1] + (pt[i][1] - pt[i - 1][1]) * (h - pt[i - 1][0]) / (pt[i][0] - pt[i - 1][0]);
    return 0;
}
static int bk_quick_lift(void) { return bk_tlift ? 56 : 0; }
int bk_quit_requested(void) { return bk_quit; }
static const char *const BK_OPT_NAMES[] = {"pscale", "clock", "listen", "bigtext", "contrast"};
int bk_option_set(const char *name, int v)
{
    if (!strcmp(name, "pscale")) bk_play_scale = clamp(v, 0, BK_NPSCALES - 1);
    else if (!strcmp(name, "clock")) bk_clock_on = v != 0;
    else if (!strcmp(name, "listen")) bk_listen_sel = clamp(v, 0, 3), bk_listen_at = bk_ms();
    else if (!strcmp(name, "bigtext")) bk_big_text = v != 0;
    else if (!strcmp(name, "contrast")) bk_contrast = v != 0;
    else if (!strcmp(name, "tsize")) bk_tsize = clamp(v, 0, 2);
    else if (!strcmp(name, "tside")) bk_tside = v != 0;
    else if (!strcmp(name, "tglow")) bk_tglow = clamp(v, 0, 2);
    else if (!strcmp(name, "tbuzz")) bk_tbuzz = v != 0;
    else if (!strcmp(name, "tlift")) bk_tlift = v != 0;
    else if (!strcmp(name, "perf")) bk_perf = v != 0;
    else if (!strcmp(name, "tpad")) bk_tpad = v != 0;
    else if (!strcmp(name, "tmode")) bk_tmode = clamp(v, 0, 2);
    else if (!strcmp(name, "hour")) bk_clock_test = v < 0 ? -1.0f : (float)v / 10.0f;
    else if (!strcmp(name, "listenms")) bk_listen_test_ms = (uint32_t)v;
    else return -1;
    (void)BK_OPT_NAMES;
    return 0;
}
static int bk_touch_probe(const char *name);   /* (brick_touch_views.inc: what the views hold, for the tests) */
int bk_option_get(const char *name)
{
    if (!strcmp(name, "pscale")) return bk_play_scale;
    if (!strcmp(name, "clock")) return bk_clock_on;
    if (!strcmp(name, "listen")) return bk_listen_sel;
    if (!strcmp(name, "bigtext")) return bk_big_text;
    if (!strcmp(name, "contrast")) return bk_contrast;
    if (!strcmp(name, "track")) return dream_ui.track;
    if (!strcmp(name, "focus")) return bk_focus;
    if (!strcmp(name, "tsize")) return bk_tsize;
    if (!strcmp(name, "tside")) return bk_tside;
    if (!strcmp(name, "tglow")) return bk_tglow;
    if (!strcmp(name, "tbuzz")) return bk_tbuzz;
    if (!strcmp(name, "tlift")) return bk_tlift;
    if (!strcmp(name, "perf")) return bk_perf;
    if (!strcmp(name, "tpad")) return bk_tpad;
    if (!strcmp(name, "tmode")) return bk_tmode;
    return bk_touch_probe(name);
}
int bk_journal_count(int worlds)
{
    int n = 0;
    for (int i = 0; i < (worlds ? 128 : MD_BIOMES); i++)
        n += bk_jr_get(worlds ? bk_jr_w : bk_jr_b, i);
    return n;
}

/* (1.4) SLEEP TIMER (OPTIONS): after 15, 30, 60 or 90 minutes the music fades over a minute and stops; the screen rests
   (dark, a moon, four pictures a second: little to draw, the battery kept) until any button wakes it. Not kept for
   the next start: a timer is for this night. */
static const int BK_SLEEP_MIN[5] = {0, 15, 30, 60, 90};
#define BK_SLEEP_FADE 60000u
static int bk_sleep_sel, bk_resting;
static uint32_t bk_sleep_end, bk_sleep_fade = BK_SLEEP_FADE;   /* (the fade: a minute; the tests and the film: shorter) */
static void bk_sleep_set(int sel)
{
    bk_sleep_sel = sel;
    bk_sleep_end = sel ? bk_stamp(bk_ms() + (uint32_t)BK_SLEEP_MIN[sel] * 60000u) : 0;
    bk_sleep_fade = BK_SLEEP_FADE;
    web_master(4096u);
}
int bk_sleep_left_min(void)                      /* (minutes left, rounded up; 0: off) */
{
    if (!bk_sleep_end)
        return 0;
    return (int)((bk_sleep_end - bk_ms() + 59999u) / 60000u);
}
void bk_sleep_in(uint32_t ms)                   /* (the tests and the film: out in ms, the fade at most as long) */
{
    bk_sleep_sel = 1, bk_sleep_end = bk_stamp(bk_ms() + ms);
    bk_sleep_fade = ms < BK_SLEEP_FADE ? (ms > 1000u ? ms : 1000u) : BK_SLEEP_FADE;
}
int bk_resting_now(void) { return bk_resting; }

static int bk_degree_of(int b)
{
    static const int8_t deg[BK_NBUTTONS] = {
        [BK_LEFT] = 0, [BK_DOWN] = 1, [BK_RIGHT] = 2, [BK_UP] = 3, [BK_Y] = 4, [BK_X] = 5, [BK_B] = 6, [BK_A] = 7,
        [BK_L1] = -1, [BK_R1] = -1, [BK_L2] = -1, [BK_R2] = -1, [BK_START] = -1, [BK_SELECT] = -1, [BK_MENU] = -1,
        [BK_F1] = -1, [BK_F2] = -1, [BK_FN] = -1};
    return deg[b];
}

/* the degree-th note of the Biome's scale above its root, in the panel's octave */
static int bk_scale_note(int degree)
{
    const md_patch *p = md_state();
    int base = 48 + (int)(p->root % 12) + 12 * dream_ui.octave, n = base, found = 0, s;
    if (bk_play_scale > 0 && bk_play_scale < BK_NPSCALES) {   /* (1.6) a scale of the player's choice */
        const bk_pscale *ps = &BK_PSCALES[bk_play_scale];
        return clamp(base + 12 * (degree / ps->n) + ps->iv[degree % ps->n], 24, 96);
    }
    for (s = 0; s < 48; s++) {
        int q = base + s;
        if (md_quantize(q, p->root, p->scale) == q) {
            if (found == degree) {
                n = q;
                break;
            }
            found++;
        }
    }
    return clamp(n, 24, 96);
}

int bk_note_for_degree(int degree) { return bk_scale_note(degree); }

static void bk_note_down(int b, int chord)
{
    const md_patch *p = md_state();
    int d = bk_degree_of(b), i, track = dream_ui.track;
    bk_note_track[b] = track;
    bk_note_kit[b] = p->kit.enabled && track == 3;
    bk_note_poly[b] = lab_v[P_PO_LEVEL] > 0 && lab_v[P_PO_PLAYMODE] > 0.5f;   /* the Sound Lab's POLY voice */
    for (i = 0; i < 3; i++)
        bk_note_on[b][i] = -1;
    if (bk_note_poly[b]) {
        for (i = 0; i < (chord ? 3 : 1); i++) {
            bk_note_on[b][i] = bk_scale_note(d + 2 * i);
            po_note_on(bk_note_on[b][i], 0);
        }
    } else if (bk_note_kit[b]) {
        dream_cmd(MD_KIT_TRIGGER, 3, d);
    } else {
        for (i = 0; i < (chord ? 3 : 1); i++) {
            bk_note_on[b][i] = bk_scale_note(d + 2 * i);
            dream_cmd(MD_TRIGGER, track, bk_note_on[b][i]);
        }
    }
    bk_notes_lit |= 1u << d;
    dream_dismiss_intro();
}

static void bk_note_up(int b)
{
    int i;
    for (i = 0; i < 3; i++)
        if (bk_note_on[b][i] >= 0) {
            if (bk_note_poly[b])
                po_note_off(bk_note_on[b][i]);
            else
                dream_cmd(MD_NOTE_OFF, bk_note_track[b], bk_note_on[b][i]);
        }
    if (bk_degree_of(b) >= 0)                    /* (a button without a degree lights nothing) */
        bk_notes_lit &= ~(1u << bk_degree_of(b));
}

#include "brick_views.inc"

/* the jump menu */
typedef struct { const char *name; int page, view; } bk_jump;
static const bk_jump BK_JUMPS[] = {
    {"SOUND LAB", -3, -1}, {"MIXER VIEW", -4, -1}, {"BIOME ATLAS", -5, -1}, {"SEQUENCE VIEW", -6, -1},
    {"DREAM MAP", -7, -1}, {"MODULATION VIEW", -8, -1}, {"VOICE VIEW", -9, -1}, {"SPACE VIEW", -10, -1}, {"GARDEN / FM-1 SCREEN", -11, -1}, {"ARCHIVE (VOICES TO FETCH)", -12, -1}, {"WORLDS", -13, -1}, {"BUTTONS (HELP)", -14, -1},
    {"PLAYGROUND", -15, -1},
    {"LIVE ATLAS", LIVE, LIVE_ATLAS}, {"LARGE TRACKS", LIVE, LIVE_TRACKS}, {"BIOME BROWSER", LIVE, LIVE_BROWSER},
    {"MIXER (FM-1 PAGE)", MIXER, -1}, {"SOUND", SOUND, -1}, {"SEQUENCE GRID", SEQ_GRID, -1}, {"SPACE (DELAY + REVERB)", SPACE, -1},
    {"GRAIN", GRAIN, -1}, {"LIVING LINKS", LIVING_LINKS, -1}, {"HARMONY", HARMONY, -1}, {"JOURNEY", JOURNEY, -1},
    {"ITINERA", ITINERA, -1}, {"BANKS", BANKS, -1}, {"MIDI", MIDI, -1}, {"QUIT", -2, -1},
};
#define BK_NJUMPS ((int)(sizeof BK_JUMPS / sizeof BK_JUMPS[0]))
#define BK_MENU_SPLIT 13                         /* the Brick's own places on the left, the FM-1's pages from here on the right */

static void bk_goto(int page, int view)
{
    bk_view = V_NONE;                            /* (an FM-1 page: the full views give way) */
    if (md_pending_biome() >= 0 || bank_pending >= 0)
        return;                                  /* a question on the screen waits for its answer */
    dream_ui.help = -1;
    if (page == LIVE) {
        dream_ui.live_view = (uint8_t)view;
        if (view == LIVE_BROWSER)
            browse_open(md_state()->biome_id);
    }
    dream_ui.page = page;
    dream_ui.dirty = 1;
    dream_dismiss_intro();
}

static int bk_garden;                            /* the garden as home (brick_garden.inc) */
static int bk_help_open, bk_help_page, bk_help_first, bk_help_done;   /* the button map (brick_help.inc) */
static int bk_guide_open, bk_guide_page;         /* THE GUIDE (brick_guide.inc, 0.73) */
static void bk_guide_press(int b);
static int sp_on;                                /* (the start screen: brick_splash.inc) */

static void bk_jump_go(int idx)
{
    const bk_jump *j = &BK_JUMPS[idx];
    if (j->page == -2)
        bk_quit = 1;
    else if (j->page == -14) {               /* the button map */
        bk_help_open = 1;
        bk_help_page = lab_open ? 2 : 0;
    } else if (j->page == -11) {               /* home: to the garden; already there, the FM-1's own screen instead */
        int there = bk_garden && bk_garden_visible();
        bk_garden = !there;
        lab_open = 0;
        bk_view = V_NONE;
        bk_goto(LIVE, LIVE_ATLAS);
        bk_notice(bk_garden ? "GARDEN" : "FM-1 SCREEN", "");
    } else if (j->page == -3) {
        lab_open = 1;
        bk_view = V_NONE;
    } else if (j->page <= -4)
        bk_view_open(j->page == -4 ? V_MIXER : j->page == -5 ? V_ATLAS : j->page == -6 ? V_SEQ : j->page == -7 ? V_DREAM : j->page == -8 ? V_MOD : j->page == -9 ? V_VOICE : j->page == -12 ? V_ARCHIVE : j->page == -13 ? V_WORLDS : j->page == -15 ? V_TOYS : V_SPACE);
    else {
        lab_open = 0;
        bk_goto(j->page, j->view);
    }
}

static void bk_menu_press(int b)
{
    if (b == BK_UP)
        bk_menu_sel = (bk_menu_sel + BK_NJUMPS - 1) % BK_NJUMPS;
    else if (b == BK_DOWN)
        bk_menu_sel = (bk_menu_sel + 1) % BK_NJUMPS;
    else if (b == BK_LEFT || b == BK_RIGHT) {    /* the other column (brick_stage.inc BK_MENU_SPLIT: 12) */
        int r = bk_menu_sel >= BK_MENU_SPLIT ? bk_menu_sel - BK_MENU_SPLIT : bk_menu_sel;
        bk_menu_sel = bk_menu_sel >= BK_MENU_SPLIT ? (r < BK_MENU_SPLIT ? r : BK_MENU_SPLIT - 1)
                                                   : (r + BK_MENU_SPLIT < BK_NJUMPS ? r + BK_MENU_SPLIT : BK_NJUMPS - 1);
    }
    else if (b == BK_A) {
        bk_jump_go(bk_menu_sel);
        bk_menu_open = 0;
    } else if (b == BK_B || b == BK_MENU || b == BK_SELECT)   /* (SELECT: the key that opened it on a handheld) */
        bk_menu_open = 0;
}

/* the quick bar (L2 tapped): every place of the Brick in a row at the bottom; LEFT/RIGHT (or L1/R1) choose, A goes,
   B or L2 closes. Its entries: indexes into BK_JUMPS (-1: the garden, home) */
/* -2: the whole jump menu (for handhelds without a MENU key), -3: play mode on/off (without the Brick's FN switch) */
/* -4: the light picture on/off (bk_light) */
static const int BK_QUICK[] = {-1, 1, 0, 2, 3, 4, 10, 9, 5, 6, 7, 12, 11, -7, -2, -3, -4, -5, -6};   /* (-7: OPTIONS, 1.1) */   /* (12: PLAYGROUND, 0.65; -5: CALM MOTION, 0.75; -6: DEUTSCH / ENGLISH, 0.80) */
static uint32_t bk_fn_soft;                      /* play mode latched from the quick bar: held as if FN were on */
#define BK_NQUICK ((int)(sizeof BK_QUICK / sizeof BK_QUICK[0]))
static int bk_quick_open, bk_quick_sel = 1, bk_l2_clean;
static uint32_t bk_l2_at;
static void bk_go_garden(void)
{
    lab_open = 0;
    bk_view = V_NONE;
    if (!bk_garden)
        bk_garden = 1;
    bk_goto(LIVE, LIVE_ATLAS);
}
/* where we are, and where we came from (0.61): B always goes exactly one step back, the way we came, however far
   a single button took us; the garden at the end. Places: the Sound Lab, a full view, an FM-1 page (the FM-1's
   edit or effect pages, turned with X or Y, count as one place together), the garden. Followed in bk_tick. */
typedef struct { int8_t lab, view, garden, live; int16_t page; } bk_place;
#define BK_HIST 12
static bk_place bk_hist[BK_HIST], bk_here, bk_left;   /* bk_left: the place last left (the quick bar's first pick) */
static int bk_left_set;
static int bk_nhist, bk_here_set;
static bk_place bk_place_now(void)
{
    bk_place p;
    p.lab = (int8_t)lab_open, p.view = (int8_t)(lab_open ? 0 : bk_view), p.garden = (int8_t)bk_garden;
    p.page = (int16_t)dream_ui.page, p.live = (int8_t)dream_ui.live_view;
    return p;
}
static int bk_place_eq(bk_place a, bk_place b)
{
    if (a.lab || b.lab)
        return a.lab == b.lab;
    if (a.view || b.view)
        return a.view == b.view;
    if (a.page != b.page)
        return 0;
    return a.page != LIVE || (a.live == b.live && (a.live != LIVE_ATLAS || a.garden == b.garden));
}
static int bk_place_fm1(bk_place p) { return !p.lab && !p.view && p.page != LIVE; }
static void bk_track_place(void)
{
    bk_place n;
    int i;
    if (dream_ui.help >= 0 || md_pending_biome() >= 0 || bank_pending >= 0 || bk_menu_open || bk_quick_open ||
        bk_help_open || bk_guide_open || bk_journal_on || sp_on || intro_active)
        return;                                  /* (a question, a help or a menu on top: not a place) */
    n = bk_place_now();
    if (!bk_here_set) {
        bk_here = n, bk_here_set = 1;
        return;
    }
    if (bk_place_eq(n, bk_here)) {
        bk_here = n;
        return;
    }
    bk_left = bk_here, bk_left_set = 1;
    for (i = bk_nhist - 1; i >= 0; i--)          /* back somewhere we were: the way there is the way back */
        if (bk_place_eq(bk_hist[i], n)) {
            bk_nhist = i, bk_here = n;
            return;
        }
    if (!(bk_place_fm1(n) && bk_place_fm1(bk_here))) {
        if (bk_nhist == BK_HIST) {
            memmove(bk_hist, bk_hist + 1, (BK_HIST - 1) * sizeof bk_hist[0]);
            bk_nhist--;
        }
        bk_hist[bk_nhist++] = bk_here;
    }
    bk_here = n;
}
/* where B leads now (the top bar shows it), "" at home */
static const char *bk_back_name(void)
{
    static const char *const VN[V_COUNT] = {"", "MIXER", "ATLAS", "SEQUENCE", "DREAM MAP", "MODULATION", "VOICE", "SPACE",
                                            "ARCHIVE", "WORLDS", "PLAYGROUND"};
    bk_place p;
    if (!bk_nhist)
        return "";
    p = bk_hist[bk_nhist - 1];
    if (p.lab)
        return "SOUND LAB";
    if (p.view > 0 && p.view < V_COUNT)
        return VN[p.view];
    if (p.page == LIVE && p.live == LIVE_ATLAS)
        return p.garden ? "GARDEN" : "FM-1 SCREEN";
    return "FM-1 PAGE";
}
static int bk_quick_for(bk_place p)              /* the quick bar's entry for a place, -1 none */
{
    static const int VJ[V_COUNT] = {-9, 1, 2, 3, 4, 5, 6, 7, 9, 10, 12};
    int want = p.lab ? 0 : p.view > 0 && p.view < V_COUNT ? VJ[p.view] : p.page == LIVE && p.live == LIVE_ATLAS && p.garden ? -1 : -9;
    for (int i = 0; i < (int)(sizeof BK_QUICK / sizeof BK_QUICK[0]); i++)
        if (BK_QUICK[i] == want)
            return i;
    return -1;
}
static const char *bk_place_name(void)
{
    if (lab_open)
        return "SOUND LAB";
    if (!bk_view && bk_garden_visible())
        return "GARDEN";
    return bk_page_name();
}
static void bk_back(void)
{
    if (!bk_nhist) {                             /* nowhere left to go back to: home */
        if (bk_garden && !bk_garden_visible())
            bk_go_garden();
        else if (!bk_garden && (lab_open || bk_view || dream_ui.page != LIVE || dream_ui.live_view != LIVE_ATLAS)) {
            lab_open = 0;
            bk_goto(LIVE, LIVE_ATLAS);
        } else {
            bk_notice(bk_garden ? "GARDEN" : "FM-1 SCREEN", "HOME   L2: ALL PLACES");
            return;
        }
    } else {
        bk_place p = bk_hist[--bk_nhist];
        lab_open = 0;
        bk_goto(p.page, p.live);                 /* (closes any view) */
        bk_garden = p.garden;
        if (p.lab)
            lab_open = 1;
        else if (p.view)
            bk_view_open(p.view);
        bk_left = bk_here, bk_left_set = 1;
        bk_here = bk_place_now(), bk_here_set = 1;
    }
    bk_notice(bk_place_name(), "");
}

/* home at once: the Lab and any view closed, the way back forgotten (B held) */
static void bk_home(void)
{
    if (md_pending_biome() >= 0 || bank_pending >= 0)
        return;
    sq_edit = 0, ar_smp = 0, mf_pond = 0, dm_fly = 0;
    bk_nhist = 0;
    if (bk_garden)
        bk_go_garden();
    else {
        lab_open = 0;
        bk_goto(LIVE, LIVE_ATLAS);
    }
    bk_here = bk_place_now(), bk_here_set = 1;
    bk_notice(bk_garden ? "GARDEN" : "FM-1 SCREEN", "HOME");
}

#define BK_QUICK_SETTINGS 13                     /* (1.5) the quick bar's stones from here on are settings, before them places */
static void bk_quick_press(int b)
{
    static int last[2] = {0, BK_QUICK_SETTINGS};   /* (each group's stone last chosen) */
    if (b == BK_UP || b == BK_DOWN) {            /* (1.5) UP/DOWN: from the places to the settings and back */
        int g = bk_quick_sel >= BK_QUICK_SETTINGS;
        last[g] = bk_quick_sel;
        bk_quick_sel = last[!g];
    } else if (b == BK_LEFT || b == BK_L1)
        bk_quick_sel = (bk_quick_sel + BK_NQUICK - 1) % BK_NQUICK;
    else if (b == BK_RIGHT || b == BK_R1)
        bk_quick_sel = (bk_quick_sel + 1) % BK_NQUICK;
    else if (b == BK_A) {
        bk_quick_open = 0;
        if (BK_QUICK[bk_quick_sel] == -2)
            bk_menu_open = 1;
        else if (BK_QUICK[bk_quick_sel] == -7)    /* OPTIONS (1.1) */
            bk_opt_open = 1;
        else if (BK_QUICK[bk_quick_sel] == -6) {   /* DEUTSCH / ENGLISH */
            if (bk_lang_cycle() < 0)
                bk_notice("ENGLISH", "NO LANGUAGE TABLE FOUND (LANG/DE.TXT, FR.TXT, ES.TXT)");
            else
                bk_notice(BK_LANG_NAME[bk_lang_idx], BK_LANG_SAY[bk_lang_idx]);
            st_dirty = 1;
        } else if (BK_QUICK[bk_quick_sel] == -5) {
            bk_calm = !bk_calm;
            st_dirty = 1;                        /* (kept for the next start: brick-prefs.txt) */
            bk_notice(bk_calm ? "CALM MOTION" : "LIVELY MOTION", bk_calm ? "SHAPES STILL, NOTHING DRIFTING" : "");
        } else if (BK_QUICK[bk_quick_sel] == -4) {
            bk_light = !bk_light, bk_light_auto = 0;
            st_dirty = 1;
            bk_notice(bk_light ? "LIGHT PICTURE" : "FULL PICTURE", bk_light ? "LESS DECORATION, MORE SPEED" : "");
        } else if (BK_QUICK[bk_quick_sel] == -3) {
            bk_fn_soft ^= 1u << BK_FN;
            bk_notice(bk_fn_soft ? "PLAY MODE" : "PLAY MODE OFF", bk_fn_soft ? "L2: QUICK BAR TO END IT" : "");
        } else if (BK_QUICK[bk_quick_sel] < 0)
            bk_go_garden();
        else
            bk_jump_go(BK_QUICK[bk_quick_sel]);
    } else if (b == BK_B || b == BK_L2 || b == BK_MENU)
        bk_quick_open = 0;
}

static void bk_press(int b)
{
    int shift = (bk_held >> BK_R2) & 1u, play = (bk_held >> BK_FN) & 1u;
    bk_target t = {T_NONE, 0, 0};
    if (bk_opt_open) {                           /* (1.1) OPTIONS: its own keys */
        bk_opt_press(b);
        bk_tgt[b] = t;
        return;
    }
    if (lab_mapview && lab_open) {               /* (1.5) the module map: its own keys */
        lab_map_press(b);
        bk_tgt[b] = t;
        return;
    }
    if (bk_guide_open) {                         /* THE GUIDE: its own keys */
        bk_guide_press(b);
        bk_tgt[b] = t;
        return;
    }
    if (bk_journal_on) {                         /* (1.6) the journal: any button closes it, back to OPTIONS */
        if (b != BK_FN && b != BK_R2)
            bk_journal_on = 0, bk_opt_open = 1;
        bk_tgt[b] = t;
        return;
    }
    if (bk_help_open) {                          /* the button map: L1/R1 turn its pages, X the guide, anything else closes it */
        if (b == BK_X) {
            bk_help_open = 0;
            bk_guide_open = 1, bk_guide_page = 0;
        } else if (b == BK_Y) {                  /* (1.1) Y: OPTIONS - the buttons as you like them */
            bk_help_open = 0;
            bk_opt_open = 1;
        } else if (b == BK_L1 || b == BK_R1)
            bk_help_page = (bk_help_page + 5 + (b == BK_R1 ? 1 : -1)) % 5;
        else if (b != BK_FN && b != BK_R2)
            bk_help_open = 0;
        bk_tgt[b] = t;
        return;
    }
    if (b == BK_L2)                              /* (L2 alone, tapped: the quick bar; held with others: fine steps) */
        bk_l2_clean = !bk_quick_open, bk_l2_at = bk_ms();
    else
        bk_l2_clean = 0;
    if (bk_quick_open) {
        bk_quick_press(b);
        bk_tgt[b] = t;
        return;
    }
    if (bk_menu_open) {
        bk_menu_press(b);
        bk_tgt[b] = t;
        return;
    }
    if (play && bk_degree_of(b) >= 0) {
        t.kind = T_NOTE;
        bk_note_down(b, shift);
    } else if (bk_view == V_SEQ && sq_edit && !lab_open && shift && !play &&
               (b == BK_UP || b == BK_DOWN || b == BK_LEFT || b == BK_RIGHT || b == BK_L1 || b == BK_R1 || b == BK_A ||
                b == BK_X || b == BK_Y)) {
        t = (bk_target){T_VIEWKEY, (int8_t)(b + SQ_SHIFT), 0};   /* the sequence tools (R2 held) */
    } else if (bk_view == V_TOYS && !lab_open && shift && !play && (b == BK_X || b == BK_A)) {
        t = (bk_target){T_VIEWKEY, (int8_t)(b + SQ_SHIFT), 0};   /* the playground's KEEP and its undo (R2 held) */
    } else if (bk_view && !lab_open && !shift && (b == BK_UP || b == BK_DOWN || b == BK_LEFT || b == BK_RIGHT ||
                                                  b == BK_L1 || b == BK_R1 || b == BK_A || b == BK_B ||
                                                  ((bk_view == V_SEQ || bk_view == V_MOD || bk_view == V_VOICE || bk_view == V_SPACE || bk_view == V_ARCHIVE || bk_view == V_TOYS) && b == BK_X) ||
                                                  ((bk_view == V_WORLDS || bk_view == V_ARCHIVE || bk_view == V_ATLAS || bk_view == V_MOD || bk_view == V_DREAM || bk_view == V_TOYS) && b == BK_Y) ||
                                                  (bk_view == V_SEQ && b == BK_Y && sq_edit))) {
        t = (bk_target){T_VIEWKEY, (int8_t)b, 0};
    } else if (lab_open && !shift && (b == BK_UP || b == BK_DOWN || b == BK_LEFT || b == BK_RIGHT || b == BK_L1 ||
                                       b == BK_R1 || b == BK_A || b == BK_B || b == BK_F1 || b == BK_X || b == BK_Y ||
                                       b == BK_F2)) {
        switch (b) {
        case BK_F2: t = (bk_target){T_LABMAP, 0, 0}; break;   /* (1.5) all modules at once */
        case BK_X: t = (bk_target){T_LABHUSH, 0, 0}; break;   /* (1.3) the module silent and back: before / after */
        case BK_Y: t = (bk_target){T_LABNEXT, 0, 1}; break;   /* (1.3) on to the next module that sounds */
        case BK_UP: t = (bk_target){T_LABFOCUS, 0, (bk_held >> BK_L2) & 1u ? -5 : -1}; break;   /* (L2 held: five rows) */
        case BK_DOWN: t = (bk_target){T_LABFOCUS, 0, (bk_held >> BK_L2) & 1u ? 5 : 1}; break;
        case BK_LEFT: t = (bk_target){T_LABVAL, 0, -1}; break;
        case BK_RIGHT: t = (bk_target){T_LABVAL, 0, 1}; break;
        case BK_L1: t = (bk_target){T_LABMOD, 0, -1}; break;
        case BK_R1: t = (bk_target){T_LABMOD, 0, 1}; break;
        case BK_A: t = (bk_target){T_LABA, 0, 0}; break;
        case BK_B: t = (bk_target){T_BACK, 0, 0}; break;   /* B: back where we came from */
        default: t = (bk_target){T_LAB, 0, 0}; break;     /* F1: the Lab closed */
        }
    } else if (shift) {
        switch (b) {
        case BK_A: t = (bk_target){T_BUTTON, B_ARP, 0}; break;
        case BK_B: t = (bk_target){T_BUTTON, B_LFO, 0}; break;
        case BK_X: t = (bk_target){T_BUTTON, B_ENV, 0}; break;
        case BK_Y: t = (bk_target){T_BUTTON, B_GLO, 0}; break;
        case BK_START: case BK_F1: t = (bk_target){T_BUTTON, B_REC, 0}; break;
        case BK_L1: t = (bk_target){T_BUTTON, B_OCTDN, 0}; break;
        case BK_R1: t = (bk_target){T_BUTTON, B_OCTUP, 0}; break;
        case BK_SELECT: t = (bk_target){T_SAVE, 0, 0}; break;
        case BK_UP: t = (bk_target){T_ENC, EN_ALGO, -1}; break;
        case BK_DOWN: t = (bk_target){T_ENC, EN_ALGO, 1}; break;
        case BK_LEFT: t = (bk_target){T_ENC, EN_PRESET, -1}; break;
        case BK_RIGHT: t = (bk_target){T_ENC, EN_PRESET, 1}; break;
        case BK_MENU: t = (bk_target){T_DREAM, 0, 0}; break;
        case BK_F2: t = (bk_target){T_ATLAS, 0, 0}; break;
        }
    } else {
        switch (b) {
        case BK_A: t = (bk_target){T_BUTTON, B_SEL, 0}; break;
        case BK_B:                               /* BACK: one step the way we came (a question or a help on the
                                                    screen: the FM-1's HOME, which answers no / closes it) */
            if (dream_ui.help < 0 && md_pending_biome() < 0 && bank_pending < 0)
                t = (bk_target){T_BACK, 0, 0};
            else
                t = (bk_target){T_BUTTON, B_HOME, 0};
            break;
        case BK_X: t = (bk_target){T_BUTTON, B_EDIT, 0}; break;
        case BK_Y: t = (bk_target){T_BUTTON, B_FX, 0}; break;
        case BK_START: t = (bk_target){T_BUTTON, B_PLAY, 0}; break;
        case BK_SELECT: t = (bk_target){T_SELHELP, 0, 0}; break;   /* tap: where am I; held: the help */
        case BK_F1: t = (bk_target){T_LAB, 0, 0}; break;
        case BK_L1: t = (bk_target){T_ENC, EN_SELECT, -1}; break;
        case BK_R1: t = (bk_target){T_ENC, EN_SELECT, 1}; break;
        case BK_UP: t = (bk_target){T_FOCUS, 0, -1}; break;
        case BK_DOWN: t = (bk_target){T_FOCUS, 0, 1}; break;
        case BK_LEFT: t = (bk_target){T_ENC, -1, -1}; break;      /* -1: the focused K */
        case BK_RIGHT: t = (bk_target){T_ENC, -1, 1}; break;
        case BK_MENU: t = (bk_target){T_MENU, 0, 0}; break;
        case BK_F2: t = (bk_target){T_MIXER, 0, 0}; break;
        }
    }
    bk_tgt[b] = t;
    bk_down_at[b] = bk_ms();
    bk_next_rep[b] = bk_ms() + (t.kind == T_ENC && t.arg == EN_PRESET ? 450u : BK_REP_WAIT[bk_rep_speed]);
    if (t.kind == T_BACK)
        bk_back();
    else if (t.kind == T_VIEWKEY)
        bk_view_key(t.arg, 1);
    else if (t.kind == T_ATLAS)
        bk_view_open(V_ATLAS);
    else if (t.kind == T_DREAM)
        bk_view_open(V_DREAM);
    else if (t.kind == T_LAB) {
        lab_open = !lab_open;
        if (lab_open)
            bk_view = V_NONE;
    }
    else if (t.kind == T_LABMOD) {
        lab_mod = (lab_mod + M_COUNT + t.dir) % M_COUNT;
        if ((bk_held >> (t.dir < 0 ? BK_R1 : BK_L1)) & 1u)   /* (1.5) L1 and R1 together: the module map (the first one's
                                                                step is undone by this one's) */
            lab_map_open();
    } else if (t.kind == T_LABMAP)
        lab_map_open();
    else if (t.kind == T_LABA)
        lab_a();
    else if (t.kind == T_LABHUSH)
        lab_hush(lab_mod);
    else if (t.kind == T_LABNEXT) {
        int k;
        for (k = 1; k < M_COUNT && !lab_active((lab_mod + k) % M_COUNT); k++)
            ;
        if (k < M_COUNT)
            lab_mod = (lab_mod + k) % M_COUNT;
        else
            bk_notice("NOTHING ELSE SOUNDS", "THE LAB'S OTHER MODULES ARE SILENT");
    }
    else if (t.kind == T_LABFOCUS || t.kind == T_LABVAL) {
        int ps[P_COUNT], np = lab_params(lab_mod, ps);
        if (t.kind == T_LABFOCUS)
            lab_focus[lab_mod] = clamp(lab_focus[lab_mod] + t.dir, 0, np - 1);
        else
            lab_turn(t.dir, 1);
    } else if (t.kind == T_ENC || t.kind == T_FOCUS) {
        if (t.kind == T_FOCUS)
            bk_focus = clamp(bk_focus + t.dir, 0, 3);
        else {
            int role = t.arg < 0 ? EN_K1 + bk_focus : t.arg;
            web_enc(role, t.dir);
            if (t.arg < 0) {
                bk_focus_dir[bk_focus] = t.dir;
                bk_focus_at[bk_focus] = bk_ms();
                bk_turns++, bk_turn_dir = t.dir;
            }
        }
    } else if (t.kind == T_MENU) {
        bk_menu_open = 1;
    } else if (t.kind == T_MIXER)
        bk_view_open(V_MIXER);                   /* the full MIXER view (the FM-1 page: MENU > MIXER (FM-1 PAGE)) */
}

static void bk_release(int b)
{
    if (b == BK_L2 && bk_l2_clean && bk_ms() - bk_l2_at < 500u && !bk_menu_open && !bk_help_open) {
        int q = bk_left_set ? bk_quick_for(bk_left) : -1;
        if (q >= 0)
            bk_quick_sel = q;                    /* (the place last left first: L2, A goes back and forth) */
        bk_quick_open = 1;
        bk_l2_clean = 0;
    }
    if (bk_tgt[b].kind == T_SELHELP && bk_handheld)   /* (1.5.8) a handheld has no MENU key: SELECT tapped opens the
                                                    menu (every place, OPTIONS, QUIT) - one button, as Kid Bunka asked */
        bk_menu_open = 1;
    else if (bk_tgt[b].kind == T_SELHELP)        /* SELECT tapped: where am I, and the way on (until 0.60 it turned
                                                    the FM-1 to its sequence page unseen: a slip took you elsewhere) */
        bk_notice(bk_place_name(), bk_nhist ? "B: BACK   L2: ALL PLACES   HOLD SELECT: HELP" : "L2: ALL PLACES   HOLD SELECT: HELP");
    if (bk_tgt[b].kind == T_NOTE)
        bk_note_up(b);
    if (bk_tgt[b].kind == T_VIEWKEY && bk_tgt[b].arg == BK_A)
        vo_listen(0, 0);                         /* the VOICE view's listening note ends */
    bk_tgt[b].kind = T_NONE;
}

/* a held turn speeds up without a jolt (0.62): the gap between steps shrinks from 90 to 35 ms over the first second
   and a half, only then it takes 2, later 4 steps at a time (before: 70 ms at once, then 2 and 4 in jumps) */
static int bk_rep(uint32_t held, int fine, int most, uint32_t *gap)
{
    int g = 90 - (int)(held > 280u ? (held - 280u) / 22u : 0u);
    if (fine) {
        *gap = 140u;
        return 1;
    }
    g = g * (int)BK_REP_GAP[bk_rep_speed] / 100;   /* (1.1: the chosen repeat speed) */
    *gap = (uint32_t)(g < 30 ? 30 : g);
    return held > 3500u ? most : held > 2200u ? 2 : 1;
}
/* repeats of held encoder controls, SAVE's hold, the sticks; then the FM-1 panel's held buttons */
static void bk_tick(uint32_t dt_ms)
{
    uint32_t now = bk_ms(), btn = 0;
    ar_poll();                                   /* an ARCHIVE fetch, followed */
    if (bk_help_first && !bk_help_done && !sp_on && bk_dir[0]) {   /* the very first start: the button map, once */
        char path[600];
        FILE *f;
        snprintf(path, sizeof path, "%s/buttons-seen.txt", bk_dir);
        if (!(f = fopen(path, "r"))) {           /* (0.73: the guide, which leads to the button map) */
            bk_guide_open = 1;
            bk_guide_page = 0;
            if ((f = fopen(path, "w")))
                fputs("the guide was shown once (SELECT held, then X, shows it again)\n", f);
        }
        if (f)
            fclose(f);
        bk_help_done = 1;
    }
    {   /* the sound near its limit for 3 s on end (a weaker handheld, many modules): said once a minute, with
           what costs most - before it starts to crackle */
        static uint32_t high_since, said_at;
        if (web_cpu() >= 85u) {
            if (!high_since)
                high_since = bk_stamp(now);
            else if (now - high_since > 3000u && (!said_at || now - said_at > 60000u)) {
                bk_notice("SOUND AT ITS LIMIT", lab_active(M_PARTICLES) && lab_v[P_PA_MODE] >= 2.5f ? "SPECTRAL PARTICLES COST MOST"
                                              : lab_active(M_SEEDS) ? "SEEDS AND PARTICLES COST MOST" : "FEWER SOUND LAB MODULES AT ONCE");
                said_at = bk_stamp(now);
            }
        } else
            high_since = 0;
    }
    wd_poll();                                   /* a WORLD arriving, wandering */
    dm_tick();                                   /* the Dream Map's free flight: catching stars */
    ty_tick();                                   /* the PLAYGROUND's toys, on the sound's clock */
    st_tick();                                   /* DREAM TALES: a chapter's deed done? */
    {   /* (1.1) START + SELECT held 3 s (the physical ones): every button itself again - the way out of any mapping */
        static uint32_t esc_at;
        static int esc_done;
        const uint32_t both = (1u << BK_START) | (1u << BK_SELECT);
        if ((bk_phys_now & both) == both) {
            if (!esc_at)
                esc_at = bk_stamp(now);
            else if (!esc_done && now - esc_at >= 3000u) {
                bk_map_reset(), bk_opt_capture = -1, st_dirty = 1, esc_done = 1;
                bk_notice("BUTTONS RESET", "EVERY BUTTON IS ITSELF AGAIN");
            }
        } else
            esc_at = 0, esc_done = 0;
    }
    {   /* (1.1) held arrows keep going in the menu, the quick bar, the guide and the options - after a wait, then
           closer and closer (before: one step a press, a long list tapped through) */
        static const int dirs[6] = {BK_UP, BK_DOWN, BK_LEFT, BK_RIGHT, BK_L1, BK_R1};
        static uint32_t since[6], next_at[6];
        int over = bk_menu_open || bk_quick_open || bk_guide_open || bk_opt_open || (lab_mapview && lab_open);
        for (int k2 = 0; k2 < 6; k2++) {
            int d = dirs[k2];
            if (!over || !((bk_held >> d) & 1u) || ((bk_held >> BK_R2) & 1u)) {
                since[k2] = 0;
                continue;
            }
            if (!since[k2]) {
                since[k2] = bk_stamp(now), next_at[k2] = now + BK_REP_WAIT[bk_rep_speed];
                continue;
            }
            if ((int32_t)(now - next_at[k2]) >= 0) {
                uint32_t gap = BK_REP_GAP[bk_rep_speed] * (now - since[k2] > 1200u ? 60u : 100u) / 100u;
                next_at[k2] = now + gap;
                bk_press(d);
                bk_tgt[d].kind = T_NONE;
            }
        }
    }
    int b, k, fine = (bk_held >> BK_L2) & 1u;
    for (b = 0; b < BK_NBUTTONS; b++) {
        bk_target *t = &bk_tgt[b];
        if (t->kind == T_NONE)
            continue;
        if (t->kind == T_BUTTON)
            btn |= 1u << t->arg;
        if ((t->kind == T_ENC || t->kind == T_FOCUS) && (int32_t)(now - bk_next_rep[b]) >= 0) {
            uint32_t held = now - bk_down_at[b];
            int preset = t->kind == T_ENC && t->arg == EN_PRESET;
            uint32_t gap;
            int n = bk_rep(held, fine, 4, &gap);
            if (preset)
                n = 1, gap = 260u;
            bk_next_rep[b] = now + gap;
            if (t->kind == T_FOCUS)
                bk_focus = clamp(bk_focus + t->dir, 0, 3);
            else {
                web_enc(t->arg < 0 ? EN_K1 + bk_focus : t->arg, t->dir * n);
                if (t->arg < 0) {
                    bk_focus_dir[bk_focus] = t->dir;
                    bk_focus_at[bk_focus] = now;
                    bk_turns++, bk_turn_dir = t->dir;
                }
            }
        }
        if (t->kind == T_VIEWKEY && t->arg >= 0 && t->arg <= BK_RIGHT && (int32_t)(now - bk_next_rep[b]) >= 0) {
            uint32_t held = now - bk_down_at[b];
            uint32_t gap;
            int n = bk_rep(held, fine, 4, &gap);
            bk_next_rep[b] = now + gap;
            bk_view_key(t->arg, n);
        }
        if ((t->kind == T_LABFOCUS || t->kind == T_LABVAL) && (int32_t)(now - bk_next_rep[b]) >= 0) {
            uint32_t held = now - bk_down_at[b];
            uint32_t gap;
            int n = bk_rep(held, fine, 5, &gap);
            if (t->kind == T_LABFOCUS)
                gap = fine ? 140u : 100u;
            bk_next_rep[b] = now + gap;
            if (t->kind == T_LABFOCUS) {
                int ps[P_COUNT], np = lab_params(lab_mod, ps);
                lab_focus[lab_mod] = clamp(lab_focus[lab_mod] + t->dir, 0, np - 1);
            } else
                lab_turn(t->dir, n);
        }
        if (b == BK_B && (t->kind == T_BACK || (t->kind == T_VIEWKEY && t->arg == BK_B)) && now - bk_down_at[b] >= 700u) {
            bk_home();                           /* B held: home at once, however deep (0.64) */
            t->kind = T_NONE;
        }
        if (t->kind == T_SELHELP && now - bk_down_at[b] >= 600u) {   /* SELECT held: the help, for where we are */
            bk_help_open = 1;
            bk_help_page = (bk_held >> BK_FN) & 1u ? 3 : lab_open ? 2 : bk_view ? 4 : 0;
            t->kind = T_NONE;
        }
        if (t->kind == T_SAVE && now - bk_down_at[b] >= 1000u) {
            bk_pulse_btn = 1u << B_SAVE;
            bk_pulse_until = now + 60u;
            t->kind = T_NONE;                    /* one save per hold */
        }
    }
    bk_touch_tick(now);
    if (bk_opt_open || bk_menu_open || bk_quick_open) {   /* (1.6) the left stick is a cursor in the menus: tilt = one step, held = repeat */
        static int dir_held;                     /* 0 none, else the BK_ button */
        static uint32_t dir_next;
        int want = 0;
        float sx = bk_stick[0], sy = bk_stick[1];
        if (fabsf(sy) > fabsf(sx) && fabsf(sy) > 0.6f)
            want = sy > 0 ? BK_UP : BK_DOWN;
        else if (fabsf(sx) > 0.6f)
            want = sx > 0 ? BK_RIGHT : BK_LEFT;
        if (!want)
            dir_held = 0;
        else if (want != dir_held || now >= dir_next) {
            dir_next = now + (want != dir_held ? 420u : 160u);
            dir_held = want;
            bk_press(want);
        }
        bk_stick_acc[0] = bk_stick_acc[1] = 0;
    }
    for (k = (bk_opt_open || bk_menu_open || bk_quick_open) ? 2 : 0; k < 4; k++) {   /* Brick Pro: deflection = speed, up to 24 detents a second */
        float v = bk_stick[k], a = fabsf(v);
        if (a < 0.18f) {
            bk_stick_acc[k] = 0;
            continue;
        }
        bk_stick_acc[k] += (v > 0 ? 1.0f : -1.0f) * (a - 0.18f) / 0.82f * 24.0f * (float)dt_ms / 1000.0f;
        while (bk_stick_acc[k] >= 1.0f || bk_stick_acc[k] <= -1.0f) {
            int d = bk_stick_acc[k] > 0 ? 1 : -1;
            web_enc(EN_K1 + k, d);
            bk_turns++, bk_turn_dir = d;
            bk_stick_acc[k] -= (float)d;
            bk_focus_dir[k] = d;
            bk_focus_at[k] = now;
        }
    }
    if (bk_listen_sel && !bk_resting && md_playing() && !lab_open) {   /* (1.6) LISTEN MODE: a while without a button */
        uint32_t wait = bk_listen_test_ms ? bk_listen_test_ms : (uint32_t)BK_LISTEN_MIN[bk_listen_sel] * 60000u;
        if (now - bk_listen_at > wait) {
            bk_resting = 1, bk_listen_rest = 1;
            bk_menu_open = bk_help_open = bk_guide_open = bk_opt_open = bk_quick_open = lab_mapview = bk_journal_on = 0;
        }
    }
    {   /* (1.6) the journal: a Biome or world counts once it has been there for a moment */
        static int seen_b = -1, seen_w = -1;
        static uint32_t since_b, since_w;
        int bid = (int)md_state()->biome_id, wid = bk_world_current();
        if (bid != seen_b)
            seen_b = bid, since_b = now;
        else if (bid >= 0 && bid < MD_BIOMES && now - since_b > 3000u)
            bk_jr_set(bk_jr_b, bid);
        if (wid != seen_w)
            seen_w = wid, since_w = now;
        else if (wid >= 0 && wid < 128 && now - since_w > 3000u)
            bk_jr_set(bk_jr_w, wid);
    }
    if (bk_sleep_end && !bk_resting) {          /* (1.4) the sleep timer: the last minute fades, then it stops */
        int32_t left = (int32_t)(bk_sleep_end - now);
        if (left <= 0) {
            if (md_playing())
                bk_pulse_btn = 1u << B_PLAY, bk_pulse_until = now + 60u;
            web_master(0);
            bk_resting = 1, bk_sleep_end = 0, bk_sleep_sel = 0;
            bk_menu_open = bk_help_open = bk_guide_open = bk_opt_open = bk_quick_open = lab_mapview = 0;
        } else if ((uint32_t)left < bk_sleep_fade) {
            float g = (float)left / (float)bk_sleep_fade;
            web_master((uint32_t)(4096.0f * g * g));
        }
    }
    if (bk_pulse_btn && (int32_t)(now - bk_pulse_until) < 0)
        btn |= bk_pulse_btn;
    else
        bk_pulse_btn = 0;
    web_buttons(btn);
    bk_track_place();
}

static int sp_on;                                /* the start screen (brick_splash.inc) */
static void sp_end(void);

void bk_input(uint32_t held)
{
    uint32_t ch;
    {   /* (1.5.8) a physical button changed: when, for the columns at half pace */
        static uint32_t raw_was;
        if (held != raw_was)
            raw_was = held, bk_input_at = bk_ms();
    }
    {   /* (1.1) the physical buttons as the player has them: a button being assigned, the way back, the mapping */
        static uint32_t phys_was;
        uint32_t newly = held & ~phys_was, m = 0;
        phys_was = held;
        bk_phys_now = held;                      /* (START + SELECT held: watched in bk_tick) */
        if (!bk_map_set)
            bk_map_reset();
        if (bk_opt_capture >= 0 && newly) {      /* OPTIONS > ASSIGN: the next button pressed takes the part */
            for (int k = 0; k < 10; k++)
                if ((newly >> BK_REMAP[k]) & 1u) {
                    bk_map_assign(bk_opt_capture, BK_REMAP[k]);
                    st_dirty = 1;
                    bk_notice("ASSIGNED", "");
                    bk_opt_capture = -1;
                    break;
                }
            for (int i = 0; i < BK_NBUTTONS; i++)   /* (that press does nothing else) */
                if ((held >> i) & 1u)
                    m |= 1u << bk_map[i];
            bk_held = m | bk_fn_soft;
            return;
        }
        for (int i = 0; i < BK_NBUTTONS; i++)
            if ((held >> i) & 1u)
                m |= 1u << bk_map[i];
        held = m;
    }
    held |= bk_fn_soft;                          /* (play mode from the quick bar stands for the FN switch) */
    ch = held ^ bk_held;
    int b;
    if (ch)
        bk_listen_at = bk_ms();
    if (bk_resting) {                            /* (1.4) resting after the sleep timer: any button wakes, nothing else */
        if (held & ~bk_held & ~(1u << BK_FN)) {
            bk_resting = 0;
            web_master(4096u);
            if (bk_listen_rest)
                bk_notice("WELCOME BACK", "THE MUSIC PLAYED ON");
            else
                bk_notice("GOOD MORNING", "START: PLAY AGAIN");
            bk_listen_rest = 0;
        }
        bk_held = held;
        return;
    }
    if (sp_on == 1) {                            /* any button ends the start screen, and does nothing else */
        if (held & ~bk_held & ~(1u << BK_FN))
            sp_end();
        bk_held = held;
        return;
    }
    /* R2 and FN first: a control pressed together with them is decided with them */
    bk_held = (bk_held & ~((1u << BK_R2) | (1u << BK_FN))) | (held & ((1u << BK_R2) | (1u << BK_FN)));
    if ((ch >> BK_FN) & 1u)                      /* the switch moved: what play mode held is let go */
        for (b = 0; b < BK_NBUTTONS; b++)
            if (bk_tgt[b].kind == T_NOTE)
                bk_release(b);
    for (b = 0; b < BK_NBUTTONS; b++) {
        if (b == BK_FN || !((ch >> b) & 1u))
            continue;
        if ((held >> b) & 1u) {
            bk_held |= 1u << b;
            bk_press(b);
        } else {
            bk_held &= ~(1u << b);
            bk_release(b);
        }
    }
    bk_held = held;
    bk_tick(0);
}

void bk_sticks(int lx, int ly, int rx, int ry)
{
    bk_stick[0] = (float)lx / 32768.0f;
    bk_stick[1] = (float)-ly / 32768.0f;        /* up = more */
    bk_stick[2] = (float)rx / 32768.0f;
    bk_stick[3] = (float)-ry / 32768.0f;
}

/* -------------------------------------------------------------- sound --- */
/* (0.71) the FM-1 draws its own 240 x 240 screen every 58-96 ms inside the sound's rendering (app_frame, called by
   the unchanged plat_host.c between the blocks) - some 15-20 % of the sound thread's time. Where the stage does not
   show that screen (the garden, the Sound Lab, the full views, the playground) the drawing waits: its next frame
   is pushed on before each block (dream_ui.frame_at), the input and everything else in app_frame still runs.
   Shown again, the screen is drawn at once (dirty). The sound is not touched: the same blocks, bit for bit */
static int getenv_fm1_always(void)               /* MD_FM1_ALWAYS=1: the FM-1 draws as before (to compare) */
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("MD_FM1_ALWAYS");
        v = e && *e == '1';
    }
    return v;
}
int bk_garden_visible(void);
static void bk_fm1_quiet(void)
{
    static int was_quiet;
    if (lab_open || bk_view || bk_garden_visible()) {
        dream_ui.frame_at = plat_ms();
        dream_ui.dirty = 0;
        was_quiet = 1;
    } else if (was_quiet) {
        dream_ui.dirty = 1;
        was_quiet = 0;
    }
}

void bk_render(float *lr, uint32_t n)
{
    while (n) {
        uint32_t m = n > 256u ? 256u : n, before = bk_ms();
        if (!getenv_fm1_always())
            bk_fm1_quiet();
        web_render_lr(lr, m);
        lr += 2 * m;
        n -= m;
        bk_frames += m;
        bk_tick(bk_ms() - before);
        bk_store_keep();
        lab_keep();
    }
}

/* ------------------------------------------------------------- status --- */
static const char *const BK_PAGE_NAMES[DREAM_PAGE_COUNT] = {
    "LIVE", "SOUND", "RHYTHM", "GRAIN", "MIX", "SYSTEM", "ENVELOPE", "GROOVE", "TAPE", "HARMONY", "FILTER",
    "APPEARANCE", "MIXER", "BASSLINE", "SPACE", "MODULATION", "PHRASE", "MIDI", "BANKS", "SEQUENCE", "STEP DETAIL",
    "PROGRESSION", "CLOUD", "KIT", "KIT PAD", "KIT SEND", "KIT SEQ", "SEQ FLOW", "LFO ROUTE", "LFO ROUTE 2",
    "BLUETOOTH", "JOURNEY", "JOURNEY TIME", "JOURNEY KEEP", "MIDI LEARN", "MIDI MAP", "LIVING LINKS", "LINK MOTION",
    "BIOME DRIFT", "DRIFT SCENES", "SEQ GRID", "SEQ TOOLS", "SEQ RULES", "USB AUDIO", "ITINERA"};
const char *bk_page_name(void)
{
    static const char *const views[V_COUNT] = {"", "MIXER VIEW", "BIOME ATLAS", "SEQUENCE VIEW", "DREAM MAP", "MODULATION VIEW", "VOICE VIEW", "SPACE VIEW", "ARCHIVE", "WORLDS", "PLAYGROUND"};
    if (bk_view)
        return views[bk_view];
    return dream_ui.page >= 0 && dream_ui.page < DREAM_PAGE_COUNT ? BK_PAGE_NAMES[dream_ui.page] : "?";
}
const char *bk_where(void) { return bk_place_name(); }
const char *bk_biome_name(void) { return md_biome_name(md_state()->biome_id); }
int bk_playing(void) { return md_playing(); }
uint32_t bk_cpu_pct(void) { return web_cpu(); }
const uint16_t *bk_fm1_screen(void) { return web_fb(); }
/* (studio) load a Biome by its number, as the atlas does: the change is pending and settles over the next
   blocks (the engine's own scene load, with the fade) */
void bk_scene(int biome_id) { dream_cmd(MD_SCENE, 0, biome_id); }
int bk_biome_count(void) { return MD_BIOMES; }
/* (studio) the transport: 1 play, 0 stop (the same as the START key) */
void bk_play(int on) { dream_cmd(MD_PLAY, 0, on); }

/* -------------------------------------------------------------- start --- */
void bk_init(const char *save_dir)
{
    int i;
    snprintf(bk_dir, sizeof bk_dir, "%s", save_dir ? save_dir : "");
    for (i = 0; i < BK_NBUTTONS; i++)
        bk_tgt[i].kind = T_NONE;
    bk_store_load();
    bk_store_seen = web_store_writes();
    lab_init();
    lab_load();
    {   /* (0.79, Kid Bunka's wish) the ambience beds start silent at every start - forest, air (the wind), water,
           stardust: mixed in by hand, or by a WORLD that brings them; never left on from an earlier session */
        static const char *const beds[4] = {"TEXTURES.FOREST", "TEXTURES.AIR", "TEXTURES.WATER", "TEXTURES.STARDUST"};
        for (int k = 0; k < 4; k++) {
            int pi = bk_lab_find(beds[k]);
            if (pi >= 0)
                lab_v[pi] = 0;
        }
    }
    sa_scan();
    lab_v[P_SA_FILE] = (float)sa_kept_index();
    for (i = 0; i < P_COUNT; i++)
        lab_prev[i] = LAB_P[i].def;
    web_master(4096u);                           /* the Brick's own volume keys set the level */
    web_boot();
}

static int bk_lab_find(const char *what)
{
    const char *dot = strchr(what, '.');
    int m, p;
    if (!dot)
        return -1;
    for (m = 0; m < M_COUNT; m++)
        if (strlen(LAB_MOD_NAMES[m]) == (size_t)(dot - what) && !strncmp(what, LAB_MOD_NAMES[m], (size_t)(dot - what)))
            for (p = 0; p < P_COUNT; p++)
                if (LAB_P[p].mod == m && !strcmp(LAB_P[p].name, dot + 1))
                    return p;
    return -1;
}
int bk_lab_set(const char *what, float value)
{
    int p = bk_lab_find(what);
    if (p < 0)
        return -1;
    lab_set(p, value);
    return 0;
}
float bk_lab_get(const char *what)
{
    int p = bk_lab_find(what);
    return p < 0 ? NAN : lab_v[p];
}

/* ---------------------------------------------------------------- MIDI --- */
static uint32_t bk_midi_seen_at, bk_midi_count;
static int bk_midi_ndev;
static char bk_midi_name[48];
void bk_midi_devices(int count, const char *first_name)
{
    bk_midi_ndev = count;
    snprintf(bk_midi_name, sizeof bk_midi_name, "%s", first_name ? first_name : "");
}

void bk_midi_in(const uint8_t *m, int len)
{
    uint8_t st = len > 0 ? m[0] : 0, d1 = len > 1 ? m[1] & 127u : 0, d2 = len > 2 ? m[2] & 127u : 0;
    int ch = st & 15, sel = (int)lab_v[P_PO_MIDI];
    int to_poly = st >= 0x80 && st < 0xF0 && lab_v[P_PO_LEVEL] > 0 && sel > 0 && (sel == 1 || ch == sel - 2);
    if (len <= 0 || st < 0x80)
        return;
    bk_midi_seen_at = bk_ms();
    bk_midi_count++;
    if (to_poly) {
        switch (st & 0xF0) {
        case 0x90:
            if (d2) {
                po_note_on_vel(d1, 0, d2);
                return;
            }
            /* (velocity 0: a note off) */
        case 0x80:
            po_note_off(d1);
            return;
        case 0xB0:
            if (d1 == 64)
                po_pedal(d2 >= 64);
            else if (d1 == 1)
                po_mod = (float)d2 / 127.0f;
            else if (d1 == 120 || d1 == 123) {
                po_pedal(0);
                po_all_off();
            }
            break;                               /* (controllers also reach the engine: MIDI learn) */
        case 0xE0:
            po_bend = ((float)(((int)d2 << 7) | d1) - 8192.0f) / 8192.0f * 2.0f;
            return;
        }
    }
    web_midi_in((uint32_t)(st >= 0xF0 ? 0x0F : st >> 4) | (uint32_t)st << 8 | (uint32_t)d1 << 16 | (uint32_t)d2 << 24);
}

/* (0.73) a MIDI device gone (unplugged): everything it held is let go - the engine's notes from that port
   (md_midi_lost, the FM-1's own release for a lost transport), POLY's notes and its sustain pedal. Without this a
   key held while the cable was pulled stayed sounding (the case Garden had) */
void bk_midi_lost(void)
{
    md_midi_lost(PORT_USB);
    po_pedal(0);
    po_all_off();
    bk_midi_seen_at = bk_ms();
}

int bk_midi_out(uint8_t *out, int max)
{
    int n = 0;
    uint32_t i = 0;
    for (; i < bk_tx_n; i++) {
        uint32_t pkt = bk_tx[i], st = (pkt >> 8) & 0xFFu, len = st >= 0xF8 ? 1 : (st & 0xE0) == 0xC0 ? 2 : st >= 0xF0 ? 1 : 3;
        if (n + (int)len > max)
            break;
        out[n++] = (uint8_t)st;
        if (len > 1)
            out[n++] = (uint8_t)(pkt >> 16);
        if (len > 2)
            out[n++] = (uint8_t)(pkt >> 24);
    }
    memmove(bk_tx, bk_tx + i, (bk_tx_n - i) * sizeof bk_tx[0]);
    bk_tx_n -= i;
    return n;
}

int bk_poly_voices(int held_only)
{
    int n = 0;
    for (int i = 0; i < PO_VOICES; i++)
        n += po_v[i].on && (!held_only || po_v[i].gate);
    return n;
}
uint32_t bk_track_hits(int track) { return md_status()->events[track % 6]; }
void bk_sync_loading(int on) { vo_sync = on; }
void bk_help_first_run(int on) { bk_help_first = on; }
int bk_help_shown(void) { return bk_help_open ? bk_help_page + 1 : 0; }
int bk_guide_shown(void) { return bk_guide_open ? bk_guide_page + 1 : 0; }
void bk_guide_show(int on) { bk_guide_open = on != 0, bk_guide_page = 0; }
int bk_play_mode(void) { return (bk_held >> BK_FN) & 1u; }
int bk_menu_shown(void) { return bk_menu_open != 0; }
void bk_list_biomes(void)                        /* (the test runner: every Biome, for writing worlds) */
{
    static const char *const notes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    for (int id = 0; id < MD_BIOMES; id++) {
        md_biome_card c;
        if (!md_biome_card_info(id, &c))
            continue;
        printf("%d\t%s\t%s\t%d\t%s %s\n", id, md_biome_name(id), md_biome_family_name((unsigned)md_biome_family_index(id)),
               c.tempo, notes[c.root % 12], md_scale_name(c.scale));
    }
}
int bk_voices_ready(void) { return vo_cur && !vo_busy && vo_cur->index == (int)lab_v[P_VO_FILE] ? (vo_cur->ok ? vo_cur->nph : -1) : 0; }
int bk_track_value(int track, const char *what)
{
    static const char *const names[MX_ROWS] = {"level", "pan", "send", "delay", "reverb"};
    track %= 6;
    static const char *const lfo[MF_COUNT] = {"a_target", "a_shape", "a_depth", "a_rate", "b_target", "b_shape", "b_depth", "b_rate"};
    if (!strcmp(what, "mute"))
        return md_state()->track[track].mute;
    {
        const md_sequence *q = &md_state()->sequence[track];
        const md_step *st = &q->steps[q->edit];
        if (!strcmp(what, "seq_enabled")) return q->enabled;
        if (!strcmp(what, "seq_length")) return q->length;
        if (!strcmp(what, "seq_edit")) return q->edit;
        if (!strcmp(what, "seq_note")) return st->note;
        if (!strcmp(what, "seq_velocity")) return st->velocity;
        if (!strcmp(what, "seq_generator")) return q->generator;
        if (!strcmp(what, "seq_rule")) return (int)md_step_rule(st);
        if (!strcmp(what, "seq_sum")) {          /* (the notes summed: a transpose or a paste shows) */
            int sum = 0;
            for (int i = 0; i < q->length; i++)
                sum += q->steps[i].note;
            return sum;
        }
        if (!strcmp(what, "seq_first")) {        /* the first sounding step (a mirror shows) */
            for (int i = 0; i < q->length; i++)
                if (q->steps[i].note)
                    return i;
            return -1;
        }
    }
    {   /* (the links: "track" is the link's number) */
        static const char *const lk[LF_COUNT] = {"link_source", "link_signal", "link_dest", "link_target", "link_depth", "link_release"};
        for (int f = 0; f < LF_COUNT; f++)
            if (!strcmp(what, lk[f]))
                return lk_value(track, f);
        if (!strncmp(what, "s_", 2))
            for (int f = 0; f < SF2_COUNT; f++)
                if (!strcmp(what + 2, SP_F[f].key)) return sp_value(f);
        if (!strncmp(what, "v_", 2))
            for (int f = 0; f < VF_COUNT; f++)
                if (!strcmp(what + 2, VF_KEYS[f])) return vf_value(track, f);
        if (!strncmp(what, "macro", 5) && what[5] >= '1' && what[5] <= '4') return md_state()->macro[what[5] - '1'];
        if (!strcmp(what, "drift_mode")) return md_state()->drift.mode;
        if (!strcmp(what, "drift_position")) return md_state()->drift.position;
        if (!strcmp(what, "drift_rate")) return md_state()->drift.rate;
        if (!strcmp(what, "drift_depth")) return md_state()->drift.depth;
        if (!strcmp(what, "drift_captured")) return md_state()->drift.captured;
        if (!strcmp(what, "drift_live")) return md_drift_position();
        if (!strcmp(what, "link_live"))
            return (int)(fabsf(md_link_value((unsigned)(track & 3))) * 1000.0f);
    }
    for (int f = 0; f < MF_COUNT; f++)
        if (!strcmp(what, lfo[f]))
            return mf_value(track, f);
    for (int r = 0; r < MX_ROWS; r++)
        if (!strcmp(what, names[r]))
            return mx_value(track, r);
    return -1;
}

void bk_say(const char *a, const char *b) { bk_notice(a, b); }

void bk_shutdown(void)
{
    bk_store_keep();
    if (lab_dirty)
        lab_save();
}

/* the stage's own, larger faces (tools/gen_stage_font.py); the FM-1 engine above keeps its own */
#include "stage_font.h"
#define FONT_T FONT_ST_T
#define FONT_XS FONT_ST_XS
#define FONT_H FONT_ST_H
#define FONT_S FONT_ST_S
#define FONT_B FONT_ST_B
#define FONT_M FONT_ST_M
#define FONT_L FONT_ST_L
#include "brick_stage.inc"
