/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: what a finger does in each view (1.6). Included by brick_options.inc.
 * Coordinates are the stage's own (0..BK_W x 0..BK_H), the same the views draw in.
 *   bk_touch_view_tap   a tap: chooses what is under it; returns 1 if it was something
 *   bk_touch_view_grab  a finger lands on a slider, a step field or a pad: the view takes it (1), else a drag is the D-pad
 *   bk_touch_view_move  the grabbing finger moves;  bk_touch_view_up  it lifts
 *   bk_touch_view_step  a D-pad step by a drag, before the keys: the view may take it itself (1) */
/* a tap in a list: the first chooses the item, a tap on the chosen one does it (A) */
static int tv_g_track[BK_TF], tv_g_row[BK_TF];
static float tv_g_y0[BK_TF], tv_g_v0[BK_TF];

/* THE MIXER: six columns of 108 px, 118 apart; PAN, FX SEND, DELAY, REVERB at 84 + 74 r, the fader below */
static int tv_mixer_at(float x, float y, int *track, int *row)
{
    for (int i = 0; i < md_active_tracks() && i < 6; i++) {
        int cx0 = BK_SX + 12 + i * 118, y0 = BK_SY + 50;
        if (x < cx0 || x >= cx0 + 108 || y < y0 || y >= y0 + 660)
            continue;
        *track = i;
        if (y >= y0 + 360 && y < y0 + 600)
            *row = MX_LEVEL;
        else if (y >= y0 + 600)
            *row = -2;                           /* the MUTED / LEVEL label: a tap mutes */
        else if (y >= y0 + 47)
            *row = clamp(MX_PAN + ((int)y - (y0 + 47)) / 74, MX_PAN, MX_ROWS - 1);
        else
            *row = -1;                           /* the head: only chooses the track */
        return 1;
    }
    return 0;
}

static void tv_mixer_set(int t, int row, float x, float y, float y0, float v0)
{
    int cx = BK_SX + 12 + t * 118 + 54;
    if (row == MX_LEVEL)                         /* the fader: where the finger is is the level */
        dream_cmd(MX_CMD[row], t, clamp((int)(1000.0f * ((float)(BK_SY + 50 + 380 + 220) - y) / 220.0f), 0, 1000));
    else if (row == MX_PAN)                      /* the branch: across */
        dream_cmd(MX_CMD[row], t, clamp((int)(1000.0f * (x - ((float)cx - 40.0f)) / 80.0f), 0, 1000));
    else                                         /* the flowers: up opens them, 200 px for all */
        dream_cmd(MX_CMD[row], t, clamp((int)(v0 + (y0 - y) * 5.0f), 0, 1000));
}


/* THE SEQUENCE: overview rows of 98 + 6 from y 82; while editing, the chosen track's roll (330) and the others (60) */
static int tv_seq_rowy(int row)
{
    int sel = dream_ui.track;
    return BK_SY + 52 + (sq_edit ? row * 66 + (row > sel ? 270 : 0) : row * 104);
}
static int tv_seq_row_at(float y)
{
    for (int r = 0; r < md_active_tracks() && r < 6; r++) {
        int y0 = tv_seq_rowy(r), rh = sq_edit ? (r == dream_ui.track ? 330 : 60) : 98;
        if (y >= y0 && y < y0 + rh)
            return r;
    }
    return -1;
}
static float tv_s_y[BK_TF], tv_s_x0[BK_TF], tv_s_y0[BK_TF];
static int tv_s_kit[BK_TF];
/* a finger in the roll of the chosen track: the step under it becomes the cursor (kit: and the pad) */
static int tv_seq_roll(float x, float y, int *step, int *pad)
{
    int t = dream_ui.track, y0 = tv_seq_rowy(t), gx = BK_SX + 8 + 150, len;
    const md_patch *p = md_state();
    float cw = 544.0f / 32.0f;
    if (!sq_edit || y < y0 + 40 || y >= y0 + 330 || x < gx || x >= gx + 544)
        return 0;
    len = sq_kit(t) ? p->kit.length : p->sequence[t].length;
    *step = clamp((int)((x - (float)gx) / cw), 0, len - 1);
    *pad = clamp((int)((y - (float)(y0 + 40)) / (float)((330 - 60) / 8)), 0, 7);
    return 1;
}

/* THE SOUND LAB: parameter rows (45 apart from y 72, 340 wide), the row of seeds, L1/R1 at the sides, the module list on the right */
static int tv_lab_param_at(float x, float y)
{
    int ps[P_COUNT], np = lab_params(lab_mod, ps), i;
    if (x < BK_SX + 8 || x >= BK_SX + 348 || y < BK_SY + 68)
        return -1;
    i = lab_list_top + ((int)y - (BK_SY + 68)) / 45;
    return i < np && i < lab_list_top + 14 ? i : -1;
}
static int tv_lab_tap(int x, int y)
{
    int i = tv_lab_param_at((float)x, (float)y);
    if (i >= 0) {
        lab_focus[lab_mod] = i;
        return 1;
    }
    if (x >= BK_RIGHT_X + 8 && x < BK_W - 6) {   /* the right panel's module list */
        for (int k = 0; k < M_COUNT; k++) {
            int ky = BK_SY + k * 22 + (k >= M_SOURCES ? 6 : 0);
            if (y >= ky && y < ky + 22) {
                lab_mod = k;
                return 1;
            }
        }
    }
    if (y >= BK_SY + 30 && y < BK_SY + 70) {     /* the row of seeds */
        float best = 1e9f;
        int bi = -1;
        for (int k = 0; k < M_COUNT; k++) {
            float sx = (float)(BK_SX + 360 - (M_COUNT * 22 + 14) / 2 + k * 22 + (k >= M_SOURCES ? 14 : 0) + 11);
            if (fabsf((float)x - sx) < best)
                best = fabsf((float)x - sx), bi = k;
        }
        if (bi >= 0 && best < 16.0f) {
            lab_mod = bi;
            return 1;
        }
    }
    if (y < BK_SY + 34) {                        /* the L1 / R1 names at the top */
        if (x < BK_SX + 200)
            lab_mod = (lab_mod + M_COUNT - 1) % M_COUNT;
        else if (x > BK_SX + 520)
            lab_mod = (lab_mod + 1) % M_COUNT;
        else
            return 0;
        return 1;
    }
    return 0;
}

/* PLAY MODE: the eight stones of the right panel are pads; a finger strikes the note under it, a slide strikes the next */
static int tv_p_btn[BK_TF] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static int tv_pad_at(float x, float y)           /* the stone's degree 0..7 under a point, or -1 */
{
    int w = BK_W - BK_RIGHT_X - 14;
    if (!((bk_held >> BK_FN) & 1u) || bk_touch_overlay())
        return -1;
    for (int k = 0; k < 8; k++) {
        int kx = BK_RIGHT_X + 8 + (k % 2) * (w / 2), ky = BK_SY + 26 + (k / 2) * 62;
        if (x >= kx - 3 && x < kx + w / 2 - 3 && y >= ky - 3 && y < ky + 59)
            return k;
    }
    return -1;
}
static const int TV_DEG_BTN[8] = {BK_LEFT, BK_DOWN, BK_RIGHT, BK_UP, BK_Y, BK_X, BK_B, BK_A};
static void tv_pad_strike(int k, int d)
{
    if (tv_p_btn[k] >= 0)
        bk_note_up(tv_p_btn[k]);
    tv_p_btn[k] = d >= 0 ? TV_DEG_BTN[d] : -1;
    if (d >= 0)
        bk_note_down(tv_p_btn[k], 0);
}
static int bk_touch_view_tap(int x, int y)
{
    int t, row;
    if (lab_open && !bk_view)
        return tv_lab_tap(x, y);
    if (bk_view == V_MIXER && tv_mixer_at((float)x, (float)y, &t, &row)) {
        dream_ui.track = t;
        if (row == -2)
            bk_press(BK_A);                      /* mute */
        else if (row >= 0)
            mx_row = row;
        return 1;
    }
    if (bk_view == V_SEQ) {
        int r = tv_seq_row_at((float)y);
        if (r < 0)
            return 0;
        if (!sq_edit) {
            if (x < BK_SX + 70) {                /* the number: mute */
                dream_ui.track = r;
                bk_press(BK_A);
            } else if (dream_ui.track == r)
                bk_press(BK_X);                  /* the chosen row again: edit it */
            else
                dream_ui.track = r;
        } else if (r != dream_ui.track) {
            sq_edit = 0, dream_ui.track = r;     /* (another row while editing: leave this one, choose that) */
        }
        return 1;
    }
    if (bk_view == V_TOYS) {                     /* the playground: a tap in the field is its A (throw, pulse, a planet ...) */
        if (x >= BK_SX && x < BK_SX + 720 && y >= BK_SY + 40 && y < BK_SY + 700) {
            bk_press(BK_A);
            return 1;
        }
        return 0;
    }
    if (!bk_view && !lab_open && bk_garden_visible() && x >= BK_SX && x < BK_SX + 720 && y >= BK_SY && y < BK_SY + 720) {
        bk_ui_burst((float)x, (float)y, bk_track_col(dream_ui.track), 10);   /* the garden answers a touch: sparks */
        return 1;
    }
    if (bk_view == V_ATLAS) {
        int fx = BK_SX + 10, tx = BK_SX + 214, rows = 12, tw = 245, th = 46;
        if (at_nfam < 0)
            at_build();
        if (x >= fx && x < fx + 190 && y >= BK_SY + 52 && y < BK_SY + 52 + at_nfam * 32) {
            int f = (y - BK_SY - 52) / 32;
            if (f != at_fam)
                at_fam = f, at_pos = 0, at_top = 0;
            at_col = 0;
            return 1;
        }
        if (x >= tx && x < tx + 2 * tw + 8 && y >= BK_SY + 52 && y < BK_SY + 52 + rows * (th + 6)) {
            int c = (x - tx) / (tw + 8), r = (y - BK_SY - 52) / (th + 6), i = (at_top + r) * 2 + c;
            if (i >= 0 && i < at_n[at_fam]) {
                if (at_col == 1 && at_pos == i)
                    bk_press(BK_A);              /* the chosen tile again: load it */
                else
                    at_col = 1, at_pos = i;
                return 1;
            }
        }
        if (md_pending_biome() >= 0 && y >= BK_SY + 676 && y < BK_SY + 710 && x >= BK_SX + 214) {
            bk_press(BK_A);                      /* "NEXT: ...  A CHANGE NOW" */
            return 1;
        }
        return 0;
    }
    if (bk_view == V_WORLDS) {
        int idx[WD_MAX], n;
        if (wd_n < 0)
            wd_load();
        if (!wd_nfam)
            return 0;
        {
            int fstep = wd_nfam > 13 ? 572 / wd_nfam : 44;
            if (x >= BK_SX + 10 && x < BK_SX + 206 && y >= BK_SY + 56 && y < BK_SY + 56 + wd_nfam * fstep) {
                wd_fam = clamp((y - BK_SY - 56) / fstep, 0, wd_nfam - 1);
                return 1;
            }
        }
        n = wd_list(wd_fam, idx);
        if (x >= BK_SX + 218 && x < BK_SX + 712 && y >= BK_SY + 56 && y < BK_SY + 56 + 10 * 48) {
            int i = wd_top[wd_fam] + (y - BK_SY - 56) / 48;
            if (i >= 0 && i < n) {
                if (wd_pos[wd_fam] == i)
                    bk_press(BK_A);              /* the chosen world again: enter it */
                else
                    wd_pos[wd_fam] = i;
                return 1;
            }
        }
        return 0;
    }
    if (bk_view == V_ARCHIVE && !ar_smp) {
        int idx[AR_MAX], n;
        if (ar_n < 0)
            ar_load();
        if (!ar_n)
            return 0;
        if (x >= BK_SX + 10 && x < BK_SX + 206 && y >= BK_SY + 56 && y < BK_SY + 56 + ar_nbaskets * 64) {
            ar_basket = clamp((y - BK_SY - 56) / 64, 0, ar_nbaskets - 1);
            return 1;
        }
        n = ar_list(ar_basket, idx);
        if (x >= BK_SX + 218 && x < BK_SX + 712 && y >= BK_SY + 56 && y < BK_SY + 56 + 13 * 40) {
            int i = ar_top[ar_basket] + (y - BK_SY - 56) / 40;
            if (i >= 0 && i < n) {
                if (ar_pos[ar_basket] == i)
                    bk_press(BK_A);              /* the chosen poem again: fetch or use it */
                else
                    ar_pos[ar_basket] = i;
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

static int bk_touch_view_grab(int k, float x, float y)
{
    int t, row, st, pad;
    if ((pad = tv_pad_at(x, y)) >= 0) {
        tv_pad_strike(k, pad);
        return 1;
    }
    if (lab_open && !bk_view) {                  /* (a finger landing on a parameter row chooses it: dragging then changes it) */
        int i = tv_lab_param_at(x, y);
        if (i >= 0)
            lab_focus[lab_mod] = i;
        return 0;
    }
    if (bk_view == V_SEQ && tv_seq_roll(x, y, &st, &pad)) {
        t = dream_ui.track;
        tv_s_kit[k] = sq_kit(t);
        tv_s_x0[k] = x, tv_s_y0[k] = y, tv_s_y[k] = y;
        dream_cmd(tv_s_kit[k] ? MD_KIT_STEP : MD_SEQ_STEP, t, st);
        if (tv_s_kit[k])
            dream_cmd(MD_KIT_PAD, 3, pad);
        return 1;
    }
    if (bk_view == V_MIXER && tv_mixer_at(x, y, &t, &row) && row >= 0) {
        dream_ui.track = t;
        mx_row = row;
        tv_g_track[k] = t, tv_g_row[k] = row, tv_g_y0[k] = y, tv_g_v0[k] = (float)mx_value(t, row);
        if (row == MX_LEVEL || row == MX_PAN)
            tv_mixer_set(t, row, x, y, y, tv_g_v0[k]);   /* (the fader and the branch jump to the finger) */
        return 1;
    }
    return 0;
}

static void bk_touch_view_move(int k, float x, float y)
{
    int st, pad;
    if (tv_p_btn[k] >= 0) {                      /* a slide over the stones: each is struck as the finger comes to it */
        int d = tv_pad_at(x, y);
        if (d >= 0 && TV_DEG_BTN[d] != tv_p_btn[k])
            tv_pad_strike(k, d);
        return;
    }
    if (bk_view == V_SEQ && tv_seq_roll(x, y, &st, &pad)) {
        int t = dream_ui.track;
        dream_cmd(tv_s_kit[k] ? MD_KIT_STEP : MD_SEQ_STEP, t, st);   /* the cursor follows across */
        if (tv_s_kit[k])
            dream_cmd(MD_KIT_PAD, 3, pad);
        else if (md_state()->sequence[t].steps[st].note) {          /* up and down: the note, a tone of the scale per 14 px */
            int saved = sq_field;
            while (fabsf(y - tv_s_y[k]) >= 14.0f) {
                int d = y < tv_s_y[k] ? 1 : -1;
                sq_field = SF_NOTE;
                sq_key(d > 0 ? BK_UP : BK_DOWN, 1);
                tv_s_y[k] += d > 0 ? -14.0f : 14.0f;
            }
            sq_field = saved;
        }
        return;
    }
    if (bk_view == V_MIXER)
        tv_mixer_set(tv_g_track[k], tv_g_row[k], x, y, tv_g_y0[k], tv_g_v0[k]);
}

static void bk_touch_view_up(int k, float x, float y)
{
    (void)x, (void)y;
    if (tv_p_btn[k] >= 0) {
        tv_pad_strike(k, -1);
        return;
    }
    if (bk_view == V_SEQ && fabsf(tv_s_x0[k] - x) < 8.0f && fabsf(tv_s_y0[k] - y) < 8.0f)   /* a tap on a step: on or off */
        sq_key(BK_A, 1);
}
static int bk_touch_view_step(int axis, int d) { (void)axis, (void)d; return 0; }

static int bk_touch_probe(const char *name)      /* (the tests: expect_opt mxrow 2, wpos 3 ...) */
{
    if (!strcmp(name, "labmod")) return lab_mod;
    if (!strcmp(name, "labfocus")) return lab_focus[lab_mod];
    if (!strcmp(name, "lit")) return (int)bk_notes_lit;
    if (!strcmp(name, "mxrow")) return mx_row;
    if (!strcmp(name, "atfam")) return at_fam;
    if (!strcmp(name, "atpos")) return at_pos;
    if (!strcmp(name, "wfam")) return wd_fam;
    if (!strcmp(name, "wpos")) return wd_pos[wd_fam];
    if (!strcmp(name, "arbasket")) return ar_basket;
    if (!strcmp(name, "arpos")) return ar_pos[ar_basket];
    if (!strcmp(name, "sqedit")) return sq_edit;
    if (!strcmp(name, "sqstep")) return md_state()->sequence[dream_ui.track].edit;
    if (!strcmp(name, "sqnote")) return md_state()->sequence[dream_ui.track].steps[md_state()->sequence[dream_ui.track].edit].note;
    return -1;
}
