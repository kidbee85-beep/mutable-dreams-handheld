/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: drawing the PLAYGROUND (brick_toys.inc). Discs, arcs and short
 * lines on the views' night: cheap enough for the Miyoo's light picture too. 0.68: everything that rings shows it
 * (sparks and rings), trails behind what moves, soft glows, the tabs' pill gliding, each toy fading in. */

_Static_assert(TY_SX == BK_SX && TY_SY == BK_SY, "the playground's box follows the stage");

static const char *const TY_NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static uint32_t ty_ci_col(int ci)                /* a spark's colour */
{
    return ci < 6 ? bk_track_col(ci) : ci == 6 ? ARGB(255, 255, 222, 150) : ci == 7 ? ARGB(255, 190, 225, 255) : BK_INK;
}

/* --------------------------------------------------- organic (0.70) --- */
/* a living blob: a body of overlapping discs round a centre, each breathing on its own - a leaf, a cell, a pad,
   never a perfect circle. Shaded: a darker rim below, the body, a lit cap */
static void ty_blob(float x, float y, float r, uint32_t c, float t, int seed, uint32_t alpha)
{
    if (bk_calm)                                 /* (calm: the shape holds still) */
        t = 0;
    /* (cheap: one body and a rim of small lobes that breathe - the outline lives, the area is drawn about twice) */
    uint32_t dark = gd_mix(c, ARGB(255, 8, 16, 20), 0.45f), lit = gd_mix(c, BK_INK, 0.35f);
    bk_disc(x + r * 0.05f, y + r * 0.1f, r * 0.86f, BK_ALPHA(dark, alpha));
    bk_disc(x, y - r * 0.03f, r * 0.8f, BK_ALPHA(c, alpha));
    for (int k = 0; k < 5; k++) {
        float a = (float)k * 1.2566f + (float)seed * 0.9f + 0.3f * sinf(t * 0.7f + (float)(seed + k));
        float d = r * (0.52f + 0.07f * sinf(t * 1.3f + (float)(k * 3 + seed))), lr = r * (0.36f + 0.05f * sinf(t * 1.1f + (float)(k * 5 + seed)));
        bk_disc(x + cosf(a) * d, y + sinf(a) * d * 0.92f - r * 0.03f, lr, BK_ALPHA(c, alpha));
    }
    bk_disc(x - r * 0.2f, y - r * 0.28f, r * 0.36f, BK_ALPHA(lit, (uint32_t)(alpha * 110u / 255u)));
    bk_disc(x - r * 0.32f, y - r * 0.4f, r * 0.13f, ARGB((uint32_t)(alpha * 170u / 255u), 255, 255, 255));
}

/* the playground's border: a vine instead of a line - it sways, it has leaves, and buds along it open while the
   toys play (ty_notes moving); fungus lights glow at its feet */
static void bk_vines_at(float t, uint32_t col, float x0, float y0, float x1, float y1);
static void ty_draw_vines(float t, uint32_t col)
{
    bk_vines_at(t, col, (float)(BK_SX + 22), (float)(BK_SY + 64), (float)(BK_SX + 698), (float)(BK_SY + 706));
}
/* (0.71: the same vine for every full view's frame) */
static void bk_vines_at(float t, uint32_t col, float x0, float y0, float x1, float y1)
{
    static float bloom;
    static uint32_t last_notes;
    float px = 0, py = 0;
    if (ty_notes != last_notes)
        bloom = fminf(1.0f, bloom + 0.12f * (float)(ty_notes - last_notes)), last_notes = ty_notes;
    bloom *= 1.0f - bk_frame_s * 0.5f;
    for (int side = 0; side < 4; side++) {       /* top, right, bottom, left: the vine along each */
        int segs = (int)((side % 2 ? y1 - y0 : x1 - x0) / 15.0f);
        for (int k = 0; k <= segs; k++) {
            float u = (float)k / (float)segs, wv = 5.0f * sinf(u * 12.0f + (bk_calm ? 0.0f : t * 0.9f) + (float)side * 2.0f), x, y;   /* (calm: it stays) */
            if (side == 0) x = x0 + (x1 - x0) * u, y = y0 + wv;
            else if (side == 1) x = x1 + wv, y = y0 + (y1 - y0) * u;
            else if (side == 2) x = x1 - (x1 - x0) * u, y = y1 + wv;
            else x = x0 + wv, y = y1 - (y1 - y0) * u;
            if (k) {                             /* (the stem: short bars, row fills - a thick line scans a whole square) */
                if (side % 2 == 0)
                    bk_rect((int)fminf(px, x), (int)((py + y) * 0.5f) - 1, (int)fabsf(x - px) + 1, 3, ARGB(255, 56, 98, 66));
                else
                    bk_rect((int)((px + x) * 0.5f) - 1, (int)fminf(py, y), 3, (int)fabsf(y - py) + 1, ARGB(255, 56, 98, 66));
            }
            if (k % 4 == 2 && !bk_light) {       /* a pair of leaves, swaying (not in the light picture) */
                float a = t * 1.2f + (float)(k + side * 7), la = 0.5f + 0.25f * sinf(a);
                bk_disc(x + cosf(la) * 6.0f, y - sinf(la) * 6.0f, 4.0f, ARGB(200, 90, 150, 96));
                bk_disc(x - cosf(la) * 6.0f, y + sinf(la) * 6.0f, 3.2f, ARGB(170, 70, 130, 86));
            }
            if (k % 10 == 5 && !bk_light) {      /* a bud: it opens with the music */
                float o = bloom * (0.6f + 0.4f * sinf(t * 2.0f + (float)k));
                uint32_t bc = bk_track_col((k + side) % 6);
                if (o > 0.1f)
                    for (int q = 0; q < 5; q++) {
                        float a = (float)q * 1.2566f + t * 0.3f;
                        bk_disc(x + cosf(a) * 5.0f * o, y + sinf(a) * 5.0f * o, 3.2f * o + 1.0f, BK_ALPHA(bc, (uint32_t)(220.0f * o)));
                    }
                bk_disc(x, y, 2.6f, ARGB(255, 255, 230, 170));
            }
            px = x, py = y;
        }
    }
    for (int k = 0; k < 5 && !bk_light; k++) {   /* fungus lights at the vine's feet, breathing */
        float fx = x0 + 30.0f + (float)k * 18.0f + (k > 2 ? 560.0f : 0.0f), fy = y1 - 4.0f - (float)((k * 7) % 11);
        float br = 0.5f + 0.5f * sinf(t * 1.4f + (float)k * 1.9f);
        bk_line(fx, y1, fx, fy, 2.0f, ARGB(160, 180, 190, 170));
        bk_disc(fx, fy, 9.0f + 3.0f * br, BK_ALPHA(col, (uint32_t)(30.0f * br)));
        bk_disc(fx, fy, 4.5f, ARGB(255, 200, 240, 210));
    }
}

/* pollen drifting through the playground; near what just rang, it glows brighter */
static void ty_draw_pollen(float t)
{
    for (int i = 0; i < 18; i++) {
        uint32_t h = (uint32_t)(i + 7) * 2654435761u;
        float sp = 0.02f + (float)(h >> 28) * 0.004f, life = fmodf(t * sp + (float)(h & 1023u) / 1023.0f, 1.0f);
        float x = (float)(BK_SX + 40) + (float)(h % 640u) + 26.0f * sinf(t * 0.4f + (float)i), y = (float)(BK_SY + 690) - life * 600.0f;
        float fade = sinf(life * 3.1416f), lit = 0;
        for (int k = 0; k < TY_SPARKS; k += 4)   /* (some sparks, not all: near a hit, it glows) */
            if (ty_sp[k].life > 0.3f && fabsf(ty_sp[k].x - x) + fabsf(ty_sp[k].y - y) < 90.0f)
                lit = 1.0f;
        bk_disc(x, y, 3.5f + 3.0f * lit, ARGB((uint32_t)((18.0f + 50.0f * lit) * fade), 255, 240, 190));
        bk_disc(x, y, 1.3f + 0.8f * lit, ARGB((uint32_t)((120.0f + 120.0f * lit) * fade), 255, 248, 220));
    }
}

/* a small bead: body and highlight only (for the many at rest) */
static void ty_bead(float x, float y, float r, uint32_t c, uint32_t alpha)
{
    bk_disc(x, y, r, BK_ALPHA(c, alpha));
    bk_disc(x - r * 0.3f, y - r * 0.35f, r * 0.32f, ARGB((uint32_t)(150u * alpha / 255u), 255, 255, 255));
}

/* a glowing ball: a soft halo, a shadow, the body, a rim of light and a highlight (a little 3-D, cheaply) */
static void ty_orb(float x, float y, float r, uint32_t c, float glow, uint32_t alpha)
{
    if (glow > 0.02f)
        bk_disc(x, y, r * (1.7f + 0.8f * glow), BK_ALPHA(c, (uint32_t)(70.0f * glow * (float)alpha / 255.0f)));
    bk_disc(x + r * 0.15f, y + r * 0.3f, r, ARGB((uint32_t)(70u * alpha / 255u), 0, 0, 0));
    bk_disc(x, y, r, BK_ALPHA(gd_mix(c, ARGB(255, 20, 24, 36), 0.25f), alpha));
    bk_disc(x - r * 0.12f, y - r * 0.12f, r * 0.82f, BK_ALPHA(gd_mix(c, BK_INK, 0.15f + 0.45f * glow), alpha));
    bk_disc(x - r * 0.35f, y - r * 0.38f, r * 0.28f, ARGB((uint32_t)(170u * alpha / 255u), 255, 255, 255));
}

static void ty_draw_sparks(void)
{
    for (int k = 0; k < TY_SPARKS; k++) {
        float l = ty_sp[k].life;
        uint32_t c;
        if (l <= 0)
            continue;
        c = ty_ci_col(ty_sp[k].ci);
        if (ty_sp[k].ring)                       /* a ring spreading from the hit */
            bk_arc(ty_sp[k].x, ty_sp[k].y, ty_sp[k].size + 34.0f * (1.0f - l), 1.0f + 2.0f * l, 0, 6.2832f, BK_ALPHA(c, (uint32_t)(210.0f * l)));
        else {
            bk_disc(ty_sp[k].x, ty_sp[k].y, ty_sp[k].size * (1.0f + l), BK_ALPHA(c, (uint32_t)(60.0f * l)));
            bk_disc(ty_sp[k].x, ty_sp[k].y, ty_sp[k].size * (0.4f + 0.6f * l), BK_ALPHA(gd_mix(c, BK_INK, 0.5f), (uint32_t)(255.0f * l)));
        }
    }
}

/* the eight lily pads along the bottom: their tone; a hit pushes the pad down, it springs back with a wobble */
static void ty_draw_pads(float t, uint32_t col)
{
    float w = (TY_R - TY_L) / 8.0f;
    bk_rrect(BK_SX + 24, (int)TY_B - 2, 672, TY_PAD_H + 8, 16, ARGB(40, 30, 80, 110));   /* (the water) */
    for (int k = 0; k < 8; k++) {
        float cx = TY_L + w * ((float)k + 0.5f), cy = TY_B + (float)TY_PAD_H * 0.5f + 2.0f, f = ty_pad_flash[k];
        float wob = f * sinf((1.0f - f) * 18.0f);  /* (a damped spring) */
        float bob = 2.0f * sinf(t * 1.3f + (float)k * 0.9f) + 7.0f * wob, rr = w * 0.40f * (1.0f + 0.08f * wob);
        uint32_t leaf = gd_mix(ARGB(255, 40, 92, 70), col, 0.2f + 0.65f * f);
        int note = bk_scale_note(k);
        if (f > 0.05f)                           /* its glow (the ring of light: the sparks' own) */
            bk_disc(cx, cy + bob, rr * (1.2f + 0.3f * f), BK_ALPHA(col, (uint32_t)(70.0f * f)));
        ty_blob(cx, cy + bob, rr, leaf, t, k, 255);   /* (a living leaf, never quite round; its own shadow) */
        for (int v = 0; v < 3; v++) {            /* (its veins, from the stem out) */
            float a = -0.5f + (float)v * 0.6f;
            bk_line(cx, cy + bob, cx + cosf(a) * rr * 0.82f, cy + bob + sinf(a) * rr * 0.82f, 1.2f, ARGB(40, 255, 255, 255));
        }
        bk_disc(cx - rr * 0.35f, cy + bob - rr * 0.33f, rr * 0.3f, ARGB((uint32_t)(40 + 90 * f), 255, 255, 255));
        bk_text_c((int)cx, (int)(cy + bob) - 9, &FONT_S, 1, TY_NOTE_NAMES[note % 12], f > 0.3f ? ARGB(255, 12, 16, 24) : BK_INK);
    }
}

static void ty_draw_bounce(float t, uint32_t col)
{
    float ax = TY_L + 46.0f, ay = TY_T + 40.0f, gx = (float)ty_tilt * 9.0f, gy = (float)ty_grav * 5.0f;
    static float tilt_shown;
    (void)t;
    tilt_shown += ((float)ty_tilt - tilt_shown) * bk_glide(8.0f);
    {   /* the box, its floor leaning with the tilt (a hint of it, drawn) */
        float lean = tilt_shown * 6.0f;
        bk_line(TY_L, TY_B + 1.0f - lean, TY_R, TY_B + 1.0f + lean, 2.0f, BK_ALPHA(col, 90));
    }
    bk_disc(ax, ay, 22.0f, ARGB(26, 255, 255, 255));   /* gravity, as an arrow in a pebble */
    if (ty_grav || ty_tilt) {
        float len = sqrtf(gx * gx + gy * gy), ux = gx / (len + 1e-3f), uy = gy / (len + 1e-3f);
        float ex = ax + gx * 0.6f, ey = ay + gy * 0.6f;
        bk_line(ax, ay, ex, ey, 3.0f, BK_ALPHA(col, 220));
        bk_line(ex, ey, ex - ux * 7.0f - uy * 5.0f, ey - uy * 7.0f + ux * 5.0f, 3.0f, BK_ALPHA(col, 220));
        bk_line(ex, ey, ex - ux * 7.0f + uy * 5.0f, ey - uy * 7.0f - ux * 5.0f, 3.0f, BK_ALPHA(col, 220));
    } else
        bk_text_c((int)ax, (int)ay - 8, BK_TINY, 1, "0", BK_DIM);
    for (int i = 0; i < TY_BALLS; i++) {
        float x, y, h, fade, sp;
        uint32_t bc;
        if (!ty_ball[i].on)
            continue;
        x = ty_ball[i].x, y = ty_ball[i].y, h = ty_ball[i].hit;
        fade = ty_ball[i].rest > 3.0f ? 1.0f - (ty_ball[i].rest - 3.0f) : 1.0f;
        bc = bk_track_col(i % 6);
        sp = fabsf(ty_ball[i].vx) + fabsf(ty_ball[i].vy);
        if (sp > 60.0f)                          /* its trail: where it was, fading, thinner */
            for (int q = TY_BTRAIL - 1; q >= 1; q -= 2) {   /* (every other place: the same streak, half the discs) */
                float u = 1.0f - (float)q / (float)TY_BTRAIL;
                bk_disc(ty_ball[i].tx[q], ty_ball[i].tyy[q], TY_BALL_R * (0.35f + 0.5f * u), BK_ALPHA(bc, (uint32_t)(110.0f * u * fade)));
            }
        if (!bk_light)
            bk_disc(x, TY_B + 6.0f, TY_BALL_R * (0.5f + 0.5f * fminf(1.0f, 120.0f / (TY_B - y + 40.0f))), ARGB((uint32_t)(50 * fade), 0, 0, 0));   /* (its shadow on the water) */
        ty_orb(x, y, TY_BALL_R * (1.0f + 0.18f * h), bc, h, (uint32_t)(255 * fade));
    }
}

static void ty_draw_orbits(float t, uint32_t col)
{
    const float cx = (float)(BK_SX + 360), cy = (float)(BK_SY + 330);
    float pulse = 0;
    char buf[32];
    for (int i = 0; i < TY_PLANETS; i++)
        pulse = fmaxf(pulse, ty_pl[i].flash * (ty_pl[i].on ? 1.0f : 0.0f));
    bk_line(cx, cy - 40.0f, cx, cy - 272.0f, 2.0f + 2.0f * pulse, BK_ALPHA(col, (uint32_t)(60 + 170 * pulse)));   /* the gate */
    if (pulse > 0.05f)
        bk_line(cx, cy - 40.0f, cx, cy - 272.0f, 8.0f * pulse, BK_ALPHA(col, (uint32_t)(50 * pulse)));
    bk_disc(cx, cy - 280.0f, 6.0f + 3.0f * pulse, BK_ALPHA(col, (uint32_t)(120 + 135 * pulse)));
    for (int k = 0; k < 12; k++) {               /* the sun: rays turning slowly, a corona breathing */
        float a = (float)k * 0.5236f + t * 0.15f, r0 = 32.0f, r1 = 46.0f + 6.0f * sinf(t * 2.0f + (float)k) + 12.0f * pulse;
        bk_line(cx + cosf(a) * r0, cy + sinf(a) * r0, cx + cosf(a) * r1, cy + sinf(a) * r1, 2.0f, ARGB((uint32_t)(70 + 90 * pulse), 255, 214, 130));
    }
    bk_disc(cx, cy, 44.0f + 8.0f * pulse + 2.0f * sinf(t * 1.7f), ARGB((uint32_t)(30 + 60 * pulse), 255, 220, 140));
    ty_orb(cx, cy, 26.0f + 4.0f * pulse, ARGB(255, 255, 206, 120), 0.4f + pulse, 255);
    for (int i = 0; i < TY_PLANETS; i++) {
        float rr = 80.0f + (float)i * 46.0f, a = ty_pl[i].ph * 6.2831853f - 1.5707963f;
        float px = cx + cosf(a) * rr, py = cy + sinf(a) * rr, f = ty_pl[i].flash, span = 0.25f + 0.12f * TY_RATIOS[ty_pl[i].ratio];
        int sel = i == ty_sel, on = ty_pl[i].on;
        uint32_t pc = bk_track_col((i + 1) % 6);
        bk_arc(cx, cy, rr, sel ? 2.0f : 1.2f, 0, 6.2832f, BK_ALPHA(sel ? col : BK_INK, (uint32_t)(sel ? 110 : 30)));
        if (!on) {
            bk_arc(px, py, 9.0f, 1.5f, 0, 6.2832f, BK_ALPHA(pc, 110));
        } else {
            for (int k = 1; k <= 10; k++) {      /* its wake: a trail of light behind it, fading, thinning (discs: cheap) */
                float b = a - span * (float)k / 10.0f, u = 1.0f - (float)k / 11.0f;
                bk_disc(cx + cosf(b) * rr, cy + sinf(b) * rr, 1.5f + 5.0f * u, BK_ALPHA(pc, (uint32_t)(150.0f * u)));
            }
            ty_orb(px, py, 12.0f + 4.0f * f, pc, f, 255);
        }
        if (sel)
            bk_arc(px, py, 20.0f + 2.0f * sinf(t * 5.0f), 2.0f, 0, 6.2832f, BK_ALPHA(BK_INK, 200));
    }
    for (int i = 0; i < TY_PLANETS; i++) {       /* the planets' list: speed and note */
        int y = BK_SY + 618 + (i / 3) * 34, x = BK_SX + 30 + (i % 3) * 226, sel = i == ty_sel;
        uint32_t pc = bk_track_col((i + 1) % 6);
        if (sel)
            bk_rrect(x - 6, y - 4, 218, 30, 12, BK_ALPHA(col, 60));
        bk_disc((float)(x + 9), (float)(y + 11), 8.0f + 3.0f * ty_pl[i].flash, ty_pl[i].on ? pc : BK_ALPHA(pc, 70));
        snprintf(buf, sizeof buf, "%s A BAR  %s", TY_RATIO_NAMES[ty_pl[i].ratio], TY_NOTE_NAMES[bk_scale_note(ty_pl[i].degree) % 12]);
        bk_text(x + 24, y + 2, &FONT_S, 1, buf, ty_pl[i].on ? (sel ? BK_INK : BK_DIM) : BK_FAINT);
    }
}

static void ty_draw_rain(float t, uint32_t col)
{
    float target = ty_cloud, cx, cy = TY_T + 34.0f, lean;
    static float shown_prev = 0.5f;
    ty_cloud_shown += (target - ty_cloud_shown) * bk_glide(10.0f);
    cx = TY_L + ty_cloud_shown * (TY_R - TY_L);
    lean = fminf(fmaxf((ty_cloud_shown - shown_prev) * 900.0f, -14.0f), 14.0f);   /* (the cloud leans as it moves) */
    shown_prev = ty_cloud_shown;
    if (ty_thunder > 0.05f)                      /* thunder: a flash over the sky */
        bk_rrect(BK_SX + 24, BK_SY + 64, 672, 538, 14, ARGB((uint32_t)(46.0f * ty_thunder * ty_thunder), 220, 230, 255));
    for (int i = 0; i < TY_DROPS; i++)
        if (ty_drop[i].on) {                     /* a drop: a streak with a bright head */
            float x = ty_drop[i].x, y = ty_drop[i].y, l = 8.0f + fminf(ty_drop[i].vy * 0.035f, 26.0f);
            bk_line(x, y - l, x, y, 3.0f, ARGB(110, 170, 210, 255));
            bk_line(x, y - l * 0.5f, x, y, 2.0f, ARGB(220, 200, 230, 255));
            bk_disc(x, y, 3.2f, ARGB(255, 230, 244, 255));
        }
    for (int k = 0; k < TY_RIPPLES; k++)
        if (ty_rip[k].on) {
            float a = ty_rip[k].age / 1.2f;
            bk_arc(ty_rip[k].x, TY_B + 6.0f, 6.0f + 40.0f * a, 1.8f, 3.3f, 6.1f, ARGB((uint32_t)(200.0f * (1.0f - a)), 190, 225, 255));
            if (a < 0.6f)
                bk_arc(ty_rip[k].x, TY_B + 6.0f, 3.0f + 22.0f * a, 1.2f, 3.3f, 6.1f, ARGB((uint32_t)(160.0f * (1.0f - a / 0.6f)), 220, 240, 255));
        }
    {   /* the cloud: soft puffs with a dark belly and a lit top, breathing, darker with more rain */
        float dark = (float)ty_rain / 8.0f;
        uint32_t cc = gd_mix(ARGB(255, 210, 220, 236), ARGB(255, 92, 102, 128), dark);
        uint32_t belly = gd_mix(cc, ARGB(255, 40, 46, 66), 0.35f), lit = gd_mix(cc, BK_INK, 0.4f);
        static const float px[7] = {-66, -34, -2, 30, 64, 14, -20}, py[7] = {8, -6, -18, -6, 8, 12, 12}, pr[7] = {26, 32, 38, 32, 24, 30, 28};
        for (int k = 0; k < 7; k++)
            bk_disc(cx + px[k] + lean * 0.3f, cy + py[k] + 7.0f, pr[k], belly);
        for (int k = 0; k < 7; k++) {
            float br = 2.0f * sinf(t * 0.8f + (float)k * 1.7f), sx = cx + px[k] + lean * (py[k] < 0 ? 0.6f : 0.2f);
            bk_disc(sx, cy + py[k] + 2.0f * sinf(t * 1.1f + (float)k), pr[k] + br, cc);
        }
        for (int k = 1; k < 4; k++)
            bk_disc(cx + px[k] - 6.0f + lean * 0.6f, cy + py[k] - pr[k] * 0.35f, pr[k] * 0.45f, BK_ALPHA(lit, 150));
        if (ty_thunder > 0.25f) {                /* the lightning, zigzag, fading */
            float x0 = cx, y0 = cy + 34.0f, seg = (TY_B - y0) / (float)TY_BOLT;
            uint32_t a = (uint32_t)(255.0f * fminf(1.0f, ty_thunder * 1.4f));
            for (int k = 0; k < TY_BOLT; k++) {
                float x1 = cx + ty_bolt[k] * (float)(k + 1) * 0.5f, y1 = y0 + seg;
                bk_line(x0, y0, x1, y1, 7.0f, ARGB(a / 4u, 200, 220, 255));
                bk_line(x0, y0, x1, y1, 2.5f, ARGB(a, 255, 250, 210));
                x0 = x1, y0 = y1;
            }
        }
    }
    if (ty_in_time) {                            /* IN TIME: the sixteenths, ticking along the top */
        uint32_t step_ms = (uint32_t)(15000.0f / (float)(md_tempo_now() > 20 ? md_tempo_now() : 20));
        int s = (int)((bk_ms() / (step_ms ? step_ms : 1u)) % 16u);
        for (int k = 0; k < 16; k++)
            bk_disc((float)(BK_SX + 380 + k * 18), (float)(BK_SY + 50), k == s ? 4.5f : 2.5f, k == s ? col : BK_ALPHA(BK_INK, k % 4 ? 50 : 110));
    }
}

/* SWING: eight pendulums from one pivot, the longest the lowest; ghosts behind each, a glow where it passes the middle */
static void ty_draw_swing(float t, uint32_t col)
{
    const float px = (float)(BK_SX + 360), py = TY_T + 6.0f, amp = 0.52f;
    float cyc = TY_CYCLES[ty_cycle], left = cyc - ty_sw_t, u = ty_sw_t / cyc;
    char buf[40];
    (void)t;
    {   /* the middle, where they ring: a soft beam, brighter as they come together at the cycle's end */
        float near = u > 0.9f ? (u - 0.9f) * 10.0f : u < 0.05f ? 1.0f - u * 20.0f : 0.0f;
        bk_line(px, py, px, TY_B + 40.0f, 1.2f + 4.0f * near, BK_ALPHA(col, (uint32_t)(40 + 80 * near)));
    }
    bk_rrect((int)px - 64, (int)py - 10, 128, 12, 6, ARGB(255, 70, 62, 52));   /* the beam, with a lit edge */
    bk_rrect((int)px - 62, (int)py - 10, 124, 3, 2, ARGB(120, 255, 230, 190));
    for (int k = TY_PENDS - 1; k >= 0; k--) {
        float len = 500.0f - (float)k * 52.0f, th = amp * ty_swing_x(k, ty_sw_t);
        float bx = px + len * sinf(th), by = py + len * cosf(th), f = ty_sw_flash[k];
        uint32_t pc = bk_track_col(k % 6);
        for (int g = 3; g >= 1; g--) {           /* ghosts: where it was a moment ago */
            float tg = amp * ty_swing_x(k, ty_sw_t - 0.035f * (float)g);
            bk_disc(px + len * sinf(tg), py + len * cosf(tg), 12.0f - (float)g * 2.0f, BK_ALPHA(pc, (uint32_t)(70 - g * 18)));
        }
        bk_line(px, py, bx, by, 1.4f, ARGB((uint32_t)(90 + 100 * f), 220, 225, 235));
        ty_orb(bx, by, 13.0f + 3.0f * f, pc, f, 255);
        bk_text_c((int)bx, (int)by + 17, BK_TINY, 1, TY_NOTE_NAMES[bk_scale_note(k) % 12], BK_ALPHA(BK_DIM, (uint32_t)(130 + 125 * f)));
    }
    bk_disc(px, py - 4.0f, 6.0f, ARGB(255, 190, 170, 140));
    snprintf(buf, sizeof buf, "TOGETHER AGAIN IN %d S", (int)ceilf(left));   /* the cycle's end, counted down */
    bk_text(BK_SX + 40, BK_SY + 600, &FONT_S, 1, buf, BK_DIM);
    snprintf(buf, sizeof buf, "%s", ty_one_way ? "ONE WAY" : "BOTH WAYS");
    bk_text(BK_SX + 40, BK_SY + 630, BK_TINY, 1, buf, BK_FAINT);
    bk_rrect(BK_SX + 40, BK_SY + 656, 640, 6, 3, ARGB(30, 255, 255, 255));
    bk_rrect(BK_SX + 40, BK_SY + 656, 6 + (int)(634.0f * u), 6, 3, BK_ALPHA(col, 200));
    bk_disc((float)(BK_SX + 43) + 634.0f * u, (float)(BK_SY + 659), 6.0f, col);
}

/* FIREFLY: the notes glowing to be caught (they grow in), the firefly with a light trail, the melody as beads */
static void ty_draw_firefly(float t, uint32_t col)
{
    const uint32_t glow = ARGB(255, 255, 236, 140);
    float hx = ty_fx < 0 ? (TY_L + TY_R) * 0.5f : ty_fx, hy = ty_fx < 0 ? (TY_T + TY_B) * 0.5f : ty_fy;
    bk_line(TY_L, (TY_T + TY_B) * 0.5f, TY_R, (TY_T + TY_B) * 0.5f, 1.0f, ARGB(24, 255, 255, 255));   /* (above: an octave up) */
    for (int i = 0; i < TY_SEEDS; i++) {
        float x = ty_seed[i].x, y = ty_seed[i].y, br = 0.6f + 0.4f * sinf(t * 3.0f + (float)i * 2.0f);
        float grow = fminf(1.0f, ty_seed[i].age * 3.0f), pop = grow < 1.0f ? 1.0f + 0.3f * sinf(grow * 3.1416f) : 1.0f;
        uint32_t c = bk_track_col(ty_seed[i].degree % 6);
        if (!ty_seed[i].on)
            continue;
        bk_disc(x, y, (26.0f + 4.0f * br) * grow, BK_ALPHA(c, (uint32_t)(40.0f * br)));
        for (int k = 0; k < 3; k++) {            /* motes circling it */
            float a = t * 1.6f + (float)k * 2.094f + (float)i;
            bk_disc(x + cosf(a) * 24.0f * grow, y + sinf(a) * 24.0f * grow, 2.0f, BK_ALPHA(c, 180));
        }
        ty_orb(x, y, 15.0f * grow * pop, gd_mix(c, BK_INK, 0.2f), 0.3f * br, 255);
        if (grow > 0.6f)
            bk_text_c((int)x, (int)y - 9, &FONT_S, 1, TY_NOTE_NAMES[bk_scale_note(ty_seed[i].degree) % 12], ARGB(255, 14, 18, 26));
    }
    for (int k = 1; k < 40; k++) {               /* the light trail: a ribbon fading behind it (not across the wrap) */
        int a = ((ty_trail_w - k + 1) % TY_TRAIL + TY_TRAIL) % TY_TRAIL, b = ((ty_trail_w - k) % TY_TRAIL + TY_TRAIL) % TY_TRAIL;
        float ax = k == 1 ? hx : ty_trail_x[a], ay = k == 1 ? hy : ty_trail_y[a], u = 1.0f - (float)k / 40.0f;
        if (fabsf(ax - ty_trail_x[b]) + fabsf(ay - ty_trail_y[b]) > 80.0f)
            continue;
        bk_line(ax, ay, ty_trail_x[b], ty_trail_y[b], 1.0f + 5.0f * u, BK_ALPHA(glow, (uint32_t)(90.0f * u)));
    }
    for (int k = ty_nloop; k >= 1; k--) {        /* the melody on the trail: one bead per note, the sounding one big */
        int idx = ((ty_trail_w - k * 3) % TY_TRAIL + TY_TRAIL) % TY_TRAIL, d = ty_loop[ty_nloop - k], on = (ty_nloop - k) == ty_loop_pos;
        uint32_t c = bk_track_col(d % 6);
        if (on)
            bk_disc(ty_trail_x[idx], ty_trail_y[idx], 16.0f, BK_ALPHA(c, 70));
        bk_disc(ty_trail_x[idx], ty_trail_y[idx], on ? 9.0f : 6.0f, on ? gd_mix(c, BK_INK, 0.4f) : BK_ALPHA(c, 200));
    }
    {   /* the firefly: a pulsing glow, the body leaning into its way, wings beating */
        float pul = 0.5f + 0.5f * sinf(t * 6.0f), wv = 6.0f + 4.0f * sinf(t * 30.0f);
        float nx = -ty_fvy, ny = ty_fvx;          /* (sideways to its way, for the wings) */
        bk_disc(hx, hy, 34.0f + 8.0f * pul + 16.0f * ty_feat, BK_ALPHA(glow, (uint32_t)(36.0f + 30.0f * pul + 80.0f * ty_feat)));
        bk_disc(hx + nx * 9.0f - ty_fvx * 4.0f, hy + ny * 9.0f - ty_fvy * 4.0f, wv, ARGB(90, 220, 235, 255));
        bk_disc(hx - nx * 9.0f - ty_fvx * 4.0f, hy - ny * 9.0f - ty_fvy * 4.0f, wv, ARGB(90, 220, 235, 255));
        bk_disc(hx - ty_fvx * 7.0f, hy - ty_fvy * 7.0f, 7.0f, ARGB(255, 120, 100, 60));   /* (its body behind the light) */
        bk_disc(hx, hy, 10.0f + 2.0f * pul, glow);
        bk_disc(hx + ty_fvx * 3.0f - 2.0f, hy + ty_fvy * 3.0f - 3.0f, 4.0f, ARGB(230, 255, 255, 255));
    }
    for (int k = 0; k < TY_LOOP; k++) {          /* the melody, bead by bead, the one sounding lifted */
        float x = (float)(BK_SX + 76 + k * 38), y = (float)(BK_SY + 668);   /* (clear of the corner sprigs) */
        if (k < ty_nloop) {
            int on = k == ty_loop_pos;
            uint32_t c = bk_track_col(ty_loop[k] % 6);
            if (on)
                bk_disc(x, y - 6.0f, 22.0f, BK_ALPHA(c, 60));
            if (on)
                ty_orb(x, y - 6.0f, 15.0f, c, 0.6f, 255);
            else
                ty_bead(x, y, 12.0f, c, 255);
            bk_text_c((int)x, (int)y - (on ? 6 : 0) - 8, BK_TINY, 1, TY_NOTE_NAMES[bk_scale_note(ty_loop[k]) % 12], ARGB(255, 14, 18, 26));
        } else
            bk_arc(x, y, 9.0f, 1.2f, 0, 6.2832f, ARGB(50, 255, 255, 255));
    }
    if (!ty_nloop)
        bk_text_c(BK_SX + 360, BK_SY + 622, &FONT_S, 1, "CATCH THE GLOWING NOTES: THEY BECOME A MELODY", BK_DIM);
    (void)col;
}

/* WHEEL: four rings of steps round a centre, the hits as beads, the hand sweeping round; a turn (A) glides */
static void ty_draw_wheel(float t, uint32_t col)
{
    const float cx = (float)(BK_SX + 360), cy = (float)(BK_SY + 330);
    static float rot_shown[TY_RINGS];
    int n = md_active_tracks();
    char buf[48];
    uint32_t step_ms = (uint32_t)(15000.0f / (float)(md_tempo_now() > 20 ? md_tempo_now() : 20));
    float frac = (float)(bk_ms() % (step_ms ? step_ms : 1u)) / (float)(step_ms ? step_ms : 1u);
    {   /* the hand, one turn a bar, with a fading sweep behind it */
        float u = ((float)(ty_wheel_n % 16u) + frac) / 16.0f, a = u * 6.2831853f - 1.5707963f;
        for (int k = 0; k < 3; k++) {
            float b = a - (float)k * 0.08f;
            bk_line(cx, cy, cx + cosf(b) * 268.0f, cy + sinf(b) * 268.0f, 2.0f, BK_ALPHA(col, (uint32_t)(80 - k * 25)));
        }
    }
    for (int r = 0; r < TY_RINGS; r++) {
        int steps = ty_ring_[r].steps, now = (int)(ty_wheel_n % (uint32_t)steps), sel = r == ty_ring_sel;
        float rr = 250.0f - (float)r * 52.0f, f = ty_ring_[r].flash, d;
        uint32_t rc = bk_track_col(n > 0 ? r % n : 0);
        d = (float)ty_ring_[r].rot - rot_shown[r];   /* (a turn glides the shorter way round) */
        if (d > (float)steps * 0.5f) rot_shown[r] += (float)steps;
        if (d < -(float)steps * 0.5f) rot_shown[r] -= (float)steps;
        rot_shown[r] += ((float)ty_ring_[r].rot - rot_shown[r]) * bk_glide(10.0f);
        bk_arc(cx, cy, rr, sel ? 2.0f : 1.0f, 0, 6.2832f, BK_ALPHA(sel ? col : BK_INK, sel ? bk_u8(130.0f + 40.0f * sinf(t * 3.0f)) : 40u));   /* (a negative float cast to unsigned: undefined, found by UBSan) */
        for (int i = 0; i < steps; i++) {
            float pos = (float)i + (rot_shown[r] - (float)ty_ring_[r].rot), a = pos / (float)steps * 6.2831853f - 1.5707963f;
            float x = cx + cosf(a) * rr, y = cy + sinf(a) * rr;
            int hit = ty_ring_hit(r, i), here = i == now;
            if (hit && here)
                ty_orb(x, y, 10.0f + 5.0f * f, rc, f, ty_ring_[r].on ? 255u : 90u);
            else if (hit)
                ty_bead(x, y, 8.5f, rc, ty_ring_[r].on ? 255u : 90u);
            else
                bk_disc(x, y, here ? 4.5f : 3.0f, ARGB(here ? 200 : 70, 255, 255, 255));
        }
        if (sel) {                               /* the chosen ring's line: hits in steps, its track */
            snprintf(buf, sizeof buf, "RING %d: %d IN %d   TRACK %d", r + 1, ty_ring_[r].hits, steps, (n > 0 ? r % n : 0) + 1);
            bk_text_c((int)cx, BK_SY + 622, &FONT_S, 1, buf, BK_INK);
        }
    }
    ty_orb(cx, cy, 16.0f + 3.0f * ty_ring_[0].flash, col, 0.3f + ty_ring_[0].flash, 255);
    if (bk_text_w(BK_TINY, "EACH RING A TRACK   R2+X: KEEP WHAT THIS TRACK PLAYED", 1) <= 680)
        bk_text_c((int)cx, BK_SY + 652, BK_TINY, 1, "EACH RING A TRACK   R2+X: KEEP WHAT THIS TRACK PLAYED", BK_FAINT);
    else {                                       /* (1.3: larger letters or German - two lines, not off the edge) */
        bk_text_c((int)cx, BK_SY + 648, BK_TINY, 1, "EACH RING A TRACK", BK_FAINT);
        bk_text_c((int)cx, BK_SY + 672, BK_TINY, 1, "R2+X: KEEP WHAT THIS TRACK PLAYED", BK_FAINT);
    }
}

/* ECHO: four big lily pads in a cross under the D-pad's arrows, the round in the middle, the pond's mood around */
static void ty_draw_echo(float t, uint32_t col)
{
    static const float ox[4] = {-150, 0, 150, 0}, oy[4] = {0, -150, 0, 150};
    static const char *const arrow[4] = {"<", "^", ">", "v"};
    static float shake;
    const float cx = (float)(BK_SX + 360), cy = (float)(BK_SY + 320);
    char buf[48];
    uint32_t now = bk_ms();
    float lose = ty_ec_state == EC_LOST ? fminf(1.0f, (float)(ty_ec_at - now) / 1800.0f) : 0.0f;
    shake = ty_ec_state == EC_LOST ? 6.0f * lose * sinf(t * 60.0f) : 0.0f;
    bk_disc(cx, cy, 250.0f, gd_mix(ARGB(255, 12, 22, 40), ARGB(255, 60, 20, 34), lose));   /* (the pond: opaque, cheap) */
    bk_disc(cx - 60.0f, cy - 70.0f, 120.0f, ARGB(255, 16, 30, 52));   /* (a lighter shallow) */
    bk_arc(cx, cy, 250.0f + 4.0f * sinf(t * 1.5f), 2.0f, 0, 6.2832f, BK_ALPHA(col, 50));
    for (int p = 0; p < 4; p++) {
        float x = cx + ox[p] + shake, y = cy + oy[p], f = ty_ec_flash[p], wob = f * sinf((1.0f - f) * 16.0f);
        float r = 64.0f * (1.0f + 0.06f * wob);
        uint32_t c = bk_track_col(p == 0 ? 0 : p == 1 ? 2 : p == 2 ? 4 : 1), leaf = gd_mix(ARGB(255, 40, 92, 70), c, 0.25f + 0.7f * f);
        int wrong = ty_ec_state == EC_LOST && p == ty_ec_wrong;
        if (wrong)
            leaf = gd_mix(leaf, ARGB(255, 200, 60, 70), 0.7f);
        if (f > 0.02f) {
            bk_disc(x, y, r * (1.25f + 0.3f * f), BK_ALPHA(c, (uint32_t)(80.0f * f)));
            bk_arc(x, y, r + 30.0f * (1.0f - f), 2.5f, 0, 6.2832f, BK_ALPHA(c, (uint32_t)(180.0f * f)));
        }
        bk_disc(x + 3.0f, y + 7.0f, r, ARGB(100, 0, 0, 0));
        ty_blob(x, y + 6.0f * wob, r, leaf, t, p + 3, 255);
        for (int v = 0; v < 6; v++) {
            float a = (float)v * 1.047f + 0.3f;
            bk_line(x, y + 6.0f * wob, x + cosf(a) * r * 0.8f, y + 6.0f * wob + sinf(a) * r * 0.8f, 1.4f, ARGB(40, 255, 255, 255));
        }
        bk_disc(x - r * 0.35f, y - r * 0.35f + 6.0f * wob, r * 0.25f, ARGB((uint32_t)(50 + 100 * f), 255, 255, 255));
        bk_text_c((int)x, (int)(y + 6.0f * wob) - 26, &FONT_B, 1, arrow[p], f > 0.3f ? ARGB(255, 12, 16, 24) : BK_INK);
        bk_text_c((int)x, (int)(y + 6.0f * wob) + 4, BK_TINY, 1, TY_NOTE_NAMES[bk_scale_note(TY_ECHO_DEG[p]) % 12], BK_ALPHA(BK_INK, 180));
    }
    {   /* the middle: the round, and what to do */
        const char *what = ty_ec_state == EC_IDLE ? "A: PLAY" : ty_ec_state == EC_LISTEN ? "LISTEN" : ty_ec_state == EC_YOURS ? "YOUR TURN"
                         : ty_ec_state == EC_WON ? "WELL SUNG" : "FADED";
        float br = ty_ec_state == EC_YOURS ? 0.5f + 0.5f * sinf(t * 5.0f) : 0.0f;
        ty_orb(cx + shake, cy, 46.0f, ty_ec_state == EC_LOST ? ARGB(255, 200, 80, 90) : col, br + (ty_ec_state == EC_WON ? 1.0f : 0.0f), 255);
        snprintf(buf, sizeof buf, "%d", ty_ec_state == EC_IDLE || ty_ec_state == EC_LOST ? ty_ec_last : ty_ec_len);
        bk_text_c((int)(cx + shake), (int)cy - 30, &FONT_L, 1, buf, ARGB(255, 20, 18, 14));
        bk_text_c((int)cx, (int)cy + 52, &FONT_S, 1, what, BK_INK);
        if (ty_ec_state == EC_YOURS)             /* how far into the phrase */
            for (int k = 0; k < ty_ec_len && k < 24; k++)
                bk_disc(cx - (float)(ty_ec_len < 24 ? ty_ec_len : 24) * 4.5f + 4.5f + (float)k * 9.0f, cy + 78.0f, k < ty_ec_pos ? 3.5f : 2.0f,
                        k < ty_ec_pos ? col : ARGB(80, 255, 255, 255));
    }
    snprintf(buf, sizeof buf, "BEST ROUND %d", ty_ec_best);
    bk_text(BK_SX + 40, BK_SY + 640, &FONT_S, 1, buf, BK_DIM);
    bk_text_c(BK_SX + 470, BK_SY + 644, BK_TINY, 1, "THE POND SINGS, YOU SING IT BACK", BK_FAINT);
}

/* DREAM TALES: a chapter's card over the stage, rising out of the night (brick_story.inc). Also where the tale is
   read and written: here, on the drawing side, never from the sound's thread */
static const char *const ST_ROMAN[9] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"};
static void st_draw_card(void)
{
    uint32_t now = bk_ms(), age;
    float a, rise;
    int c, x = BK_SX + 70, y = BK_SY + 170, w = 580, h = 300;
    uint32_t col = ARGB(255, 250, 206, 140);
    char buf[96];
    if (st_chapter < 0)
        st_load();
    if (st_dirty) {
        st_dirty = 0;
        st_save();
    }
    if (st_card == -2 && bk_view == V_TOYS && !lab_open)   /* the tale's first card: on the first visit */
        st_card = 0, st_card_at = bk_stamp(now), st_card_intro = 1;
    if (st_card < 0 && st_chapter >= 0 && st_chapter < ST_N && bk_garden_shown() && !bk_menu_open && !bk_help_open && !bk_quick_open && !sp_on
        && !((bk_held >> BK_FN) & 1u)) {         /* (0.80: not in play mode - its stepping stones' names are there) */
        /* (0.72) in the garden, quietly at its foot: where the tale stands, so it is found without the playground */
        float br = 0.65f + 0.35f * sinf((float)now / 900.0f);
        snprintf(buf, sizeof buf, T("A DREAM TALE, CHAPTER %s: %s   (L2: PLAYGROUND)"), ST_ROMAN[st_chapter], T(ST_CHAPTERS[st_chapter].deed));
        {
            int tw = bk_text_w(BK_TINY, buf, 1);
            tw = tw > 660 ? 660 : tw;            /* (0.80: a longer line, as German makes it, kept in the stage) */
            bk_rrect(BK_SX + 360 - tw / 2 - 16, BK_SY + 652, tw + 32, 24, 12, ARGB(150, 6, 9, 16));
            gd_seedling((float)(BK_SX + 360 - tw / 2 - 6), (float)(BK_SY + 664), col);
            bk_text_fit(BK_SX + 366 - tw / 2, BK_SY + 656, BK_TINY, buf, tw, BK_ALPHA(col, (uint32_t)(255.0f * br)));
        }
        return;
    }
    if (st_card < 0 || bk_menu_open || bk_help_open || sp_on)
        return;
    age = now - st_card_at;
    if (age > 6000u) {
        st_card = -1, st_card_intro = 0;
        return;
    }
    a = age < 400u ? (float)age / 400.0f : age > 5400u ? (float)(6000u - age) / 600.0f : 1.0f;
    rise = bk_calm ? 0.0f : (1.0f - a) * 30.0f;
    c = st_card;
    y += (int)rise;
    bk_rrect(x - 6, y - 6, w + 12, h + 12, 28, ARGB((uint32_t)(60.0f * a), 250, 206, 140));   /* a soft glow round it */
    bk_rrect(x, y, w, h, 24, ARGB((uint32_t)(240.0f * a), 8, 10, 22));
    bk_rframe(x, y, w, h, 24, 2, BK_ALPHA(col, (uint32_t)(200.0f * a)));
    if (a > 0.6f) {                              /* sprigs twining at its corners, a seedling on top */
        gd_sprig((float)(x + 6), (float)(y + h - 6), 1, -1, col);
        gd_sprig((float)(x + w - 6), (float)(y + h - 6), -1, -1, col);
        gd_sprig((float)(x + 6), (float)(y + 6), 1, 1, col);
        gd_sprig((float)(x + w - 6), (float)(y + 6), -1, 1, col);
        gd_seedling((float)(x + w / 2), (float)(y - 2), col);
    }
    for (int k = 0; k < 18; k++) {               /* stars on the card, twinkling */
        uint32_t hh = (uint32_t)(k + 1) * 2654435761u;
        float sx = (float)(x + 20 + (int)(hh % (uint32_t)(w - 40))), sy = (float)(y + 14 + (int)((hh >> 12) % (uint32_t)(h - 28)));
        bk_disc(sx, sy, 1.2f + (float)((hh >> 20) % 3u) * 0.6f, ARGB((uint32_t)(a * (90.0f + 80.0f * sinf((float)now / 400.0f + (float)k))), 255, 245, 220));
    }
    if (c >= ST_N) {                             /* (the whole tale told) */
        bk_text_c(x + w / 2, y + 70, &FONT_M, 1, "The tale is told", BK_ALPHA(BK_INK, (uint32_t)(255.0f * a)));
        bk_text_c(x + w / 2, y + 150, &FONT_S, 1, "The garden is yours now. Dream on.", BK_ALPHA(BK_DIM, (uint32_t)(255.0f * a)));
        return;
    }
    snprintf(buf, sizeof buf, T(st_card_intro ? "A DREAM TALE   CHAPTER %s" : "CHAPTER %s   TOLD"), ST_ROMAN[c]);
    bk_text_c(x + w / 2, y + 26, BK_TINY, 1, buf, BK_ALPHA(col, (uint32_t)(255.0f * a)));
    bk_title(ST_CHAPTERS[c].title, buf, sizeof buf);
    bk_text_c(x + w / 2, y + 52, &FONT_M, 1, buf, BK_ALPHA(BK_INK, (uint32_t)(255.0f * a)));
    bk_text_c(x + w / 2, y + 130, &FONT_S, 1, ST_CHAPTERS[c].line1, BK_ALPHA(BK_DIM, (uint32_t)(255.0f * a)));
    bk_text_c(x + w / 2, y + 160, &FONT_S, 1, ST_CHAPTERS[c].line2, BK_ALPHA(BK_DIM, (uint32_t)(255.0f * a)));
    {   /* what comes next: this chapter's deed (a new tale), else the next one's */
        int nx = st_card_intro ? c : c + 1;
        char head[32];
        if (nx < ST_N) {                         /* (0.76: the chapter above, the deed alone in the pebble - it fits at 640 x 480) */
            snprintf(head, sizeof head, T("%s, CHAPTER %s"), T(st_card_intro ? "NOW" : "NEXT"), ST_ROMAN[nx]);
            snprintf(buf, sizeof buf, "%s", ST_CHAPTERS[nx].deed);
        } else {
            head[0] = 0;
            snprintf(buf, sizeof buf, "THE LAST PAGE IS TURNED");
        }
        if (head[0])
            bk_text_c(x + w / 2, y + 196, BK_TINY, 1, head, BK_ALPHA(BK_DIM, (uint32_t)(255.0f * a)));
        bk_rrect(x + 30, y + 218, w - 60, 40, 20, BK_ALPHA(col, (uint32_t)(40.0f * a)));
        {
            int tw = bk_text_w(BK_TINY, buf, 1);
            bk_text_fit(x + w / 2 - (tw < w - 80 ? tw : w - 80) / 2, y + 228, BK_TINY, buf, w - 80, BK_ALPHA(col, (uint32_t)(255.0f * a)));
        }
    }
}

/* SEED: a dandelion seed on the night wind; wind streaks, spores of light, burrs; the two dials it turns, live */
static void ty_draw_seed(float t, uint32_t col)
{
    const float top = TY_T + 40.0f, bot = TY_B + 20.0f;
    const md_patch *p = md_state();
    float sx = ty_sx < 0 ? TY_L + 200.0f : ty_sx, sy = ty_sx < 0 ? (TY_T + TY_B) * 0.5f : ty_sy;
    char buf[48];
    for (int k = 0; k < 14; k++) {               /* the wind: streaks drifting left, faster in a gust */
        uint32_t h = (uint32_t)(k + 3) * 2654435761u;
        float y = top + (float)(h % 520u), len = 30.0f + (float)((h >> 9) % 60u) + 80.0f * ty_gust;
        float x = TY_R - fmodf(ty_scroll * (0.8f + (float)((h >> 5) % 5u) * 0.15f) + (float)((h >> 13) % 700u), 700.0f);
        bk_line(x, y, x + len, y + 3.0f * sinf(t + (float)k), 1.5f, ARGB((uint32_t)(40 + 60 * ty_gust), 200, 220, 255));
    }
    for (int i = 0; i < TY_SPORES; i++)
        if (ty_spore[i].on) {                    /* a spore: a soft light with a heart */
            float br = 0.6f + 0.4f * sinf(t * 4.0f + ty_spore[i].ph);
            uint32_t c = bk_track_col(ty_spore[i].degree % 6);
            bk_disc(ty_spore[i].x, ty_spore[i].y, 22.0f * br, BK_ALPHA(c, 50));
            ty_blob(ty_spore[i].x, ty_spore[i].y, 11.0f, gd_mix(c, BK_INK, 0.3f), t, i, 255);
        }
    for (int i = 0; i < TY_BURRS; i++)
        if (ty_burr[i].on) {                     /* a burr: a dark ball of hooks, turning */
            float x = ty_burr[i].x, y = ty_burr[i].y;
            for (int k = 0; k < 10; k++) {
                float a = ty_burr[i].rot + (float)k * 0.628f;
                bk_line(x, y, x + cosf(a) * 22.0f, y + sinf(a) * 22.0f, 2.0f, ARGB(220, 90, 70, 60));
                bk_disc(x + cosf(a) * 22.0f, y + sinf(a) * 22.0f, 2.2f, ARGB(255, 140, 110, 80));
            }
            bk_disc(x, y, 12.0f, ARGB(255, 60, 44, 40));
            bk_disc(x - 3.0f, y - 4.0f, 4.0f, ARGB(90, 255, 220, 200));
        }
    {   /* the seed: its pappus (a crown of fine hairs), the stalk, the seed below; it leans with its flight */
        float lean = fmaxf(fminf(ty_svx * 0.0015f, 0.5f), -0.5f), hurt = ty_seed_hurt;
        float hx = sx + sinf(lean) * 26.0f, hy = sy - cosf(lean) * 26.0f;
        if (ty_seed_glow > 0.05f)
            bk_disc(hx, hy, 50.0f * ty_seed_glow + 20.0f, BK_ALPHA(col, (uint32_t)(60.0f * ty_seed_glow)));
        bk_disc(hx, hy, 26.0f, ARGB((uint32_t)(30 + 40 * ty_seed_glow), 255, 255, 255));
        for (int k = 0; k < 16; k++) {
            float a = lean + (float)k * 0.3927f + 0.08f * sinf(t * 3.0f + (float)k);
            float l = 22.0f + 3.0f * sinf(t * 2.0f + (float)k * 1.7f);
            uint32_t hc = hurt > 0.1f ? ARGB(220, 255, 160, 150) : ARGB(220, 240, 245, 255);
            bk_line(hx, hy, hx + cosf(a) * l, hy + sinf(a) * l, 1.0f, hc);
            bk_disc(hx + cosf(a) * l, hy + sinf(a) * l, 1.6f, hc);
        }
        bk_line(hx, hy, sx, sy + 10.0f, 1.6f, ARGB(230, 220, 210, 180));
        bk_disc(sx, sy + 12.0f, 4.5f, ARGB(255, 150, 110, 70));
    }
    {   /* the dials it turns, live: GLOW (height) on the left edge, WIND (right) along the bottom */
        float g = (float)p->macro[2] / 1000.0f, w = (float)p->macro[1] / 1000.0f;
        bk_line(TY_L + 8.0f, top, TY_L + 8.0f, bot, 2.0f, ARGB(50, 255, 255, 255));
        bk_disc(TY_L + 8.0f, bot - g * (bot - top), 7.0f, col);
        bk_text(BK_SX + 44, BK_SY + 110, BK_TINY, 1, "GLOW", BK_ALPHA(col, 200));
        bk_line(TY_L + 30.0f, bot + 26.0f, TY_R - 30.0f, bot + 26.0f, 2.0f, ARGB(50, 255, 255, 255));
        bk_disc(TY_L + 30.0f + w * (TY_R - TY_L - 60.0f), bot + 26.0f, 7.0f, ARGB(255, 180, 220, 255));
        bk_text(BK_SX + 600, (int)bot + 34, BK_TINY, 1, "WIND", ARGB(200, 180, 220, 255));
    }
    snprintf(buf, sizeof buf, "STREAK %d   BEST %d", ty_seed_streak, ty_seed_best);
    bk_text(BK_SX + 44, BK_SY + 660, &FONT_S, 1, buf, BK_DIM);
}

/* JELLY: a deep pond with light shafts, plankton glowing as it rises, the jellyfish breathing its bell, tentacles
   trailing; the two dials it turns on the sides */
static void ty_draw_jelly(float t, uint32_t col)
{
    const float top = TY_T + 50.0f, bot = TY_B + 10.0f;
    const md_patch *p = md_state();
    float jx = ty_jx < 0 ? (TY_L + TY_R) * 0.5f : ty_jx, jy = ty_jx < 0 ? (TY_T + TY_B) * 0.5f : ty_jy;
    char buf[40];
    for (int k = 0; k < 4; k++) {                /* light shafts from above, slowly swaying (row fills: cheap) */
        float x = TY_L + 90.0f + (float)k * 160.0f + 30.0f * sinf(t * 0.3f + (float)k * 1.7f);
        for (int q = 0; q < 3; q++)              /* (three nested: soft edges, brighter in the middle) */
            bk_rect((int)x + q * 6, (int)TY_T + 40 + q * 30, 30 + k * 6 - q * 12, 420 - k * 50 - q * 60, ARGB(7, 170, 225, 255));
    }
    for (int i = 0; i < TY_PLANKTON; i++)
        if (ty_pk[i].on) {
            float br = 0.6f + 0.4f * sinf(t * 3.0f + ty_pk[i].ph);
            uint32_t c = bk_track_col(ty_pk[i].degree % 6);
            bk_disc(ty_pk[i].x, ty_pk[i].y, 16.0f * br, BK_ALPHA(c, 50));
            bk_disc(ty_pk[i].x, ty_pk[i].y, 4.0f + 1.5f * br, gd_mix(c, BK_INK, 0.5f));
        }
    {   /* the jellyfish: tentacles first, waving behind; then the bell, squeezed by a pulse */
        float pu = ty_jpulse, w = 46.0f * (1.0f - 0.18f * pu), h = 34.0f * (1.0f + 0.25f * pu);
        uint32_t bell = ARGB(255, 200, 170, 240), glow = ARGB(255, 255, 210, 250);
        for (int k = 0; k < 5; k++) {
            float bx = jx - w * 0.7f + (float)k * w * 0.35f, px = bx, py = jy + 6.0f;
            for (int q = 1; q <= 9; q++) {
                float y = jy + 6.0f + (float)q * (9.0f + 3.0f * (1.0f - pu)), x = bx + 9.0f * sinf(t * 2.2f + (float)q * 0.6f + (float)k) * ((float)q / 9.0f) - ty_jvx * 0.02f * (float)q;
                bk_disc(x, y, 2.6f - (float)q * 0.2f, ARGB((uint32_t)(200 - q * 18), 220, 190, 250));
                px = x, py = y;
            }
            (void)px, (void)py;
        }
        bk_disc(jx, jy, w * 1.5f, BK_ALPHA(glow, (uint32_t)(26.0f + 40.0f * pu)));   /* its glow */
        for (int k = -2; k <= 2; k++)            /* the bell: a dome of discs, lit on top */
            bk_disc(jx + (float)k * w * 0.32f, jy - h * 0.35f + fabsf((float)k) * h * 0.18f, w * 0.52f - fabsf((float)k) * 4.0f, BK_ALPHA(bell, 235));
        for (int k = 0; k < 9; k++) {            /* its frilled rim */
            float a = 3.1416f * (float)k / 8.0f;
            bk_disc(jx - cosf(a) * w, jy + 2.0f + 2.0f * sinf(t * 4.0f + (float)k), 5.0f, ARGB(230, 230, 200, 255));
        }
        bk_disc(jx - w * 0.3f, jy - h * 0.6f, w * 0.28f, ARGB(110, 255, 255, 255));
        bk_disc(jx, jy - h * 0.25f, w * 0.22f, BK_ALPHA(col, (uint32_t)(140 + 100 * pu)));   /* (its heart, lit by a pulse) */
    }
    {   /* the dials: MIST (depth) on the right edge, FIREFLIES (right) along the bottom */
        float m = (float)p->macro[3] / 1000.0f, f = (float)p->macro[0] / 1000.0f;
        bk_rect((int)TY_R - 10, (int)top, 2, (int)(bot - top), ARGB(50, 255, 255, 255));
        bk_disc(TY_R - 9.0f, top + m * (bot - top), 7.0f, ARGB(255, 190, 210, 255));
        bk_text(BK_SX + 630, BK_SY + 110, BK_TINY, 1, "MIST", ARGB(200, 190, 210, 255));
        bk_rect((int)TY_L + 40, (int)bot + 26, (int)(TY_R - TY_L) - 80, 2, ARGB(50, 255, 255, 255));
        bk_disc(TY_L + 40.0f + f * (TY_R - TY_L - 80.0f), bot + 27.0f, 7.0f, ARGB(255, 255, 230, 140));
        bk_text(BK_SX + 44, (int)bot + 34, BK_TINY, 1, "FIREFLIES", ARGB(200, 255, 230, 140));
    }
    snprintf(buf, sizeof buf, "CAUGHT %d", ty_jcaught);
    bk_text(BK_SX + 44, BK_SY + 660, &FONT_S, 1, buf, BK_DIM);
}

/* STARFALL: a night sky over the pond, faint lanes, stars falling with tails, the lotus cup gliding under them;
   three moons for the round's chances */
static void ty_draw_stars(float t, uint32_t col)
{
    static float cup_x = -1;
    float tx = ty_fl_x(ty_fl_lane), w = (TY_R - TY_L) / 8.0f;   /* (w: a lane's width) */
    char buf[48];
    if (cup_x < 0)
        cup_x = tx;
    cup_x += (tx - cup_x) * bk_glide(18.0f);
    if (ty_fl_miss > 0.05f)                      /* (a star lost: the sky blushes) */
        bk_rrect(BK_SX + 24, (int)TY_T, 672, (int)(TY_B - TY_T), 20, ARGB((uint32_t)(40.0f * ty_fl_miss), 160, 60, 90));
    for (int k = 0; k < 8; k++) {                /* the lanes: faint dotted threads, the cup's own brighter */
        float x = ty_fl_x(k);
        uint32_t c = k == ty_fl_lane ? BK_ALPHA(col, 70) : ARGB(26, 200, 230, 255);
        for (float y = TY_T + 30.0f; y < TY_FL_CUP - 20.0f; y += 34.0f)
            bk_rect((int)x - 1, (int)y, 2, 10, c);
    }
    for (int i = 0; i < TY_FALL; i++)
        if (ty_fs[i].on) {                       /* a star: its tail of fading beads, its glow, a body that twinkles */
            float x = ty_fl_x(ty_fs[i].lane), y = ty_fs[i].y, tw = 0.75f + 0.25f * sinf(t * 9.0f + ty_fs[i].ph);
            uint32_t c = gd_mix(bk_track_col(ty_fs[i].lane % 6), ARGB(255, 255, 240, 200), 0.4f);
            for (int q = 1; q <= 6; q++)
                bk_disc(x + 3.0f * sinf(t * 3.0f + (float)q + ty_fs[i].ph), y - (float)q * 12.0f, 7.0f - (float)q * 0.9f, BK_ALPHA(c, (uint32_t)(160 - q * 24)));
            ty_orb(x, y, 12.0f * tw, c, tw, 255);
            for (int q = 0; q < 5; q++) {        /* (its five little rays, turning) */
                float a = (float)q * 1.2566f + t * 0.8f;
                bk_disc(x + cosf(a) * 19.0f * tw, y + sinf(a) * 19.0f * tw, 2.2f, ARGB(220, 255, 250, 230));
            }
        }
    {   /* the lotus cup: petals round a glowing heart, lit when it catches */
        float cy = TY_FL_CUP, f = ty_fl_catch;
        uint32_t petal = gd_mix(ARGB(255, 236, 170, 200), BK_INK, 0.4f * f);
        bk_disc(cup_x, cy + 16.0f, w * 0.34f, ARGB(90, 120, 200, 160));   /* (its own soft light under it) */
        if (f > 0.05f)
            bk_disc(cup_x, cy, 40.0f + 20.0f * f, BK_ALPHA(col, (uint32_t)(80.0f * f)));
        for (int k = 0; k < 5; k++) {
            float a = 3.1416f + (float)k * 0.785f, open = 1.0f + 0.25f * f;
            ty_blob(cup_x + cosf(a) * 18.0f * open, cy + sinf(a) * 12.0f * open + 6.0f, 11.0f, petal, t, k, 255);
        }
        bk_disc(cup_x, cy + 4.0f, 8.0f + 3.0f * f, ARGB(255, 255, 230, 150));
    }
    for (int k = 0; k < 3; k++) {                /* the round's three moons (dark: spent) */
        float mx = TY_R - 30.0f - (float)k * 34.0f, my = TY_T + 12.0f;
        int lit = ty_fl_round ? k < ty_fl_lives : 1;
        bk_disc(mx, my, 11.0f, lit ? ARGB(255, 250, 236, 190) : ARGB(120, 70, 80, 100));
        bk_disc(mx + 5.0f, my - 4.0f, 9.0f, ARGB(255, 10, 14, 24));
    }
    if (ty_fl_over)
        bk_text_c(BK_SX + 360, BK_SY + 300, &FONT_M, 1, "The stars rest", BK_INK);
    if (ty_fl_round)
        snprintf(buf, sizeof buf, "CAUGHT %d   BEST %d", ty_fl_score, ty_fl_best);
    else
        snprintf(buf, sizeof buf, "A: A ROUND   BEST %d   %s", ty_fl_best, TY_FL_SPEEDS[ty_fl_speed]);
    bk_text(BK_SX + 44, BK_SY + 660, &FONT_S, 1, buf, BK_DIM);
}

static void bk_draw_toys(float t)
{
    uint32_t col = ARGB(255, 250, 206, 140), now = bk_ms();
    char buf[96];
    static float pill = -1;
    bk_view_frame("PLAYGROUND", col, "");
    if (pill < 0)
        pill = (float)ty_toy;
    {   /* the toys as a carousel: the chosen one in the middle, its neighbours fading to the sides, dots below
           (L1/R1 turn it; it glides, the shorter way round) */
        const float mid = (float)(BK_SX + 500);
        float d = (float)ty_toy - pill;
        if (d > TY_COUNT * 0.5f) pill += TY_COUNT;
        if (d < -TY_COUNT * 0.5f) pill -= TY_COUNT;
        pill += ((float)ty_toy - pill) * bk_glide(12.0f);
        ty_blob(mid - 34.0f, (float)(BK_SY + 26), 22.0f, col, t, 1, 255);   /* (a seed pod, not a pill) */
        ty_blob(mid + 34.0f, (float)(BK_SY + 26), 22.0f, col, t, 4, 255);
        ty_blob(mid, (float)(BK_SY + 25), 26.0f, col, t, 2, 255);
        for (int k = -2; k <= 2; k++) {
            int i = (((int)floorf(pill + 0.5f) + k) % TY_COUNT + TY_COUNT) % TY_COUNT;
            float off = (floorf(pill + 0.5f) + (float)k - pill), x = mid + off * 140.0f, near = 1.0f - fminf(fabsf(off), 1.0f);
            float fade = 1.0f - fminf(fabsf(off) * 0.55f, 1.0f);
            if (fade <= 0.05f || x < BK_SX + 300 || x > BK_SX + 690)
                continue;
            if (near > 0.5f) {                   /* (0.80: a long name, as German has, in the smaller face; the pod is 120 wide) */
                const felucca_font_t *nf = bk_text_w(&FONT_S, TY_NAMES[i], 1) <= 118 ? &FONT_S : &FONT_T;
                int nw = bk_text_w(nf, TY_NAMES[i], 1);
                nw = nw > 124 ? 124 : nw;
                bk_text_fit((int)x - nw / 2, BK_SY + (nf == &FONT_S ? 16 : 18), nf, TY_NAMES[i], 124, gd_mix(BK_DIM, ARGB(255, 22, 18, 10), near));
            }
            else                                 /* (the neighbours smaller, beyond the arrows) */
                bk_text_c((int)x, BK_SY + 19, BK_TINY, 1, TY_NAMES[i], BK_ALPHA(BK_DIM, (uint32_t)(220.0f * fade)));
        }
        bk_text_c((int)mid - 74, BK_SY + 14, &FONT_S, 1, "<", BK_ALPHA(col, 160));
        bk_text_c((int)mid + 74, BK_SY + 14, &FONT_S, 1, ">", BK_ALPHA(col, 160));
        for (int i = 0; i < TY_COUNT; i++)
            bk_disc(mid - (float)(TY_COUNT - 1) * 6.0f + (float)i * 12.0f, (float)(BK_SY + 52) + (i == ty_toy ? 1.5f * sinf(t * 4.0f) : 0.0f),
                    i == ty_toy ? 3.5f : 2.0f, i == ty_toy ? col : ARGB(90, 200, 255, 210));
    }
    snprintf(buf, sizeof buf, "PLAYS TRACK %d", dream_ui.track + 1);   /* who plays: X changes it */
    bk_disc((float)(BK_SX + 46), (float)(BK_SY + 52), 6.0f + 1.5f * sinf(t * 3.0f), bk_track_col(dream_ui.track));
    bk_text(BK_SX + 58, BK_SY + 45, BK_TINY, 1, buf, BK_DIM);
    ty_draw_vines(t, col);                       /* (0.70: a living border, pollen in the air) */
    if (!bk_light && !bk_calm)
        ty_draw_pollen(t);
    if (ty_toy == TY_BOUNCE)
        ty_draw_bounce(t, col);
    else if (ty_toy == TY_ORBITS)
        ty_draw_orbits(t, col);
    else if (ty_toy == TY_RAIN)
        ty_draw_rain(t, col);
    else if (ty_toy == TY_SWING)
        ty_draw_swing(t, col);
    else if (ty_toy == TY_FIREFLY)
        ty_draw_firefly(t, col);
    else if (ty_toy == TY_WHEEL)
        ty_draw_wheel(t, col);
    else if (ty_toy == TY_ECHO)
        ty_draw_echo(t, col);
    else if (ty_toy == TY_SEED)
        ty_draw_seed(t, col);
    else if (ty_toy == TY_STARS)
        ty_draw_stars(t, col);
    else
        ty_draw_jelly(t, col);
    if (st_chapter >= 0 && st_chapter < ST_N) {  /* where the tale stands: its chapter and the deed it waits for */
        snprintf(buf, sizeof buf, T("TALE %s: %s"), ST_ROMAN[st_chapter], T(ST_CHAPTERS[st_chapter].deed));
        bk_text_fit(BK_SX + 40, BK_SY + 70, BK_TINY, buf, 600, BK_ALPHA(col, 170));
    }
    if (ty_toy == TY_BOUNCE || ty_toy == TY_RAIN || ty_toy == TY_STARS)
        ty_draw_pads(t, col);
    ty_draw_sparks();
    if (ty_switch_at && now - ty_switch_at < 260u) {   /* a new toy: it comes up out of the night */
        float u = (float)(now - ty_switch_at) / 260.0f;
        bk_rrect(BK_SX + 8, BK_SY + 60, 704, 652, 16, ARGB((uint32_t)(220.0f * (1.0f - u) * (1.0f - u)), 6, 9, 16));
    }
}
