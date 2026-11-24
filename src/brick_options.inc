/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: OPTIONS (1.1). The buttons as the player wants them (A and B
 * swapped, X and Y swapped, or any of ten buttons given another's part), how held buttons repeat, the language,
 * the light picture, calm motion, and the way to the button map and the guide. Kept in save/brick-prefs.txt.
 * From the quick bar (L2). UP/DOWN a row, LEFT/RIGHT its value, A does it, B closes. While a button is being
 * assigned, the next button pressed takes the part (6 s, then nothing changes). START + SELECT held 3 s anywhere:
 * every button itself again (brick_core.c bk_input). Included by brick_stage.inc. */

enum { OP_SWAP_AB, OP_SWAP_XY, OP_ASSIGN, OP_REPEAT, OP_SLEEP, OP_LANG, OP_SCALE, OP_CLOCK, OP_LISTEN, OP_TEXT, OP_CONTRAST, OP_TSIZE, OP_TSIDE, OP_TGLOW, OP_TPAD, OP_TMODE, OP_TBUZZ, OP_TLIFT, OP_PERF, OP_LIGHT, OP_CALM, OP_HELP,
       OP_GUIDE, OP_JOURNAL, OP_RESET, OP_COUNT };
#define OP_VIS 11                                /* (1.6) rows shown at once: the list scrolls with the chosen row */
static int bk_opt_top;
static const char *const OP_NAMES[OP_COUNT] = {"SWAP A AND B", "SWAP X AND Y", "ASSIGN A BUTTON", "HELD BUTTONS REPEAT", "SLEEP TIMER",
                                               "LANGUAGE", "SCALE FOR PLAYING", "CLOCK GARDEN", "LISTEN MODE", "TEXT SIZE", "CONTRAST", "TOUCH BUTTON SIZE", "TOUCH BUTTONS SIDE", "TOUCH BUTTON GLOW", "BUTTON PAD ON SCREEN", "TOUCH BUTTONS", "TOUCH BUZZ", "QUICK BAR HEIGHT", "PERFORMANCE METER",
                                               "LIGHT PICTURE", "CALM MOTION", "THE BUTTON MAP", "THE GUIDE", "GARDEN JOURNAL",
                                               "EVERY BUTTON ITSELF AGAIN"};
static const char *const OP_ABOUT[OP_COUNT] = {
    "As on Nintendo-style handhelds: A on the right, B below", "X and Y change places",
    "Choose a part with LEFT/RIGHT, press A, then the button that should play it", "How soon and how fast a held button goes on",
    "The music fades out and stops after this time; the screen rests until a button wakes it",
    "The whole garden in English, German, French or Spanish",
    "The notes of play mode and the playground: the Biome's own scale, or another one of the world's",
    "Evening, night and morning of the garden follow the time on your handheld",
    "After a while without a button the screen rests, dark and frugal, and the music plays on",
    "A little larger small print in the menus, help and options", "Brighter, clearer small print",
    "The size of the buttons on a touch screen", "Swap the left and right button groups on a touch screen",
    "How clearly the touch buttons show on the screen", "Show the Brick's buttons on a touch screen, as a pad (off: the places name their own touch buttons)", "AUTO: they step aside at a controller's button and return at a touch; SHOWN: always; HIDDEN: away while a controller is there", "A short buzz when a touch button is pressed", "Lift the quick bar above the phone's gesture strip",
    "Shows in a corner how busy the sound is, in percent: for trying a new device",
    "Less decoration, a faster picture", "What only decorates holds still",
    "Every button and what it does, on the drawn Brick", "Fourteen pages: how it all works",
    "Every Biome and world you have visited", "Undo every change of buttons"};
static const char *const OP_REP_NAMES[3] = {"CALM", "NORMAL", "QUICK"};
static int bk_opt_row, bk_opt_part;              /* bk_opt_part: which of BK_REMAP the ASSIGN row offers */
static uint32_t bk_opt_cap_at;   /* (buf below: 96, the assign card's line fits) */
static float bk_opt_glide = -1;

static int bk_opt_swapped(int a, int b) { return bk_map_phys(a) == b && bk_map_phys(b) == a; }
static void bk_opt_swap(int a, int b)
{
    if (bk_opt_swapped(a, b))
        bk_map_assign(a, a);                     /* (back: each itself) */
    else
        bk_map_assign(a, b);
    st_dirty = 1;
}

static void bk_opt_press(int b)
{
    if (bk_opt_capture >= 0)                     /* (assigning: brick_core.c takes the next press itself) */
        return;
    if (b == BK_UP || b == BK_DOWN)
        bk_opt_row = (bk_opt_row + OP_COUNT + (b == BK_DOWN ? 1 : -1)) % OP_COUNT;
    else if (b == BK_B || b == BK_MENU)
        bk_opt_open = 0;
    else if (b == BK_LEFT || b == BK_RIGHT || b == BK_A) {
        int d = b == BK_LEFT ? -1 : 1;
        switch (bk_opt_row) {
        case OP_SWAP_AB: bk_opt_swap(BK_A, BK_B); break;
        case OP_SWAP_XY: bk_opt_swap(BK_X, BK_Y); break;
        case OP_ASSIGN:
            if (b == BK_A) {
                bk_opt_capture = BK_REMAP[bk_opt_part];
                bk_opt_cap_at = bk_stamp(bk_ms());
            } else
                bk_opt_part = (bk_opt_part + 10 + d) % 10;
            break;
        case OP_REPEAT:
            bk_rep_speed = clamp(bk_rep_speed + (b == BK_A ? 0 : d), 0, 2);
            if (b == BK_A)
                bk_rep_speed = (bk_rep_speed + 1) % 3;
            st_dirty = 1;
            break;
        case OP_SLEEP:                           /* (1.4) each choice starts the time anew */
            bk_sleep_set((bk_sleep_sel + 5 + (b == BK_LEFT ? -1 : 1)) % 5);
            if (bk_sleep_sel) {
                char t[40];
                snprintf(t, sizeof t, "%d MIN", BK_SLEEP_MIN[bk_sleep_sel]);
                bk_notice("SLEEP TIMER", t);
            } else
                bk_notice("SLEEP TIMER", "OFF");
            break;
        case OP_LANG:
            if (bk_lang_cycle() < 0)
                bk_notice("ENGLISH", "NO LANGUAGE TABLE FOUND (LANG/DE.TXT, FR.TXT, ES.TXT)");
            st_dirty = 1;
            break;
        case OP_SCALE:
            bk_play_scale = (bk_play_scale + BK_NPSCALES + (b == BK_LEFT ? -1 : 1)) % BK_NPSCALES;
            st_dirty = 1;
            bk_notice(BK_PSCALES[bk_play_scale].name, "THE NOTES OF PLAY MODE AND THE PLAYGROUND");
            break;
        case OP_CLOCK: bk_clock_on = !bk_clock_on, st_dirty = 1; break;
        case OP_LISTEN:
            bk_listen_sel = (bk_listen_sel + 4 + (b == BK_LEFT ? -1 : 1)) % 4;
            bk_listen_at = bk_ms();
            st_dirty = 1;
            break;
        case OP_TEXT: bk_big_text = !bk_big_text, st_dirty = 1; break;
        case OP_CONTRAST: bk_contrast = !bk_contrast, st_dirty = 1; break;
        case OP_TSIZE: bk_tsize = (bk_tsize + 3 + (b == BK_LEFT ? -1 : 1)) % 3, st_dirty = 1; break;
        case OP_TSIDE: bk_tside = !bk_tside, st_dirty = 1; break;
        case OP_TGLOW: bk_tglow = (bk_tglow + 3 + (b == BK_LEFT ? -1 : 1)) % 3, st_dirty = 1; break;
        case OP_TPAD: bk_tpad = !bk_tpad, st_dirty = 1; break;
        case OP_TMODE: bk_tmode = (bk_tmode + 3 + (b == BK_LEFT ? -1 : 1)) % 3, st_dirty = 1; break;
        case OP_TBUZZ: bk_tbuzz = !bk_tbuzz, st_dirty = 1; break;
        case OP_TLIFT: bk_tlift = !bk_tlift, st_dirty = 1; break;
        case OP_PERF: bk_perf = !bk_perf, st_dirty = 1; break;
        case OP_JOURNAL:
            if (b == BK_A)
                bk_opt_open = 0, bk_journal_open();
            break;
        case OP_LIGHT: bk_light = !bk_light, bk_light_auto = 0, st_dirty = 1; break;
        case OP_CALM: bk_calm = !bk_calm, st_dirty = 1; break;
        case OP_HELP:
            if (b == BK_A)
                bk_opt_open = 0, bk_help_open = 1, bk_help_page = 0;
            break;
        case OP_GUIDE:
            if (b == BK_A)
                bk_opt_open = 0, bk_guide_open = 1, bk_guide_page = 0;
            break;
        case OP_RESET:
            if (b == BK_A) {
                bk_map_reset(), st_dirty = 1;
                bk_notice("BUTTONS RESET", "EVERY BUTTON IS ITSELF AGAIN");
            }
            break;
        }
    }
}

/* a row's value, as it reads */
static const char *bk_opt_value(int r, char *buf, size_t n)
{
    switch (r) {
    case OP_SWAP_AB: return bk_opt_swapped(BK_A, BK_B) ? "ON" : "OFF";
    case OP_SWAP_XY: return bk_opt_swapped(BK_X, BK_Y) ? "ON" : "OFF";
    case OP_ASSIGN:
        snprintf(buf, n, "%s  <  >", BK_BNAMES[BK_REMAP[bk_opt_part]]);
        return buf;
    case OP_REPEAT: return OP_REP_NAMES[bk_rep_speed];
    case OP_SLEEP:
        if (!bk_sleep_sel)
            return "OFF";
        snprintf(buf, n, "%d MIN  (%d LEFT)", BK_SLEEP_MIN[bk_sleep_sel], bk_sleep_left_min());
        return buf;
    case OP_LANG: return BK_LANG_NAME[bk_lang_idx];
    case OP_SCALE: return BK_PSCALES[bk_play_scale].name;
    case OP_CLOCK: return bk_clock_on ? "ON" : "OFF";
    case OP_LISTEN:
        if (!bk_listen_sel)
            return "OFF";
        snprintf(buf, n, "%d MIN", BK_LISTEN_MIN[bk_listen_sel]);
        return buf;
    case OP_TEXT: return bk_big_text ? "LARGE" : "NORMAL";
    case OP_CONTRAST: return bk_contrast ? "HIGH" : "NORMAL";
    case OP_TSIZE: return bk_tsize == 0 ? "SMALL" : bk_tsize == 1 ? "NORMAL" : "LARGE";
    case OP_TSIDE: return bk_tside ? "SWAPPED" : "NORMAL";
    case OP_TGLOW: return bk_tglow == 0 ? "FAINT" : bk_tglow == 1 ? "NORMAL" : "BRIGHT";
    case OP_TPAD: return bk_tpad ? "ON" : "OFF";
    case OP_TMODE: return bk_tmode == 0 ? "AUTO" : bk_tmode == 1 ? "SHOWN" : "HIDDEN";
    case OP_TBUZZ: return bk_tbuzz ? "ON" : "OFF";
    case OP_TLIFT: return bk_tlift ? "HIGH" : "LOW";
    case OP_PERF: return bk_perf ? "ON" : "OFF";
    case OP_JOURNAL: return "A: OPEN";
    case OP_LIGHT: return bk_light ? "ON" : "OFF";
    case OP_CALM: return bk_calm ? "ON" : "OFF";
    case OP_HELP: case OP_GUIDE: return "A: OPEN";
    case OP_RESET: return bk_map_custom() ? "A: RESET" : "NOTHING MOVED";
    }
    return "";
}

static void bk_draw_options(float t)
{
    uint32_t col = bk_track_col(dream_ui.track), now = bk_ms();
    int x = 70, y = 110, rh = bk_small ? 50 : 46, i;
    char buf[96];
    int about_lines = 1;
    if (bk_opt_capture >= 0 && now - bk_opt_cap_at > 6000u)   /* (nothing pressed: nothing changes) */
        bk_opt_capture = -1;
    if (bk_opt_glide < 0)
        bk_opt_glide = (float)bk_opt_row;
    bk_opt_glide += ((float)bk_opt_row - bk_opt_glide) * bk_glide(14.0f);
    if (bk_opt_row < bk_opt_top)                 /* (1.6) the list scrolls: the chosen row stays in the window */
        bk_opt_top = bk_opt_row;
    if (bk_opt_row >= bk_opt_top + OP_VIS)
        bk_opt_top = bk_opt_row - OP_VIS + 1;
    bk_rect(0, 0, BK_W, BK_H, ARGB(255, 4, 7, 14));
    if (!bk_light)
        bk_vines_at(t, col, 40.0f, 30.0f, (float)(BK_W - 40), (float)(BK_H - 60));
    gd_seedling(70, 52, col);
    bk_text(90, 36, BK_TINY, 1, "OPTIONS", BK_ALPHA(col, 220));
    bk_text(x, 62, &FONT_M, 1, "Options", BK_INK);
    {   /* the chosen row: a glowing leaf gliding behind it */
        float gy = (float)y + (bk_opt_glide - (float)bk_opt_top) * (float)rh;
        bk_rrect(x - 14, (int)gy - 6, 600, rh - 4, 16, BK_ALPHA(col, 46));
        ty_blob((float)(x - 2), gy + (float)(rh - 4) * 0.5f - 6.0f, 9.0f, col, t, 5, 255);
    }
    if (OP_COUNT > OP_VIS) {                     /* a thin scroll bar beside the rows */
        int bh = OP_VIS * rh - 10, th = bh * OP_VIS / OP_COUNT, ty0 = y + (bh - th) * bk_opt_top / (OP_COUNT - OP_VIS);
        bk_rrect(x + 596, y, 4, bh, 2, ARGB(30, 255, 255, 255));
        bk_rrect(x + 596, ty0, 4, th, 2, BK_ALPHA(col, 200));
    }
    for (i = bk_opt_top; i < OP_COUNT && i < bk_opt_top + OP_VIS; i++) {
        int ry = y + (i - bk_opt_top) * rh, sel = i == bk_opt_row;
        const char *v = bk_opt_value(i, buf, sizeof buf);
        int vw;
        bk_text_fit(x + 16, ry, &FONT_S, OP_NAMES[i], 330, sel ? BK_INK : BK_DIM);
        vw = bk_text_w(&FONT_H, v, 1) + 24;
        vw = vw > 220 ? 220 : vw;
        bk_rrect(x + 560 - vw, ry - 2, vw, 30, 15, sel ? BK_ALPHA(col, 230) : ARGB(40, 255, 255, 255));
        bk_text_fit(x + 572 - vw, ry + 1, &FONT_H, v, vw - 20, sel ? ARGB(255, 12, 18, 26) : BK_INK);
    }
    {   /* what the chosen row does */
        int ay = y + OP_VIS * rh + 4, nl;
        char ln[2][64];
        if ((nl = bk_wrap(BK_TINY, OP_ABOUT[bk_opt_row], 600, ln, 2)) > 0) {   /* (1.2: two lines where the font is large) */
            about_lines = nl;
            bk_tr_keys = 1;
            for (int q = 0; q < nl; q++)
                bk_text(x + 16, ay + q * 22, BK_TINY, 1, ln[q], BK_ALPHA(col, 230));
            bk_tr_keys = 0;
        } else
            bk_text_fit(x + 16, ay, BK_TINY, OP_ABOUT[bk_opt_row], 600, BK_ALPHA(col, 230));
    }
    bk_big_ctx = 0;                              /* (the panel and the keys below keep their size: no room) */
    {   /* the buttons, at a glance: each part and the button that plays it now */
        int px = BK_W - 300, py = 110;
        bk_rrect(px - 16, py - 14, 268, 10 * 34 + 56, 18, ARGB(255, 10, 16, 22));
        bk_text(px, py - 6, BK_TINY, 1, "PART  >  BUTTON", BK_FAINT);
        for (int k = 0; k < 10; k++) {
            int lg = BK_REMAP[k], ph = bk_map_phys(lg), moved = ph != lg, yy = py + 22 + k * 34;
            bk_tr_keys = 1;                      /* (button names as printed on the Brick) */
            bk_rrect(px, yy, 70, 26, 13, ARGB(235, 232, 236, 222));
            bk_text_c(px + 35, yy + 2, &FONT_T, 1, BK_BNAMES[lg], ARGB(255, 14, 20, 28));
            bk_text(px + 86, yy + 2, &FONT_T, 1, ">", BK_FAINT);
            bk_rrect(px + 110, yy, 70, 26, 13, moved ? BK_ALPHA(col, 235) : ARGB(60, 255, 255, 255));
            bk_text_c(px + 145, yy + 2, &FONT_T, 1, BK_BNAMES[ph], moved ? ARGB(255, 14, 20, 28) : BK_INK);
            bk_tr_keys = 0;
            if (bk_opt_capture == lg && !bk_calm)
                bk_arc((float)(px + 145), (float)(yy + 13), 20.0f + 4.0f * sinf(t * 6.0f), 2.0f, 0, 6.2832f, col);
        }
    }
    if (bk_opt_capture >= 0) {                   /* assigning: a card over it all */
        int cw = 600, ch = 150, cx = BK_W / 2 - cw / 2, cy = BK_H / 2 - ch / 2;
        float left = 1.0f - (float)(now - bk_opt_cap_at) / 6000.0f;
        bk_rrect(cx + 4, cy + 6, cw, ch, 22, ARGB(150, 0, 0, 0));
        bk_rrect(cx, cy, cw, ch, 22, ARGB(250, 14, 22, 30));
        bk_rrect(cx, cy, (int)((float)cw * (left < 0 ? 0 : left)), 5, 2, col);
        snprintf(buf, sizeof buf, T("PRESS THE BUTTON THAT SHOULD BE %s"), BK_BNAMES[bk_opt_capture]);
        bk_text_c(BK_W / 2, cy + 40, &FONT_B, 1, buf, BK_INK);
        bk_text_c(BK_W / 2, cy + 90, BK_TINY, 1, "THE BUTTON THAT WAS IT TAKES THE OTHER'S PART", BK_DIM);
    }
    {   /* (its keys by the buttons that play them now) */
        char ka[16], kb[16], line[160];
        snprintf(ka, sizeof ka, "%s", bk_key_label("A"));
        snprintf(kb, sizeof kb, "%s", bk_key_label("B"));
        snprintf(line, sizeof line, T("UP/DOWN: A ROW   LEFT/RIGHT: CHANGE   %s: DO IT   %s: CLOSE"), ka, kb);
        if (about_lines < 2) {
            bk_text_c(BK_W / 2, BK_H - 132, BK_TINY, 1, line, BK_FAINT);
            bk_text_c(BK_W / 2, BK_H - 108, BK_TINY, 1, "START + SELECT HELD 3 S, ANYWHERE: EVERY BUTTON ITSELF AGAIN", BK_FAINT);
        } else                                   /* (two lines of about: the keys stay, the escape gives way) */
            bk_text_c(BK_W / 2, BK_H - 108, BK_TINY, 1, line, BK_FAINT);
    }
}
int bk_options_shown(void) { return bk_opt_open; }   /* (the tests) */
void bk_options_show(int on) { bk_opt_open = on != 0; }
int bk_button_part(int phys) { return bk_map_set ? bk_map[phys] : phys; }

/* ---- (1.6) THE GARDEN JOURNAL: every Biome and world you have stayed in, as small lamps by family ---- */
static void bk_draw_journal(float t)
{
    uint32_t col = bk_track_col(dream_ui.track);
    int nb = 0, nv = 0, y = 110, f, nw = 0, nwv = 0, nfam;
    char buf[64];
    bk_rect(0, 0, BK_W, BK_H, ARGB(255, 3, 5, 12));
    gd_seedling(26, 30, col);
    bk_text(46, 14, &FONT_M, 1, "Garden journal", BK_INK);
    if (at_nfam < 0)
        at_build();
    nfam = at_nfam;
    for (f = 0; f < nfam; f++)
        for (int i = 0; i < at_n[f]; i++)
            nb++, nv += bk_jr_get(bk_jr_b, at_ids[f][i]);
    nw = bk_world_count() > 0 ? bk_world_count() : 0;
    for (int i = 0; i < nw && i < 128; i++)
        nwv += bk_jr_get(bk_jr_w, i);
    snprintf(buf, sizeof buf, "%d / %d %s   %d / %d %s", nv, nb, bk_tr("BIOMES"), nwv, nw, bk_tr("WORLDS"));
    bk_text(46, 56, &FONT_S, 1, buf, col);
    bk_text(46, 84, &FONT_T, 1, "A BIOME OR WORLD COUNTS WHEN YOU STAY A FEW SECONDS", BK_DIM);
    for (f = 0; f < nfam && f < AT_MAX_FAM; f++) {
        int rowh = 28, x0 = 250, perrow = (BK_W - 40 - x0) / 14, vis = 0, k;
        int rows = (at_n[f] + perrow - 1) / perrow;
        for (k = 0; k < at_n[f]; k++)
            vis += bk_jr_get(bk_jr_b, at_ids[f][k]);
        if (y + rows * 14 + 6 > BK_H - 44)
            break;
        bk_text(46, y, &FONT_T, 1, md_biome_family_name((unsigned)f), vis ? BK_INK : BK_FAINT);
        snprintf(buf, sizeof buf, "%d/%d", vis, at_n[f]);
        bk_text(x0 - 12 - bk_text_w(&FONT_T, buf, 1), y, &FONT_T, 1, buf, BK_DIM);
        for (k = 0; k < at_n[f]; k++) {
            int on = bk_jr_get(bk_jr_b, at_ids[f][k]), cx = x0 + (k % perrow) * 14, cy = y + 6 + (k / perrow) * 14;
            if (on)
                bk_disc((float)cx + 5, (float)cy, 4.2f + 0.8f * sinf(t * 1.3f + (float)k), BK_ALPHA(col, 235));
            else
                bk_disc((float)cx + 5, (float)cy, 2.2f, ARGB(70, 255, 255, 255));
        }
        y += rows * 14 + 10;
        (void)rowh;
    }
    bk_text(46, BK_H - 34, &FONT_T, 1, "ANY BUTTON: BACK", BK_DIM);
}

/* ---------------------------------------------------------------------------------------------------------------
 * (1.6) Touch on the stage itself. brick_touch.inc passes every finger that began on the stage as a raw event in the
 * stage's own coordinates (0..BK_W x 0..BK_H); the gestures are made here, so the tests can make them too:
 *   a tap          chooses and does what is under it (OPTIONS rows, quick bar, menu, tracks, dials, and each view's
 *                  own things: brick_touch_views.inc), or closes what only shows (help, guide, journal);
 *   a drag         is the D-pad: a step every 30 px (44 in a list) the way the finger goes; on a dial's panel it turns
 *                  that dial itself; with two fingers across it is L1/R1 (the page, the track), up/down R2-less;
 *   a hold         600 ms without moving: the help for where you are (as SELECT held).
 * ------------------------------------------------------------------------------------------------------------- */
int bk_touch_overlay(void) { return bk_opt_open || bk_menu_open || bk_quick_open || bk_help_open || bk_guide_open || bk_journal_on; }

#define BK_TF 10
static struct { int used, kind; float x0, y0, x, y, ax, ay, moved; uint32_t t0; } bk_tfing[BK_TF];
enum { TF_FREE, TF_STAGE, TF_DIAL, TF_GRAB };   /* kind: a finger's job, fixed when it lands */
static int bk_touch_n(void)
{
    int n = 0;
    for (int i = 0; i < BK_TF; i++)
        n += bk_tfing[i].used != 0;
    return n;
}
#include "brick_touch_views.inc"

void bk_touch_tap(int x, int y)
{
    bk_listen_at = bk_ms();
    if (bk_resting) {                            /* (a touch wakes the resting screen, as a button does) */
        bk_press(BK_A);
        return;
    }
    if (bk_opt_open && bk_opt_capture < 0) {
        int rh = bk_small ? 50 : 46, r = (y - 104) / rh;
        if (x >= 56 && x < 670 && y >= 104 && r >= 0 && r < OP_VIS && bk_opt_top + r < OP_COUNT) {
            bk_opt_row = bk_opt_top + r;
            bk_press(BK_A);
        }
    } else if (bk_quick_open) {
        int cw = (BK_W - 24) / BK_NQUICK, by = BK_H - 62 - bk_quick_lift();
        if (y >= by - 48 && x >= 12 && x < 12 + cw * BK_NQUICK) {
            bk_quick_sel = (x - 12) / cw;
            bk_press(BK_A);
        }
    } else if (bk_menu_open) {
        int rx = BK_SX + 486, best = -1;
        float bd = 1e9f;
        for (int i = 0; i < BK_NJUMPS; i++) {
            if (i < BK_MENU_SPLIT) {
                float dx = (float)x - (bk_menu_hx[i] + 100.0f), dy = (float)y - bk_menu_hy[i];
                if (fabsf(dx) < 130.0f && fabsf(dy) < 26.0f && dx * dx + dy * dy < bd)
                    bd = dx * dx + dy * dy, best = i;
            } else {
                int ry = BK_SY + 70 + (i - BK_MENU_SPLIT) * 40;
                if (x >= rx - 6 && x < rx + 222 && y >= ry - 4 && y < ry + 32)
                    best = i, bd = 0;
            }
        }
        if (best >= 0) {
            bk_menu_sel = best;
            bk_press(BK_A);
        }
    } else if (bk_help_open || bk_guide_open || bk_journal_on) {
        bk_press(BK_B);                          /* (a tap closes what only shows) */
    } else {                                     /* the stage itself: a track's plant chooses the track, a dial's panel its focus */
        int n = md_active_tracks();
        if (x >= 8 && x < BK_LEFT_W - 6 && y >= BK_SY && y < BK_SY + 6 * 117) {
            int i = (y - BK_SY) / 117;
            if (i < n)
                dream_ui.track = i;
        } else if (!lab_open && !((bk_held >> BK_FN) & 1u) && x >= BK_RIGHT_X + 8 && x < BK_W - 6 && y >= BK_SY && y < BK_SY + 4 * 84) {
            bk_focus = (y - BK_SY) / 84;
        } else if (!bk_touch_view_tap(x, y)) {
            ;                                    /* (nothing under the finger) */
        }
    }
}

static void bk_touch_dpad(int axis, int d)       /* one step as the D-pad: axis 0 across, 1 down; d +1 right / down */
{
    if (bk_touch_overlay()) {
        if (bk_help_open || bk_guide_open)
            bk_press(axis == 0 ? (d > 0 ? BK_R1 : BK_L1) : (d > 0 ? BK_DOWN : BK_UP));
        else if (!bk_journal_on)
            bk_press(axis == 0 ? (d > 0 ? BK_RIGHT : BK_LEFT) : (d > 0 ? BK_DOWN : BK_UP));
        return;
    }
    if (bk_touch_view_step(axis, d))             /* (a view that takes the step itself, a slider say) */
        return;
    bk_press(axis == 0 ? (d > 0 ? BK_RIGHT : BK_LEFT) : (d > 0 ? BK_DOWN : BK_UP));
}

static void bk_touch_dial(int k, int d)          /* a dial of the right panel turned itself: d +1 more */
{
    web_enc(EN_K1 + k, d);
    bk_turns++, bk_turn_dir = d;
    bk_focus_dir[k] = d;
    bk_focus_at[k] = bk_ms();
    bk_focus = k;
}

void bk_touch_event(int phase, int id, float x, float y)   /* phase 0 down, 1 move, 2 up; id: a slot 0..9 */
{
    uint32_t now = bk_ms();
    int k = id < 0 || id >= BK_TF ? 0 : id;
    typeof(bk_tfing[0]) *f = &bk_tfing[k];
    bk_listen_at = now;
    if (phase == 0) {
        memset(f, 0, sizeof *f);
        f->used = 1, f->x0 = f->x = x, f->y0 = f->y = y, f->t0 = now;
        f->kind = TF_STAGE;
        if (!bk_touch_overlay() && !bk_resting && !lab_open && x >= BK_RIGHT_X + 8 && x < BK_W - 6 && y >= BK_SY && y < BK_SY + 4 * 84 &&
            !((bk_held >> BK_FN) & 1u))
            f->kind = TF_DIAL;                   /* (a dial's panel: the finger turns that dial) */
        else if (!bk_touch_overlay() && !bk_resting && bk_touch_view_grab(k, x, y))
            f->kind = TF_GRAB;                   /* (a view's slider or pad: the view has the finger) */
        return;
    }
    if (!f->used)
        return;
    if (phase == 1) {
        float step = (bk_touch_overlay() ? 44.0f : f->kind == TF_DIAL ? 20.0f : 30.0f);
        int fingers = bk_touch_n();
        f->moved += fabsf(x - f->x) + fabsf(y - f->y);
        if (f->kind == TF_GRAB) {
            bk_touch_view_move(k, x, y);
            f->x = x, f->y = y;
            return;
        }
        f->ax += x - f->x, f->ay += y - f->y;
        f->x = x, f->y = y;
        for (;;) {
            int axis = fabsf(f->ax) >= fabsf(f->ay) ? 0 : 1;
            float v = axis ? f->ay : f->ax;
            int d = v > 0 ? 1 : -1;
            if (fabsf(v) < step)
                break;
            if (axis)
                f->ay -= d * step;
            else
                f->ax -= d * step;
            if (f->kind == TF_DIAL)
                bk_touch_dial(clamp(((int)f->y0 - BK_SY) / 84, 0, 3), axis ? -d : d);   /* (up = more) */
            else if (fingers >= 2) {             /* two fingers across: the shoulder buttons (only the first finger counts) */
                int first = -1;
                for (int i = 0; i < BK_TF && first < 0; i++)
                    if (bk_tfing[i].used)
                        first = i;
                if (first == k && axis == 0 && !bk_touch_overlay())
                    bk_press(d > 0 ? BK_R1 : BK_L1);
                else if (first == k && axis == 0)
                    bk_touch_dpad(0, d);
            } else
                bk_touch_dpad(axis, d);
        }
        return;
    }
    /* phase 2: up */
    if (f->kind == TF_GRAB)
        bk_touch_view_up(k, x, y);
    else if (f->moved < 14.0f && now - f->t0 < 600u)
        bk_touch_tap((int)f->x0, (int)f->y0);
    f->used = 0;
}

void bk_touch_tick(uint32_t now)                 /* a finger held still for 600 ms: the help for where you are */
{
    for (int i = 0; i < BK_TF; i++) {
        typeof(bk_tfing[0]) *f = &bk_tfing[i];
        if (f->used && f->kind == TF_STAGE && f->moved < 14.0f && f->t0 && now - f->t0 > 600u && !bk_touch_overlay() && !bk_resting) {
            bk_help_open = 1;
            bk_help_page = (bk_held >> BK_FN) & 1u ? 3 : lab_open ? 2 : bk_view ? 4 : 0;
            f->t0 = 0;                           /* (once; the finger's lifting is no tap now) */
            f->moved = 1000.0f;
        }
    }
}

void bk_touch_cmd(int cmd)                       /* the touch dock's own things */
{
    bk_listen_at = bk_ms();
    if (bk_resting) {
        bk_press(BK_A);
        return;
    }
    if (cmd == 0) {                              /* HOME: the garden at once, every place left */
        bk_menu_open = bk_help_open = bk_guide_open = bk_opt_open = bk_quick_open = bk_journal_on = 0;
        bk_home();
    } else if (cmd == 1) {                       /* OPTIONS */
        bk_menu_open = bk_help_open = bk_guide_open = bk_quick_open = bk_journal_on = 0;
        bk_opt_open = !bk_opt_open;
    } else if (cmd == 2) {                       /* HELP for where you are */
        if (bk_help_open)
            bk_help_open = 0;
        else {
            bk_menu_open = bk_guide_open = bk_opt_open = bk_quick_open = bk_journal_on = 0;
            bk_help_open = 1;
            bk_help_page = (bk_held >> BK_FN) & 1u ? 3 : lab_open ? 2 : bk_view ? 4 : 0;
        }
    }
}
int bk_touch_state(int what)
{
    if (what == 0)
        return md_playing();
    if (what == 1)
        return (int)((bk_held >> BK_FN) & 1u);
    if (what == 2)
        return (int)((bk_held >> BK_R2) & 1u);
    return 0;
}
