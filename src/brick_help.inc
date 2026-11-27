/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the button map.
 *
 * A Brick drawn in the middle of the stage, every button labelled with what it does: in the garden and on the
 * FM-1's pages, with R2 held, in the Sound Lab, in play mode (FN) and in the full views. L1/R1 turn the pages, any
 * other button closes it. SELECT held (anywhere) or MENU > BUTTONS opens it; the very first start (the SDL program, not the test runner)
 * shows it once after the start screen. Included by brick_stage.inc. */

enum { HB_L2, HB_L1, HB_R1, HB_R2, HB_UPDN, HB_LR, HB_A, HB_B, HB_X, HB_Y, HB_SELECT, HB_START, HB_MENU, HB_F1, HB_F2,
       HB_FN, HB_COUNT };
static const char *const HB_NAMES[HB_COUNT] = {"L2", "L1", "R1", "R2", "UP/DOWN", "LEFT/RIGHT", "A", "B", "X", "Y",
                                               "SELECT", "START", "MENU", "F1", "F2", "FN"};
#define HELP_PAGES 5
static const char *const HELP_TITLES[HELP_PAGES] = {"In the garden and on the FM-1's pages", "With R2 held", "In the Sound Lab",
                                                     "In play mode (from the quick bar)", "In the full views"};
static const char *const HELP_TEXT[HELP_PAGES][HB_COUNT] = {
    {"TAP: QUICK BAR, HOLD: FINE", "TRACK", "TRACK", "HOLD: MORE (IN THE BAR)", "CHOOSE ONE OF THE FOUR DIALS", "TURN THE DIAL", "SELECT, MUTE", "BACK, HOLD: HOME",
     "EDIT PAGES", "EFFECT PAGES", "TAP: WHERE AM I / HOLD: HELP", "PLAY / STOP", "WHERE TO? THE MENU", "SOUND LAB", "MIXER", "PLAY MODE"},
    {0, "OCTAVE DOWN", "OCTAVE UP", "(HELD)", "MODEL", "BIOME", "LOCK", "FREEZE", "ENVELOPE", "GLOBAL", "HOLD: SAVE",
     "RECORD", "DREAM MAP", "RECORD", "BIOME ATLAS", 0},
    {"FINE, 5-ROW JUMPS", "MODULE", "MODULE", 0, "PARAMETER", "VALUE", "NEUTRAL / BACK", "BACK, HOLD: HOME", "MODULE SILENT / BACK", "NEXT SOUNDING MODULE", "HOLD: THIS HELP", "PLAY / STOP",
     "THE MENU", "CLOSE", "ALL MODULES (OR L1 + R1)", "PLAY MODE"},
    {0, "TRACK", "TRACK", "CHORDS", "NOTE 4 / NOTE 2", "NOTE 1 / NOTE 3", "NOTE 8", "NOTE 7", "NOTE 6", "NOTE 5", "END PLAY MODE", "PLAY / STOP", 0,
     0, 0, "OFF: CONTROL AGAIN"},
    {"FINE STEPS", "SECTION / FIELD", "SECTION / FIELD", "TOOLS (SEQUENCE)", "ROW / FIELD", "VALUE",
     "MUTE, LOAD, ENTER, FETCH, DREAM", "BACK, HOLD: HOME", "EDIT, NEXT PANEL, LINKS, SAMPLER",
     "GENERATE, POND, FLY, SURPRISE, WANDER", "HOLD: THIS HELP", "PLAY / STOP", "THE MENU", "SOUND LAB", "MIXER", "PLAY MODE"}};

static int bk_help_open, bk_help_page, bk_help_first, bk_help_done;
/* a button's label on a help page; on a handheld without them, the Brick's own MENU, F1, F2 and FN have none (the
   quick bar, L2, reaches all of it) */
static const char *hb_text(int page, int b)
{
    {   /* (1.1) a moved button: the label of the part it plays now */
        static const int hb2bk[HB_COUNT] = {BK_L2, BK_L1, BK_R1, BK_R2, -1, -1, BK_A, BK_B, BK_X, BK_Y, BK_SELECT, BK_START,
                                            BK_MENU, BK_F1, BK_F2, BK_FN};
        if (hb2bk[b] >= 0 && bk_map_set) {
            int lg = bk_map[hb2bk[b]];
            for (int h = 0; h < HB_COUNT; h++)
                if (hb2bk[h] == lg)
                    b = h;
        }
    }
    if (bk_handheld && (b == HB_MENU || b == HB_F1 || b == HB_F2 || b == HB_FN))
        return 0;
    if (bk_handheld && b == HB_L2 && page == 0)
        return "TAP: QUICK BAR (ALL)";
    if (bk_handheld && b == HB_SELECT && (page == 0 || page == 4))
        return "TAP: MENU / HOLD: HELP";     /* (1.5.8: the menu on one key) */
    if (b == HB_FN && !bk_keyboard)              /* (1.6.2 fix) the Brick's side FN switch does not reach the program */
        return page == 3 ? 0 : "NOT USED: L2, PLAY MODE";
    return HELP_TEXT[page][b];
}   /* (also declared in brick_core.c) */

/* where each button sits on the drawn Brick (b: its top left) and where its label goes (left or right column) */
static void hb_place(int b, int bx, int by, float *px, float *py, int *side)
{
    static const float pos[HB_COUNT][2] = {
        {22, -12}, {64, -6}, {196, -6}, {238, -12}, {70, 244}, {70, 276},
        {210, 290}, {176, 290}, {210, 254}, {176, 254},   /* (1.5) A B X Y as on the Brick: a square, Y X above, B A below */
        {100, 352}, {160, 352}, {130, 330}, {96, 410}, {164, 410}, {-8, 210}};
    static const int left[HB_COUNT] = {1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1};
    *px = (float)bx + pos[b][0], *py = (float)by + pos[b][1], *side = left[b];
}

static void bk_draw_help(float t)
{
    int bx = BK_W / 2 - 130, by = 190, page = bk_help_page % HELP_PAGES, nl = 0, nr = 0;
    uint32_t col = bk_track_col(dream_ui.track), body = ARGB(255, 46, 52, 64), dark = ARGB(255, 22, 26, 34);
    char buf[48];
    bk_rect(0, 0, BK_W, BK_H, ARGB(244, 3, 5, 12));   /* (the whole screen: room for words large enough to read) */
    gd_seedling(26, 30, col);
    bk_text(46, 14, &FONT_M, 1, "Help: the buttons", BK_INK);
    bk_text(46, 52, &FONT_S, 1, HELP_TITLES[page], col);
    for (int k = 0; k < HELP_PAGES; k++)          /* which page: seeds */
        bk_disc((float)(BK_W - 30 - (HELP_PAGES - 1 - k) * 18), 34, k == page ? 6.0f : 4.0f, k == page ? col : ARGB(90, 255, 255, 255));
    snprintf(buf, sizeof buf, "%d / %d", page + 1, HELP_PAGES);
    bk_text(BK_W - 30 - (HELP_PAGES - 1) * 18 - 8 - bk_text_w(&FONT_T, buf, 1) - 14, 24, &FONT_T, 1, buf, BK_DIM);
    /* the Brick: body, shoulders, screen with a little garden, the switch */
    bk_rrect(bx + 6, by - 20, 50, 22, 8, page == 1 ? dark : body);
    bk_rrect(bx + 204, by - 20, 50, 22, 8, body);
    bk_rrect(bx, by, 260, 470, 34, body);
    bk_rframe(bx, by, 260, 470, 34, 2, ARGB(255, 70, 78, 92));
    bk_rrect(bx + 26, by + 28, 208, 160, 12, ARGB(255, 8, 12, 22));
    for (int k = 0; k < 6; k++) {                 /* (the garden, small) */
        float sx = (float)(bx + 52 + k * 32), sy = (float)(by + 168);
        bk_line(sx, sy, sx + sinf(t + (float)k) * 3.0f, sy - 46.0f - (float)((k * 7) % 3) * 10.0f, 2.0f, bk_track_col(k));
        bk_disc(sx + sinf(t + (float)k) * 3.0f, sy - 48.0f - (float)((k * 7) % 3) * 10.0f, 5.0f, bk_track_col(k));
    }
    bk_rrect(bx - 10, by + 196, 10, 30, 4, (bk_held >> BK_FN) & 1u ? col : ARGB(255, 90, 98, 112));
    /* each side's labels in the order of their buttons from top to bottom: no lines crossing */
    int slot[HB_COUNT];
    for (int b = 0; b < HB_COUNT; b++) {
        float px, py, qx, qy;
        int side, sd, k = 0;
        hb_place(b, bx, by, &px, &py, &side);
        slot[b] = 0;
        if (!hb_text(page, b))
            continue;
        for (int c = 0; c < HB_COUNT; c++) {
            if (c == b || !hb_text(page, c))
                continue;
            hb_place(c, bx, by, &qx, &qy, &sd);
            if (sd == side && (qy < py || (qy == py && c < b)))
                k++;
        }
        slot[b] = k;
    }
    (void)nl, (void)nr;
    /* the buttons, as pebbles; the ones in use lit; each with a line to its label */
    for (int b = 0; b < HB_COUNT; b++) {
        float px, py, lx, ly;
        int side, used = hb_text(page, b) != 0;
        uint32_t pc = used ? ARGB(255, 226, 232, 216) : ARGB(255, 80, 88, 102);
        hb_place(b, bx, by, &px, &py, &side);
        bk_tr_keys = 1;                          /* (the buttons on the drawn Brick: their own names) */
        if (b == HB_UPDN) {                       /* the D-pad: a cross */
            bk_rrect((int)px - 12, (int)py - 6, 24, 62, 6, dark);
            bk_rrect((int)px - 31, (int)py + 13, 62, 24, 6, dark);
            bk_rrect((int)px - 9, (int)py - 3, 18, 18, 4, pc);
            bk_rrect((int)px - 9, (int)py + 35, 18, 18, 4, pc);
        } else if (b == HB_LR) {
            bk_rrect((int)px - 28, (int)py - 9, 18, 18, 4, pc);
            bk_rrect((int)px + 10, (int)py - 9, 18, 18, 4, pc);
        } else if (b >= HB_A && b <= HB_Y) {
            bk_disc(px, py, 13, pc);
            bk_text_c((int)px, (int)py - 7, &FONT_T, 1, HB_NAMES[b], ARGB(255, 16, 22, 30));
        } else if (b == HB_FN)
            ;                                    /* (the switch is drawn above) */
        else if (b <= HB_R2) {
            bk_text_c((int)px, (int)py - 6, &FONT_T, 1, HB_NAMES[b], used ? BK_INK : BK_FAINT);
        } else {
            bk_rrect((int)px - 18, (int)py - 8, 36, 16, 8, pc);
            bk_text_c((int)px, (int)py - 6, &FONT_T, 1, HB_NAMES[b][0] == 'S' && HB_NAMES[b][1] == 'E' ? "SEL" :
                      b == HB_START ? "STA" : HB_NAMES[b], ARGB(255, 16, 22, 30));
        }
        bk_tr_keys = 0;
        if (!used)
            continue;
        ly = (float)(96 + slot[b] * 66);
        lx = side ? (float)(bx - 40) : (float)(bx + 300);
        bk_line(px + (side ? -14.0f : 14.0f), py, lx, ly + 18, 1.2f, BK_ALPHA(col, 110));
        bk_disc(lx, ly + 18, 3, col);
        {
            int w = 300, tx = side ? (int)lx - 12 - w : (int)lx + 12;
            const char *tx_s = T(hb_text(page, b));   /* (0.80: in the chosen language before it is broken in two) */
            char l1[64], l2[64];
            int cut = 0, last_sp = -1;
            bk_tr_keys = 1;                      /* (the button's own name) */
            bk_rrect(tx, (int)ly + 2, bk_text_w(&FONT_T, HB_NAMES[b], 1) + 18, 24, 12, ARGB(230, 232, 236, 222));
            bk_text(tx + 9, (int)ly + 4, &FONT_T, 1, HB_NAMES[b], ARGB(255, 16, 22, 30));
            bk_tr_keys = 0;
            for (int c = 0; tx_s[c]; c++) {       /* (two lines at most, broken at a space) */
                char tmp[64];
                snprintf(tmp, sizeof tmp, "%.*s", c + 1, tx_s);
                if (tx_s[c] == ' ')
                    last_sp = c;
                if (bk_tw(&FONT_S, tmp) > w && last_sp > 0) {
                    cut = last_sp;
                    break;
                }
            }
            if (cut) {
                snprintf(l1, sizeof l1, "%.*s", cut, tx_s);
                snprintf(l2, sizeof l2, "%s", tx_s + cut + 1);
                bk_text(tx, (int)ly + 28, &FONT_S, 1, l1, BK_INK);
                bk_text_fit(tx, (int)ly + 48, &FONT_S, l2, w, BK_INK);
            } else
                bk_text(tx, (int)ly + 28, &FONT_S, 1, tx_s, BK_INK);
        }
    }
    {   /* (1.1) its keys by the buttons that play them; Y leads to OPTIONS */
        char l[160], k1[16], kr[16], kx[16], ky[16];
        snprintf(k1, sizeof k1, "%s", bk_key_label("L1"));
        snprintf(kr, sizeof kr, "%s", bk_key_label("R1"));
        snprintf(kx, sizeof kx, "%s", bk_key_label("X"));
        snprintf(ky, sizeof ky, "%s", bk_key_label("Y"));
        snprintf(l, sizeof l, T("%s / %s: OTHER PAGES   %s: THE GUIDE   %s: OPTIONS   ANY OTHER BUTTON: CLOSE"), k1, kr, kx, ky);
        bk_text_c(BK_W / 2, BK_H - 70, &FONT_H, 1, l, BK_DIM);
    }
}
