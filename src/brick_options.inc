/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: OPTIONS (1.1). The buttons as the player wants them (A and B
 * swapped, X and Y swapped, or any of ten buttons given another's part), how held buttons repeat, the language,
 * the light picture, calm motion, and the way to the button map and the guide. Kept in save/brick-prefs.txt.
 * From the quick bar (L2). UP/DOWN a row, LEFT/RIGHT its value, A does it, B closes. While a button is being
 * assigned, the next button pressed takes the part (6 s, then nothing changes). START + SELECT held 3 s anywhere:
 * every button itself again (brick_core.c bk_input). Included by brick_stage.inc. */

enum { OP_SWAP_AB, OP_SWAP_XY, OP_ASSIGN, OP_REPEAT, OP_SLEEP, OP_LANG, OP_LIGHT, OP_CALM, OP_HELP, OP_GUIDE, OP_RESET, OP_COUNT };
static const char *const OP_NAMES[OP_COUNT] = {"SWAP A AND B", "SWAP X AND Y", "ASSIGN A BUTTON", "HELD BUTTONS REPEAT", "SLEEP TIMER",
                                               "LANGUAGE", "LIGHT PICTURE", "CALM MOTION", "THE BUTTON MAP", "THE GUIDE",
                                               "EVERY BUTTON ITSELF AGAIN"};
static const char *const OP_ABOUT[OP_COUNT] = {
    "As on Nintendo-style handhelds: A on the right, B below", "X and Y change places",
    "Choose a part with LEFT/RIGHT, press A, then the button that should play it", "How soon and how fast a held button goes on",
    "The music fades out and stops after this time; the screen rests until a button wakes it",
    "The whole garden in English or German", "Less decoration, a faster picture", "What only decorates holds still",
    "Every button and what it does, on the drawn Brick", "Fourteen pages: how it all works", "Undo every change of buttons"};
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
            if (bk_lang_choose(!bk_lang) < 0)
                bk_notice("DEUTSCH", "LANG/DE.TXT IS MISSING");
            st_dirty = 1;
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
    case OP_LANG: return bk_lang ? "DEUTSCH" : "ENGLISH";
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
    bk_rect(0, 0, BK_W, BK_H, ARGB(255, 4, 7, 14));
    if (!bk_light)
        bk_vines_at(t, col, 40.0f, 30.0f, (float)(BK_W - 40), (float)(BK_H - 60));
    gd_seedling(70, 52, col);
    bk_text(90, 36, BK_TINY, 1, "OPTIONS", BK_ALPHA(col, 220));
    bk_text(x, 62, &FONT_M, 1, "Options", BK_INK);
    {   /* the chosen row: a glowing leaf gliding behind it */
        float gy = (float)y + bk_opt_glide * (float)rh;
        bk_rrect(x - 14, (int)gy - 6, 600, rh - 4, 16, BK_ALPHA(col, 46));
        ty_blob((float)(x - 2), gy + (float)(rh - 4) * 0.5f - 6.0f, 9.0f, col, t, 5, 255);
    }
    for (i = 0; i < OP_COUNT; i++) {
        int ry = y + i * rh, sel = i == bk_opt_row;
        const char *v = bk_opt_value(i, buf, sizeof buf);
        int vw;
        bk_text_fit(x + 16, ry, &FONT_S, OP_NAMES[i], 330, sel ? BK_INK : BK_DIM);
        vw = bk_text_w(&FONT_H, v, 1) + 24;
        vw = vw > 220 ? 220 : vw;
        bk_rrect(x + 560 - vw, ry - 2, vw, 30, 15, sel ? BK_ALPHA(col, 230) : ARGB(40, 255, 255, 255));
        bk_text_fit(x + 572 - vw, ry + 1, &FONT_H, v, vw - 20, sel ? ARGB(255, 12, 18, 26) : BK_INK);
    }
    {   /* what the chosen row does */
        int ay = y + OP_COUNT * rh + 4, nl;
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
