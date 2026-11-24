/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka.
 * (1.5.8) Touch: the Brick's buttons on the screen, for Android phones and tablets without a controller. The stage
 * stays 4:3 in the middle; the bars beside it carry a D-pad, L1/L2 and SELECT on the left and A/B/X/Y, R1/R2, START
 * and MENU on the right (on a screen too square for bars the stage is made smaller until they fit). Every finger
 * presses what it is on, sliding moves with it, so held buttons and combinations work as on the device. A tap
 * leaves a ring that spreads and fades. A controller's or keyboard's button hides the pad (the stage full size
 * again); the next touch brings it back. Android's back key is B.
 * On by default on Android; MD_TOUCH=1 / =0 decides it elsewhere (the Mac: the mouse is a finger, for testing).
 * Only in the Android and Mac builds: SDL_RenderGeometry needs SDL 2.0.18, the handhelds' SDLs are older. */
#if defined(__ANDROID__) || defined(__APPLE__)
#define MD_HAVE_TOUCH 1
#include <math.h>
#ifdef __ANDROID__
#include <jni.h>
#endif

static int touch_on, touch_shown = 1;            /* the layer at all; shown now (hidden after a controller's button) */
static uint32_t held_touch;
static int tw_w, tw_h;                           /* the renderer's output, in pixels */
static SDL_Rect stage_dst;                       /* where the stage goes in it */
typedef struct { float x, y, w, h; int bit, round; const char *lab; uint8_t r, g, b; } tbtn;
static tbtn tb[40];
static int tbn;
static float dp_x, dp_y, dp_r;                   /* the D-pad: centre, radius */
#define TFINGERS 10
static struct { SDL_FingerID id; int used; float x, y; int stage; float sx, sy, ax, ay, moved; uint32_t t0, last; } fing[TFINGERS];
/* (1.6) what the stage gestures made, for the main loop to hand to the core under its lock */
static struct { int phase, slot; float x, y; } tq[256];   /* a finger of the stage: 0 down, 1 move, 2 up, in the stage's own coordinates */
static int tqn;
static void tq_add(int phase, int slot, float px, float py)
{
    float sx = stage_dst.w > 0 ? (px - stage_dst.x) * BK_W / stage_dst.w : 0.0f, sy = stage_dst.h > 0 ? (py - stage_dst.y) * BK_H / stage_dst.h : 0.0f;
    if (tqn >= 250 && phase == 1)                /* (a stalled loop: moves are given up first, never a down or an up) */
        return;
    if (tqn < 256)
        tq[tqn].phase = phase, tq[tqn].slot = slot, tq[tqn].x = sx, tq[tqn].y = sy, tqn++;
}
static void touch_buzz(void);
static void tq_add_cmd(int c);
static uint32_t press_at[BK_NBUTTONS];           /* (the tap's ring) */
static float ring_x[BK_NBUTTONS], ring_y[BK_NBUTTONS];

/* (1.6) the safe edges of the screen (a camera cut-out, the system bars): Android's own numbers, asked once a second */
static int ins_l, ins_t, ins_r, ins_b, ins_ok;
static void touch_insets_poll(void)
{
#ifdef __ANDROID__
    static uint32_t at;
    uint32_t now = SDL_GetTicks();
    if (at && now - at < 1000u)
        return;
    at = now | 1u;
    {
        JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
        jobject act = (jobject)SDL_AndroidGetActivity();
        int v[4] = {0, 0, 0, 0}, got = 0;
        if (env && act) {
            jclass ac = (*env)->GetObjectClass(env, act);
            jmethodID gw = (*env)->GetMethodID(env, ac, "getWindow", "()Landroid/view/Window;");
            jobject win = gw ? (*env)->CallObjectMethod(env, act, gw) : 0;
            jobject dec = 0, wi = 0;
            if (win) {
                jclass wc = (*env)->GetObjectClass(env, win);
                jmethodID gd = (*env)->GetMethodID(env, wc, "getDecorView", "()Landroid/view/View;");
                dec = gd ? (*env)->CallObjectMethod(env, win, gd) : 0;
                (*env)->DeleteLocalRef(env, wc);
            }
            if (dec) {
                jclass vc = (*env)->GetObjectClass(env, dec);
                jmethodID gi = (*env)->GetMethodID(env, vc, "getRootWindowInsets", "()Landroid/view/WindowInsets;");
                wi = gi ? (*env)->CallObjectMethod(env, dec, gi) : 0;
                (*env)->DeleteLocalRef(env, vc);
            }
            if (wi) {
                static const char *const SYS[4] = {"getSystemWindowInsetLeft", "getSystemWindowInsetTop", "getSystemWindowInsetRight", "getSystemWindowInsetBottom"};
                static const char *const CUT[4] = {"getSafeInsetLeft", "getSafeInsetTop", "getSafeInsetRight", "getSafeInsetBottom"};
                jclass wic = (*env)->GetObjectClass(env, wi);
                jmethodID gc = (*env)->GetMethodID(env, wic, "getDisplayCutout", "()Landroid/view/DisplayCutout;");
                jobject cut = 0;
                if (!gc)
                    (*env)->ExceptionClear(env);
                else
                    cut = (*env)->CallObjectMethod(env, wi, gc);
                for (int i = 0; i < 4; i++) {
                    jmethodID m = (*env)->GetMethodID(env, wic, SYS[i], "()I");
                    if (m)
                        v[i] = (*env)->CallIntMethod(env, wi, m);
                    else
                        (*env)->ExceptionClear(env);
                }
                if (cut) {
                    jclass cc = (*env)->GetObjectClass(env, cut);
                    for (int i = 0; i < 4; i++) {
                        jmethodID m = (*env)->GetMethodID(env, cc, CUT[i], "()I");
                        int c = m ? (*env)->CallIntMethod(env, cut, m) : 0;
                        if (!m)
                            (*env)->ExceptionClear(env);
                        v[i] = c > v[i] ? c : v[i];
                    }
                    (*env)->DeleteLocalRef(env, cc);
                    (*env)->DeleteLocalRef(env, cut);
                }
                got = 1;
                (*env)->DeleteLocalRef(env, wic);
                (*env)->DeleteLocalRef(env, wi);
            }
            if (dec)
                (*env)->DeleteLocalRef(env, dec);
            if (win)
                (*env)->DeleteLocalRef(env, win);
            (*env)->DeleteLocalRef(env, ac);
            (*env)->DeleteLocalRef(env, act);
            if ((*env)->ExceptionCheck(env))
                (*env)->ExceptionClear(env);
        }
        if (got)
            ins_l = v[0], ins_t = v[1], ins_r = v[2], ins_b = v[3], ins_ok = 1;
    }
#endif
}
static int lay_key;
static int lay_ins, lay_tpad;                              /* the settings the layout was made for (a change makes it again) */
static int lay_portrait;
static void place_bar(float rx, float ry, float rw, float rh, int group, float fsz)
{   /* one group of buttons in a rectangle, anchored to its bottom (a thumb rests there): group 0 the D-pad, L1/L2, FN and
       SELECT; group 1 A/B/X/Y, R1/R2, START and MENU */
    float eh = fminf(rh, rw * 1.7f), oy = ry + rh - eh, cx = rx + rw / 2.0f;
    float u = fminf(rw, eh * 0.62f) * fsz, sh_w = u * 0.40f, sh_h = eh * 0.11f, top = oy + eh * 0.04f, row = oy + eh * 0.85f;
    if (group == 0) {
        tb[tbn++] = (tbtn){cx - sh_w - u * 0.03f, top, sh_w, sh_h, BK_L2, 0, "L2", 150, 170, 255};
        tb[tbn++] = (tbtn){cx + u * 0.03f, top, sh_w, sh_h, BK_L1, 0, "L1", 150, 170, 255};
        dp_x = cx, dp_y = oy + eh * 0.50f, dp_r = fminf(u * 0.40f, eh * 0.24f);
        tb[tbn++] = (tbtn){cx - u * 0.45f, row, u * 0.42f, eh * 0.09f, BK_FN, 0, "FN", 255, 220, 140};
        tb[tbn++] = (tbtn){cx + u * 0.03f, row, u * 0.42f, eh * 0.09f, BK_SELECT, 0, "SELECT", 190, 190, 210};
    } else {
        float cy = oy + eh * 0.50f, d = fminf(u * 0.40f, eh * 0.24f) * 0.66f, r = d * 0.62f;
        tb[tbn++] = (tbtn){cx - sh_w - u * 0.03f, top, sh_w, sh_h, BK_R1, 0, "R1", 150, 170, 255};
        tb[tbn++] = (tbtn){cx + u * 0.03f, top, sh_w, sh_h, BK_R2, 0, "R2", 150, 170, 255};
        tb[tbn++] = (tbtn){cx + d - r, cy - r, 2 * r, 2 * r, BK_A, 1, "A", 110, 230, 190};
        tb[tbn++] = (tbtn){cx - r, cy + d - r, 2 * r, 2 * r, BK_B, 1, "B", 255, 140, 130};
        tb[tbn++] = (tbtn){cx - r, cy - d - r, 2 * r, 2 * r, BK_X, 1, "X", 120, 190, 255};
        tb[tbn++] = (tbtn){cx - d - r, cy - r, 2 * r, 2 * r, BK_Y, 1, "Y", 255, 210, 110};
        tb[tbn++] = (tbtn){cx - u * 0.45f, row, u * 0.42f, eh * 0.09f, BK_START, 0, "START", 190, 190, 210};
        tb[tbn++] = (tbtn){cx + u * 0.03f, row, u * 0.42f, eh * 0.09f, BK_MENU, 0, "MENU", 255, 190, 240};
    }
}

static int touch_settings_key(void)
{
    return bk_option_get("tsize") | bk_option_get("tside") << 2;
}

/* ---- (1.6) the places' own buttons: no pad of the Brick's buttons; a dock of what is always there (back, home, places,
   quick bar, Sound Lab, mixer, options, help) on one side; on the other PLAY, PLAY MODE (the FN switch), SHIFT (R2 kept)
   and, under them, what the place you are in names (the hint line's A, X, Y, L1/R1 ... as touch buttons) ---- */
static bk_chip tch[12];
static int tchn, t_playing, t_sig;
static char tlab[40][32];
enum { SP_HOME = 24, SP_OPTIONS = 25, SP_HELP = 26, SP_SHIFT = 27, SP_HIDE = 28 };
static void touch_chips_refresh(void)            /* (under the core's lock) what the place names now; a change makes the layout again */
{
    bk_chip c[12];
    int n = bk_touch_chips(c, 12), sig = 0, pl = bk_touch_state(0);
    for (int i = 0; i < n; i++) {
        sig = sig * 31 + c[i].btn + 1;
        for (const char *q = c[i].label; *q; q++)
            sig = sig * 131 + (unsigned char)*q;
    }
    sig = sig * 7 + pl + 1;
    memcpy(tch, c, sizeof c);
    tchn = n, t_playing = pl;
    if (sig != t_sig)
        t_sig = sig, tbn = 0;
}
static void nat_item(float x, float y, float w, float h, int bit, const char *label, uint8_t r, uint8_t g, uint8_t b)
{
    if (tbn >= 40)
        return;
    {   /* "< TRACK" and "TRACK >": the arrow stays, the word is in the stage's language */
        size_t L = strlen(label);
        if (!strncmp(label, "< ", 2))
            snprintf(tlab[tbn], sizeof tlab[0], "< %s", bk_touch_text(label + 2));
        else if (L > 2 && !strcmp(label + L - 2, " >")) {
            char base[32];
            snprintf(base, sizeof base, "%.*s", (int)L - 2, label);
            snprintf(tlab[tbn], sizeof tlab[0], "%s >", bk_touch_text(base));
        } else
            snprintf(tlab[tbn], sizeof tlab[0], "%s", bk_touch_text(label));
    }
    tb[tbn] = (tbtn){x, y, w, h, bit, 2, tlab[tbn], r, g, b};
    tbn++;
}
typedef struct { const char *lab; int bit; uint8_t r, g, b; } nat_def;
static void nat_stack(float rx, float ry, float rw, float rh, const nat_def *d, int n)
{
    float ih, gap, w;
    if (n <= 0)
        return;
    ih = rh / ((float)n * 1.14f + 0.14f);
    ih = fminf(ih, rw * 0.62f);
    gap = ih * 0.14f;
    w = rw * 0.92f;
    for (int i = 0; i < n; i++)
        nat_item(rx + (rw - w) / 2.0f, ry + gap + (float)i * (ih + gap), w, ih, d[i].bit, d[i].lab, d[i].r, d[i].g, d[i].b);
}
static void nat_chip_color(int btn, uint8_t *r, uint8_t *g, uint8_t *b)
{
    switch (btn) {
    case BK_A: *r = 110, *g = 230, *b = 190; break;
    case BK_B: *r = 255, *g = 140, *b = 130; break;
    case BK_X: *r = 120, *g = 190, *b = 255; break;
    case BK_Y: *r = 255, *g = 210, *b = 110; break;
    case BK_L1: case BK_R1: *r = 175, *g = 160, *b = 255; break;
    default: *r = 200, *g = 205, *b = 220; break;
    }
}
static int nat_build(nat_def *out, int max, int part)   /* part 0: the dock; 1: transport, then the chips */
{
    int n = 0;
    if (part == 0) {
        static const nat_def D[] = {{"BACK", BK_B, 255, 140, 130}, {"HOME", SP_HOME, 170, 220, 190}, {"PLACES", BK_MENU, 255, 190, 240},
                                    {"QUICK BAR", BK_L2, 150, 170, 255}, {"SOUND LAB", BK_F1, 240, 200, 150}, {"MIXER", BK_F2, 200, 230, 160},
                                    {"OPTIONS", SP_OPTIONS, 190, 190, 210}, {"HELP", SP_HELP, 190, 190, 210},
                                    {"HIDE", SP_HIDE, 150, 150, 165}};
        for (unsigned i = 0; i < sizeof D / sizeof D[0] && n < max; i++)
            out[n++] = D[i];
    } else {
        out[n++] = (nat_def){t_playing ? "STOP" : "PLAY", BK_START, 140, 230, 150};
        out[n++] = (nat_def){"PLAY MODE", BK_FN, 255, 220, 140};
        out[n++] = (nat_def){"SHIFT", SP_SHIFT, 230, 150, 200};
        for (int i = 0; i < tchn && n < max; i++) {
            nat_def d = {tch[i].label, tch[i].btn, 0, 0, 0};
            nat_chip_color(tch[i].btn, &d.r, &d.g, &d.b);
            out[n++] = d;
        }
    }
    return n;
}
static void touch_layout_native(int W, int H, int side)
{
    nat_def dk[16], tr[24];
    int nd = nat_build(dk, 16, 0), nt = nat_build(tr, 24, 1);
    float s, sw, sh, mn, il, ir;
    float dpi = 1.0f;
    {
        float d = 0, hd = 0, vd = 0;
        const char *e = getenv("MD_DPI");
        if (e && atof(e) > 40.0)
            d = (float)atof(e);
        else if (SDL_GetDisplayDPI(0, &d, &hd, &vd) != 0 || d < 40.0f)
            d = 0;
        if (d > 0.0f)
            dpi = d;
    }
    tbn = 0, dp_r = 0;                           /* (no D-pad drawn: a drag on the stage is the D-pad) */
    if (!touch_shown) {
        s = fminf((float)W / BK_W, (float)H / BK_H);
        sw = BK_W * s, sh = BK_H * s;
        stage_dst.x = (int)((W - sw) / 2), stage_dst.y = (int)((H - sh) / 2), stage_dst.w = (int)sw, stage_dst.h = (int)sh;
        return;
    }
    if (H > W) {                                 /* upright: the stage on top, the buttons in rows of three below it */
        float ph, cw, ih, gap, rows;
        int all = nd + nt, cols = 3;
        s = fminf((float)W / BK_W, (float)H * 0.55f / BK_H);
        sw = BK_W * s, sh = BK_H * s;
        stage_dst.x = (int)((W - sw) / 2), stage_dst.y = (int)fmaxf(H * 0.01f, (float)ins_t), stage_dst.w = (int)sw, stage_dst.h = (int)sh;
        ph = H - (stage_dst.y + sh) - (float)ins_b;
        rows = ceilf((float)all / (float)cols);
        cw = ((float)(W - ins_l - ins_r)) / (float)cols;
        ih = fminf(ph / (rows * 1.12f + 0.12f), cw * 0.55f);
        gap = ih * 0.12f;
        {
            nat_def lst[40];
            int n = 0;
            for (int i = 3; i < nt; i++)         /* (the places' own first, nearest the stage; then PLAY, PLAY MODE, SHIFT; the dock last) */
                lst[n++] = tr[i];
            lst[n++] = tr[0], lst[n++] = tr[1], lst[n++] = tr[2];
            for (int i = 0; i < nd; i++)
                lst[n++] = dk[i];
            for (int i = 0; i < n && i < 40; i++) {
                int c = i % cols, r = i / cols;
                nat_item((float)ins_l + c * cw + cw * 0.04f, stage_dst.y + sh + gap + (float)r * (ih + gap), cw * 0.92f, ih, lst[i].bit, lst[i].lab, lst[i].r, lst[i].g, lst[i].b);
            }
        }
        return;
    }
    mn = fmaxf(H * 0.17f, fminf(0.95f * dpi, H * 0.30f));   /* the bars: at least this wide, about an inch */
    il = ins_ok ? (float)ins_l : (float)W / (float)H > 1.9f ? W * 0.03f : 0.0f;
    ir = ins_ok ? (float)ins_r : il;
    s = fminf((float)W / BK_W, (float)H / BK_H);
    if (W - BK_W * s < 2.0f * (mn + fmaxf(il, ir)))
        s = fmaxf(0.2f, (W - 2.0f * (mn + fmaxf(il, ir))) / BK_W);
    sw = BK_W * s, sh = BK_H * s;
    stage_dst.x = (int)((W - sw) / 2), stage_dst.y = (int)((H - sh) / 2), stage_dst.w = (int)sw, stage_dst.h = (int)sh;
    {
        float lw = stage_dst.x - il, rw = W - (stage_dst.x + sw) - ir, top = (float)ins_t, hh = (float)(H - ins_t - ins_b);
        if (!side) {
            nat_stack(il, top, lw, hh, dk, nd);
            nat_stack(stage_dst.x + sw, top, rw, hh, tr, nt);
        } else {
            nat_stack(il, top, lw, hh, tr, nt);
            nat_stack(stage_dst.x + sw, top, rw, hh, dk, nd);
        }
    }
}

static int hide_after, touch_hide_now;           /* (the HIDE button pressed: the buttons go once the press is settled) */
static void touch_mode_update(void)              /* (1.6) TOUCH BUTTONS: AUTO as before; SHOWN always; HIDDEN away while a controller is there */
{
    int m = bk_option_get("tmode");
    if (m == 1)
        touch_shown = 1;
    else if (m == 2)
        touch_shown = SDL_NumJoysticks() == 0;   /* (no controller: they stay, a touch-only device never loses its way) */
}
static void touch_layout(int W, int H)
{
    if (touch_hide_now) {                        /* (the HIDE button: after its press is settled) */
        touch_hide_now = 0;
        touch_shown = 0, tbn = 0;
    }
    touch_mode_update();
    float s, sw, sh, fsz, wide;
    int key = touch_settings_key(), size = key & 3, side = key >> 2 & 1, portrait = H > W, ins;
    touch_insets_poll();
    ins = ins_l + ins_t * 7 + ins_r * 31 + ins_b * 131;
    int tpad = bk_option_get("tpad");
    if (W == tw_w && H == tw_h && tbn && key == lay_key && portrait == lay_portrait && ins == lay_ins && tpad == lay_tpad)
        return;
    tw_w = W, tw_h = H, lay_key = key, lay_portrait = portrait, lay_ins = ins, lay_tpad = tpad;
    if (!tpad) {
        touch_layout_native(W, H, side);
        return;
    }
    fsz = size == 0 ? 0.85f : 1.0f;
    tbn = 0;
    {   /* (1.6) a tablet's bars may be narrower than a phone's: they need about 1.15 inch for the D-pad and the A/B/X/Y
           diamond, which on a big screen is a smaller share of the height, so the stage stays larger (MD_DPI=N tests it) */
        static float dpi;
        float bw_min = H * 0.30f;
        if (dpi <= 0.0f) {
            float d = 0, hd = 0, vd = 0;
            const char *e = getenv("MD_DPI");
            if (e && atof(e) > 40.0)
                d = (float)atof(e);
            else if (SDL_GetDisplayDPI(0, &d, &hd, &vd) != 0 || d < 40.0f)
                d = 0;
            dpi = d > 0.0f ? d : 1.0f;           /* (1: unknown, the phone's share) */
        }
        if (dpi > 1.0f)
            bw_min = fminf(H * 0.30f, fmaxf(H * 0.20f, 1.15f * dpi));
        if (size == 2)
            bw_min *= 1.2f;
        wide = (float)W / (float)H;
        if (!touch_shown) {                      /* hidden: the stage as large as it fits */
            s = fminf((float)W / BK_W, (float)H / BK_H);
            sw = BK_W * s, sh = BK_H * s;
            stage_dst.x = (int)((W - sw) / 2), stage_dst.y = (int)((H - sh) / 2);
            stage_dst.w = (int)sw, stage_dst.h = (int)sh;
            return;
        }
        if (portrait) {                          /* (1.6) held upright: the stage on top, the buttons in two groups below */
            float ph;
            s = fminf((float)W / BK_W, (float)H * 0.56f / BK_H);
            sw = BK_W * s, sh = BK_H * s;
            stage_dst.x = (int)((W - sw) / 2), stage_dst.y = (int)fmaxf(H * 0.02f, (float)ins_t);
            stage_dst.w = (int)sw, stage_dst.h = (int)sh;
            ph = H - (stage_dst.y + sh) - (float)ins_b;
            place_bar((float)ins_l, stage_dst.y + sh, (W - ins_l - ins_r) / 2.0f, ph, side, fsz);
            place_bar(ins_l + (W - ins_l - ins_r) / 2.0f, stage_dst.y + sh, (W - ins_l - ins_r) / 2.0f, ph, !side, fsz);
            return;
        }
        {
            float il = ins_ok ? (float)ins_l : wide > 1.9f ? W * 0.03f : 0.0f;   /* (without Android's numbers: a long phone's cut-out guessed) */
            float ir = ins_ok ? (float)ins_r : il, im = fmaxf(il, ir), bw, rwl, rwr;
            s = fminf((float)W / BK_W, (float)H / BK_H);
            if (W - BK_W * s < 2.0f * (bw_min + im))      /* too square for the bars: the stage smaller */
                s = fmaxf(0.2f, (W - 2.0f * (bw_min + im)) / BK_W);
            sw = BK_W * s, sh = BK_H * s;
            stage_dst.x = (int)((W - sw) / 2), stage_dst.y = (int)((H - sh) / 2);
            stage_dst.w = (int)sw, stage_dst.h = (int)sh;
            bw = (W - sw) / 2.0f;
            rwl = bw - il, rwr = bw - ir;
            place_bar(il, (float)ins_t, rwl, (float)(H - ins_t - ins_b), side, fsz);
            place_bar(W - bw, (float)ins_t, rwr, (float)(H - ins_t - ins_b), !side, fsz);
        }
    }
}

/* what a finger at (x, y) presses */
static uint32_t touch_mask_at(float x, float y)
{
    float dx = x - dp_x, dy = y - dp_y, dist = sqrtf(dx * dx + dy * dy);
    if (dist < dp_r * 1.35f && dist > dp_r * 0.14f) {   /* the D-pad: eight ways, the diagonals press two */
        float a = atan2f(dy, dx) * 180.0f / 3.14159265f;  /* 0 right, 90 down */
        int sct = (int)floorf((a + 202.5f) / 45.0f) & 7; /* 0 left, 1 up-left, 2 up, 3 up-right, 4 right, ... */
        static const uint32_t M[8] = {1u << BK_LEFT, 1u << BK_LEFT | 1u << BK_UP, 1u << BK_UP, 1u << BK_UP | 1u << BK_RIGHT,
                                      1u << BK_RIGHT, 1u << BK_RIGHT | 1u << BK_DOWN, 1u << BK_DOWN, 1u << BK_DOWN | 1u << BK_LEFT};
        return M[sct];
    }
    for (int i = 0; i < tbn; i++) {
        const tbtn *b = &tb[i];
        if (b->round == 2) {                     /* (1.6) the places' own buttons, stacked: hardly larger than drawn */
            float mx = b->w * 0.03f, my = b->h * 0.05f;
            if (x >= b->x - mx && x < b->x + b->w + mx && y >= b->y - my && y < b->y + b->h + my)
                return 1u << b->bit;
        } else if (b->round) {
            float r = b->w / 2.0f, ex = x - (b->x + r), ey = y - (b->y + r);
            if (ex * ex + ey * ey < r * r * 1.45f)   /* (a little larger than drawn: thumbs are wide) */
                return 1u << b->bit;
        } else {
            float mx = b->w * 0.10f, my = b->h * 0.25f;
            if (x >= b->x - mx && x < b->x + b->w + mx && y >= b->y - my && y < b->y + b->h + my)
                return 1u << b->bit;
        }
    }
    return 0;
}

static int touch_near_button(float x, float y) { (void)x; (void)y; return 0; }
static int fn_down;                              /* (a finger is on the FN button) */
static uint32_t sp_down, touch_latch;            /* the dock's pressed special buttons; R2 latched by SHIFT */
static void touch_recount(void)
{
    uint32_t m = 0, was = held_touch | (fn_down ? 1u << BK_FN : 0u), now = SDL_GetTicks();
    for (int i = 0; i < TFINGERS; i++)
        if (fing[i].used && !fing[i].stage) {
            uint32_t f = touch_mask_at(fing[i].x, fing[i].y);
            for (int b = 0; b < BK_NBUTTONS; b++)
                if ((f & ~was & ~m) >> b & 1u)
                    press_at[b] = now ? now : 1u, ring_x[b] = fing[i].x, ring_y[b] = fing[i].y;
            m |= f;
        }
    if ((m & ~was) & 1u << BK_FN)                /* (1.6) FN: a tap latches it, as the Brick's switch does */
        fn_latched ^= 1u << BK_FN;
    {   /* the dock's own things: HOME, OPTIONS, HELP once each press; SHIFT latches R2 */
        uint32_t sp = (m >> 24) & 0x1Fu, rising = sp & ~sp_down;
        sp_down = sp;
        for (int c = 0; c < 3; c++)
            if (rising >> c & 1u)
                tq_add_cmd(c);
        if (rising >> 3 & 1u)
            touch_latch ^= 1u << BK_R2;
        if (rising >> 4 & 1u)                    /* HIDE: the buttons away until the next touch */
            hide_after = 1;
    }
    if (m & ~was)
        touch_buzz();
    fn_down = (m >> BK_FN) & 1u;
    held_touch = m & 0x00FFFFFFu & ~(1u << BK_FN);
    if (hide_after) {
        hide_after = 0;
        touch_hide_now = 1;
    }
}

static void tq_add_cmd(int c)
{
    if (tqn < 256)
        tq[tqn].phase = 3, tq[tqn].slot = c, tq[tqn].x = tq[tqn].y = 0, tqn++;
}
static void touch_flush(void)                    /* (under the core's lock) what the fingers of the stage did, to the core */
{
    for (int i = 0; i < tqn; i++) {
        if (tq[i].phase == 3)
            bk_touch_cmd(tq[i].slot);
        else
            bk_touch_event(tq[i].phase, tq[i].slot, tq[i].x, tq[i].y);
    }
    tqn = 0;
}

static void touch_release_all(void)
{
    sp_down = 0;
    for (int i = 0; i < TFINGERS; i++)
        if (fing[i].used && fing[i].stage)
            tq_add(2, i, fing[i].x, fing[i].y);
    memset(fing, 0, sizeof fing);
    held_touch = 0;
}

static void touch_finger(SDL_FingerID id, float nx, float ny, int phase)   /* phase 0 down, 1 move, 2 up */
{
    int k = -1, fr = -1;
    float px = nx * tw_w, py = ny * tw_h;
    for (int i = 0; i < TFINGERS; i++) {
        if (fing[i].used && fing[i].id == id)
            k = i;
        if (!fing[i].used && fr < 0)
            fr = i;
    }
    touch_mode_update();
    if (phase == 0 && !touch_shown && bk_option_get("tmode") != 2) {   /* the first touch only brings the pad back (not when HIDDEN: then the stage takes it) */
        touch_shown = 1, tbn = 0;
        return;
    }
    if (phase == 2) {
        if (k >= 0) {
            if (fing[k].stage)
                tq_add(2, k, px, py);
            fing[k].used = 0;
        }
    } else {
        if (k < 0) {
            if (phase != 0 || fr < 0)
                return;
            k = fr, fing[k].used = 1, fing[k].id = id;
            fing[k].x = px, fing[k].y = py, fing[k].last = SDL_GetTicks();
            fing[k].stage = touch_mask_at(px, py) == 0 && px >= stage_dst.x && px < stage_dst.x + stage_dst.w &&
                            py >= stage_dst.y && py < stage_dst.y + stage_dst.h;
            if (fing[k].stage)
                tq_add(0, k, px, py);
        } else if (fing[k].stage)
            tq_add(1, k, px, py);
        fing[k].x = px, fing[k].y = py, fing[k].last = SDL_GetTicks();
    }
    touch_recount();
}
/* (1.6) A finger whose lifting never came (a system gesture took it, the window lost focus, a stalled loop) must not
   keep a button held: each turn of the loop every finger we hold is looked for in SDL's own list, and one that SDL no
   longer has is let go; one that has not been heard of for 20 s as well. */
static void touch_sync(void)
{
    uint32_t now = SDL_GetTicks();
    int any = 0, changed = 0;
    for (int i = 0; i < TFINGERS; i++)
        any += fing[i].used != 0;
    if (!any)
        return;
    for (int i = 0; i < TFINGERS; i++) {
        int alive = 0;
        if (!fing[i].used)
            continue;
        if (fing[i].id == -1) {                  /* (the Mac's mouse as a finger: its button tells) */
            alive = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) != 0;
        } else {
            int nd = SDL_GetNumTouchDevices();
            for (int d = 0; d < nd && !alive; d++) {
                SDL_TouchID dev = SDL_GetTouchDevice(d);
                int nf = SDL_GetNumTouchFingers(dev);
                for (int f = 0; f < nf && !alive; f++) {
                    SDL_Finger *sf = SDL_GetTouchFinger(dev, f);
                    alive = sf && sf->id == fing[i].id;
                }
            }
            if (nd == 0)                         /* (an SDL that lists no touch device: only the time limit) */
                alive = 1;
        }
        if (!alive || now - fing[i].last > 20000u) {
            if (fing[i].stage)
                tq_add(2, i, fing[i].x, fing[i].y);
            fing[i].used = 0;
            changed = 1;
        }
    }
    if (changed)
        touch_recount();
}

static void touch_buzz(void)                     /* (1.6) a short buzz on a pressed button (Android's own haptic feedback) */
{
#ifdef __ANDROID__
    static uint32_t last;
    uint32_t now = SDL_GetTicks();
    if (!bk_option_get("tbuzz") || now - last < 40u)
        return;
    last = now;
    {
        JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
        jobject act = (jobject)SDL_AndroidGetActivity();
        if (env && act) {
            jclass ac = (*env)->GetObjectClass(env, act);
            jmethodID gw = (*env)->GetMethodID(env, ac, "getWindow", "()Landroid/view/Window;");
            jobject win = gw ? (*env)->CallObjectMethod(env, act, gw) : 0;
            if (win) {
                jclass wc = (*env)->GetObjectClass(env, win);
                jmethodID gd = (*env)->GetMethodID(env, wc, "getDecorView", "()Landroid/view/View;");
                jobject view = gd ? (*env)->CallObjectMethod(env, win, gd) : 0;
                if (view) {
                    jclass vc = (*env)->GetObjectClass(env, view);
                    jmethodID ph = (*env)->GetMethodID(env, vc, "performHapticFeedback", "(I)Z");
                    if (ph)
                        (*env)->CallBooleanMethod(env, view, ph, 3);   /* (KEYBOARD_TAP) */
                    (*env)->DeleteLocalRef(env, vc);
                    (*env)->DeleteLocalRef(env, view);
                }
                (*env)->DeleteLocalRef(env, wc);
                (*env)->DeleteLocalRef(env, win);
            }
            (*env)->DeleteLocalRef(env, ac);
            (*env)->DeleteLocalRef(env, act);
            if ((*env)->ExceptionCheck(env))
                (*env)->ExceptionClear(env);
        }
    }
#endif
}

static void touch_hide(void)                     /* a controller's or keyboard's button: the pad away */
{
    if (touch_shown && bk_option_get("tmode") != 1) {
        touch_shown = 0, tbn = 0;
        touch_release_all();
    }
}

/* ---- drawing ---- */
static void t_disc(SDL_Renderer *r, float cx, float cy, float rad, SDL_Color c)
{
    enum { N = 32 };
    SDL_Vertex v[N + 1];
    int idx[N * 3];
    v[0] = (SDL_Vertex){{cx, cy}, c, {0, 0}};
    for (int i = 0; i < N; i++) {
        float a = i * 6.2831853f / N;
        v[i + 1] = (SDL_Vertex){{cx + rad * cosf(a), cy + rad * sinf(a)}, c, {0, 0}};
        idx[i * 3] = 0, idx[i * 3 + 1] = i + 1, idx[i * 3 + 2] = (i + 1) % N + 1;
    }
    SDL_RenderGeometry(r, 0, v, N + 1, idx, N * 3);
}
static void t_ring(SDL_Renderer *r, float cx, float cy, float rad, float th, SDL_Color c)
{
    enum { N = 40 };
    SDL_Vertex v[2 * N];
    int idx[N * 6];
    for (int i = 0; i < N; i++) {
        float a = i * 6.2831853f / N, ca = cosf(a), sa = sinf(a);
        v[2 * i] = (SDL_Vertex){{cx + rad * ca, cy + rad * sa}, c, {0, 0}};
        v[2 * i + 1] = (SDL_Vertex){{cx + (rad - th) * ca, cy + (rad - th) * sa}, c, {0, 0}};
    }
    for (int i = 0; i < N; i++) {
        int a = 2 * i, b = 2 * ((i + 1) % N);
        idx[i * 6] = a, idx[i * 6 + 1] = a + 1, idx[i * 6 + 2] = b;
        idx[i * 6 + 3] = b, idx[i * 6 + 4] = a + 1, idx[i * 6 + 5] = b + 1;
    }
    SDL_RenderGeometry(r, 0, v, 2 * N, idx, N * 6);
}
static void t_pill(SDL_Renderer *r, float x, float y, float w, float h, SDL_Color c)
{   /* one shape, one fan from the middle: no part laid over another (half-transparent overlaps showed as stripes) */
    enum { N = 16 };
    SDL_Vertex v[2 * (N + 1) + 1];
    int idx[2 * (N + 1) * 3], n = 0, k = 0;
    float rad = h / 2.0f, cy = y + rad, xl = x + rad, xr = x + w - rad;
    if (xr < xl)
        xl = xr = x + w / 2.0f;
    v[n++] = (SDL_Vertex){{x + w / 2.0f, cy}, c, {0, 0}};
    for (int i = 0; i <= N; i++) {               /* the right end, top to bottom, then the left, bottom to top */
        float a = -1.5707963f + i * 3.1415927f / N;
        v[n++] = (SDL_Vertex){{xr + rad * cosf(a), cy + rad * sinf(a)}, c, {0, 0}};
    }
    for (int i = 0; i <= N; i++) {
        float a = 1.5707963f + i * 3.1415927f / N;
        v[n++] = (SDL_Vertex){{xl + rad * cosf(a), cy + rad * sinf(a)}, c, {0, 0}};
    }
    for (int i = 1; i < n; i++)
        idx[k++] = 0, idx[k++] = i, idx[k++] = i + 1 < n ? i + 1 : 1;
    SDL_RenderGeometry(r, 0, v, n, idx, k);
}
static void t_tri(SDL_Renderer *r, float x0, float y0, float x1, float y1, float x2, float y2, SDL_Color c)
{
    SDL_Vertex v[3] = {{{x0, y0}, c, {0, 0}}, {{x1, y1}, c, {0, 0}}, {{x2, y2}, c, {0, 0}}};
    SDL_RenderGeometry(r, 0, v, 3, 0, 0);
}
/* a 5 x 7 font: capitals, digits and a few signs (labels are drawn in capitals; an umlaut or accent gives its plain letter) */
static const struct { char c; uint8_t row[7]; } TFONT[] = {
    {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}}, {'C', {14, 17, 16, 16, 16, 17, 14}},
    {'D', {30, 17, 17, 17, 17, 17, 30}}, {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}}, {'I', {14, 4, 4, 4, 4, 4, 14}},
    {'J', {7, 2, 2, 2, 2, 18, 12}},      {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}}, {'O', {14, 17, 17, 17, 17, 17, 14}},
    {'P', {30, 17, 17, 30, 16, 16, 16}}, {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},       {'U', {17, 17, 17, 17, 17, 17, 14}},
    {'V', {17, 17, 17, 17, 17, 10, 4}},  {'W', {17, 17, 17, 21, 21, 27, 17}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},      {'2', {14, 17, 1, 2, 4, 8, 31}},
    {'3', {30, 1, 1, 14, 1, 1, 30}},     {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 30, 1, 1, 17, 14}},
    {'6', {6, 8, 16, 30, 17, 17, 14}},   {'7', {31, 1, 2, 4, 8, 8, 8}},       {'8', {14, 17, 17, 14, 17, 17, 14}},
    {'9', {14, 17, 17, 15, 1, 2, 12}},
    {'.', {0, 0, 0, 0, 0, 12, 12}},      {',', {0, 0, 0, 0, 12, 4, 8}},       {':', {0, 12, 12, 0, 12, 12, 0}},
    {'/', {1, 1, 2, 4, 8, 16, 16}},      {'+', {0, 4, 4, 31, 4, 4, 0}},       {'-', {0, 0, 0, 31, 0, 0, 0}},
    {'<', {2, 4, 8, 16, 8, 4, 2}},       {'>', {8, 4, 2, 1, 2, 4, 8}},        {'(', {2, 4, 8, 8, 8, 4, 2}},
    {')', {8, 4, 2, 2, 2, 4, 8}},        {'?', {14, 17, 1, 2, 4, 0, 4}},      {'!', {4, 4, 4, 4, 4, 0, 4}},
    {'\'', {4, 4, 8, 0, 0, 0, 0}},       {'=', {0, 0, 31, 0, 31, 0, 0}}};
static int t_glyph(unsigned char c)              /* the glyph for a byte, -1: a space or nothing */
{
    if (c >= 'a' && c <= 'z')
        c = (unsigned char)(c - 'a' + 'A');
    for (unsigned g = 0; g < sizeof TFONT / sizeof TFONT[0]; g++)
        if ((unsigned char)TFONT[g].c == c)
            return (int)g;
    return -1;
}
static int t_flat(unsigned char lead, unsigned char c)   /* a UTF-8 accent or umlaut: its plain capital */
{
    if (lead != 0xC3)
        return 0;
    if (c >= 0x80 && c <= 0x85) return 'A';
    if (c == 0x87) return 'C';
    if (c >= 0x88 && c <= 0x8B) return 'E';
    if (c >= 0x8C && c <= 0x8F) return 'I';
    if (c == 0x91) return 'N';
    if ((c >= 0x92 && c <= 0x96) || c == 0x98) return 'O';
    if (c >= 0x99 && c <= 0x9C) return 'U';
    if (c == 0x9F) return 'S';
    if (c >= 0xA0 && c <= 0xA5) return 'A';
    if (c == 0xA7) return 'C';
    if (c >= 0xA8 && c <= 0xAB) return 'E';
    if (c >= 0xAC && c <= 0xAF) return 'I';
    if (c == 0xB1) return 'N';
    if ((c >= 0xB2 && c <= 0xB6) || c == 0xB8) return 'O';
    if (c >= 0xB9 && c <= 0xBC) return 'U';
    return 0;
}
static int t_text_len(const char *s)             /* glyph cells in a label (an accent's two bytes are one) */
{
    int n = 0;
    for (; *s; s++, n++)
        if ((unsigned char)*s == 0xC3 && s[1])
            s++;
    return n;
}
static void t_text(SDL_Renderer *r, float cx, float cy, float px, const char *s, SDL_Color c)
{
    SDL_FRect q[48 * 18];
    int n = 0, len = t_text_len(s);
    float x = cx - (len * 6 - 1) * px / 2.0f, y = cy - 3.5f * px;
    for (; *s; s++, x += 6 * px) {
        int g;
        unsigned char ch = (unsigned char)*s;
        if (ch == 0xC3 && s[1]) {
            ch = (unsigned char)t_flat(ch, (unsigned char)s[1]);
            s++;
        }
        g = ch ? t_glyph(ch) : -1;
        if (g < 0)
            continue;
        for (int row = 0; row < 7; row++)
            for (int col = 0; col < 5; col++)
                if (TFONT[g].row[row] >> (4 - col) & 1 && n < (int)(sizeof q / sizeof q[0]))
                    q[n++] = (SDL_FRect){x + col * px, y + row * px, px * 1.02f, px * 1.02f};
    }
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRectsF(r, q, n);
}

static void t_rrect(SDL_Renderer *r, float x, float y, float w, float h, float rad, SDL_Color c)
{   /* a rounded rectangle, one fan from its middle */
    enum { N = 6 };
    SDL_Vertex v[4 * (N + 1) + 1];
    int idx[4 * (N + 1) * 3], n = 0, k = 0;
    if (rad > h / 2.0f)
        rad = h / 2.0f;
    if (rad > w / 2.0f)
        rad = w / 2.0f;
    v[n++] = (SDL_Vertex){{x + w / 2.0f, y + h / 2.0f}, c, {0, 0}};
    for (int q = 0; q < 4; q++) {                /* clockwise: top right, bottom right, bottom left, top left */
        float ccx = (q == 0 || q == 1) ? x + w - rad : x + rad, ccy = (q == 1 || q == 2) ? y + h - rad : y + rad;
        for (int i = 0; i <= N; i++) {
            float a = -1.5707963f + (float)q * 1.5707963f + (float)i * 1.5707963f / (float)N;
            v[n++] = (SDL_Vertex){{ccx + rad * cosf(a), ccy + rad * sinf(a)}, c, {0, 0}};
        }
    }
    for (int i = 1; i < n; i++)
        idx[k++] = 0, idx[k++] = i, idx[k++] = i + 1 < n ? i + 1 : 1;
    SDL_RenderGeometry(r, 0, v, n, idx, k);
}
static void t_label(SDL_Renderer *r, float x, float y, float w, float h, const char *lab, SDL_Color c)
{   /* a label in one line, or in two at the space nearest its middle when that makes it larger */
    int len = t_text_len(lab), best = -1, i = 0;
    float px1 = fminf(h * 0.075f, w * 0.88f / ((float)len * 6.0f)), px2 = 0;
    char a[40], b[40];
    for (const char *q = lab; *q; q++, i++)
        if (*q == ' ' && i > 2 && i < len - 2 && (best < 0 || abs(i - len / 2) < abs(best - len / 2)))
            best = i;
    if (best > 0 && len > 9) {
        int l1 = best, l2 = len - best - 1, m = l1 > l2 ? l1 : l2;
        px2 = fminf(h * 0.05f, w * 0.88f / ((float)m * 6.0f));
        if (px2 > px1 * 1.2f) {
            snprintf(a, sizeof a, "%.*s", best, lab);
            snprintf(b, sizeof b, "%s", lab + best + 1);
            t_text(r, x + w / 2.0f, y + h / 2.0f - px2 * 4.2f, px2, a, c);
            t_text(r, x + w / 2.0f, y + h / 2.0f + px2 * 4.2f, px2, b, c);
            return;
        }
    }
    t_text(r, x + w / 2.0f, y + h / 2.0f, px1, lab, c);
}
static void t_ring_pill(SDL_Renderer *r, float x, float y, float w, float h, float th, SDL_Color c)
{   /* the pill's outline: a pill in the colour, the pill a little smaller in the stage's black over it is not allowed (see-through), so four thin bars */
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRectF(r, &(SDL_FRect){x + h * 0.5f, y, w - h, th});
    SDL_RenderFillRectF(r, &(SDL_FRect){x + h * 0.5f, y + h - th, w - h, th});
}
static void touch_draw(SDL_Renderer *r)
{
    uint32_t now = SDL_GetTicks();
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    if (dp_r > 0.0f) {   /* the D-pad: a soft disc, the cross on it, the way pressed lit */
        SDL_Color base = {255, 255, 255, 18}, arm = {170, 200, 255, 46}, lit = {140, 220, 255, 150};
        float a = dp_r * 0.34f;
        t_disc(r, dp_x, dp_y, dp_r * 1.08f, base);
        t_ring(r, dp_x, dp_y, dp_r * 1.08f, fmaxf(1.5f, dp_r * 0.025f), (SDL_Color){150, 200, 255, 70});
        SDL_SetRenderDrawColor(r, arm.r, arm.g, arm.b, arm.a);
        SDL_RenderFillRectF(r, &(SDL_FRect){dp_x - a, dp_y - dp_r * 0.95f, 2 * a, dp_r * 1.9f});
        SDL_RenderFillRectF(r, &(SDL_FRect){dp_x - dp_r * 0.95f, dp_y - a, dp_r * 1.9f, 2 * a});
        for (int k = 0; k < 4; k++) {            /* arrows: up, right, down, left */
            static const int bit[4] = {BK_UP, BK_RIGHT, BK_DOWN, BK_LEFT};
            static const float ux[4] = {0, 1, 0, -1}, uy[4] = {-1, 0, 1, 0};
            int on = held_touch >> bit[k] & 1u;
            float tx = dp_x + ux[k] * dp_r * 0.78f, ty = dp_y + uy[k] * dp_r * 0.78f, s = dp_r * 0.16f;
            SDL_Color c = on ? (SDL_Color){255, 255, 255, 235} : (SDL_Color){220, 235, 255, 150};
            if (on) {
                SDL_SetRenderDrawColor(r, lit.r, lit.g, lit.b, lit.a);
                SDL_RenderFillRectF(r, &(SDL_FRect){ux[k] ? (ux[k] > 0 ? dp_x + a : dp_x - dp_r * 0.95f) : dp_x - a,
                                                    uy[k] ? (uy[k] > 0 ? dp_y + a : dp_y - dp_r * 0.95f) : dp_y - a,
                                                    ux[k] ? dp_r * 0.95f - a : 2 * a, uy[k] ? dp_r * 0.95f - a : 2 * a});
            }
            t_tri(r, tx + ux[k] * s, ty + uy[k] * s, tx - ux[k] * s * 0.6f + uy[k] * s, ty - uy[k] * s * 0.6f + ux[k] * s,
                  tx - ux[k] * s * 0.6f - uy[k] * s, ty - uy[k] * s * 0.6f - ux[k] * s, c);
        }
    }
    for (int i = 0; i < tbn; i++) {
        const tbtn *b = &tb[i];
        int on = b->bit == BK_FN ? (int)(fn_latched >> BK_FN & 1u) : b->bit == SP_SHIFT ? (int)(touch_latch >> BK_R2 & 1u)
               : b->bit == BK_START ? t_playing : b->bit >= 24 ? (int)(sp_down >> (b->bit - 24) & 1u) : (int)(held_touch >> b->bit & 1u);
        int ga = bk_option_get("tglow") == 0 ? 20 : bk_option_get("tglow") == 1 ? 34 : 70;
        SDL_Color fill = on ? (SDL_Color){b->r, b->g, b->b, 170} : (SDL_Color){b->r, b->g, b->b, (uint8_t)ga};
        SDL_Color edge = {b->r, b->g, b->b, on ? 255 : (uint8_t)(ga * 3 + 20)};
        SDL_Color txt = on ? (SDL_Color){20, 22, 34, 255} : (SDL_Color){235, 240, 255, 215};
        float len = (float)t_text_len(b->lab);
        if (b->round == 1) {
            float rad = b->w / 2.0f;
            t_disc(r, b->x + rad, b->y + rad, rad, fill);
            t_ring(r, b->x + rad, b->y + rad, rad, fmaxf(1.5f, rad * 0.07f), edge);
            t_text(r, b->x + rad, b->y + rad, rad * 0.16f, b->lab, txt);
        } else {
            float px = fminf(b->h * 0.075f, b->w * 0.80f / (len * 6.0f));
            if (b->round == 2) {                 /* (the places' own buttons: a rounded plate in the colour, filled solid when pressed) */
                SDL_Color dark = {(uint8_t)(b->r / 7 + 8), (uint8_t)(b->g / 7 + 8), (uint8_t)(b->b / 7 + 10), 255};
                SDL_Color lit2 = {b->r, b->g, b->b, 255};
                float bd = fmaxf(2.0f, b->h * 0.03f);
                t_rrect(r, b->x, b->y, b->w, b->h, b->h * 0.22f, (SDL_Color){b->r, b->g, b->b, (uint8_t)(on ? 255 : 110 + ga)});
                t_rrect(r, b->x + bd, b->y + bd, b->w - 2 * bd, b->h - 2 * bd, b->h * 0.2f, on ? lit2 : dark);
                t_label(r, b->x, b->y, b->w, b->h, b->lab, on ? (SDL_Color){18, 22, 34, 255} : (SDL_Color){238, 242, 255, 255});
            } else {
                t_pill(r, b->x, b->y, b->w, b->h, fill);
                t_text(r, b->x + b->w / 2.0f, b->y + b->h / 2.0f, px, b->lab, txt);
            }
        }
    }
    for (int k = 0; k < BK_NBUTTONS; k++) {      /* the tap's ring: spreads and fades in 0.35 s */
        uint32_t age = now - press_at[k];
        if (press_at[k] && age < 350u) {
            float t = age / 350.0f, rad = dp_r * (0.25f + 0.55f * t);
            t_ring(r, ring_x[k], ring_y[k], rad, fmaxf(1.5f, dp_r * 0.05f * (1.0f - t)),
                   (SDL_Color){200, 240, 255, (uint8_t)(160.0f * (1.0f - t))});
        } else
            press_at[k] = 0;
    }
}

/* the stage and the pad into the window (in place of the renderer's own letterboxing) */
static void touch_present(SDL_Renderer *r, SDL_Texture *tex)
{
    int W = 0, H = 0;
    SDL_GetRendererOutputSize(r, &W, &H);
    touch_layout(W, H);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    SDL_RenderCopy(r, tex, 0, &stage_dst);
    if (touch_shown)
        touch_draw(r);
}

/* MD_TOUCH_LAYOUTS=DIR: the pad on phone and tablet screens of every shape, checked (every button inside the screen, none on
   the stage or on another, the stage not too small) and drawn to DIR/touch-WxH.bmp with a grey box for the stage */
static int touch_layouts(const char *dir)
{
    static const int SZ[][2] = {{1920, 1200}, {2400, 1080}, {1080, 2400}, {1200, 1920}, {1280, 800}, {1024, 768}, {800, 1280}, {2560, 1600}};
    int bad = 0, sv_w = tw_w, sv_h = tw_h, sv_key = lay_key;
    SDL_Rect sv_stage = stage_dst;
    snprintf(tch[0].label, sizeof tch[0].label, "MUTE"), tch[0].btn = BK_A;      /* (what a place may name: seven of them) */
    snprintf(tch[1].label, sizeof tch[1].label, "SURPRISE"), tch[1].btn = BK_Y;
    snprintf(tch[2].label, sizeof tch[2].label, "EDIT"), tch[2].btn = BK_X;
    snprintf(tch[3].label, sizeof tch[3].label, "< TRACK"), tch[3].btn = BK_L1;
    snprintf(tch[4].label, sizeof tch[4].label, "TRACK >"), tch[4].btn = BK_R1;
    snprintf(tch[5].label, sizeof tch[5].label, "A FETCH / PLAY / SILENCE"), tch[5].btn = BK_B;
    snprintf(tch[6].label, sizeof tch[6].label, "HOLD: LISTEN"), tch[6].btn = BK_START;
    tchn = 7;
    for (int k = 0; k < 3; k++) {                /* (the pad: normal, then large with the sides swapped; then the places' own buttons) */
        bk_option_set("tpad", k < 2), bk_option_set("tsize", k == 1 ? 2 : 1), bk_option_set("tside", k == 1);
        for (unsigned z = 0; z < sizeof SZ / sizeof SZ[0]; z++) {
            int W = SZ[z][0], H = SZ[z][1], wrong = 0;
            tw_w = 0, tbn = 0;
            touch_layout(W, H);
            for (int i = 0; i < tbn; i++) {
                const tbtn *b = &tb[i];
                SDL_Rect r = {(int)b->x, (int)b->y, (int)b->w, (int)b->h};
                if (r.x < 0 || r.y < 0 || r.x + r.w > W || r.y + r.h > H)
                    wrong++, fprintf(stderr, "touch: %dx%d %s outside the screen\n", W, H, b->lab);
                if (SDL_HasIntersection(&r, &stage_dst))
                    wrong++, fprintf(stderr, "touch: %dx%d %s on the stage\n", W, H, b->lab);
                for (int j = i + 1; j < tbn; j++) {
                    SDL_Rect q = {(int)tb[j].x, (int)tb[j].y, (int)tb[j].w, (int)tb[j].h};
                    int hit = SDL_HasIntersection(&r, &q);
                    if (b->round == 1 && tb[j].round == 1) {   /* (two discs: by their centres) */
                        float ex = b->x + b->w / 2 - tb[j].x - tb[j].w / 2, ey = b->y + b->h / 2 - tb[j].y - tb[j].h / 2;
                        hit = sqrtf(ex * ex + ey * ey) < (b->w + tb[j].w) / 2 * 0.98f;
                    }
                    if (hit && getenv("MD_TOUCH_DEBUG"))
                        fprintf(stderr, "  %s %.0f,%.0f %.0fx%.0f / %s %.0f,%.0f %.0fx%.0f\n", b->lab, b->x, b->y, b->w, b->h, tb[j].lab, tb[j].x, tb[j].y, tb[j].w, tb[j].h);
                    if (hit)
                        wrong++, fprintf(stderr, "touch: %dx%d %s on %s\n", W, H, b->lab, tb[j].lab);
                }
            }
            for (int i = 0; i < tbn; i++)
                if (tb[i].round == 2 && tb[i].h < 40.0f)
                    wrong++, fprintf(stderr, "touch: %dx%d %s only %.0f px high\n", W, H, tb[i].lab, tb[i].h);
            if (dp_r > 0 && (dp_x - dp_r < 0 || dp_x + dp_r > W || dp_y - dp_r < 0 || dp_y + dp_r > H))
                wrong++, fprintf(stderr, "touch: %dx%d D-pad outside the screen\n", W, H);
            if (stage_dst.w < (W < H ? W * 0.55f : fminf((float)H * 1.333f, (float)W) * 0.44f))
                wrong++, fprintf(stderr, "touch: %dx%d stage only %d wide\n", W, H, stage_dst.w);
            fprintf(stderr, "touch: %dx%d%s stage %dx%d at %d,%d, %d wrong\n", W, H, k == 1 ? " large+swapped" : k == 2 ? " own buttons" : "", stage_dst.w, stage_dst.h,
                    stage_dst.x, stage_dst.y, wrong);
            bad += wrong;
            if (dir && dir[0]) {
                SDL_Surface *sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
                SDL_Renderer *sr = sf ? SDL_CreateSoftwareRenderer(sf) : NULL;
                if (sr) {
                    char path[600];
                    SDL_SetRenderDrawColor(sr, 0, 0, 0, 255);
                    SDL_RenderClear(sr);
                    SDL_SetRenderDrawColor(sr, 40, 52, 70, 255);
                    SDL_RenderFillRect(sr, &stage_dst);
                    touch_draw(sr);
                    snprintf(path, sizeof path, "%s/touch-%dx%d%s.bmp", dir, W, H, k == 1 ? "-large" : k == 2 ? "-own" : "");
                    SDL_SaveBMP(sf, path);
                    SDL_DestroyRenderer(sr);
                }
                if (sf)
                    SDL_FreeSurface(sf);
            }
        }
    }
    bk_option_set("tsize", 1), bk_option_set("tside", 0), bk_option_set("tpad", 0), tchn = 0;
    tw_w = sv_w, tw_h = sv_h, lay_key = sv_key, stage_dst = sv_stage, tbn = 0;
    return bad;
}

static void touch_selftest(void)                 /* MD_TOUCH_SELFTEST=1: every button's middle pressed once, logged */
{
    static const char *const D[4] = {"UP", "RIGHT", "DOWN", "LEFT"};
    static const float ux[4] = {0, 1, 0, -1}, uy[4] = {-1, 0, 1, 0};
    int bad = 0;
    for (int i = 0; i < tbn; i++) {
        uint32_t m = touch_mask_at(tb[i].x + tb[i].w / 2, tb[i].y + tb[i].h / 2);
        bad += m != 1u << tb[i].bit;
        fprintf(stderr, "touch: %-6s at %4.0f,%4.0f -> mask %05x%s\n", tb[i].lab, tb[i].x + tb[i].w / 2, tb[i].y + tb[i].h / 2,
                m, m == 1u << tb[i].bit ? "" : "  WRONG");
    }
    for (int k = 0; k < 4; k++) {
        static const int bit[4] = {BK_UP, BK_RIGHT, BK_DOWN, BK_LEFT};
        uint32_t m = touch_mask_at(dp_x + ux[k] * dp_r * 0.7f, dp_y + uy[k] * dp_r * 0.7f);
        bad += m != 1u << bit[k];
        fprintf(stderr, "touch: pad %-5s -> mask %05x%s\n", D[k], m, m == 1u << bit[k] ? "" : "  WRONG");
    }
    {
        uint32_t m = touch_mask_at(stage_dst.x + stage_dst.w / 2.0f, stage_dst.y + stage_dst.h / 2.0f);
        bad += m != 0;
        fprintf(stderr, "touch: stage middle -> mask %05x%s\n", m, m ? "  WRONG" : "");
    }
    {   /* a finger whose lifting never comes: held, then (20 s unheard) let go by touch_sync */
        float x = dp_x, y = dp_y - dp_r * 0.7f;
        int ok;
        touch_release_all();
        touch_finger(77, x / (float)tw_w, y / (float)tw_h, 0);
        ok = (held_touch >> BK_UP & 1u) != 0;
        for (int i = 0; i < TFINGERS; i++)
            if (fing[i].used)
                fing[i].last -= 21000u;
        touch_sync();
        ok = ok && held_touch == 0;
        bad += !ok;
        fprintf(stderr, "touch: a lost lifting is let go after 20 s: %s\n", ok ? "ok" : "WRONG");
    }
    fprintf(stderr, "touch: layout %dx%d, stage %d,%d %dx%d, %d wrong\n", tw_w, tw_h, stage_dst.x, stage_dst.y, stage_dst.w,
            stage_dst.h, bad);
}
#endif
