/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: drawing the full views (brick_views.inc). */

/* ------------------------------------------------------------ drawing --- */
static void gd_backdrop(uint32_t col);        /* brick_garden.inc: the garden's night behind the views */
static void gd_sprig(float x, float y, float dx, float dy, uint32_t col);
static void gd_seedling(float x, float y, uint32_t col);
static uint32_t gd_mix(uint32_t a, uint32_t b, float k);
static void gd_flower(float x, float y, float r, float open, uint32_t col, uint32_t heart);

static void bk_vines_at(float t, uint32_t col, float x0, float y0, float x1, float y1);   /* (brick_toys_draw.inc) */
/* a full view's frame, in the garden's style: the night behind, sprigs at the corners, a seedling and the title
 * written as a name (Biome Atlas, not BIOME ATLAS); the keys at the top right */
static void bk_garden_frame(uint32_t col)
{
    {   /* (1.3: grounds baked per screen, bk_ground) - (1.5.4) and per track, mixer row and Lab module; while the Lab's
           picture slides in after a module change (its box moving), every frame anew */
        static int last_mod = -1;
        static uint32_t mod_at;
        int sliding;
        if (lab_mod != last_mod)
            last_mod = lab_mod, mod_at = bk_ms();
        sliding = lab_open && bk_ms() - mod_at < 320u;
        bk_ground_key(1, sliding ? (int)(0x40000000u | (bk_draw_gen & 0xffffu)) :
                         bk_view | lab_open << 8 | (dream_ui.track & 7) << 9 | (mx_row & 15) << 12 | (lab_mod & 31) << 16);
    }
    gd_backdrop(col);
    bk_rect(BK_SX - 6, BK_SY - 6, 732, 6, ARGB(255, 0, 0, 0));
    bk_rect(BK_SX - 6, BK_SY + 720, 732, 6, ARGB(255, 0, 0, 0));
    bk_rect(BK_SX - 6, BK_SY, 6, 720, ARGB(255, 0, 0, 0));
    bk_rect(BK_SX + 720, BK_SY, 6, 720, ARGB(255, 0, 0, 0));
    bk_vines_at((float)bk_ms() / 1000.0f, col, (float)(BK_SX - 3), (float)(BK_SY - 3), (float)(BK_SX + 723), (float)(BK_SY + 723));   /* (0.71: a vine, not a line) */
    gd_sprig((float)(BK_SX + 2), (float)(BK_SY + 712), 1, -1, col);
    gd_sprig((float)(BK_SX + 718), (float)(BK_SY + 712), -1, -1, col);
    bk_draw_yield();
}

static void bk_view_frame(const char *title, uint32_t col, const char *help)
{
    char name[40];
    int i, up = 1;
    for (i = 0; title[i] && i < 39; i++) {       /* BIOME ATLAS > Biome Atlas */
        char c = title[i];
        name[i] = up || c < 'A' || c > 'Z' ? c : (char)(c - 'A' + 'a');
        up = c == ' ' || c == '-' || c == '/';
    }
    name[i] = 0;
    bk_garden_frame(col);
    gd_seedling((float)(BK_SX + 22), (float)(BK_SY + 20), col);
    bk_text(BK_SX + 38, BK_SY + 10, &FONT_B, 1, name, col);
    (void)help;                                  /* (the buttons: the bar at the bottom, large) */
}

static void bk_draw_mixer(float t)
{
    const md_patch *p = md_state();
    int n = md_active_tracks(), i, r, sel = dream_ui.track;
    char buf[32];
    bk_view_frame("MIXER", ARGB(255, 200, 225, 205), "L/R TRACK  UP/DN VALUE  L1/R1 ROW  A MUTE  B BACK");
    for (i = 0; i < 6; i++) {
        int x = BK_SX + 12 + i * 118, y = BK_SY + 50, w = 108, h = 660, on = i < n, muted = p->track[i].mute;
        uint32_t col = muted || !on ? BK_FAINT : bk_track_col(i);
        float lv = lab_stem_level[i] * 3.0f;
        lv = lv > 1 ? 1 : lv;
        bk_ground(x, y, w, h, 12, i == sel ? BK_ALPHA(bk_track_col(i), 36) : ARGB(14, 255, 255, 255));   /* (1.3: baked) */
        if (i == sel)
            bk_rframe(x, y, w, h, 12, 2, bk_track_col(i));
        snprintf(buf, sizeof buf, "%d", i + 1);
        bk_disc((float)(x + 18), (float)(y + 20), 12, col);
        bk_text_c(x + 18, y + 10, &FONT_H, 1, buf, ARGB(255, 12, 16, 24));
        if (!on) {
            bk_text(x + 10, y + 40, &FONT_T, 1, "OFF IN THIS", BK_FAINT);
            bk_text(x + 10, y + 56, &FONT_T, 1, "BIOME", BK_FAINT);
            continue;
        }
        {   /* the sound's name: its family small, its own word large */
            const char *nm = p->kit.enabled && i == 3 ? "KIT" : md_model_name(p->track[i].model), *sp = strchr(nm, ' ');
            if (sp) {
                char fam[24];
                snprintf(fam, sizeof fam, "%.*s", (int)(sp - nm) < 23 ? (int)(sp - nm) : 23, nm);
                bk_text(x + 34, y + 4, &FONT_T, 1, fam, BK_DIM);
                bk_text_fit(x + 34, y + 20, &FONT_T, sp + 1, w - 40, muted ? BK_FAINT : BK_INK);
            } else
                bk_text_fit(x + 34, y + 12, &FONT_T, nm, w - 40, muted ? BK_FAINT : BK_INK);
        }
        for (r = MX_PAN; r < MX_ROWS; r++) {     /* PAN a bud on a branch; the sends flowers opening with their amount */
            float cx = (float)(x + w / 2), cy = (float)(y + 84 + (r - 1) * 74), v = (float)mx_value(i, r) / 1000.0f;
            int focus = i == sel && r == mx_row;
            uint32_t c = focus ? BK_INK : col;
            if (focus)
                bk_ground(x + 6, (int)cy - 30, w - 12, 70, 14, ARGB(26, 255, 255, 255));
            if (r == MX_PAN) {
                float bx = cx + (v - 0.5f) * 72.0f, sw = sinf(t * 1.3f + (float)i) * 1.5f;
                for (int k = 0; k < 18; k++) {    /* (the branch, gently bowed) */
                    float u0 = (float)k / 18.0f, u1 = (float)(k + 1) / 18.0f;
                    bk_line(cx - 40 + 80 * u0, cy + 6 - 8 * sinf(u0 * 3.1416f), cx - 40 + 80 * u1, cy + 6 - 8 * sinf(u1 * 3.1416f),
                            2.0f, BK_ALPHA(col, 120));
                }
                bk_disc(cx, cy - 2, 2.5f, ARGB(90, 255, 255, 255));
                bk_disc(bx, cy - 3 + sw - 8.0f * sinf((bx - cx + 40) / 80.0f * 3.1416f) + 8, 8, c);
                bk_disc(bx - 2, cy - 5 + sw - 8.0f * sinf((bx - cx + 40) / 80.0f * 3.1416f) + 8, 3, ARGB(120, 255, 255, 255));
            } else {
                float open = v, rr = 7.0f + 13.0f * open;
                bk_disc(cx, cy, 24, ARGB(14, 255, 255, 255));
                for (int k = 0; k < 7; k++) {    /* petals: more open, larger */
                    float a = (float)k * 0.8976f + t * 0.15f + (float)i;
                    bk_disc(cx + cosf(a) * rr * 0.75f, cy + sinf(a) * rr * 0.75f, 3.0f + 6.0f * open,
                            BK_ALPHA(col, (uint32_t)(open > 0.01f ? 90 + 120 * open : 30)));
                }
                bk_disc(cx, cy, 4.0f + 3.0f * open, c);
            }
            if (focus) {
                snprintf(buf, sizeof buf, "%d", (int)(v * 100.0f + 0.5f));
                bk_text(x + w - 12 - bk_text_w(&FONT_T, buf, 1), (int)cy - 26, &FONT_T, 1, buf, BK_INK);
            }
            bk_text_c((int)cx, (int)cy + 24, &FONT_T, 1, MX_NAMES[r], focus ? BK_INK : BK_DIM);
        }
        {   /* the fader and the meter */
            int fy = y + 380, fh = 220, fx = x + 30, lvl = mx_value(i, MX_LEVEL), focus = i == sel && mx_row == MX_LEVEL;
            int ky = fy + fh - (int)((float)fh * (float)lvl / 1000.0f);
            /* the fader: a stem grown to the level, swaying as loud as it sounds; muted, its head hangs */
            float sway = muted ? 0 : sinf(t * (1.0f + 2.0f * lv) + (float)i) * (2.0f + 10.0f * lv), droop = muted ? 18.0f : 0;
            float hx = (float)(fx + 3) + sway + droop, hy = (float)ky + droop * 0.6f, px0 = (float)(fx + 3), py0 = (float)(fy + fh);
            bk_ground(fx + 1, fy, 4, fh, 2, ARGB(30, 255, 255, 255));
            for (int sg = 1; sg <= 10; sg++) {
                float f = (float)sg / 10.0f, nx = (float)(fx + 3) + (hx - (float)(fx + 3)) * f * f, ny = (float)(fy + fh) + (hy - (float)(fy + fh)) * f;
                bk_line(px0, py0, nx, ny, 4.0f * (1.0f - 0.4f * f), col);
                if (sg == 3 || sg == 6)
                    gd_leaf(nx, ny, (sg == 3) ? 1.0f : -1.0f, 22.0f, 8.0f, col, 0.55f);
                px0 = nx, py0 = ny;
            }
            gd_flower(hx, hy, 13.0f + 4.0f * lv, muted ? 0 : lv, focus ? BK_INK : col, muted ? BK_FAINT : focus ? col : BK_INK);
            if (!muted && lv > 0.55f)             /* loud: pollen rising from it */
                for (int pl = 0; pl < 4; pl++) {
                    float u = fmodf(t * 0.7f + (float)pl * 0.25f + (float)i * 0.13f, 1.0f);
                    bk_disc(hx + sinf(u * 7.0f + (float)pl) * 10.0f, hy - 10.0f - u * 60.0f, 1.8f, BK_ALPHA(col, (uint32_t)(220.0f * (1.0f - u) * (lv - 0.55f) * 2.2f)));
                }
            bk_ground(x + w - 34, fy, 8, fh, 4, ARGB(30, 255, 255, 255));
            if (lv > 0 && !muted)
                bk_rrect(x + w - 34, fy + fh - (int)((float)fh * lv), 8, (int)((float)fh * lv) + 1, 4,
                         lv > 0.92f ? ARGB(255, 240, 120, 100) : bk_track_col(i));
            snprintf(buf, sizeof buf, "%d", lvl / 10);
            bk_text_c(x + w / 2, fy + fh + 10, &FONT_S, 1, buf, focus ? BK_INK : BK_DIM);
            bk_text_c(x + w / 2, fy + fh + 30, &FONT_T, 1, muted ? "MUTED" : "LEVEL", muted ? ARGB(255, 240, 150, 120) : BK_DIM);
        }
    }
}

/* a Biome's own sign: grown from its number - a kind (bloom, spiral, crystal, mushrooms, frond, star-moss), how
   many parts, a turn, a second colour; each of the Biomes looks like itself wherever it is shown */
static void bk_biome_sigil(int id, float cx, float cy, float r, uint32_t c, float t)
{
    uint32_t h = (uint32_t)(id + 11) * 2654435761u, kind = (h >> 5) % 6u, parts = 3u + (h >> 9) % 6u;
    uint32_t c2 = gd_mix(c, bk_track_col((int)((h >> 13) % 6u)), 0.5f);
    float rot = (float)((h >> 17) % 628u) / 100.0f + t * 0.15f, k;
    switch (kind) {
    case 0:                                       /* a bloom: parts petals round a heart */
        for (uint32_t i = 0; i < parts; i++) {
            float a = rot + (float)i * 6.283f / (float)parts;
            bk_disc(cx + cosf(a) * r * 0.5f, cy + sinf(a) * r * 0.5f, r * 0.36f, BK_ALPHA(c, 200));
        }
        bk_disc(cx, cy, r * 0.25f, c2);
        break;
    case 1: {                                     /* a spiral, parts turns */
        float px = cx, py = cy;
        for (int i = 1; i <= 30; i++) {
            float u = (float)i / 30.0f, a = rot + u * (float)parts * 1.8f, rr = u * r;
            float nx = cx + cosf(a) * rr, ny = cy + sinf(a) * rr;
            bk_line(px, py, nx, ny, 2.2f * (1.0f - 0.5f * u), c);
            px = nx, py = ny;
        }
        bk_disc(px, py, r * 0.15f, c2);
        break;
    }
    case 2:                                       /* crystals: parts shards from a point */
        for (uint32_t i = 0; i < parts; i++) {
            float a = -1.57f + ((float)i - (float)(parts - 1) / 2.0f) * 0.35f, L = r * (0.6f + 0.4f * (float)((h >> (i + 3)) & 1u));
            for (int d = 0; d <= 6; d++) {
                float u = (float)d / 6.0f;
                bk_disc(cx + cosf(a) * L * u, cy + r * 0.6f + sinf(a) * L * u * 1.4f, r * 0.14f * (1.0f - u) + 0.6f, BK_ALPHA(i & 1 ? c2 : c, 210));
            }
        }
        break;
    case 3:                                       /* mushrooms: a little group */
        for (uint32_t i = 0; i < (parts > 4 ? 3u : 2u); i++) {
            float mx = cx + ((float)i - 1.0f) * r * 0.55f, hh = r * (0.5f + 0.3f * (float)((h >> (i * 2)) & 1u)), cr = r * (0.42f - 0.08f * (float)i);
            bk_line(mx, cy + r * 0.7f, mx, cy + r * 0.7f - hh, 2.0f, ARGB(230, 230, 220, 200));
            for (int q = -2; q <= 2; q++)
                bk_disc(mx + (float)q * cr * 0.35f, cy + r * 0.7f - hh - (2.0f - fabsf((float)q)) * cr * 0.22f, cr * 0.5f, i & 1 ? c2 : c);
        }
        break;
    case 4: {                                     /* a frond: a curved spine with paired leaflets */
        float px = cx - r * 0.2f, py = cy + r;
        for (int i = 1; i <= 8; i++) {
            float u = (float)i / 8.0f, nx = cx - r * 0.2f + sinf(u * 2.0f + rot * 0.2f) * r * 0.5f, ny = cy + r - u * r * 1.9f;
            bk_line(px, py, nx, ny, 2.0f, c);
            k = r * 0.35f * (1.0f - u * 0.6f);
            bk_disc(nx - k, ny + 2, k * 0.45f, BK_ALPHA(c2, 200));
            bk_disc(nx + k, ny + 2, k * 0.45f, BK_ALPHA(c2, 200));
            px = nx, py = ny;
        }
        break;
    }
    default:                                      /* star-moss: rays and dots */
        for (uint32_t i = 0; i < parts * 2u; i++) {
            float a = rot + (float)i * 3.1416f / (float)parts, L = r * ((i & 1) ? 0.55f : 0.95f);
            bk_line(cx, cy, cx + cosf(a) * L, cy + sinf(a) * L, 1.6f, BK_ALPHA(c, 210));
            bk_disc(cx + cosf(a) * L, cy + sinf(a) * L, (i & 1) ? 1.8f : 2.6f, c2);
        }
        bk_disc(cx, cy, r * 0.2f, c);
    }
}

/* a Biome's card in the Atlas: its sky (the garden's palette for it, gd_sky) and its land as a dark line of hills,
   so every Biome shows its own colours before it is loaded */
static void at_card(int id, int x, int y, int w, int h, float a, float t)
{
    static const uint32_t SKY[8][2] = {
        {ARGB(255, 4, 6, 16), ARGB(255, 70, 40, 104)},   {ARGB(255, 2, 12, 16), ARGB(255, 16, 96, 92)},
        {ARGB(255, 10, 6, 14), ARGB(255, 130, 64, 48)},  {ARGB(255, 8, 4, 18), ARGB(255, 120, 44, 96)},
        {ARGB(255, 4, 10, 22), ARGB(255, 60, 110, 150)}, {ARGB(255, 3, 12, 8), ARGB(255, 40, 100, 56)},
        {ARGB(255, 12, 8, 6), ARGB(255, 120, 92, 40)},   {ARGB(255, 2, 3, 8), ARGB(255, 34, 46, 90)}};
    uint32_t bh = (uint32_t)(id + 7) * 2654435761u, pal = (bh >> 3) % 8u, land = (bh >> 9) % 6u;
    uint32_t ground = ARGB(255, 6, 9, 14), night = ARGB(255, 8, 11, 18);   /* (opaque: plain spans, cheap on the Brick) */
    int j, r = 10;
    for (j = 0; j < h; j++) {                    /* the sky, from its top to its glow at the horizon */
        int in = j < r ? r - (int)sqrtf((float)(r * r - (r - j) * (r - j))) : j >= h - r ? r - (int)sqrtf((float)(r * r - (j - (h - r)) * (j - (h - r)))) : 0;
        bk_span(x + in, x + w - in, y + j, gd_mix(night, gd_mix(SKY[pal][0], SKY[pal][1], (float)j / (float)h), a));
    }
    for (j = 0; j < 3; j++) {                    /* its stars, twinkling each in its own time */
        float sx = (float)x + 50.0f + (float)((bh >> (j * 7)) % (uint32_t)(w - 70)), sy = (float)y + 6.0f + (float)((bh >> (j * 5 + 3)) % 18u);
        float tw = 0.5f + 0.5f * sinf(t * (1.3f + (float)j * 0.7f) + (float)(id + j));
        bk_disc(sx, sy, 1.0f + 0.8f * tw, ARGB((uint32_t)(60 + 160 * tw * a), 255, 250, 230));
    }
    if (a > 0.95f) {                             /* the chosen one: its moon drifting over it */
        float mx = (float)x + 60.0f + fmodf(t * 12.0f, (float)(w - 80));
        bk_disc(mx, (float)y + 14.0f, 9.0f, ARGB(40, 255, 250, 230));
        bk_disc(mx, (float)y + 14.0f, 4.5f, ARGB(220, 255, 250, 230));
    }
    for (int hl = 0; hl < 3; hl++) {             /* its hills: rounder or sharper by its kind of land */
        float hx = (float)x + 70.0f + (float)hl * 70.0f + (float)((bh >> (hl * 4)) & 31u), hw = 34.0f + (float)((bh >> (hl * 3 + 2)) & 15u);
        float hh = (land == 1 || land == 5 ? 20.0f : land == 4 ? 12.0f : 14.0f) + (float)((bh >> (hl * 5 + 1)) & 7u);
        for (j = 0; j < (int)hh && j < h - 4; j++) {
            float v = (float)j / hh, half = land == 1 || land == 5 ? hw * (1.0f - v) : land == 4 ? hw * (v < 0.8f ? 0.8f : 1.0f - v) : hw * sqrtf(1.0f - v * v);
            int x0 = (int)(hx - half), x1 = (int)(hx + half);
            if (x0 < x + 4) x0 = x + 4;
            if (x1 > x + w - 6) x1 = x + w - 6;
            if (x1 > x0)
                bk_span(x0, x1, y + h - 3 - j, ground);
        }
    }
}

static void bk_draw_atlas(float t)
{
    const md_patch *p = md_state();
    int f, i, x = BK_SX, y = BK_SY, cur = p->biome_id, pend = md_pending_biome();
    char buf[48];
    if (at_nfam < 0)
        at_build();
    bk_view_frame("BIOME ATLAS", ARGB(255, 170, 215, 190), at_col == 0 ? "UP/DOWN FAMILY  RIGHT ITS BIOMES  Y SURPRISE" : "D-PAD BIOME  LEFT FAMILIES  A LOAD  Y SURPRISE");
    for (f = 0; f < at_nfam; f++) {             /* the families */
        int fy = y + 52 + f * 32, act = f == at_fam, here = f == md_biome_family_index(cur);
        uint32_t c = bk_track_col(f);
        bk_rrect(x + 10, fy, 190, 28, 8, act ? BK_ALPHA(c, 200) : ARGB(14, 255, 255, 255));
        bk_text_fit(x + 20, fy + 6, &FONT_T, md_biome_family_name((unsigned)f), 140, act ? ARGB(255, 8, 12, 20) : here ? BK_INK : BK_DIM);
        snprintf(buf, sizeof buf, "%d", at_n[f]);
        bk_text(x + 188 - bk_text_w(&FONT_T, buf, 1), fy + 6, &FONT_T, 1, buf, act ? ARGB(255, 8, 12, 20) : BK_FAINT);
        if (here)
            bk_disc((float)(x + 14), (float)(fy + 14), 3, act ? ARGB(255, 8, 12, 20) : BK_INK);
        if (act && at_col == 0) {                /* the D-pad is on the families: a breathing frame and a seed pointing on */
            float br = 0.5f + 0.5f * sinf(t * 3.0f);
            bk_rframe_plain(x + 7, fy - 3, 196, 34, 10, 2, ARGB((uint32_t)(150 + 100 * br), 255, 250, 235));
            bk_disc((float)(x + 207) + 3.0f * br, (float)(fy + 14), 4, BK_INK);
        }
    }
    {   /* the family's Biomes as tiles, two columns, scrolled to the focus */
        int rows = 12, tx = x + 214, tw = 245, th = 46;
        if (at_pos / 2 < at_top)
            at_top = at_pos / 2;
        if (at_pos / 2 >= at_top + rows)
            at_top = at_pos / 2 - rows + 1;
        for (i = at_top * 2; i < at_n[at_fam] && i < (at_top + rows) * 2; i++) {
            int id = at_ids[at_fam][i], cx = tx + (i % 2) * (tw + 8), cy = y + 52 + (i / 2 - at_top) * (th + 6);
            int foc = i == at_pos, playing = id == cur, waiting = id == pend;
            uint32_t c = bk_track_col(at_fam);
            float glow = playing ? 0.5f + 0.5f * sinf(t * 2.0f) : 0.0f;
            at_card(id, cx, cy, tw, th, foc && at_col ? 1.0f : 0.55f + 0.25f * glow + (foc ? 0.2f : 0.0f), t);   /* a little window onto its place */
            if (foc && at_col == 0)                  /* (the D-pad is on the families: only a quiet mark) */
                bk_rframe_plain(cx, cy, tw, th, 10, 1, BK_ALPHA(BK_INK, 110));
            else if (foc || playing)
                bk_rframe(cx, cy, tw, th, 10, foc ? 3 : 2, foc ? BK_ALPHA(BK_INK, 230) : c);
            if (lb_exists(id))
                bk_disc((float)(cx + tw - 14), (float)(cy + 14), 4, LAB_MOD_COL[M_MASTER]);   /* its own Sound Lab */
            bk_disc((float)(cx + 24), (float)(cy + th / 2), foc ? 19.0f : 16.0f, ARGB(150, 6, 9, 16));
            bk_biome_sigil(id, (float)(cx + 24), (float)(cy + th / 2), foc ? 17.0f : 14.0f, foc ? BK_INK : c, foc ? t : 0.0f);
            bk_text_fit(cx + 46, cy + 4, &FONT_S, md_biome_name(id), tw - 64, BK_INK);
            bk_text(cx + 46, cy + 26, &FONT_T, 1, playing ? "PLAYING" : waiting ? "A: CHANGE NOW" : "", foc ? BK_INK : c);
        }
        if (at_top > 0)
            bk_text_c(tx + tw, y + 44, &FONT_T, 1, "...", BK_DIM);
        if ((at_top + rows) * 2 < at_n[at_fam])
            bk_text_c(tx + tw, y + 52 + rows * (th + 6), &FONT_T, 1, "...", BK_DIM);
    }
    if (pend >= 0) {
        snprintf(buf, sizeof buf, "NEXT: %s   A CHANGE NOW   B KEEP", md_biome_name(pend));
        bk_rrect(x + 214, y + 676, 500, 34, 10, ARGB(235, 30, 40, 56));
        bk_text_fit(x + 226, y + 684, &FONT_T, buf, 476, BK_INK);
    } else if (at_n[at_fam] > 0) {               /* the chosen Biome's whole name, its family and what it brings */
        int id = at_ids[at_fam][at_pos];
        uint32_t c = bk_track_col(at_fam);
        bk_rrect(x + 214, y + 676, 498, 34, 10, ARGB(200, 10, 14, 24));
        bk_biome_sigil(id, (float)(x + 234), (float)(y + 693), 11.0f, c, t);
        bk_text_fit(x + 252, y + 681, &FONT_S, md_biome_name(id), 300, BK_INK);
        snprintf(buf, sizeof buf, "%s%s", id == cur ? "PLAYING" : at_col ? "A LOAD" : "RIGHT TO CHOOSE", lb_exists(id) ? "  * OWN LAB" : "");
        bk_text(x + 704 - bk_text_w(&FONT_T, buf, 1), y + 685, &FONT_T, 1, buf, id == cur ? c : BK_DIM);
    }
}

/* the edited track as a piano roll (or the kit as eight pad rows), in x, y, w, h */
static void sq_draw_roll(int row, int x, int y, int w, int h, float t)
{
    const md_patch *p = md_state();
    const md_sequence *q = &p->sequence[row];
    uint32_t col = bk_track_col(row);
    int i, hit = 0, head = md_sequence_playhead((unsigned)row, &hit), gx = x + 150, gw = w - 160;
    float cw = (float)gw / 32.0f;
    char buf[80];
    (void)t;
    if (sq_kit(row)) {
        const md_kit *k = &p->kit;
        for (int pad = 0; pad < 8; pad++) {
            int py = y + 40 + pad * ((h - 60) / 8), ph = (h - 60) / 8 - 4;
            bk_text_fit(x + 12, py + 6, &FONT_T, md_pad_name(pad), 130, pad == k->pad ? BK_INK : BK_DIM);
            for (i = 0; i < 32; i++) {
                int cx = gx + (int)(cw * (float)i), on = (k->gates[pad] >> i) & 1u;
                if (i >= k->length) {
                    bk_disc((float)cx + cw / 2, (float)(py + ph / 2), 1.5f, ARGB(30, 255, 255, 255));
                    continue;
                }
                bk_rrect(cx + 1, py, (int)cw - 2, ph, 4, on ? BK_ALPHA(col, pad == k->pad ? 255 : 170) : ARGB(i % 4 ? 14 : 28, 255, 255, 255));
                if (i == k->step && pad == k->pad)
                    bk_rframe(cx - 1, py - 2, (int)cw + 2, ph + 4, 4, 2, BK_INK);
            }
        }
        snprintf(buf, sizeof buf, "KIT  PAD %s  STEP %d / %d   UP/DN PAD  A HIT", md_pad_name(k->pad), k->step + 1, k->length);
        bk_text(x + 12, y + 10, &FONT_T, 1, buf, BK_INK);
        return;
    }
    {   /* the notes' range, an octave at least */
        int lo = 127, hi = 0;
        for (i = 0; i < q->length; i++)
            if (q->steps[i].note) {
                lo = q->steps[i].note < lo ? q->steps[i].note : lo;
                hi = q->steps[i].note > hi ? q->steps[i].note : hi;
            }
        if (hi < lo) {
            lo = p->track[row].note - 6;
            hi = p->track[row].note + 6;
        }
        lo -= 2;
        hi += 2;
        if (hi - lo < 14)
            hi = lo + 14;
        {
            int ry = y + 40, rh = h - 70;
            float nh = (float)rh / (float)(hi - lo + 1);
            for (int nt = lo; nt <= hi; nt++)   /* the scale's tones as lanes, C named */
                if (md_quantize(nt, p->root, p->scale) == nt) {
                    float ly = (float)ry + (float)(hi - nt) * nh;
                    bk_rect(gx, (int)ly, gw, (int)nh > 1 ? (int)nh - 1 : 1, ARGB(nt % 12 == p->root % 12 ? 26 : 12, 255, 255, 255));
                    if (nt % 12 == 0) {
                        snprintf(buf, sizeof buf, "C%d", nt / 12 - 1);
                        bk_text(gx - 26, (int)ly - 4, &FONT_T, 1, buf, BK_FAINT);
                    }
                }
            {   /* the vine: from note to note, swaying a little, brighter where the bar has already played */
                float lx = -1, ly = 0;
                for (i = 0; i < q->length && i < 32; i++) {
                    const md_step *st = &q->steps[i];
                    if (!st->note)
                        continue;
                    float nx = (float)gx + cw * (float)i + cw * 0.5f, ny = (float)ry + (float)(hi - st->note) * nh + nh * 0.5f;
                    if (lx >= 0) {
                        float mx = (lx + nx) * 0.5f, my = (ly + ny) * 0.5f + 10.0f + 4.0f * sinf(t * 1.2f + (float)i);
                        uint32_t a = head >= 0 && i <= head ? 150 : 70;
                        bk_line(lx, ly, mx, my, 2.0f, BK_ALPHA(col, a));
                        bk_line(mx, my, nx, ny, 2.0f, BK_ALPHA(col, a));
                        if ((i & 3) == 1)               /* (a leaf now and then on the way) */
                            bk_disc(mx + 4.0f, my + 3.0f, 3.0f, BK_ALPHA(col, a));
                    }
                    lx = nx, ly = ny;
                }
            }
            if (head >= 0 && head < q->length) {     /* the playhead: a soft column of light, a firefly at its top */
                float hx = (float)gx + cw * (float)head + cw * 0.5f;
                for (int k = 0; k < 4; k++)
                    bk_rrect((int)(hx - cw * 0.5f) + k * 2, ry, (int)cw - k * 4, rh, 4, ARGB(10, 230, 255, 200));
                bk_disc(hx, (float)ry - 6.0f + 2.0f * sinf(t * 7.0f), 9.0f, ARGB(60, 220, 255, 160));
                bk_disc(hx, (float)ry - 6.0f + 2.0f * sinf(t * 7.0f), 3.5f, ARGB(255, 235, 255, 190));
            }
            for (i = 0; i < 32; i++) {
                int cx = gx + (int)(cw * (float)i);
                const md_step *st = &q->steps[i];
                if (i >= q->length) {
                    bk_disc((float)cx + cw / 2, (float)(ry + rh / 2), 1.5f, ARGB(30, 255, 255, 255));
                    continue;
                }
                if (i % 4 == 0)
                    bk_rect(cx, ry, 1, rh, ARGB(30, 255, 255, 255));
                if (st->note) {
                    float ny = (float)ry + (float)(hi - st->note) * nh;
                    float bx = (float)cx + cw * 0.5f, by = ny + nh * 0.5f;
                    float g = (float)md_step_gate(st) / 100.0f, vr = 3.5f + 4.5f * (float)st->velocity / 100.0f;
                    uint32_t a = 90 + (uint32_t)(165 * st->velocity / 100);
                    if (md_step_chance(st) < 100)        /* a step that may not play flickers */
                        a = (uint32_t)((float)a * (0.55f + 0.45f * (0.5f + 0.5f * sinf(t * 5.0f + (float)i * 1.7f))));
                    bk_line(bx, by, bx + cw * (0.2f + 0.8f * g), by - 3.0f, 2.2f, BK_ALPHA(col, a * 3 / 4));   /* its gate: a leafy tail */
                    bk_disc(bx + cw * (0.2f + 0.8f * g), by - 3.0f, 2.4f, BK_ALPHA(col, a * 3 / 4));
                    if (i == head) {                     /* the sounding one blooms, a ring and sparks leave it */
                        static int last = -1;
                        static float at;
                        if (last != head)
                            last = head, at = t;
                        float u = (t - at) / 0.6f;
                        if (u >= 0 && u < 1) {
                            bk_arc(bx, by, vr + 4.0f + 26.0f * u, 2.0f, 0, 6.3f, BK_ALPHA(col, (uint32_t)(200.0f * (1.0f - u))));
                            for (int sp = 0; sp < 6; sp++)
                                bk_disc(bx + sinf((float)sp * 1.05f) * 18.0f * u, by + cosf((float)sp * 1.05f) * 18.0f * u - 12.0f * u,
                                        2.0f * (1.0f - u) + 0.5f, BK_ALPHA(BK_INK, (uint32_t)(220.0f * (1.0f - u))));
                        }
                        gd_flower(bx, by, vr + 4.0f, 1.0f, col, BK_INK);
                    } else {
                        bk_disc(bx, by, vr + 3.0f, BK_ALPHA(col, a / 5));
                        bk_disc(bx, by, vr, BK_ALPHA(col, a));
                        bk_disc(bx - vr * 0.35f, by - vr * 0.35f, vr * 0.3f, ARGB(a / 2, 255, 255, 255));   /* a berry's shine */
                    }
                    if (md_step_rule(st) != MD_RULE_ALWAYS)   /* a rule: 1, 2 or F above the step */
                        bk_text_c(cx + (int)cw / 2, (int)ny - 16, &FONT_T, 1, md_step_rule(st) == MD_RULE_FILL ? "F" : md_step_rule(st) == MD_RULE_FIRST ? "1" : "2", BK_INK);
                    if (md_step_chance(st) < 100)        /* a step that may not play: a ring */
                        bk_arc((float)cx + cw / 2, ny + nh / 2, 5, 1, 0, 6.28f * (float)md_step_chance(st) / 100.0f, BK_INK);
                }
                if (i == q->edit)
                    bk_rframe(cx - 1, ry - 4, (int)cw + 2, rh + 8, 4, 2, BK_INK);
            }
        }
    }
    {   /* the step under the cursor, the field in focus */
        const md_step *st = &q->steps[q->edit];
        char nn[8];
        int fx = x + 12;
        if (st->note)
            snprintf(nn, sizeof nn, "%s%d", BK_NOTE_NAMES[st->note % 12], st->note / 12 - 1);
        else
            snprintf(nn, sizeof nn, "REST");
        snprintf(buf, sizeof buf, "STEP %d/%d", q->edit + 1, q->length);
        bk_text(fx, y + 10, &FONT_T, 1, buf, BK_INK);
        fx += bk_text_w(&FONT_T, buf, 1) + 16;
        for (int f = 0; f < SF_COUNT; f++) {
            char v[24];
            switch (f) {
            case SF_NOTE: snprintf(v, sizeof v, "%s", nn); break;
            case SF_VELOCITY: snprintf(v, sizeof v, "%d", st->velocity); break;
            case SF_CHANCE: snprintf(v, sizeof v, "%u%%", md_step_chance(st)); break;
            case SF_GATE: snprintf(v, sizeof v, "%u%%", md_step_gate(st)); break;
            case SF_RULE: snprintf(v, sizeof v, "%s", SQ_RULES[md_step_rule(st) & 3]); break;
            default: snprintf(v, sizeof v, "%d", q->length); break;
            }
            snprintf(buf, sizeof buf, "%s %s", SF_NAMES[f], v);
            if (f == sq_field)
                bk_rrect(fx - 6, y + 6, bk_text_w(&FONT_T, buf, 1) + 12, 20, 6, BK_ALPHA(col, 200));
            bk_text(fx, y + 10, &FONT_T, 1, buf, f == sq_field ? ARGB(255, 8, 12, 20) : BK_DIM);
            fx += bk_text_w(&FONT_T, buf, 1) + 14;
        }
        snprintf(buf, sizeof buf, "GENERATOR %s (Y: NEXT)", md_generator_name(q->generator));
        bk_text(x + 12, y + h - 22, &FONT_T, 1, buf, BK_FAINT);
        int tx = x + 12 + bk_text_w(&FONT_T, buf, 1) + 22;   /* (1.3: after it, in any language; German ran into it) */
        tx = tx < x + 250 ? x + 250 : tx;
        {   /* the tools: what they act on, what the clipboard holds, the engine's last answer */
            static const char *const answers[] = {"", "COPIED", "PASTED", "TRANSPOSED", "MIRRORED", "WRONG CLIPBOARD",
                                                  "NOTE LIMIT", "NO NOTES", "UNAVAILABLE", "ACCENTED", "ARTICULATED", "NO CHANGE"};
            static uint32_t seen, at;
            uint32_t notice = md_sequence_notice(), clip = md_sequence_clipboard(), r = notice & 15u;
            if (notice != seen) {
                seen = notice;
                at = bk_ms();
            }
            snprintf(buf, sizeof buf, "R2: TOOLS ON %s   CLIP %s", sq_phrase ? "PHRASE" : "STEP", clip == 2 ? "PHRASE" : clip == 1 ? "STEP" : "EMPTY");
            bk_text_fit(tx, y + h - 22, &FONT_T, buf, x + w - 12 - tx, (bk_held >> BK_R2) & 1u ? BK_INK : BK_FAINT);
            {
                unsigned fill = md_fill_status();    /* bit 0 a fill plays, bit 1 one waits for the next bar */
                if (fill)
                    bk_text(x + 560, y + h - 22, &FONT_T, 1, fill & 1u ? "FILL" : "FILL QUEUED", BK_INK);
            }
            if (r && r < sizeof answers / sizeof answers[0] && bk_ms() - at < 2500u)
                bk_text(x + w - 12 - bk_text_w(&FONT_H, answers[r], 1), y + h - 26, &FONT_H, 1, answers[r],
                        r >= 5 && r <= 8 ? ARGB(255, 240, 150, 120) : col);
        }
    }
}

static void bk_draw_seq(float t)
{
    const md_patch *p = md_state();
    int row, i, sel = dream_ui.track, n = md_active_tracks(), yy = BK_SY + 52;
    char buf[24];
    bk_view_frame("SEQUENCE", ARGB(255, 210, 200, 240),
                  sq_edit ? "L/R STEP  UP/DN VALUE  L1/R1 FIELD  A ON/OFF  Y GENERATE  X DONE" : "UP/DN TRACK  A MUTE  X EDIT  B BACK");
    for (row = 0; row < 6; row++) {
        const md_sequence *q = &p->sequence[row];
        const md_track *tr = &p->track[row];
        int rh = sq_edit ? (row == sel ? 330 : 60) : 98;
        int y = yy, drums = p->kit.enabled && row == 3, hit = 0, compact = sq_edit && row != sel;
        unsigned length = row < n ? sequence_row_length(p, (unsigned)row) : 0, kit_steps = 0;
        int head = md_sequence_playhead((unsigned)row, &hit);
        uint32_t col = tr->mute || row >= n ? BK_FAINT : bk_track_col(row);
        float cw = 540.0f / 32.0f;
        yy += rh + 6;
        bk_rrect(BK_SX + 8, y, 704, rh, 12, row == sel ? BK_ALPHA(bk_track_col(row), 30) : ARGB(12, 255, 255, 255));
        if (row == sel)
            bk_rframe(BK_SX + 8, y, 704, rh, 12, 2, bk_track_col(row));
        if (sq_edit && row == sel && row < n) {
            snprintf(buf, sizeof buf, "%d", row + 1);
            bk_text(BK_SX + 20, y + 40, &FONT_B, 1, buf, col);
            bk_text_fit(BK_SX + 20, y + 66, &FONT_T, drums ? "KIT" : md_model_name(tr->model), 100, BK_DIM);
            sq_draw_roll(row, BK_SX + 8, y, 704, rh, t);
            continue;
        }
        snprintf(buf, sizeof buf, "%d", row + 1);
        bk_text(BK_SX + 20, y + (compact ? 6 : 10), &FONT_B, 1, buf, col);
        bk_text_fit(BK_SX + 44, y + (compact ? 8 : 12), &FONT_T, drums ? "KIT" : md_model_name(tr->model), 110, BK_DIM);
        if (!compact) {
            bk_text(BK_SX + 20, y + 40, &FONT_T, 1, !length ? "-" : drums ? "KIT STEPS" : q->enabled ? "SEQUENCE" : "GENERATED", BK_FAINT);
            if (tr->mute)
                bk_text(BK_SX + 20, y + 60, &FONT_T, 1, "MUTED", ARGB(255, 240, 150, 120));
        }
        if (drums)
            for (unsigned pad = 0; pad < 8; pad++)
                kit_steps |= p->kit.gates[pad];
        if (!compact && !sq_edit && length) {   /* the row as a vine: a stem waving through its steps */
            float base = (float)(y + rh / 2) + 30.0f, px0 = 0, py0 = 0;
            for (i = 0; i <= (int)length * 2; i++) {
                float sx = (float)(BK_SX + 160) + cw * (float)i / 2.0f + cw / 2, sy = base + 3.0f * sinf((float)i * 0.7f + t * 0.8f + (float)row);
                if (i)                           /* (brighter where this bar has already played) */
                    bk_line(px0, py0, sx, sy, head >= 0 && i <= head * 2 + 1 ? 2.6f : 2.0f, BK_ALPHA(col, head >= 0 && i <= head * 2 + 1 ? 170 : 90));
                px0 = sx, py0 = sy;
            }
        }
        if (!compact && !sq_edit && head >= 0 && head < (int)length) {   /* the playing step: a firefly over the vine */
            float fx = (float)(BK_SX + 160) + cw * (float)head + cw / 2, fy = (float)(y + rh / 2) + 30.0f - 4.0f + 3.0f * sinf(t * 6.0f);
            bk_disc(fx, fy, 14, BK_ALPHA(col, 40));
            bk_disc(fx, fy, 6, BK_ALPHA(ARGB(255, 240, 255, 200), 220));
        }
        for (i = 0; i < 32; i++) {
            float cx = (float)(BK_SX + 160) + cw * (float)i, mid = (float)(y + rh / 2);
            int on;
            if ((unsigned)i >= length) {
                bk_disc(cx + cw / 2, mid, 1.5f, ARGB(30, 255, 255, 255));
                continue;
            }
            on = drums ? (int)((kit_steps >> i) & 1u) : q->enabled ? q->steps[i].note != 0 :
                 p->ext[row].bass_mode == 2 ? p->ext[row].notes[i % 16] >= 0 :
                 md_hit((unsigned)i, (unsigned)clamp(tr->hits + (p->macro[0] - 500) * tr->length / 1500, 0, tr->length),
                        (unsigned)tr->length, (unsigned)tr->rotate);
            if (on && !compact && !sq_edit) {     /* a seedling, taller with velocity; the playing one blooms */
                float v = q->enabled && !drums ? (float)q->steps[i].velocity / 100.0f : 0.8f;
                float hgt = 14.0f + 46.0f * (v > 1 ? 1 : v), sx = cx + cw / 2, base = mid + 30.0f;
                {   /* a hit sounding: a ring of light spreading from its bloom, sparks rising */
                    static int last_head[6] = {-1, -1, -1, -1, -1, -1};
                    static float hit_t[6], hit_x[6], hit_y[6];
                    if (i == head && head != last_head[row])
                        last_head[row] = head, hit_t[row] = t, hit_x[row] = sx, hit_y[row] = base - hgt;
                    if (i == head) {
                        float age = t - hit_t[row];
                        if (age >= 0 && age < 0.7f) {
                            float u = age / 0.7f;
                            bk_arc(hit_x[row], hit_y[row], 8.0f + 30.0f * u, 2.0f, 0, 6.3f, BK_ALPHA(col, (uint32_t)(200.0f * (1.0f - u))));
                            for (int sp = 0; sp < 5; sp++)
                                bk_disc(hit_x[row] + sinf((float)sp * 1.9f + (float)row) * 14.0f * u, hit_y[row] - 30.0f * u - (float)sp * 4.0f * u,
                                        2.0f * (1.0f - u) + 0.5f, BK_ALPHA(BK_INK, (uint32_t)(220.0f * (1.0f - u))));
                        }
                    }
                }
                bk_line(sx, base, sx, base - hgt, 2.4f, BK_ALPHA(col, i == head ? 255 : 170));
                bk_line(sx, base - hgt * 0.45f, sx + ((i & 1) ? 5.0f : -5.0f), base - hgt * 0.45f - 4.0f, 2.0f, BK_ALPHA(col, 140));
                if (i == head)
                    gd_flower(sx, base - hgt, 8.0f, 1.0f, col, BK_INK);
                else
                    bk_disc(sx, base - hgt, 3.4f, BK_ALPHA(col, 220));
            } else if (on) {
                float v = q->enabled && !drums ? (float)q->steps[i].velocity / 100.0f : 0.8f;
                float hgt = (compact ? 8.0f : 16.0f) + (compact ? 30.0f : 56.0f) * (v > 1 ? 1 : v);
                float sx = cx + cw / 2, sw = 2.0f * sinf(t * 1.3f + (float)(i + row * 5));
                /* a seed pod on a short stalk; the sounding one swells and glows */
                bk_line(sx, mid + hgt / 2, sx + sw, mid - hgt / 2 + 5.0f, 1.6f, BK_ALPHA(col, 120));
                if (i == head)
                    bk_disc(sx + sw, mid - hgt / 2 + 4.0f, 11.0f, BK_ALPHA(col, 60));
                bk_disc(sx + sw, mid - hgt / 2 + 4.0f, i == head ? 5.5f : 3.6f, BK_ALPHA(col, i == head ? 255 : 170));
            } else if (!compact && !sq_edit) {    /* a rest: a small leaf on the vine, a larger one on each beat */
                float sx = cx + cw / 2, sy = mid + 30.0f + 3.0f * sinf((float)(i * 2 + 1) * 0.7f + t * 0.8f + (float)row);
                float lf = i % 4 ? 3.5f : 5.5f, d = (i & 1) ? 1.0f : -1.0f;
                bk_line(sx, sy, sx + lf * 0.6f, sy + d * lf, 1.6f, BK_ALPHA(col, i % 4 ? 70 : 120));
                bk_disc(sx + lf * 0.7f, sy + d * lf * 1.2f, lf * 0.55f, BK_ALPHA(col, i % 4 ? 60 : 110));
            } else
                bk_disc(cx + cw / 2, mid + 30.0f, i % 4 ? 2.2f : 3.2f, ARGB(i % 4 ? 40 : 70, 255, 255, 255));   /* a pebble */
            if (i == head && (compact || sq_edit))     /* (a soft light where it plays) */
                bk_rrect((int)cx, y + 4, (int)cw, rh - 8, 4, ARGB(22, 230, 255, 200));
        }
    }
}

/* the stations of journey preset at settings: their Biomes into ids, their count returned */
static int dm_stations(unsigned preset, unsigned settings, int *ids)
{
    int n = ITIN_SECTIONS[settings % 4][settings / 4 % 3];
    for (int i = 0; i < n; i++)
        ids[i] = ITIN_BIOMES[preset % 14][i * 31 / (n - 1)];
    return n;
}

/* a point on the dream's path from station i to i + 1 (u 0..1): a soft curve, bowing to one side then the other,
   so the way winds like a trail, not a wire */
static void dm_path(const float *px, const float *py, int i, float u, float *ox, float *oy)
{
    float dx = px[i + 1] - px[i], dy = py[i + 1] - py[i], len = sqrtf(dx * dx + dy * dy) + 0.001f;
    float bow = (i & 1 ? 1.0f : -1.0f) * fminf(26.0f, len * 0.16f), s = sinf(u * 3.14159f);
    *ox = px[i] + dx * u - dy / len * bow * s;
    *oy = py[i] + dy * u + dx / len * bow * s;
}
/* a station's floating island, in its Biome's own colour (the garden's sky glow for that Biome, gd_sky): a mossy
   top with grass, a rock below tapering away, roots hanging and swaying */
static void dm_island(int id, float cx, float cy, float w, float t, int lit)
{
    static const uint32_t GLOW[8] = {ARGB(255, 70, 40, 104), ARGB(255, 16, 96, 92), ARGB(255, 130, 64, 48), ARGB(255, 120, 44, 96),
                                     ARGB(255, 60, 110, 150), ARGB(255, 40, 100, 56), ARGB(255, 120, 92, 40), ARGB(255, 34, 46, 90)};
    uint32_t bh = (uint32_t)(id + 7) * 2654435761u, g = GLOW[(bh >> 3) % 8u];
    uint32_t top = gd_mix(g, ARGB(255, 150, 220, 170), lit ? 0.35f : 0.12f), rock = gd_mix(g, ARGB(255, 6, 8, 14), 0.72f);
    float hw = w * 0.5f;
    int j;
    int deep = w > 60 ? (int)(w * 0.42f) : 24, th = w > 60 ? (int)(w * 0.09f) : 6;   /* (a large one is deeper) */
    for (j = 0; j < deep; j++) {                 /* the rock: rows narrowing to a point, a little crooked */
        float v = (float)j / (float)deep, half = hw * (1.0f - v) * (1.0f - v * 0.3f), off = sinf(v * 5.0f + (float)id) * 3.0f * v * (float)deep / 24.0f;
        bk_span((int)(cx - half + off), (int)(cx + half + off), (int)(cy + 3 + (float)j), BK_ALPHA(gd_mix(rock, ARGB(255, 2, 3, 6), v * 0.6f), 235));
    }
    for (j = -th; j <= th / 2; j++) {            /* the top: an ellipse of moss, lit from above */
        float k = (float)j / (float)th, half = hw * sqrtf(fmaxf(0, 1.0f - k * k));
        bk_span((int)(cx - half), (int)(cx + half), (int)cy + j, BK_ALPHA(gd_mix(top, rock, j > 0 ? 0.45f : 0.0f), 240));
    }
    for (j = 0; j < 2; j++) {                    /* roots, hanging */
        float rx = cx + (j ? 0.32f : -0.28f) * hw, ry = cy + 8, sw = sinf(t * 0.8f + (float)(id + j)) * 3.0f;
        bk_line(rx, ry, rx + sw, ry + (float)deep * 0.66f + (float)((bh >> (20 + j * 3)) & 7u), 1.4f, BK_ALPHA(top, 90));
    }
    for (j = 0; j < 5; j++) {                    /* grass along its rim */
        float gx = cx - hw * 0.8f + (float)j * hw * 0.4f, sw = sinf(t * 1.1f + (float)(j + id)) * 2.0f;
        bk_line(gx, cy - (float)th * 0.5f, gx + sw - 2, cy - (float)th * 0.5f - 6.0f - (float)((bh >> (j * 2)) & 3u) * (w > 60 ? 2.0f : 1.0f), 1.4f, BK_ALPHA(top, 200));
    }
}

/* free flight over the Dream Map (Y, not while dreaming): the wisp follows the D-pad; over an island A lands there
   (its Biome loads; while music plays the engine asks, A again changes); falling stars come every 3 s and one caught
   nudges a macro, up or down - a little throw of the dice for the sound */
static float dm_wx = -1, dm_wy, dm_cx, dm_cy;
static int dm_caught = -1, dm_near = -1, dm_star_macro, dm_star_delta;
static uint32_t dm_caught_at;
#define DM_STAR_MS 3000u
static void dm_station_xy(int i, int n, float *ox, float *oy)
{
    int row = i * 4 / n, first = (row * n + 3) / 4, last = ((row + 1) * n + 3) / 4 - 1, k = i - first, cnt = last - first + 1;
    float fx = cnt > 1 ? (float)k / (float)(cnt - 1) : 0.5f;
    if (row & 1)
        fx = 1.0f - fx;
    *ox = (float)(BK_SX + 240) + fx * 420.0f;
    *oy = (float)(BK_SY + 210 + row * 122);
}
static int dm_star(uint32_t ms, int *idx, float *hx, float *hy, float *u)
{
    int s = (int)(ms / DM_STAR_MS);
    float ph = (float)(ms % DM_STAR_MS) / (float)DM_STAR_MS;
    float sx = (float)(BK_SX + 200 + (s * 211) % 380), sy = (float)(BK_SY + 150 + (s * 97) % 300);
    *idx = s;
    *u = ph / 0.4f;
    *hx = sx + *u * 200.0f;
    *hy = sy + *u * 120.0f;
    return ph < 0.4f;
}
static int dm_ids_now(int *ids)
{
    md_itinera_view iv;
    md_itinera_read(&iv);
    return dm_stations(iv.active ? iv.preset : radio_preset, radio_settings(), ids);
}
static void dm_fly_key(int b, int times)
{
    int ids[32], n = dm_ids_now(ids);
    if (dm_wx < 0 && n > 0)
        dm_station_xy(0, n, &dm_wx, &dm_wy), dm_wy -= 30.0f;
    if (b == BK_LEFT || b == BK_RIGHT)
        dm_wx = fminf(fmaxf(dm_wx + (b == BK_RIGHT ? 24.0f : -24.0f) * (float)times, (float)(BK_SX + 190)), (float)(BK_SX + 710));
    else if (b == BK_UP || b == BK_DOWN)
        dm_wy = fminf(fmaxf(dm_wy + (b == BK_DOWN ? 24.0f : -24.0f) * (float)times, (float)(BK_SY + 150)), (float)(BK_SY + 660));
    else if (b == BK_A) {
        if (md_pending_biome() >= 0)
            dream_cmd(MD_SCENE_CONFIRM, 0, 1);
        else if (dm_near >= 0 && dm_near < n && ids[dm_near] != md_state()->biome_id && dream_cmd(MD_SCENE, 0, ids[dm_near]))
            bk_notice(md_biome_name(ids[dm_near]), "LANDED");
        else if (dm_near < 0)
            bk_notice("FLY TO AN ISLAND", "");
    }
    dm_near = -1;                                /* the island under the wisp (within reach) */
    for (int i = 0; i < n; i++) {
        float sx, sy;
        dm_station_xy(i, n, &sx, &sy);
        if (fabsf(sx - dm_wx) < 40.0f && fabsf(sy - dm_wy) < 56.0f)
            dm_near = i;
    }
}
static void dm_tick(void)
{
    static const char *const nm[4] = {"DENSITY", "MOTION", "COLOUR", "SPACE"};
    int s;
    float hx, hy, u;
    if (bk_view != V_DREAM || !dm_fly || dm_wx < 0)
        return;
    if (!dm_star(bk_ms(), &s, &hx, &hy, &u) || s == dm_caught)
        return;
    if ((hx - dm_wx) * (hx - dm_wx) + (hy - dm_wy) * (hy - dm_wy) < 38.0f * 38.0f) {
        const md_patch *p = md_state();
        char buf[32];
        dm_caught = s;
        dm_caught_at = bk_ms();
        dm_cx = hx, dm_cy = hy;
        dm_star_macro = (int)(((uint32_t)s * 2654435761u >> 7) % 4u);
        dm_star_delta = (((uint32_t)s * 40503u >> 5) & 1u ? 1 : -1) * (80 + (s * 37) % 120);
        dream_cmd(MD_MACRO, dm_star_macro, clamp(p->macro[dm_star_macro] + dm_star_delta, 0, 1000));
        snprintf(buf, sizeof buf, "%s %+d", nm[dm_star_macro], dm_star_delta / 10);
        bk_notice("STAR CAUGHT", buf);
    }
}
static void dm_draw_fly(float t)
{
    int s, ids[32], n = dm_ids_now(ids);
    float hx, hy, u;
    static float ex = -1, ey;
    uint32_t col = ARGB(255, 190, 170, 240), now = bk_ms();
    if (dm_wx < 0 && n > 0)
        dm_station_xy(0, n, &dm_wx, &dm_wy), dm_wy -= 30.0f;
    if (ex < 0)
        ex = dm_wx, ey = dm_wy;
    ex += (dm_wx - ex) * bk_glide(13.0f), ey += (dm_wy - ey) * bk_glide(13.0f);   /* (the wisp eases after the D-pad) */
    if (dm_star(now, &s, &hx, &hy, &u) && s != dm_caught) {   /* the star falling, its tail behind */
        for (int k = 0; k < 12; k++) {
            float b = u - (float)k * 0.03f;
            if (b > 0)
                bk_disc(hx - (u - b) * 200.0f, hy - (u - b) * 120.0f, 4.0f - (float)k * 0.3f, ARGB((uint32_t)(240 - k * 18), 255, 245, 200));
        }
        bk_disc(hx, hy, 14.0f, ARGB(50, 255, 240, 180));
    }
    if (dm_caught >= 0 && now - dm_caught_at < 1200u) {   /* a catch: a burst of sparks and a ring */
        float a = (float)(now - dm_caught_at) / 1200.0f;
        bk_arc(dm_cx, dm_cy, 10.0f + 70.0f * a, 3.0f, 0.0f, 6.2831853f, ARGB((uint32_t)(220 * (1.0f - a)), 255, 240, 190));
        for (int k = 0; k < 10; k++) {
            float ang = (float)k * 0.628f + (float)dm_caught;
            bk_disc(dm_cx + cosf(ang) * 60.0f * a, dm_cy + sinf(ang) * 60.0f * a + 30.0f * a * a, 3.0f * (1.0f - a) + 1.0f,
                    BK_ALPHA(bk_track_col(dm_star_macro), (uint32_t)(255 * (1.0f - a))));
        }
    }
    if (dm_near >= 0 && dm_near < n) {          /* the island in reach: lit, its name, A to land */
        float sx, sy;
        char buf[48];
        dm_station_xy(dm_near, n, &sx, &sy);
        bk_arc(sx, sy, 34.0f + 3.0f * sinf(t * 4.0f), 2.5f, 0.0f, 6.2831853f, BK_ALPHA(BK_INK, 200));
        snprintf(buf, sizeof buf, "A: LAND ON %s", md_biome_name(ids[dm_near]));
        bk_rrect(BK_SX + 214, BK_SY + 640, 480, 30, 10, ARGB(220, 20, 18, 40));
        bk_text_fit(BK_SX + 226, BK_SY + 646, &FONT_T, buf, 456, BK_INK);
    }
    for (int k = 0; k < 3; k++) {                /* the wisp: yours now, brighter, with three little companions */
        float a = t * 2.4f + (float)k * 2.094f;
        bk_disc(ex + cosf(a) * 20.0f, ey + sinf(a) * 9.0f, 2.5f, BK_ALPHA(col, 200));
    }
    bk_disc(ex, ey, 24.0f, BK_ALPHA(col, 50));
    bk_disc(ex, ey, 12.0f, BK_ALPHA(col, 110));
    bk_disc(ex, ey, 6.5f, BK_INK);
}

static void bk_draw_dream(float t)
{
    static const char *const durations[4] = {"30 MIN", "1 HOUR", "2 HOURS", "4 HOURS"}, *const paces[3] = {"CALM", "FLOW", "VARIED"},
                             *const rooms[3] = {"CLOSE", "OPEN", "VAST"};
    md_itinera_view iv;
    int ids[32], n, i, x = BK_SX, y = BK_SY, cur = -1;
    unsigned preset, settings;
    uint32_t col = ARGB(255, 190, 170, 240);
    char buf[64];
    float px[32], py[32];
    md_itinera_read(&iv);
    preset = iv.active ? iv.preset : radio_preset;
    settings = radio_settings();
    n = dm_stations(preset, settings, ids);
    if (iv.active)
        cur = clamp((int)iv.section - 1, 0, n - 1);
    bk_view_frame("DREAM MAP", col, iv.active ? "A WAKE  B BACK" : dm_fly ? "D-PAD FLY  A LAND  Y JOURNEY  B BACK" : "L1/R1 JOURNEY  UP/DN SETTING  L/R VALUE  A DREAM  Y FLY");
    bk_text_c(x + 360, y + 46, &FONT_L, 1, md_itinera_name(preset), BK_INK);
    {   /* the settings, as chips */
        const char *vals[3] = {durations[settings % 4], paces[settings / 4 % 3], rooms[settings / 12]};
        static const char *const names[3] = {"LENGTH", "PACE", "ROOM"};
        for (i = 0; i < 3; i++) {
            int cx = x + 150 + i * 150, foc = dm_row == i + 1 && !iv.active && !dm_fly;
            bk_rrect(cx, y + 92, 130, 40, 10, foc ? BK_ALPHA(col, 210) : ARGB(18, 255, 255, 255));
            bk_text(cx + 10, y + 96, &FONT_T, 1, names[i], foc ? ARGB(255, 8, 12, 20) : BK_DIM);
            bk_text(cx + 10, y + 112, &FONT_H, 1, vals[i], foc ? ARGB(255, 8, 12, 20) : BK_INK);
        }
    }
    /* the stations along four rows, one chapter each, the path turning at each end */
    for (i = 0; i < n; i++) {
        int row = i * 4 / n, first = (row * n + 3) / 4, last = ((row + 1) * n + 3) / 4 - 1, k = i - first, cnt = last - first + 1;
        float fx = cnt > 1 ? (float)k / (float)(cnt - 1) : 0.5f;
        if (row & 1)
            fx = 1.0f - fx;
        px[i] = (float)(x + 240) + fx * 420.0f;  /* (the left column holds the chapters' lines) */
        py[i] = (float)(y + 210 + row * 122) + 10.0f * sinf((float)i * 1.3f + t * 0.4f);
    }
    for (i = 0; i < 4; i++) {                    /* the chapters' lines of the story, a column of their own */
        int chap = iv.active ? (int)iv.chapter : -1, cy = y + 196 + i * 122;
        snprintf(buf, sizeof buf, "%d", i + 1);
        bk_rrect(x + 16, cy - 8, 166, 44, 10, i == chap ? BK_ALPHA(col, 50) : ARGB(10, 255, 255, 255));
        bk_text(x + 26, cy, &FONT_T, 1, buf, i == chap ? col : BK_FAINT);
        {   /* the chapter's line: large, or on two smaller lines */
            const char *st = ITIN_STORY[preset % 14][i];
            char ln[2][64];
            int nl;
            if (bk_text_w(&FONT_S, st, 1) <= 134)
                bk_text(x + 42, cy, &FONT_S, 1, st, i == chap ? BK_INK : BK_DIM);
            else if ((nl = bk_wrap(&FONT_T, st, 134, ln, 2)))
                for (int q = 0; q < nl; q++)
                    bk_text(x + 42, cy - 6 + q * 18, &FONT_T, 1, ln[q], i == chap ? BK_INK : BK_DIM);
            else
                bk_text_fit(x + 42, cy, &FONT_T, st, 134, i == chap ? BK_INK : BK_DIM);
        }
    }
    for (i = 0; i < 6; i++) {                    /* a dream's sky: slow nebulae drifting, now and then a falling star */
        float nx = (float)(x + 360) + 260.0f * sinf(t * 0.03f * (float)(i + 1) + (float)i * 2.1f);
        float ny = (float)(y + 400) + 200.0f * sinf(t * 0.021f * (float)(i + 2) + (float)i * 1.3f);
        bk_disc(nx, ny, 90.0f + 30.0f * (float)(i % 3), BK_ALPHA(bk_track_col(i), 9));
    }
    {
        float ph = fmodf(t / 7.0f, 1.0f), sx0 = (float)(x + 120 + (int)(fmodf(floorf(t / 7.0f) * 211.0f, 400.0f))), sy0 = (float)(y + 150);
        if (ph < 0.12f) {
            float u = ph / 0.12f;
            for (int k = 0; k < 10; k++) {
                float b = u - (float)k * 0.025f;
                if (b > 0)
                    bk_disc(sx0 + b * 260.0f, sy0 + b * 120.0f, 2.6f - (float)k * 0.22f, BK_ALPHA(BK_INK, (uint32_t)(220 - k * 20)));
            }
        }
    }
    for (i = 0; i + 1 < n; i++) {                /* the path: the way travelled glowing, the way ahead a trail of light */
        int done = cur >= 0 && i < cur;
        float pulse = 0.7f + 0.3f * sinf(t * 2.0f - (float)i * 0.6f);
        float ax = px[i], ay = py[i], bx, by;
        for (int k = 1; k <= 16; k++, ax = bx, ay = by) {
            dm_path(px, py, i, (float)k / 16.0f, &bx, &by);
            if (done) {
                bk_line(ax, ay, bx, by, 9.0f, BK_ALPHA(col, (uint32_t)(40 * pulse)));
                bk_line(ax, ay, bx, by, 4.0f, BK_ALPHA(col, (uint32_t)(230 * pulse)));
            } else if (k > 2 && k < 15) {         /* the way ahead: stepping stones, pairs like footprints */
                float nx = by - ay, ny = ax - bx, l = sqrtf(nx * nx + ny * ny) + 0.001f, sd = (k & 1) ? 3.5f : -3.5f;
                bk_disc((ax + bx) * 0.5f + nx / l * sd, (ay + by) * 0.5f + ny / l * sd, 2.2f, ARGB(70, 230, 230, 255));
            }
        }
        if (!done)
            for (int d = 0; d < 2; d++) {         /* (motes drifting along it) */
                float u = fmodf(t * 0.12f + (float)d / 2.0f + (float)i * 0.17f, 1.0f), mx, my;
                dm_path(px, py, i, u, &mx, &my);
                bk_disc(mx, my - 6.0f * sinf(u * 3.14159f), 2.0f, ARGB(130, 230, 230, 255));
            }
    }
    for (i = 0; i < n; i++) {                    /* the stations: Biome colour by family, the visited ones lit */
        int fam = md_biome_family_index(ids[i]), here = i == cur, seen = cur >= 0 && i < cur;
        uint32_t c = bk_track_col(fam < 0 ? 0 : fam);
        float r = here ? 13.0f + 3.0f * sinf(t * 3.0f) : 9.0f;
        if (here) {
            float ring = fmodf(t * 0.5f, 1.0f);
            bk_arc(px[i], py[i], r + 9, 2, 0, 6.3f, BK_ALPHA(c, 160));
            bk_arc(px[i], py[i], r + 10 + 40.0f * ring, 2, 0, 6.3f, BK_ALPHA(c, (uint32_t)(150 * (1.0f - ring))));
        } else if (!seen) {                       /* a glint, now and then */
            float g = sinf(t * 1.7f + (float)i * 2.3f);
            if (g > 0.85f) {
                float a = (g - 0.85f) / 0.15f;
                bk_line(px[i] - 14 * a, py[i] - 12, px[i] + 14 * a, py[i] - 12, 1.5f, BK_ALPHA(BK_INK, (uint32_t)(200 * a)));
                bk_line(px[i], py[i] - 12 - 14 * a, px[i], py[i] - 12 + 14 * a, 1.5f, BK_ALPHA(BK_INK, (uint32_t)(200 * a)));
            }
        }
        dm_island(ids[i], px[i], py[i] + 14.0f, here ? 62.0f : 50.0f, t, seen || here);
        bk_disc(px[i], py[i] - 4.0f, r * 1.5f, BK_ALPHA(ARGB(255, 10, 14, 24), 150));   /* each station its Biome's own sign */
        bk_biome_sigil(ids[i], px[i], py[i] - 4.0f, r * 1.5f, seen || here || cur < 0 ? c : BK_ALPHA(c, 110), here ? t : 0.0f);
        if (n <= 16 || i % 2 == 0 || here) {
            const char *nm = md_biome_name(ids[i]);
            int ly = y + 210 + i * 4 / n * 122 + (i % 2 ? -44 : 46);   /* (by its row, not bobbing: labels never meet) */
            int w = bk_text_w(&FONT_T, nm, 1);
            char sh[24];
            if (w > (n > 16 ? 70 : 120)) {
                size_t l = (size_t)(n > 16 ? 9 : 15);
                snprintf(sh, sizeof sh, "%s", nm);
                if (strlen(sh) > l)
                    sh[l] = 0;
                l = strlen(sh);
                while (l && sh[l - 1] == ' ')
                    sh[--l] = 0;                 /* (no space before the dot) */
                snprintf(sh + l, sizeof sh - l, ".");
                nm = sh;
            }
            bk_text_c((int)px[i], ly, &FONT_T, 1, nm, here ? BK_INK : seen ? BK_DIM : BK_FAINT);
        }
    }
    {   /* the traveller: a wisp of light with a trail, on the way to the next station; before the dream, a ghost of
           it walks the whole way, slowly, showing it */
        float pos = -1;
        if (cur >= 0 && cur + 1 < n) {
            float f = (float)iv.progress / 1000.0f;
            pos = (float)cur + (f > 1 ? 1 : f);
        } else if (cur < 0 && n > 1 && !dm_fly)
            pos = fmodf(t * 0.35f, (float)(n - 1));
        if (pos >= 0) {
            for (int k = 14; k >= 0; k--) {
                float q = pos - (float)k * 0.035f, u;
                int a;
                if (q < 0)
                    continue;
                a = (int)q, u = q - (float)a;
                if (a + 1 >= n)
                    a = n - 2, u = 1;
                float wx, wy;
                dm_path(px, py, a, u, &wx, &wy);
                wy += 2.0f * sinf(t * 4.0f + (float)k) - 10.0f;   /* (hovering over the way) */
                if (k == 0) {
                    bk_disc(wx, wy, 18, BK_ALPHA(col, cur < 0 ? 30 : 60));
                    bk_disc(wx, wy, 7, cur < 0 ? BK_ALPHA(BK_INK, 150) : BK_INK);
                } else
                    bk_disc(wx, wy, 4.5f - (float)k * 0.28f, BK_ALPHA(col, (uint32_t)((cur < 0 ? 90 : 170) - k * (cur < 0 ? 6 : 11))));
            }
        }
    }
    if (dm_fly && !iv.active)
        dm_draw_fly(t);
    if (iv.active) {
        unsigned h = iv.remaining / 3600, m = iv.remaining / 60 % 60, sec = iv.remaining % 60;
        snprintf(buf, sizeof buf, "%u:%02u:%02u LEFT   STATION %d OF %d", h, m, sec, cur + 1, n);
        bk_text(x + 24, y + 676, &FONT_S, 1, buf, BK_INK);
        bk_rrect(x + 420, y + 682, 270, 10, 5, ARGB(30, 255, 255, 255));
        bk_rrect(x + 420, y + 682, (int)(270.0f * (float)iv.energy / 100.0f) + 1, 10, 5, col);
    } else {
        snprintf(buf, sizeof buf, "%d STATIONS   %s", n, dm_fly ? "FREE FLIGHT: CATCH THE FALLING STARS" : md_playing() ? "STOP THE MUSIC TO DREAM" : "A: DREAM");
        bk_text(x + 24, y + 676, &FONT_S, 1, buf, BK_DIM);
    }
}

/* the engine's LFO shapes (dreams.cpp tidal_wave), phase 0..1 -> -1..1 */
static float mf_wave(float ph, int shape)
{
    ph -= floorf(ph);
    if (shape == 0)
        return 1.0f - 4.0f * fabsf(ph - 0.5f);
    if (shape < 3) {
        float w = lab_sin(ph + 0.75f);
        if (shape == 2) {
            float u = (w + 1.0f) * 0.5f;
            w = 2.0f * u * u - 1.0f;
        }
        return w;
    }
    {
        float u = ph < 0.25f ? ph * 4.0f : (1.0f - ph) * (4.0f / 3.0f);
        return 2.0f * u * u * (3.0f - 2.0f * u) - 1.0f;
    }
}
static float mf_period(int rate) { return 0.5f + (float)(rate < 0 ? 0 : rate > 1000 ? 1000 : rate) * 0.001f * 19.5f; }

/* one LFO: its curve over two of its periods' width, the point where it is now, its target and numbers */
static void mf_lfo(int x, int y, int w, int h, int shape, int depth, int rate, int target, float t, uint32_t col, int foc_field, int base)
{
    float per = mf_period(rate), d = (float)depth / 1000.0f, px = 0, py = 0, now = t / per;
    char buf[40];
    int i;
    bk_rrect(x, y, w, h, 8, ARGB(14, 255, 255, 255));
    for (i = 0; i <= w - 16; i += 3) {           /* the wave as a vine, a leaf on each crest */
        float ph = now - 1.0f + (float)i / (float)(w - 16) * 2.0f;
        float v = mf_wave(ph, shape) * (0.15f + 0.85f * d), v2 = mf_wave(ph + 0.01f, shape);
        float cx = (float)(x + 8 + i), cy = (float)(y + h / 2 - 2) - v * (float)(h / 2 - 12);
        if (i)
            bk_line(px, py, cx, cy, 2.4f, BK_ALPHA(col, d > 0 ? 200 : 60));
        if (d > 0 && i % 12 == 0 && v2 < mf_wave(ph, shape) && mf_wave(ph - 0.01f, shape) <= mf_wave(ph, shape))
            bk_disc(cx + 3, cy - 4, 3.5f, BK_ALPHA(col, 170));
        px = cx;
        py = cy;
    }
    {   /* now: a firefly riding the vine, its trail behind it */
        for (int k2 = 6; k2 >= 0; k2--) {
            float ph = now - (float)k2 * 0.025f, v = mf_wave(ph, shape) * (0.15f + 0.85f * d);
            float fx = (float)(x + 8 + (w - 16) / 2) - (float)k2 * 4.0f, fy = (float)(y + h / 2 - 2) - v * (float)(h / 2 - 12);
            if (k2)
                bk_disc(fx, fy, 2.5f - (float)k2 * 0.25f, BK_ALPHA(ARGB(255, 220, 255, 160), (uint32_t)(d > 0 ? 150 - k2 * 20 : 40)));
            else {
                bk_disc(fx, fy, 10, BK_ALPHA(ARGB(255, 210, 255, 150), d > 0 ? 50 : 10));
                bk_disc(fx, fy, d > 0 ? 4.5f : 3.0f, d > 0 ? ARGB(255, 235, 255, 190) : BK_FAINT);
            }
        }
    }
    /* its target at the top; shape, depth and period at the foot (the shape's name short) */
    snprintf(buf, sizeof buf, "%s", md_lfo_target_name(target));
    bk_text_fit(x + 6, y + 2, &FONT_T, buf, w - 12, foc_field == base ? BK_INK : BK_DIM);
    snprintf(buf, sizeof buf, "%.4s %d%% %.1fS", md_lfo_wave_name(shape), depth / 10, per);
    bk_text(x + w - 6 - bk_text_w(&FONT_T, buf, 1), y + h - 19, &FONT_T, 1, buf,
            foc_field > base && foc_field <= base + 3 ? BK_INK : BK_FAINT);
}

/* the firefly pond: six lily pads, one per track; over each its two LFOs as fireflies. A rises and sinks above the
   pad (height = its wave x depth), B swings to and fro beside it; their trails draw the wave's shape, their speed is
   the period. Where a firefly turns at the top, the pad's bud opens; where A touches the water, a ripple. */
static void mf_firefly_at(int i, int ab, float tt, float px, float py, float *fx, float *fy)
{
    const md_patch *p = md_state();
    if (!ab) {
        float d = (float)p->ext[i].tides / 1000.0f, v = mf_wave(tt / mf_period(p->ext[i].tides_rate), p->motion.wave[i]);
        *fx = px + 6.0f * sinf(tt * 1.3f + (float)i);
        *fy = py - 70.0f - v * (12.0f + 92.0f * d);
    } else {
        float d = (float)p->motion.depth2[i] / 1000.0f, ph = (tt + 0.37f * (float)i) / mf_period(p->motion.rate2[i]);
        float v = mf_wave(ph, p->motion.wave2[i]);
        *fx = px + v * (10.0f + 58.0f * d);
        *fy = py - 34.0f + 8.0f * sinf(ph * 6.2831853f * 2.0f);
    }
}

static void mf_draw_pond(int x, int y, int w, int h, float t)
{
    const md_patch *p = md_state();
    int i, n = md_active_tracks(), sel = dream_ui.track;
    char buf[64];
    /* the water: dark, a little lighter towards its far shore, slow glints on it */
    bk_rrect(x, y + 30, w, h - 30, 48, ARGB(255, 6, 20, 30));
    for (int j = 80; j < h - 48; j += 6)          /* (between the rounded corners: plain strips, cheap) */
    {
        float f = sinf(3.14159f * (float)(j - 80) / (float)(h - 128));   /* lightest in the middle, the shore colour at both ends */
        bk_rrect(x + 3, y + j, w - 6, 6, 0, ARGB(255, 6, (uint32_t)(20 + 20 * f), (uint32_t)(30 + 24 * f)));
    }
    bk_rframe_plain(x, y + 30, w, h - 30, 48, 2, ARGB(120, 70, 120, 90));
    for (int g = 0; g < 14; g++) {
        float gx = (float)x + fmodf((float)(g * 97) + t * (6.0f + (float)(g % 3) * 3.0f), (float)w);
        float gy = (float)y + 30.0f + (float)((g * 53) % (h - 60));
        float a = 0.5f + 0.5f * sinf(t * 0.9f + (float)g);
        bk_line(gx - 14.0f, gy, gx + 14.0f, gy, 1.2f, ARGB((uint32_t)(20 + 40 * a), 190, 230, 255));
    }
    for (int r = 0; r < 9; r++) {                /* reeds along the shore, swaying */
        float rx = (float)(x + 10 + r * (w - 20) / 8), sw = 6.0f * sinf(t * 0.8f + (float)r * 1.7f);
        float rh = 50.0f + (float)((r * 37) % 40);
        bk_line(rx, (float)(y + h), rx + sw, (float)(y + h) - rh, 2.4f, ARGB(170, 30, 70, 50));
        bk_disc(rx + sw, (float)(y + h) - rh, 3.0f, ARGB(170, 90, 70, 40));
    }
    for (i = 0; i < 6; i++) {
        int on = i < n, foc = i == sel;
        float px = (float)(x + w / 6 + (i % 3) * (w / 3)), py = (float)(y + (i < 3 ? h * 44 / 100 : h * 88 / 100));
        uint32_t tc = on && !p->track[i].mute ? bk_track_col(i) : BK_FAINT;
        float da = (float)p->ext[i].tides / 1000.0f, db = (float)p->motion.depth2[i] / 1000.0f;
        float va = mf_wave(t / mf_period(p->ext[i].tides_rate), p->motion.wave[i]) * da;
        float vb = mf_wave((t + 0.37f * (float)i) / mf_period(p->motion.rate2[i]), p->motion.wave2[i]) * db;
        float open = 0.5f + 0.25f * (va + vb);
        /* the pad, with its notch, and a ripple whenever A dips to the water */
        if (on && da > 0) {
            float ph = t / mf_period(p->ext[i].tides_rate) - floorf(t / mf_period(p->ext[i].tides_rate));
            bk_arc(px, py, 30.0f + 46.0f * ph, 1.5f, 0.0f, 6.2831853f, ARGB((uint32_t)(70.0f * (1.0f - ph)), 190, 230, 255));
        }
        bk_disc(px, py, foc ? 31.0f : 28.0f, foc ? ARGB(255, 40, 96, 62) : ARGB(255, 26, 66, 44));
        bk_disc(px + 18.0f, py - 14.0f, 9.0f, ARGB(255, 8, 30, 38));
        if (foc)
            bk_arc(px, py, 35.0f, 2.0f, 0.0f, 6.2831853f, BK_ALPHA(tc, (uint32_t)(150 + 80 * sinf(t * 3.0f))));
        /* its bud opens with what the two fireflies do to the sound */
        open = open < 0 ? 0 : open > 1 ? 1 : open;
        for (int pt = 0; pt < 5; pt++) {
            float a = (float)pt * 1.2566f + t * 0.2f, rr = 4.0f + 9.0f * open;
            bk_disc(px + rr * cosf(a), py - 2.0f + rr * sinf(a) * 0.7f, 5.0f + 2.0f * open, BK_ALPHA(tc, on ? 200 : 60));
        }
        bk_disc(px, py - 2.0f, 4.0f, on ? ARGB(255, 255, 240, 170) : BK_FAINT);
        snprintf(buf, sizeof buf, "%d", i + 1);
        bk_text((int)px - 52, (int)py - 12, &FONT_S, 1, buf, tc);
        if (!on)
            continue;
        for (int ab = 0; ab < 2; ab++) {        /* its two fireflies, each with a trail of its last moments */
            int chosen = foc && ab == mf_ab, live = ab ? p->motion.depth2[i] > 0 : p->ext[i].tides > 0;
            uint32_t glow = ab ? ARGB(255, 255, 214, 140) : ARGB(255, 210, 255, 150);
            float fx, fy, qx = 0, qy = 0;
            for (int k = 14; k >= 1; k--) {
                mf_firefly_at(i, ab, t - (float)k * 0.06f, px, py, &fx, &fy);
                if (k < 14)
                    bk_line(qx, qy, fx, fy, chosen ? 2.2f : 1.4f, BK_ALPHA(glow, (uint32_t)((live ? 120 : 30) - k * (live ? 8 : 2))));
                qx = fx, qy = fy;
            }
            mf_firefly_at(i, ab, t, px, py, &fx, &fy);
            if (live) {
                float tw = 0.6f + 0.4f * sinf(t * 9.0f + (float)(i * 2 + ab));
                bk_disc(fx, fy, chosen ? 18.0f : 11.0f, BK_ALPHA(glow, (uint32_t)(40 * tw)));
                bk_disc(fx - 4.0f, fy - 3.0f, 3.5f, ARGB(90, 255, 255, 255));   /* wings */
                bk_disc(fx + 4.0f, fy - 3.0f, 3.5f, ARGB(90, 255, 255, 255));
            }
            bk_disc(fx, fy, live ? (chosen ? 5.5f : 4.0f) : 2.5f, live ? glow : BK_FAINT);
            if (chosen) {                        /* the chosen one carries a lantern ring and its letter */
                bk_arc(fx, fy, 15.0f, 2.0f, 0.0f, 6.2831853f, BK_ALPHA(BK_INK, (uint32_t)(150 + 90 * sinf(t * 4.0f))));
                bk_text((int)fx + 16, (int)fy - 22, &FONT_T, 1, ab ? "B" : "A", BK_INK);
            }
        }
        /* what the fireflies touch, under the pad */
        snprintf(buf, sizeof buf, "%s", md_lfo_target_name(foc && mf_ab ? p->motion.target2[i] : p->motion.target[i]));
        bk_text_c((int)px, (int)py + 36, &FONT_T, 1, buf, foc ? BK_INK : BK_DIM);
    }
}

static void bk_draw_mod(float t)
{
    static const char *const fields[MF_COUNT] = {"A TARGET", "A SHAPE", "A DEPTH", "A RATE", "B TARGET", "B SHAPE", "B DEPTH", "B RATE"};
    static const char *const macros[4] = {"DENSITY", "MOTION", "COLOUR", "SPACE"};
    const md_patch *p = md_state();
    int i, x = BK_SX, y = BK_SY, sel = lk_mode ? -1 : dream_ui.track, n = md_active_tracks();
    uint32_t col = ARGB(255, 150, 220, 210);
    char buf[64];
    int lw = 0;
    bk_view_frame("MODULATION", col, lk_mode == 2 ? "L1 CAPTURE A  R1 CAPTURE B  UP/DN FIELD  L/R VALUE  X LFOS"
                                     : lk_mode == 3 ? "UP/DN MACRO  L/R VALUE  X DRIFT"
                                     : lk_mode ? "UP/DN LINK  L1/R1 FIELD  L/R VALUE  A ON/OFF  X MACROS"
                                     : mf_pond ? "L1/R1 FIREFLY  UP/DN HIGH  L/R SPEED  A DANCE  X TOUCHES"
                                               : "UP/DN TRACK  L1/R1 FIELD  L/R VALUE  X LINKS  Y POND");
    if (lk_mode == 3) {
        snprintf(buf, sizeof buf, "MACRO %s: ", macros[mc_sel]);
        bk_text(x + 180, y + 18, &FONT_T, 1, buf, BK_DIM);
        lw = bk_text_w(&FONT_T, buf, 1);   /* (1.4: the value after the label as drawn, in any language) */
        snprintf(buf, sizeof buf, "%d %%", p->macro[mc_sel] / 10);
        bk_text(x + 180 + lw + 6, y + 18, &FONT_T, 1, buf, BK_INK);
    } else if (lk_mode == 2) {
        int v = df_value(df_field);
        snprintf(buf, sizeof buf, "DRIFT  %s: ", DF_NAMES[df_field]);
        bk_text(x + 180, y + 18, &FONT_T, 1, buf, BK_DIM);
        lw = bk_text_w(&FONT_T, buf, 1);   /* (1.4: the value after the label as drawn, in any language) */
        if (df_field == DF_MODE)
            snprintf(buf, sizeof buf, "%s", DF_MODES[v % 3]);
        else if (df_field == DF_CYCLE)
            snprintf(buf, sizeof buf, "%d S", 10 + v * 290 / 1000);
        else
            snprintf(buf, sizeof buf, "%d %%", v / 10);
        bk_text(x + 180 + lw + 6, y + 18, &FONT_T, 1, buf, BK_INK);
    } else if (lk_mode) {
        int v = lk_value(lk_slot, lk_field);
        snprintf(buf, sizeof buf, "LINK %d  %s: ", lk_slot + 1, LF_NAMES[lk_field]);
        bk_text(x + 180, y + 18, &FONT_T, 1, buf, BK_DIM);
        lw = bk_text_w(&FONT_T, buf, 1);   /* (1.4: the value after the label as drawn, in any language) */
        if (lk_field == LF_SOURCE || lk_field == LF_DEST)
            snprintf(buf, sizeof buf, "TRACK %d", v + 1);
        else if (lk_field == LF_SIGNAL)
            snprintf(buf, sizeof buf, "%s", md_link_signal_name(v));
        else if (lk_field == LF_TARGET)
            snprintf(buf, sizeof buf, "%s", md_link_target_name(v));
        else
            snprintf(buf, sizeof buf, lk_field == LF_DEPTH ? "%+d %%" : "%d %%", v / 10);
        bk_text(x + 180 + lw + 6, y + 18, &FONT_T, 1, buf, BK_INK);
    } else if (mf_pond) {
        int ab = mf_ab, d = ab ? p->motion.depth2[sel] : p->ext[sel].tides, r = ab ? p->motion.rate2[sel] : p->ext[sel].tides_rate;
        snprintf(buf, sizeof buf, "FIREFLY %d%s  %s  %.4s  HIGH %d%%  %.1f S", sel + 1, ab ? "B" : "A",
                 md_lfo_target_name(ab ? p->motion.target2[sel] : p->motion.target[sel]), md_lfo_wave_name(ab ? p->motion.wave2[sel] : p->motion.wave[sel]),
                 d / 10, mf_period(r));
        bk_text_fit(x + 180, y + 18, &FONT_T, buf, 520, BK_INK);
    } else {
    snprintf(buf, sizeof buf, "TRACK %d  %s: ", sel + 1, fields[mf_field]);
    bk_text(x + 180, y + 18, &FONT_T, 1, buf, BK_DIM);
        lw = bk_text_w(&FONT_T, buf, 1);   /* (1.4: the value after the label as drawn, in any language) */
    {
        int v = mf_value(sel, mf_field);
        if (mf_field == MF_A_TARGET || mf_field == MF_B_TARGET)
            snprintf(buf, sizeof buf, "%s", md_lfo_target_name(v));
        else if (mf_field == MF_A_SHAPE || mf_field == MF_B_SHAPE)
            snprintf(buf, sizeof buf, "%s", md_lfo_wave_name(v));
        else if (mf_field == MF_A_RATE || mf_field == MF_B_RATE)
            snprintf(buf, sizeof buf, "%.1f S PERIOD", mf_period(v));
        else
            snprintf(buf, sizeof buf, "%d %%", v / 10);
        bk_text(x + 180 + lw + 10, y + 18, &FONT_T, 1, buf, BK_INK);
    }
    }
    if (mf_pond && !lk_mode)
        mf_draw_pond(x + 8, y + 50, 452, 640, t);
    else
    for (i = 0; i < 6; i++) {                    /* the tracks' two LFOs */
        int ry = y + 50 + i * 82, on = i < n;
        uint32_t tc = on && !p->track[i].mute ? bk_track_col(i) : BK_FAINT;
        bk_rrect(x + 8, ry, 452, 76, 10, i == sel ? BK_ALPHA(bk_track_col(i), 30) : ARGB(10, 255, 255, 255));
        if (i == sel)
            bk_rframe(x + 8, ry, 452, 76, 10, 2, bk_track_col(i));
        snprintf(buf, sizeof buf, "%d", i + 1);
        bk_text(x + 18, ry + 8, &FONT_B, 1, buf, tc);
        if (!on)
            continue;
        mf_lfo(x + 40, ry + 6, 206, 64, p->motion.wave[i], p->ext[i].tides, p->ext[i].tides_rate, p->motion.target[i], t, tc,
               i == sel ? mf_field : -1, MF_A_TARGET);
        mf_lfo(x + 250, ry + 6, 206, 64, p->motion.wave2[i], p->motion.depth2[i], p->motion.rate2[i], p->motion.target2[i],
               t + 0.37f * (float)i, tc, i == sel ? mf_field : -1, MF_B_TARGET);
    }
    if (!(mf_pond && !lk_mode)) {
    bk_text(x + 52, y + 46 + 6 * 82 + 4, &FONT_T, 1, "LFO A", BK_FAINT);
    bk_text(x + 262, y + 46 + 6 * 82 + 4, &FONT_T, 1, "LFO B", BK_FAINT);
    /* the tide: all twelve LFOs at once, a slow landscape */
    for (i = 0; i < 6; i++) {
        float px = 0, py = 0;
        uint32_t tc = i < n && !p->track[i].mute ? bk_track_col(i) : BK_FAINT;
        for (int k = 0; k <= 440; k += 4) {
            float tt = t + (float)(k - 440) * 0.05f;
            float v = mf_wave(tt / mf_period(p->ext[i].tides_rate), p->motion.wave[i]) * (float)p->ext[i].tides / 1000.0f +
                      mf_wave((tt + 0.37f * (float)i) / mf_period(p->motion.rate2[i]), p->motion.wave2[i]) * (float)p->motion.depth2[i] / 1000.0f;
            float cx = (float)(x + 14 + k), cy = (float)(y + 628) - v * 22.0f + (float)(i - 3) * 11.0f;
            if (k)
                bk_line(px, py, cx, cy, 1.6f, BK_ALPHA(tc, 140));
            px = cx;
            py = cy;
        }
    }
    bk_text(x + 14, y + 690, &FONT_T, 1, "THE LAST 22 S OF ALL TWELVE LFOS", BK_FAINT);
    }
    /* links */
    bk_text(x + 474, y + 52, &FONT_H, 1, "LIVING LINKS", lk_mode ? BK_INK : BK_DIM);
    bk_text(x + 600, y + 54, &FONT_T, 1, lk_mode == 1 ? "X: MACROS" : lk_mode >= 2 || mf_pond ? "" : "X: EDIT", BK_FAINT);
    for (i = 0; i < 4; i++) {
        const md_link *l = &p->links[i];
        int ly = y + 76 + i * 76, on = l->depth != 0, foc = lk_mode == 1 && i == lk_slot;
        float v = md_link_value((unsigned)i);
        uint32_t c = on ? bk_track_col(l->source) : BK_FAINT;
        bk_rrect(x + 470, ly, 240, 70, 10, foc ? BK_ALPHA(col, 40) : ARGB(on ? 20 : 8, 255, 255, 255));
        if (foc)
            bk_rframe(x + 470, ly, 240, 70, 10, 2, col);
        if (!on && !foc) {
            snprintf(buf, sizeof buf, "LINK %d  OFF", i + 1);
            bk_text(x + 480, ly + 26, &FONT_T, 1, buf, BK_FAINT);
            continue;
        }
        {   /* source . signal > destination . target, the focused field lit */
            char a[32], b2[32];
            int fx = x + 480;
            snprintf(a, sizeof a, "%d", l->source + 1);
            snprintf(b2, sizeof b2, "%s", md_link_signal_name(l->signal));
            bk_text(fx, ly + 8, &FONT_T, 1, a, foc && lk_field == LF_SOURCE ? BK_INK : c);
            fx += bk_text_w(&FONT_T, a, 1) + 6;
            bk_text(fx, ly + 8, &FONT_T, 1, b2, foc && lk_field == LF_SIGNAL ? BK_INK : c);
            snprintf(a, sizeof a, "> %d", l->destination + 1);
            snprintf(b2, sizeof b2, "%s", md_link_target_name(l->target));
            fx = x + 480;
            bk_text(fx, ly + 24, &FONT_T, 1, a, foc && lk_field == LF_DEST ? BK_INK : on ? bk_track_col(l->destination) : BK_FAINT);
            fx += bk_text_w(&FONT_T, a, 1) + 6;
            bk_text(fx, ly + 24, &FONT_T, 1, b2, foc && lk_field == LF_TARGET ? BK_INK : on ? bk_track_col(l->destination) : BK_FAINT);
            snprintf(a, sizeof a, "%+d%%", l->depth / 10);
            bk_text(x + 700 - bk_text_w(&FONT_T, a, 1), ly + 8, &FONT_T, 1, a, foc && lk_field == LF_DEPTH ? BK_INK : on ? BK_DIM : BK_FAINT);
            snprintf(a, sizeof a, "REL %d", l->release / 10);
            bk_text(x + 700 - bk_text_w(&FONT_T, a, 1), ly + 24, &FONT_T, 1, a, foc && lk_field == LF_RELEASE ? BK_INK : BK_FAINT);
        }
        bk_rrect(x + 480, ly + 46, 220, 10, 5, ARGB(30, 255, 255, 255));
        if (on) {
            float a = fabsf(v) > 1 ? 1 : fabsf(v);
            bk_rrect(x + 590 + (v < 0 ? -(int)(110 * a) : 0), ly + 46, (int)(110 * a) + 1, 10, 5, c);
        }
    }
    /* macros and drift */
    if (lk_mode == 3) {                          /* the macros, large: what each does, what sounds now */
        static const char *const what[4] = {"MORE OR FEWER HITS", "MELODIC LEAPS", "TONE AND FILTER", "REVERB WASH"};
        const md_drift *d = &p->drift;
        int dy = y + 386, moved = d->mode && d->depth && d->captured == 3;
        float live = (float)md_drift_position() / 1000.0f, dep = (float)d->depth / 1000.0f;
        bk_rrect(x + 470, dy - 6, 240, 330, 12, BK_ALPHA(col, 30));
        bk_rframe(x + 470, dy - 6, 240, 330, 12, 2, col);
        bk_text(x + 482, dy + 2, &FONT_H, 1, "MACROS", BK_INK);
        bk_text(x + 482, dy + 22, &FONT_T, 1, moved ? "DOT: WHERE THE DRIFT TAKES IT" : "THE WHOLE BIOME AT ONCE", BK_DIM);
        for (i = 0; i < 4; i++) {
            int ry = dy + 46 + i * 68, foc = i == mc_sel, v = p->macro[i];
            float cx = (float)(x + 510), cy = (float)(ry + 30), f = (float)v / 1000.0f;
            if (foc)
                bk_rrect(x + 476, ry, 228, 62, 10, BK_ALPHA(col, 60));
            bk_arc(cx, cy, 22, 5, 0.4f, 5.88f, ARGB(34, 255, 255, 255));
            bk_arc(cx, cy, 22, 5, 0.4f, 0.4f + f * 5.48f, foc ? BK_INK : col);
            if (moved) {                         /* where the drift takes it: a dot on the ring */
                float a = (float)d->scene[0].macro[i] + ((float)d->scene[1].macro[i] - (float)d->scene[0].macro[i]) * live;
                float s = ((float)v + dep * (a - (float)v)) / 1000.0f;
                s = s < 0 ? 0 : s > 1 ? 1 : s;
                bk_disc(cx + 31.0f * sinf(0.4f + s * 5.48f), cy - 31.0f * cosf(0.4f + s * 5.48f), 3.5f, BK_INK);
            }
            bk_text(x + 544, ry + 8, &FONT_T, 1, macros[i], foc ? BK_INK : BK_DIM);
            bk_text(x + 544, ry + 34, &FONT_T, 1, what[i], foc ? BK_DIM : BK_FAINT);
            snprintf(buf, sizeof buf, "%d", v / 10);
            bk_text(x + 696 - bk_text_w(&FONT_S, buf, 1), ry + 4, &FONT_S, 1, buf, foc ? BK_INK : BK_DIM);
        }
        return;
    }
    if (lk_mode == 2) {                          /* the drift, large, in place of the macros */
        const md_drift *d = &p->drift;
        int dy = y + 386, cap = d->captured, on = d->mode != 0;
        float pos = (float)md_drift_position() / 1000.0f;
        bk_rrect(x + 470, dy - 6, 240, 330, 12, BK_ALPHA(col, 30));
        bk_rframe(x + 470, dy - 6, 240, 330, 12, 2, col);
        bk_text(x + 482, dy + 2, &FONT_H, 1, "BIOME DRIFT", BK_INK);
        bk_text(x + 482, dy + 22, &FONT_T, 1, md_drift_ready() ? "TWO WORLDS / ONE CONTINUUM" : "CAPTURE A AND B TO BEGIN", BK_DIM);
        for (i = 0; i < 2; i++) {                /* the two scenes */
            int sx = x + 482 + i * 118, got = (cap >> i) & 1;
            bk_rrect(sx, dy + 44, 106, 46, 10, got ? BK_ALPHA(col, 160) : ARGB(16, 255, 255, 255));
            bk_text(sx + 10, dy + 50, &FONT_B, 1, i ? "B" : "A", got ? ARGB(255, 8, 12, 20) : BK_DIM);
            bk_text(sx + 34, dy + 56, &FONT_T, 1, got ? "CAPTURED" : i ? "R1: CAPTURE" : "L1: CAPTURE", got ? ARGB(255, 8, 12, 20) : BK_FAINT);
        }
        for (i = 0; i < DF_COUNT; i++) {         /* the fields */
            int fy = dy + 104 + i * 38, foc = i == df_field, v = df_value(i);
            if (foc)
                bk_rrect(x + 478, fy - 4, 224, 32, 8, BK_ALPHA(col, 60));
            bk_text(x + 486, fy + 2, &FONT_T, 1, DF_NAMES[i], foc ? BK_INK : BK_DIM);
            if (i == DF_MODE)
                snprintf(buf, sizeof buf, "%s", DF_MODES[v % 3]);
            else if (i == DF_CYCLE)
                snprintf(buf, sizeof buf, "%d S", 10 + v * 290 / 1000);
            else
                snprintf(buf, sizeof buf, "%d %%", v / 10);
            bk_text(x + 694 - bk_text_w(&FONT_S, buf, 1), fy, &FONT_S, 1, buf, foc ? BK_INK : BK_DIM);
        }
        {   /* the journey between the worlds: where it is now */
            int by = dy + 268;
            bk_text(x + 486, by, &FONT_T, 1, "A", BK_DIM);
            bk_text(x + 694 - bk_text_w(&FONT_T, "B", 1), by, &FONT_T, 1, "B", BK_DIM);
            bk_rrect(x + 500, by + 4, 180, 8, 4, ARGB(30, 255, 255, 255));
            if (on) {
                float px = (float)(x + 500) + 180.0f * (pos < 0 ? 0 : pos > 1 ? 1 : pos);
                bk_rrect(x + 500, by + 4, (int)(px - (float)(x + 500)) + 1, 8, 4, BK_ALPHA(col, 160));
                bk_disc(px, (float)(by + 8), 8, BK_INK);
            }
            bk_text(x + 486, by + 22, &FONT_T, 1, on ? (d->mode == 2 ? "WANDERING" : "BY HAND (POSITION)") : "OFF", on ? col : BK_FAINT);
        }
        return;
    }
    bk_text(x + 474, y + 386, &FONT_H, 1, "MACROS", BK_DIM);
    for (i = 0; i < 4; i++) {
        float cx = (float)(x + 500 + (i % 2) * 110), cy = (float)(y + 440 + (i / 2) * 92), v = (float)p->macro[i] / 1000.0f;
        bk_arc(cx, cy, 24, 5, 0.4f, 5.88f, ARGB(34, 255, 255, 255));
        bk_arc(cx, cy, 24, 5, 0.4f, 0.4f + v * 5.48f, col);
        {   /* a seed circling the ring, faster the higher the macro; a bud at the value's end */
            float a = t * (0.3f + 1.6f * v) + (float)i * 1.57f, e = 0.4f + v * 5.48f;   /* (bk_arc: 0 at the top, clockwise) */
            bk_disc(cx + cosf(a) * 13.0f, cy + sinf(a) * 13.0f, 2.4f, BK_ALPHA(BK_INK, 170));
            bk_disc(cx + sinf(e) * 24.0f, cy - cosf(e) * 24.0f, 4.5f + 1.0f * sinf(t * 3.0f + (float)i), col);
        }
        bk_text(x + 532 + (i % 2) * 110, (int)cy - 14, &FONT_T, 1, macros[i], BK_DIM);
        snprintf(buf, sizeof buf, "%d", p->macro[i] / 10);
        bk_text(x + 532 + (i % 2) * 110, (int)cy + 2, &FONT_S, 1, buf, BK_INK);
    }
    {
        int dy = y + 610, on = p->drift.mode != 0;
        float pos = (float)md_drift_position() / 1000.0f;
        bk_text(x + 474, dy, &FONT_H, 1, "BIOME DRIFT", BK_DIM);
        snprintf(buf, sizeof buf, on ? "%s  CYCLE %dS  DEPTH %d" : "OFF  (X X: EDIT)", DF_MODES[p->drift.mode % 3], 10 + p->drift.rate * 290 / 1000, p->drift.depth / 10);
        bk_text(x + 474, dy + 22, &FONT_T, 1, buf, on ? BK_INK : BK_FAINT);
        bk_rrect(x + 474, dy + 44, 236, 10, 5, ARGB(30, 255, 255, 255));
        if (on)
            bk_disc((float)(x + 474) + 236.0f * (pos < 0 ? 0 : pos > 1 ? 1 : pos), (float)(dy + 49), 7, col);
        bk_text(x + 474, dy + 60, &FONT_T, 1, "SCENE A", BK_FAINT);
        bk_text(x + 710 - bk_text_w(&FONT_T, "SCENE B", 1), dy + 60, &FONT_T, 1, "SCENE B", BK_FAINT);
    }
}

/* ---- VOICE: one track's sound, drawn ---- */
static const char *const VO_MODES[4] = {"OFF", "LP 12 DB", "LP 24 DB", "LOW PASS GATE"};

static void vf_text(int t, int f, char *buf, size_t n)
{
    const md_patch *p = md_state();
    const md_track *k = &p->track[t];
    int v = vf_value(t, f), env = md_has_envelope(k->model);
    static const char *const names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    switch (f) {
    case VF_PRESET: snprintf(buf, n, "%s%s", v < 0 ? "OWN SOUND" : md_tone_name(v), v >= 0 && (p->edited_mask & (2u << t)) ? " *" : ""); break;
    case VF_MODEL: snprintf(buf, n, "%s", md_model_name(v)); break;
    case VF_CUTOFF: snprintf(buf, n, "%d HZ", md_cutoff_hz(v)); break;
    case VF_CHAR: snprintf(buf, n, "%s", VO_MODES[v & 3]); break;
    case VF_ATTACK: if (env) snprintf(buf, n, "%d MS", 8 * v * v / 1000); else snprintf(buf, n, "MELODIC ONLY"); break;
    case VF_DECAY: if (env) snprintf(buf, n, "%d MS", 60 + 16 * v * v / 1000); else snprintf(buf, n, "%d %%", v / 10); break;
    case VF_HITS: snprintf(buf, n, "%d OF %d", v, k->length); break;
    case VF_LENGTH: snprintf(buf, n, "%d STEPS", v); break;
    case VF_ROTATE: snprintf(buf, n, "%d", v); break;
    case VF_NOTE: snprintf(buf, n, "%s%d  (%d)", names[v % 12], v / 12 - 1, v); break;
    case VF_REPEATS: snprintf(buf, n, "%dX", v); break;
    case VF_DIV: snprintf(buf, n, "/%d", v); break;
    default: snprintf(buf, n, "%d %%", v / 10);
    }
}

/* a panel: frame, title, its four fields (the focused one lit); the graphic area is x+10, y+30, 330 x 86 */
static void vo_panel(int x, int y, const char *title, const char *aside, int first, int t, uint32_t col)
{
    char buf[48];
    int focus = vf_field / 4 == first / 4;
    bk_rrect(x, y, 350, 212, 12, focus ? BK_ALPHA(col, 26) : ARGB(12, 255, 255, 255));
    if (focus)
        bk_rframe(x, y, 350, 212, 12, 2, BK_ALPHA(col, 200));
    bk_text(x + 12, y + 8, &FONT_T, 1, title, focus ? col : BK_DIM);
    if (aside)
        bk_text(x + 338 - bk_text_w(&FONT_T, aside, 1), y + 8, &FONT_T, 1, aside, BK_FAINT);
    if (!focus) {                                 /* (not chosen: its picture, and its first value, large; the rest when chosen) */
        bk_text(x + 14, y + 128, &FONT_T, 1, VF_NAMES[first], BK_DIM);
        vf_text(t, first, buf, sizeof buf);
        bk_text_fit(x + 14, y + 148, &FONT_M, buf, 322, BK_INK);
        bk_text(x + 336 - bk_text_w(&FONT_T, "X: OPEN", 1), y + 186, &FONT_T, 1, "X: OPEN", BK_FAINT);
        return;
    }
    for (int r = 0; r < 4; r++) {
        int f = first + r, ry = y + 118 + r * 23, foc = f == vf_field;
        if (foc)
            bk_rrect(x + 6, ry - 2, 338, 23, 6, BK_ALPHA(col, 70));
        bk_text(x + 14, ry, &FONT_T, 1, VF_NAMES[f], foc ? BK_INK : BK_DIM);
        vf_text(t, f, buf, sizeof buf);
        bk_text(x + 336 - bk_text_w(&FONT_T, buf, 1), ry, &FONT_T, 1, buf, foc ? BK_INK : BK_DIM);
    }
}

static void bk_draw_voice(float tm)
{
    const md_patch *p = md_state();
    int t = dream_ui.track, i, n = md_active_tracks(), kit = p->kit.enabled && t == 3;
    const md_track *k = &p->track[t];
    uint32_t col = bk_track_col(t);
    float lv = lab_stem_level[t] * 3.0f, glow;
    int x = BK_SX, y = BK_SY;
    char buf[64];
    lv = lv > 1 ? 1 : lv;
    glow = 0.55f + 0.45f * lv;
    bk_view_frame("VOICE", col, "L1/R1 TRACK  UP/DN FIELD  L/R VALUE  A LISTEN  X PANEL");
    snprintf(buf, sizeof buf, "TRACK %d  %s", t + 1, kit ? "KIT" : md_model_name(k->model));
    bk_text(x + 140, y + 18, &FONT_T, 1, buf, BK_INK);

    /* SOUND: a sketch of the spectrum: TONE tilts it bright, HARMONICS thins the even partials and spreads them */
    {
        int px = x + 8, py = y + 50;
        float tone = (float)k->tone / 1000.0f, harm = (float)k->harm / 1000.0f;
        vo_panel(px, py, "SOUND", kit ? "KIT TRACK: PADS IN SEQUENCE VIEW" : "SPECTRUM SKETCH", VF_PRESET, t, col);
        for (i = 1; i <= 24; i++) {
            float a = powf((float)i, -(1.7f - 1.4f * tone)) * ((i & 1) ? 1.0f : 1.0f - 0.75f * harm);
            float bx = (float)(px + 14) + (float)(i - 1) * 13.6f * (1.0f + harm * 0.012f * (float)(i - 1));
            float h = 80.0f * (a > 1 ? 1 : a);
            if (bx > (float)(px + 338))
                break;
            {   /* a blade of grass, swaying with the sound, a seed at its tip that swells on a hit */
                float sw = sinf(tm * 1.6f + (float)i * 0.7f) * (1.5f + 6.0f * lv) * (h / 80.0f), by = (float)(py + 116);
                float mx = bx + sw * 0.35f, my = by - h * 0.55f, tx = bx + sw, ty = by - h;
                bk_line(bx, by, mx, my, 5.0f, BK_ALPHA(col, (int)(255.0f * glow)));
                bk_line(mx, my, tx, ty, 3.4f, BK_ALPHA(col, (int)(235.0f * glow)));
                if (h > 12)
                    bk_disc(tx, ty, 2.0f + 3.0f * bk_bloom[t] * (i < 6 ? 1.0f : 0.5f), BK_ALPHA(BK_INK, 120 + (int)(120 * bk_bloom[t])));
            }
        }
    }
    /* FILTER: the response from 20 Hz to 20 kHz; BREATHE's slow sway as two ghosts */
    {
        int px = x + 364, py = y + 50, gx = px + 10, gy = py + 30;
        float fc = (float)md_cutoff_hz(k->cutoff), q = 0.6f + (float)k->resonance / 1000.0f * 7.0f;
        float sway = powf(2.0f, (float)k->breathe / 1000.0f * 1.2f);
        vo_panel(px, py, "FILTER", "20 HZ - 20 KHZ", VF_CUTOFF, t, col);
        for (int g = -1; g <= 1; g++) {
            float c = g < 0 ? fc / sway : g > 0 ? fc * sway : fc, ox = 0, oy = 0;
            if (g && k->breathe < 20)
                continue;
            for (i = 0; i <= 330; i += 3) {
                float f = 20.0f * powf(1000.0f, (float)i / 330.0f), r = f / c, m2, db, yy;
                switch (k->filter & 3) {
                case 0: m2 = 1.0f; break;
                case 3: m2 = 1.0f / (1.0f + r * r); break;
                default:
                    m2 = 1.0f / ((1 - r * r) * (1 - r * r) + (r / q) * (r / q));
                    if ((k->filter & 3) == 2)
                        m2 *= m2 > 1 ? 1.0f : m2;
                }
                db = 10.0f * log10f(m2 + 1e-9f);
                db = db < -36 ? -36 : db > 18 ? 18 : db;
                yy = (float)gy + 22.0f - db * 1.8f;
                if (!g)
                    bk_line((float)(gx + i), (float)(gy + 86), (float)(gx + i), yy, 3.0f, BK_ALPHA(col, 40));
                if (i)
                    bk_line(ox, oy, (float)(gx + i), yy, g ? 1.2f : 2.4f, g ? BK_ALPHA(col, 90) : col);
                ox = (float)(gx + i), oy = yy;
            }
        }
        if (k->filter & 3) {                     /* where the cutoff sits: a moon over the hill, swaying with BREATHE */
            float cx = (float)gx + 330.0f * log10f(fc / 20.0f) / 3.0f, br = (float)k->breathe / 1000.0f;
            float mx = cx + sinf(tm * 0.5f) * 22.0f * br, my = (float)(gy + 6) + sinf(tm * 0.8f) * 2.0f;
            for (int d = 0; d < 86; d += 8)
                bk_line(cx, (float)(gy + d), cx, (float)(gy + d + 4), 1.0f, BK_FAINT);
            mx = fminf(fmaxf(mx, (float)gx + 10), (float)gx + 320);
            bk_disc(mx, my, 16.0f + 3.0f * bk_kick, BK_ALPHA(col, 30));
            bk_disc(mx, my, 8.0f, BK_ALPHA(BK_INK, 220));
            bk_disc(mx + 3.0f, my - 2.0f, 7.0f, ARGB(200, 24, 30, 44));   /* (a crescent) */
        }
    }
    /* ENVELOPE: attack and decay on a square-root time line, LEVEL its height; without one, the engine's tail */
    {
        int px = x + 8, py = y + 268, gx = px + 14, gy = py + 30, env = md_has_envelope(k->model);
        float level = 0.25f + 0.75f * (float)k->level / 1000.0f, ox = (float)gx, oy = (float)(gy + 84);
        float att = env ? 8.0f * (float)(k->attack * k->attack) / 1000.0f : 2.0f;
        float dec = env ? 60.0f + 16.0f * (float)(k->decay * k->decay) / 1000.0f : 80.0f + 6000.0f * powf((float)k->decay / 1000.0f, 2.0f);
        float total = att + dec * 1.6f, curve = 1.0f + (float)k->shape / 1000.0f * 4.0f;
        vo_panel(px, py, "ENVELOPE", env ? "ATTACK / DECAY" : "ENGINE-SHAPED TAIL", VF_ATTACK, t, col);
        for (i = 0; i <= 322; i += 2) {
            float u = (float)i / 322.0f, ms = u * u * total, v;
            if (ms < att)
                v = ms / (att > 0.01f ? att : 0.01f);
            else if (env) {
                float w = (ms - att) / dec;
                v = w >= 1 ? 0 : powf(1.0f - w, curve);
            } else
                v = expf(-(ms - att) / (dec * 0.35f));
            v *= level;
            {
                float cx = (float)(gx + i), cy = (float)(gy + 84) - v * 80.0f;
                bk_line(cx, (float)(gy + 84), cx, cy, 2.0f, BK_ALPHA(col, 40));
                if (i)
                    bk_line(ox, oy, cx, cy, 2.4f, col);
                ox = cx, oy = cy;
            }
        }
        snprintf(buf, sizeof buf, "%.1f S", total / 1000.0f);
        bk_text(px + 338 - bk_text_w(&FONT_T, buf, 1), gy + 70, &FONT_T, 1, buf, BK_FAINT);
        {   /* a firefly runs the envelope after each hit of this track, as long as the sound lasts */
            static uint32_t last_ev[6], hit_at[6];
            uint32_t ev = md_status()->events[t], now = bk_ms();
            if (ev != last_ev[t])
                last_ev[t] = ev, hit_at[t] = now ? now : 1u;
            if (hit_at[t] && (float)(now - hit_at[t]) < total) {
                float ms = (float)(now - hit_at[t]), u = sqrtf(ms / total), v;
                if (ms < att)
                    v = ms / (att > 0.01f ? att : 0.01f);
                else if (env) {
                    float w = (ms - att) / dec;
                    v = w >= 1 ? 0 : powf(1.0f - w, curve);
                } else
                    v = expf(-(ms - att) / (dec * 0.35f));
                {
                    float fx = (float)gx + u * 322.0f, fy = (float)(gy + 84) - v * level * 80.0f;
                    bk_disc(fx, fy, 12.0f, BK_ALPHA(col, 60));
                    bk_disc(fx, fy, 4.5f, BK_INK);
                }
            }
        }
    }
    /* RHYTHM: the euclidean ring; inner dots: what DENSITY makes of it; the step playing now */
    {
        int px = x + 364, py = y + 268, len = k->length > 0 ? k->length : 1;
        int eff = clamp(k->hits + (p->macro[0] - 500) * len / 1500, 0, len);
        float cx = (float)(px + 175), cy = (float)(py + 73);
        vo_panel(px, py, "RHYTHM", "INNER: WITH DENSITY", VF_HITS, t, col);
        for (i = 0; i < len; i++) {
            float a = (float)i * 6.2831853f / (float)len - 1.5707963f;
            int hit = md_hit((unsigned)i, (unsigned)k->hits, (unsigned)len, (unsigned)k->rotate);
            {   /* a hit lights up as the step passes it, then fades */
                int age = md_playing() ? (int)((md_status()->step % (unsigned)len + (unsigned)len - (unsigned)i) % (unsigned)len) : 99;
                float fl = hit && age < 3 ? 1.0f - (float)age / 3.0f : 0.0f;
                if (fl > 0)
                    bk_disc(cx + cosf(a) * 38.0f, cy + sinf(a) * 38.0f, 9.0f + 6.0f * fl, BK_ALPHA(col, (int)(90 * fl)));
                bk_disc(cx + cosf(a) * 38.0f, cy + sinf(a) * 38.0f, hit ? 5.0f + 2.0f * fl : 2.5f, hit ? col : BK_FAINT);
            }
            if (md_hit((unsigned)i, (unsigned)eff, (unsigned)len, (unsigned)k->rotate))
                bk_disc(cx + cosf(a) * 26.0f, cy + sinf(a) * 26.0f, 2.5f, BK_ALPHA(col, 160));
        }
        if (md_playing()) {
            float a = (float)(md_status()->step % (unsigned)len) * 6.2831853f / (float)len - 1.5707963f;
            bk_disc(cx + cosf(a) * 50.0f, cy + sinf(a) * 50.0f, 3.5f, BK_INK);
        }
    }
    /* PLAY: the note on two octaves of keys; repeats as dots */
    {
        int px = x + 8, py = y + 486, gx = px + 14, gy = py + 34, oct = k->note / 12 - 1;
        static const int black[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
        int first = oct * 12 + 12, wx = 0;
        vo_panel(px, py, "PLAY", "A: LISTEN", VF_NOTE, t, col);
        for (i = 0; i < 24; i++) {                /* white keys: the note's octave and the next */
            int nn = first + i;
            if (black[nn % 12])
                continue;
            bk_rrect(gx + wx * 15, gy, 14, 60, 3, nn == k->note ? col : ARGB(60, 255, 255, 255));
            wx++;
        }
        wx = 0;
        for (i = 0; i < 24; i++) {                /* black keys over them */
            int nn = first + i;
            if (black[nn % 12]) {
                bk_rrect(gx + wx * 15 - 5, gy, 10, 36, 2, nn == k->note ? col : ARGB(255, 20, 24, 34));
                continue;
            }
            wx++;
        }
        for (i = 0; i < 4; i++)
            bk_disc((float)(px + 260 + i * 18), (float)(gy + 20), 5.0f, i < k->ratchet ? col : ARGB(40, 255, 255, 255));
        bk_text(px + 252, gy + 34, &FONT_T, 1, "REPEATS", BK_FAINT);
        if (vo_listen_track == t)
            bk_text(px + 252, gy + 56, &FONT_T, 1, "LISTENING", col);
    }
    /* all six tracks: their sound at a glance */
    {
        int px = x + 364, py = y + 486;
        bk_rrect(px, py, 350, 212, 12, ARGB(12, 255, 255, 255));
        bk_text(px + 12, py + 8, &FONT_T, 1, "ALL TRACKS", BK_DIM);
        bk_text(px + 338 - bk_text_w(&FONT_T, "L1/R1", 1), py + 8, &FONT_T, 1, "L1/R1", BK_FAINT);
        for (i = 0; i < 6; i++) {
            int ry = py + 32 + i * 29, on = i < n, foc = i == t, pr = p->tone_preset[i];
            uint32_t c = on && !p->track[i].mute ? bk_track_col(i) : BK_FAINT;
            float l = lab_stem_level[i] * 3.0f;
            if (foc)
                bk_rrect(px + 6, ry - 4, 338, 26, 6, BK_ALPHA(bk_track_col(i), 50));
            snprintf(buf, sizeof buf, "%d", i + 1);
            bk_text(px + 14, ry, &FONT_T, 1, buf, c);
            if (!on)
                continue;
            snprintf(buf, sizeof buf, "%.14s", p->kit.enabled && i == 3 ? "KIT" : md_model_name(p->track[i].model));
            bk_text(px + 32, ry, &FONT_T, 1, buf, foc ? BK_INK : BK_DIM);
            snprintf(buf, sizeof buf, "%.18s", pr < 0 ? "OWN SOUND" : md_tone_name(pr));
            bk_text(px + 160, ry, &FONT_T, 1, buf, BK_FAINT);
            bk_rrect(px + 300, ry + 4, (int)(36.0f * (l > 1 ? 1 : l)) + 1, 6, 3, c);
        }
    }
    (void)tm;
}

/* ---- SPACE: the shared effects, drawn ---- */
static void sp_text(int f, char *buf, size_t n)
{
    int v = sp_value(f);
    switch (f) {
    case SF2_ECHO_TIME: snprintf(buf, n, "%d MS", v); break;
    case SF2_PITCH: snprintf(buf, n, "%+d ST", v); break;
    case SF2_REVERSE: snprintf(buf, n, v ? "REVERSE" : "FORWARD"); break;
    case SF2_FREEZE: snprintf(buf, n, v ? "FROZEN" : "OFF"); break;
    case SF2_DUCK_FROM: if (v < 0) snprintf(buf, n, "OFF"); else snprintf(buf, n, "TRACK %d", v + 1); break;
    case SF2_DUCK: snprintf(buf, n, "%d %%", v * 90 / 1000); break;
    default: snprintf(buf, n, "%d %%", v / 10);
    }
}

/* a panel 350 x 320: title, picture area x+10, y+30, 330 x 190, four fields from y+232 */
static void sp_panel(int x, int y, const char *title, const char *aside, int first, uint32_t col)
{
    char buf[40];
    int focus = sp_field / 4 == first / 4;
    bk_rrect(x, y, 350, 320, 12, focus ? BK_ALPHA(col, 26) : ARGB(12, 255, 255, 255));
    if (focus)
        bk_rframe(x, y, 350, 320, 12, 2, BK_ALPHA(col, 200));
    bk_text(x + 12, y + 8, &FONT_T, 1, title, focus ? col : BK_DIM);
    if (aside)
        bk_text(x + 338 - bk_text_w(&FONT_T, aside, 1), y + 8, &FONT_T, 1, aside, BK_FAINT);
    for (int r = 0; r < 4; r++) {
        int f = first + r, ry = y + 224 + r * 23, foc = f == sp_field;
        if (foc)
            bk_rrect(x + 6, ry - 2, 338, 23, 6, BK_ALPHA(col, 70));
        bk_text(x + 14, ry, &FONT_T, 1, SP_F[f].name, foc ? BK_INK : BK_DIM);
        sp_text(f, buf, sizeof buf);
        bk_text(x + 336 - bk_text_w(&FONT_T, buf, 1), ry, &FONT_T, 1, buf, foc ? BK_INK : BK_DIM);
    }
}

static void bk_draw_space(float tm)
{
    const md_patch *p = md_state();
    uint32_t col = ARGB(255, 140, 200, 230), ice = ARGB(255, 190, 235, 255), moss = ARGB(255, 120, 200, 140);
    int x = BK_SX, y = BK_SY, i;
    float ret = (float)p->fx_level / 1000.0f, beat = 60.0f / (float)(md_tempo_now() > 20 ? md_tempo_now() : 20);
    char buf[48];
    bk_view_frame("SPACE", col, "UP/DN FIELD  L/R VALUE  X PANEL  A FREEZE");
    bk_text(x + 140, y + 18, &FONT_T, 1, p->freeze ? "EVERYTHING HELD STILL, FROZEN" : "FOUR PLACES THE SOUND GOES TO", p->freeze ? ice : BK_DIM);

    /* THE ECHO CAVE: a drop falls into the cave's pool; its echoes ring out, one after the other (ECHO TIME apart),
       each fainter by FEEDBACK; the cave's mist is the ROOM, thicker with its tail */
    {
        int px = x + 8, py = y + 50, gx = px + 10, gy = py + 30;
        float fb = (float)p->delay_fb / 1000.0f, dsec = (float)p->delay_ms / 1000.0f, t60 = 0.4f + (float)p->reverb_time / 850.0f * 7.6f;
        float cyc = fmaxf(2.4f, dsec * 6.0f), ph = fmodf(tm, cyc), pcx = (float)(gx + 165), pcy = (float)(gy + 150);
        sp_panel(px, py, "THE ECHO CAVE", "ECHO + ROOM", SF2_ECHO_TIME, col);
        bk_rrect(gx, gy, 330, 190, 14, ARGB(255, 44, 48, 58));                  /* the rock */
        for (i = 0; i < 40; i++) {                /* its grain */
            uint32_t h = (uint32_t)(i + 3) * 2654435761u;
            bk_disc((float)(gx + 8 + (int)(h % 314u)), (float)(gy + 6 + (int)((h >> 9) % 60u)), 2.0f + (float)(h >> 28), ARGB(60, 90, 96, 110));
        }
        for (int yy = gy + 50; yy < gy + 188; yy++) {   /* the cave's mouth, an arch, darker the deeper (kept in the picture) */
            float dy = (float)(yy - (gy + 200)), hw2 = 150.0f * 150.0f - dy * dy;
            if (hw2 <= 0)
                continue;
            float hw = sqrtf(hw2), deep = (float)(yy - (gy + 50)) / 138.0f;
            bk_span(gx + 165 - (int)hw, gx + 165 + (int)hw, yy, ARGB(255, (uint32_t)(22 - 12 * deep), (uint32_t)(25 - 12 * deep), (uint32_t)(34 - 14 * deep)));
        }
        for (i = 0; i < 6; i++) {                 /* stalactites in its roof */
            float sx = (float)(gx + 70 + i * 38), sl = 16.0f + (float)((i * 37) % 22), top = (float)(gy + 200) - sqrtf(fmaxf(150.0f * 150.0f - (sx - (float)(gx + 165)) * (sx - (float)(gx + 165)), 0)) - 2;
            for (int d = 0; d < 6; d++)
                bk_disc(sx, top + sl * (float)d / 6.0f, 6.0f * (1.0f - (float)d / 6.0f) + 1, ARGB(255, 62, 66, 78));
            if (fmodf(tm * 0.5f + (float)i * 0.3f, 1.0f) < 0.1f)   /* a drip glinting */
                bk_disc(sx, top + sl + 3, 2, BK_INK);
        }
        for (i = 0; i < 5; i++) {                 /* the room's mist, more with a longer tail and more return */
            float mx = (float)(gx + 30 + i * 70) + 10.0f * sinf(tm * 0.3f + (float)i);
            bk_disc(mx, (float)(gy + 120) + 6.0f * sinf(tm * 0.4f + (float)i * 2.0f), 46, BK_ALPHA(col, (uint32_t)(4 + 16.0f * ret * fminf(t60 / 4.0f, 1.0f))));
        }
        for (int k = 0; k < 8; k++) {             /* the pool's rings: the drop, then its echoes */
            float start = (float)k * dsec + 0.35f, age = ph - start, a = k ? ret * powf(fb, (float)(k - 1)) : 1.0f;
            if (age < 0 || age > 1.6f || a < 0.02f)
                continue;
            for (int q = 0; q <= 32; q++) {
                float an = (float)q * 0.196f, r = 10.0f + age * 90.0f;
                bk_disc(pcx + cosf(an) * r, pcy + sinf(an) * r * 0.25f, 1.6f, BK_ALPHA(k ? col : BK_INK, (uint32_t)(255.0f * a * (1.0f - age / 1.6f))));
            }
        }
        if (ph < 0.35f)                           /* the drop falling */
            bk_disc(pcx, (float)(gy + 20) + ph / 0.35f * 130.0f, 4, BK_INK);
        bk_line((float)(gx + 20), pcy, (float)(gx + 310), pcy, 1.0f, ARGB(50, 200, 230, 255));   /* (the water's line) */
    }
    /* THE SPORE MEADOW: the memory of the last seconds as moss along a log; where POSITION reads a mushroom stands,
       as wide as SIZE; the grains playing now are spores, rising from where they read; a glow-worm writes */
    {
        int px = x + 364, py = y + 50, gx = px + 10, gy = py + 30;
        uint8_t pos[4], win[4];
        uint32_t tc = p->freeze ? ice : moss;
        float rp = (float)p->grain_pos / 1000.0f, rs = 0.04f + 0.5f * (float)p->grain_size / 1000.0f;
        static float spx[24], spy[24], spl[24];
        static int spn;
        sp_panel(px, py, "THE SPORE MEADOW", p->freeze ? "FROZEN" : "GRAINS", SF2_SPACE, col);
        md_grain_positions(pos, win);
        bk_rrect(gx, gy + 128, 330, 26, 12, ARGB(255, 46, 36, 30));            /* the log */
        for (i = 0; i < 330; i += 3) {           /* the moss on it: what the memory holds */
            float a = 0.25f + 0.2f * sinf((float)i * 0.11f) * sinf((float)i * 0.027f + 1.3f) + 0.12f * sinf((float)i * 0.6f);
            a = fabsf(a) * (0.4f + 0.6f * (float)p->macro[3] / 1000.0f);
            bk_line((float)(gx + i), (float)(gy + 128), (float)(gx + i), (float)(gy + 128) - a * 46.0f, 2.2f, BK_ALPHA(tc, 120));
        }
        {   /* the mushroom: its stem where POSITION reads, its cap SIZE wide */
            float ww = rs * 330.0f, cx = (float)gx + rp * (330.0f - ww) + ww / 2, ch = 70.0f;
            bk_line(cx, (float)(gy + 128), cx, (float)(gy + 128) - ch, 6.0f, ARGB(230, 230, 220, 200));
            for (int c2 = -6; c2 <= 6; c2++)
                bk_disc(cx + (float)c2 * ww / 13.0f, (float)(gy + 128) - ch - (36.0f - (float)(c2 * c2)) * 0.5f, fmaxf(ww / 11.0f, 5.0f), BK_ALPHA(tc, 200));
            bk_disc(cx - ww * 0.2f, (float)(gy + 128) - ch - 12, 3, ARGB(150, 255, 255, 255));
        }
        for (i = 0; i < 4; i++)                   /* each grain playing: a spore leaves from its place */
            if (win[i] && spn < 24 && ((int)(tm * 20.0f) + i) % 3 == 0) {
                int s2 = spn++;
                spx[s2] = (float)gx + (float)pos[i] / 128.0f * 330.0f, spy[s2] = (float)(gy + 110), spl[s2] = 0;
            }
        for (i = 0; i < spn; i++) {
            spl[i] += 0.03f, spy[i] -= 1.6f, spx[i] += 0.5f * sinf(spl[i] * 6.0f + (float)i);
            bk_disc(spx[i], spy[i], 2.5f, BK_ALPHA(BK_INK, bk_u8(220.0f * (1.0f - spl[i]))));
            if (spl[i] >= 1.0f)
                spx[i] = spx[--spn], spy[i] = spy[spn], spl[i] = spl[spn], i--;
        }
        if (!p->freeze) {                         /* the glow-worm, writing along the log */
            float hx = (float)gx + fmodf(tm * 0.25f, 1.0f) * 330.0f;
            bk_disc(hx, (float)(gy + 141), 9, BK_ALPHA(ARGB(255, 210, 255, 150), 60));
            bk_disc(hx, (float)(gy + 141), 4, ARGB(255, 220, 255, 170));
        }
    }
    /* THE STREAM: the sound's tape as water; PITCH its speed (backwards when reversed), FREEZE turns it to ice;
       leaves ride on it; NOISE: bubbles */
    {
        int px = x + 8, py = y + 378, gx = px + 10, gy = py + 30;
        float speed = p->freeze ? 0 : powf(2.0f, (float)p->grain_pitch / 12.0f) * (p->reverse ? -1.0f : 1.0f);
        static float flow, last;
        float dt = tm - last;
        uint32_t wc = p->freeze ? ice : ARGB(255, 90, 160, 210);
        last = tm;
        if (dt > 0 && dt < 0.5f)
            flow += dt * speed * 60.0f;
        sp_panel(px, py, "THE STREAM", p->freeze ? "ICE" : p->reverse ? "<  BACKWARDS" : "FLOWING  >", SF2_PITCH, col);
        for (i = 0; i < 4; i++) {                 /* banks and water in bands, rippling with the flow */
            int by = gy + 50 + i * 26;
            for (int xx = 0; xx < 330; xx += 6) {
                float wv = sinf(((float)xx - flow * (1.0f + 0.2f * (float)i)) * 0.06f + (float)i);
                bk_disc((float)(gx + xx), (float)by + wv * 4.0f, 7.0f, BK_ALPHA(wc, (uint32_t)(p->freeze ? 90 : 40 + 15 * i)));
            }
        }
        for (i = 0; i < 6; i++) {                 /* leaves riding on it */
            float lx = fmodf((float)i * 63.0f + flow * 1.2f, 330.0f);
            if (lx < 0)
                lx += 330.0f;
            float ly = (float)(gy + 64 + (i % 3) * 26) + 3.0f * sinf(tm * 2.0f + (float)i);
            bk_disc((float)gx + lx, ly, 5.0f, ARGB(255, 200, 150, 80));
            bk_disc((float)gx + lx + 4.0f, ly - 2.0f, 3.5f, ARGB(255, 220, 170, 90));
        }
        for (i = 0; i < p->grain_noise / 25; i++) {     /* bubbles */
            float life = fmodf(tm * 0.6f + (float)i * 0.37f, 1.0f);
            bk_arc((float)(gx + (i * 53) % 320 + 5), (float)(gy + 150) - life * 100.0f, 3.0f, 1.2f, 0, 6.3f, BK_ALPHA(BK_INK, (uint32_t)(160.0f * (1.0f - life))));
        }
        snprintf(buf, sizeof buf, "%+d ST", p->grain_pitch);
        bk_text(gx + 165 - bk_text_w(&FONT_S, buf, 1) / 2, gy + 160, &FONT_S, 1, buf, p->freeze ? ice : col);
    }
    /* REEDS IN THE WIND: every hit of the source track is a gust; the others' reeds bow by DUCK AMOUNT and rise again
       over RELEASE; SWING: stepping stones, every second one a little late */
    {
        int px = x + 364, py = y + 378, gx = px + 10, gy = py + 30, src = p->duck_source;
        float amt = (float)p->duck_amount * 0.9f / 1000.0f, rel = 0.03f + (float)p->duck_release / 1000.0f * 0.6f;
        float sw = (float)p->swing / 1000.0f, since = fmodf(tm, beat) / beat, lvl = src >= 0 ? 1.0f - amt * expf(-since / rel) : 1.0f;
        uint32_t sc = src >= 0 ? bk_track_col(src) : BK_FAINT;
        sp_panel(px, py, "REEDS IN THE WIND", src >= 0 ? "DUCKING" : "NO WIND (OFF)", SF2_DUCK_FROM, col);
        if (src >= 0 && since < 0.15f)            /* the gust */
            for (i = 0; i < 6; i++)
                bk_line((float)(gx + 10), (float)(gy + 30 + i * 14), (float)(gx + 10) + 200.0f * since / 0.15f, (float)(gy + 30 + i * 14), 1.5f,
                        BK_ALPHA(sc, (uint32_t)(160.0f * (1.0f - since / 0.15f))));
        for (i = 0; i < 9; i++) {                 /* the reeds, bowing with the duck */
            float rx = (float)(gx + 30 + i * 34), lean = (1.0f - lvl) * 60.0f + 4.0f * sinf(tm * 1.3f + (float)i), hh = 100.0f + (float)((i * 17) % 30);
            float px0 = rx, py0 = (float)(gy + 140);
            uint32_t rc = bk_track_col(i % 6);
            for (int sgm = 1; sgm <= 8; sgm++) {
                float f = (float)sgm / 8.0f, nx = rx + lean * f * f, ny = (float)(gy + 140) - hh * f * (1.0f - 0.25f * (1.0f - lvl) * f);
                bk_line(px0, py0, nx, ny, 3.0f * (1.0f - 0.6f * f), BK_ALPHA(rc, 200));
                px0 = nx, py0 = ny;
            }
            bk_disc(px0, py0 + 4, 4.0f, BK_ALPHA(gd_mix(rc, ARGB(255, 120, 80, 50), 0.5f), 230));
        }
        for (i = 0; i < 8; i++) {                 /* stepping stones: every second one late by SWING */
            float sx = (float)gx + 22.0f + (float)i * 40.0f + ((i & 1) ? sw * 40.0f : 0);
            bk_disc(sx, (float)(gy + 168), (i & 1) ? 7.0f : 9.0f, (i & 1) ? BK_ALPHA(col, 150) : col);
        }
    }
}

/* ---- ARCHIVE: the catalogue's baskets and poems, what is fetched, the fetch running ---- */
/* an open book in the Archive's free space: pages of parchment curving up from the spine, lines of writing; while
   a poem speaks, its letters rise out of it as little lights and a page turns now and then */
static void ar_book(float cx, float cy, float t, int speaking, const char *title, uint32_t col)
{
    int k;
    float lift = speaking ? 1.0f : 0.0f;
    bk_disc(cx, cy + 62, 150, ARGB(16, 236, 200, 150));            /* its glow on the table of night */
    for (k = -130; k <= 130; k += 2) {           /* the pages, a column at a time */
        float u = fabsf((float)k) / 130.0f, top = cy - 46.0f + 10.0f * sqrtf(u) * -1.0f + 14.0f * u * u, bot = cy + 52.0f - 6.0f * u;
        uint32_t c = gd_mix(ARGB(255, 226, 210, 176), ARGB(255, 120, 100, 76), (1.0f - u) * (1.0f - u) * 0.6f);
        bk_line(cx + (float)k, top, cx + (float)k, bot, 2.4f, BK_ALPHA(c, 235));
    }
    bk_line(cx, cy - 46, cx, cy + 52, 2.0f, ARGB(200, 90, 70, 52));   /* the spine */
    for (k = 0; k < 7; k++)                      /* lines of writing on each page */
        for (int sd = -1; sd <= 1; sd += 2) {
            float ly = cy - 26.0f + (float)k * 10.0f, x0 = cx + (float)sd * 18.0f, x1 = cx + (float)sd * (100.0f - (float)((k * 37) % 30));
            bk_line(x0, ly + 4.0f * (fabsf(x1 - cx) / 130.0f), x1, ly + 6.0f * (fabsf(x1 - cx) / 130.0f), 1.4f, ARGB(110, 70, 56, 40));
        }
    bk_line(cx - 136, cy + 54, cx + 136, cy + 54, 3.0f, ARGB(220, 70, 50, 36));   /* its cover's edge */
    if (speaking) {
        float ph = fmodf(t * 0.25f, 1.0f);       /* a page turning: an arc from right to left, now and then */
        if (ph < 0.3f) {
            float a = ph / 0.3f * 3.14159f;
            float px = cx + cosf(a) * 120.0f, py = cy - 40.0f - sinf(a) * 46.0f;
            bk_line(cx, cy - 46, px, py, 2.4f, ARGB(200, 236, 222, 190));
            bk_line(cx, cy + 50, px, py + 90.0f * (0.6f + 0.4f * fabsf(cosf(a))), 2.4f, ARGB(160, 236, 222, 190));
        }
        for (k = 0; k < 14; k++) {               /* letters rising as lights */
            float u = fmodf(t * 0.22f + (float)k / 14.0f, 1.0f), lx = cx + sinf((float)k * 2.4f + t * 0.5f) * (40.0f + 50.0f * u);
            char ch[2] = {title[k % (strlen(title) ? strlen(title) : 1)], 0};
            if (ch[0] == ' ' || !ch[0])
                ch[0] = '*';
            bk_text((int)lx, (int)(cy - 50.0f - u * 120.0f), &FONT_S, 1, ch, BK_ALPHA(col, (uint32_t)(230.0f * (1.0f - u) * lift)));
        }
    } else {                                     /* resting: a ribbon, swaying a little */
        float sw = sinf(t * 0.8f) * 4.0f;
        bk_line(cx + 6, cy + 50, cx + 12 + sw, cy + 86, 4.0f, BK_ALPHA(col, 200));
    }
}

/* the Archive's SAMPLER: the poem as a river of its sound (its peaks), where the grains are taken as lights, the
   reading place as a lantern on a stem, the tide drawn over one cycle with a firefly where it is now, the modules
   as ponds that fill with the voice, and the rows to turn below */
static void ar_draw_sampler(float t, uint32_t col)
{
    const vo_file *vf = vo_cur;
    int x = BK_SX, y = BK_SY, i, mode = clamp((int)lab_v[P_VO_MODE], 0, 4);
    char buf[96];
    float wx = (float)(x + 230), ww = 470.0f, wy = (float)(y + 150), react = 0;
    if (ar_row_at && bk_ms() - ar_row_at < 600u)
        react = 1.0f - (float)(bk_ms() - ar_row_at) / 600.0f;
    {   /* the sections at the left, the one we are in lit */
        static const char *const secs[4] = {"VOICE", "GRAINS", "TIDE", "MODULES"};
        static const char *const what[4] = {"HOW IT SPEAKS", "A GRANULAR SAMPLER", "MUTABLE INSTR. TIDES 2", "RINGS, CLOUDS, BEADS"};
        for (i = 0; i < 4; i++) {
            int by = y + 56 + i * 64, act = !strcmp(AR_ROWS[ar_row].sect, secs[i]);
            bk_rrect(x + 10, by, 196, 56, 12, act ? BK_ALPHA(col, 200) : ARGB(16, 255, 255, 255));
            bk_text(x + 22, by + 8, &FONT_B, 1, secs[i], act ? ARGB(255, 20, 16, 10) : BK_INK);
            bk_text(x + 22, by + 32, &FONT_T, 1, what[i], act ? ARGB(255, 60, 44, 30) : BK_DIM);
        }
        bk_text(x + 18, y + 330, &FONT_T, 1, "L1 / R1  SECTION", BK_DIM);
        bk_text(x + 18, y + 348, &FONT_T, 1, "UP / DOWN  ROW", BK_DIM);
        bk_text(x + 18, y + 366, &FONT_T, 1, "LEFT / RIGHT  TURN", BK_DIM);
        bk_text(x + 18, y + 384, &FONT_T, 1, "A  SPEAK / SILENCE", BK_DIM);
        bk_text(x + 18, y + 402, &FONT_T, 1, "Y  GRAINS ON / OFF", BK_DIM);
        bk_text(x + 18, y + 420, &FONT_T, 1, "X OR B  BACK TO POEMS", BK_DIM);
    }
    bk_text_fit(x + 230, y + 56, &FONT_B, vf && vf->ok ? (vf->title[0] ? vf->title : "THE VOICE") : "NO POEM IN VOICES YET", 470, col);
    snprintf(buf, sizeof buf, "%s  /  %s", VC_MODES[mode], lab_v[P_VO_LEVEL] > 0 ? "SPEAKING" : "SILENT (A)");
    bk_text(x + 230, y + 84, &FONT_T, 1, buf, lab_v[P_VO_LEVEL] > 0 ? BK_INK : BK_DIM);
    /* the river: the poem's peaks, mirrored, flowing */
    bk_rrect((int)wx - 10, (int)wy - 44, (int)ww + 20, 92, 20, ARGB(40, 20, 40, 52));
    if (vf && vf->ok) {
        for (i = 0; i < 118; i++) {
            int pk = i * VO_PEAKS / 118;
            float a = fmaxf(fabsf(vf->peak_min[pk]), fabsf(vf->peak_max[pk])) * vf->gain * 70.0f, xx = wx + (float)i * ww / 118.0f;
            float sh = 1.0f + 0.15f * sinf(t * 2.0f + (float)i * 0.3f);
            a = fminf(a * sh, 38.0f) + 1.0f;
            bk_line(xx, wy - a, xx, wy + a, 2.2f, BK_ALPHA(col, 70 + (uint32_t)(a * 3)));
        }
        if (mode == 4) {
            float cx = wx + ww * vo_centre, sw = lab_v[P_VO_SPRAY] / 100 * 44100.0f / (float)vf->frames * ww;
            bk_rrect((int)(cx - sw), (int)wy - 40, (int)(2 * sw) + 2, 80, 10, BK_ALPHA(col, 26));   /* the spray */
            bk_line(cx, wy + 40, cx, wy - 34, 2.5f, BK_ALPHA(col, 200));                          /* the lantern on its stem */
            bk_disc(cx, wy - 40, 7.0f + 4.0f * react + 1.5f * sinf(t * 3.0f), col);
            bk_disc(cx, wy - 40, 15.0f, BK_ALPHA(col, 40));
            for (i = 0; i < VO_GRAINS; i++)           /* the grains, as lights, larger while they are loud */
                if (vo_g[i].len) {
                    float u = (float)vo_g[i].age / (float)vo_g[i].len, gx = wx + ww * (float)(vo_g[i].pos / (double)vf->frames);
                    float gy = wy + ((float)((i * 37) % 60) - 30.0f) * 0.9f, w = sinf(u * 3.14159f);
                    bk_disc(gx, gy, 3.0f + 7.0f * w, BK_ALPHA(BK_INK, (uint32_t)(60 + 180 * w)));
                    bk_disc(gx, gy, 9.0f + 10.0f * w, BK_ALPHA(col, (uint32_t)(50 * w)));
                }
        } else
            for (i = 0; i < VO_VOICES; i++)           /* the reading places of the speaking voices */
                if (vo_v[i].on) {
                    float px = wx + ww * (float)(vo_v[i].pos / (double)vf->frames);
                    bk_line(px, wy - 40, px, wy + 40, 2.5f, BK_INK);
                    bk_disc(px, wy - 42, 5, col);
                }
    } else
        bk_text_c((int)(wx + ww / 2), (int)wy - 10, &FONT_S, 1, "A ON A POEM HERE: IT GOES INTO VOICES", BK_DIM);
    {   /* the tide over one cycle, the firefly where it is */
        const float *pts = vo_tide_points();      /* (Tides 2 itself, one cycle as it is set) */
        float ty = (float)(y + 262), th = 50, d = lab_v[P_VO_TIDE_DEPTH] / 100, ph = vo_tide_ph;
        float px = wx, py = ty + th / 2 - pts[0] * th;
        bk_text(x + 230, y + 206, &FONT_T, 1, "TIDE (MI TIDES 2)", d > 0 ? col : BK_FAINT);
        snprintf(buf, sizeof buf, "%s  %d %%", VO_TIDE_TO[(int)lab_v[P_VO_TIDE_TO] % 6], (int)lab_v[P_VO_TIDE_DEPTH]);
        bk_text(x + 700 - bk_text_w(&FONT_T, buf, 1), y + 206, &FONT_T, 1, buf, d > 0 ? BK_INK : BK_FAINT);
        for (i = 1; i < 128; i++) {               /* two cycles */
            float nx = wx + ww * (float)i / 127.0f, ny = ty + th / 2 - pts[i % 64] * th;
            bk_line(px, py, nx, ny, 2.2f, BK_ALPHA(col, d > 0 ? 200 : 70));
            px = nx, py = ny;
        }
        {   /* the firefly: where the tide is now, at its true height */
            float fx = wx + ww * ph * 0.5f, fy = ty + th / 2 - vo_tide * th;
            bk_disc(fx, fy, 12, BK_ALPHA(col, 50));
            bk_disc(fx, fy, 5, BK_INK);
        }
    }
    {   /* the modules: ponds that fill with the voice */
        static const char *const nm[6] = {"CLOUD", "CLOUDS", "BEADS", "SHIMMER", "RINGS", "ECHO"};
        float amt[6];
        int rack = lab_v[P_RA_IN_VOICES] > 0.5f;
        amt[0] = lab_v[P_CL_MIX] > 0 ? lab_v[P_VO_TO_CL] / 100 : 0;
        amt[1] = lab_v[P_PA_MIX] > 0 ? lab_v[P_VO_TO_PA] / 100 : 0;
        amt[2] = lab_v[P_BE_MIX] > 0 ? lab_v[P_VO_TO_BE] / 100 : 0;
        amt[3] = lab_v[P_SH_MIX] > 0 ? lab_v[P_VO_TO_SH] / 100 : 0;
        amt[4] = rack ? lab_v[P_RA_RES_MIX] / 100 : 0;
        amt[5] = rack ? lab_v[P_RA_ECHO_MIX] / 100 : 0;
        for (i = 0; i < 6; i++) {
            float cx = wx + 38 + (float)i * (ww - 76) / 5.0f, cy = (float)(y + 330), r = 20.0f;
            float lv = amt[i] * (0.4f + 0.6f * fminf(vo_env, 1.0f));
            bk_disc(cx, cy, r + 4, ARGB(60, 10, 30, 40));
            if (amt[i] > 0) {
                float rp = fmodf(t * 0.7f + (float)i * 0.3f, 1.0f);
                bk_disc(cx, cy, r * (0.35f + 0.65f * amt[i]), BK_ALPHA(col, 60 + (uint32_t)(150 * lv)));
                bk_arc(cx, cy, r * (0.5f + rp), 1.6f, 0, 6.3f, BK_ALPHA(col, (uint32_t)(140 * (1.0f - rp) * lv + 20)));
            }
            bk_arc(cx, cy, r + 4, 1.4f, 0, 6.3f, amt[i] > 0 ? col : ARGB(60, 255, 255, 255));
            bk_text_c((int)cx, (int)cy + 28, &FONT_T, 1, nm[i], amt[i] > 0 ? BK_INK : BK_FAINT);
        }
    }
    {   /* the rows: eight around the chosen one */
        int top = ar_row - 3, ry;
        if (top > AR_NROWS - 7) top = AR_NROWS - 7;
        if (top < 0) top = 0;
        for (i = top; i < top + 7 && i < AR_NROWS; i++) {
            const ar_srow *r = &AR_ROWS[i];
            int p = r->p, sel = i == ar_row;
            float f = (lab_v[p] - LAB_P[p].lo) / (LAB_P[p].hi - LAB_P[p].lo);
            ry = y + 390 + (i - top) * 40;
            if (sel) {
                bk_rrect(x + 222, ry - 4, 484, 40, 10, BK_ALPHA(col, 40 + (int)(40 * react)));
                bk_rframe(x + 222, ry - 4, 484, 40, 10, 2, col);
            }
            if (i == top || strcmp(r->sect, AR_ROWS[i - 1].sect))
                bk_text(x + 232, ry - 1, &FONT_T, 1, r->sect, BK_ALPHA(col, 200));
            bk_text_fit(x + 300, ry + 1, &FONT_S, r->name, 250, sel ? BK_INK : BK_DIM);
            lab_format(p, buf, sizeof buf);
            bk_text(x + 694 - bk_text_w(&FONT_S, buf, 1), ry + 1, &FONT_S, 1, buf, sel ? BK_INK : BK_DIM);
            if (LAB_P[p].unit == U_BIPCT || LAB_P[p].unit == U_SEMI) {
                float z = (0 - LAB_P[p].lo) / (LAB_P[p].hi - LAB_P[p].lo);
                bk_vine_bar(x + 300, ry + 28, 394, z, f, col, sel, t, i);
            } else if (LAB_P[p].unit != U_ENUM)
                bk_vine_bar(x + 300, ry + 28, 394, 0, f, col, sel, t, i);
        }
    }
}

static void bk_draw_archive(float t)
{
    uint32_t col = ARGB(255, 236, 200, 150);
    int x = BK_SX, y = BK_SY, i, idx[AR_MAX], n, rows = 13, top;
    char buf[160];
    if (ar_n < 0)
        ar_load();
    bk_view_frame(ar_smp ? "SAMPLER" : "ARCHIVE", col, "L1/R1 BASKET  UP/DN POEM  A FETCH / USE  X STOP");
    if (ar_smp) {
        ar_draw_sampler(t, col);
        return;
    }
    if (!ar_n) {
        bk_text(x + 30, y + 80, &FONT_B, 1, "No catalogue yet", col);
        bk_text(x + 30, y + 112, &FONT_S, 1, "MUTABLEDREAMS/ARCHIVE/CATALOGUE.TSV IS MISSING", BK_DIM);
        return;
    }
    for (i = 0; i < ar_nbaskets; i++) {          /* the baskets */
        int by = y + 56 + i * 64, act = i == ar_basket, k, have = 0, cnt = ar_list(i, idx);
        char nm[40];
        for (k = 0; k < cnt; k++)
            have += ar_e[idx[k]].have;
        bk_rrect(x + 10, by, 196, 56, 12, act ? BK_ALPHA(col, 200) : ARGB(16, 255, 255, 255));
        bk_title(ar_baskets[i], nm, sizeof nm);
        bk_text(x + 22, by + 8, &FONT_B, 1, nm, act ? ARGB(255, 20, 16, 10) : BK_INK);
        snprintf(buf, sizeof buf, "%d POEM%s  /  %d HERE", cnt, cnt == 1 ? "" : "S", have);
        bk_text(x + 22, by + 32, &FONT_T, 1, buf, act ? ARGB(255, 60, 44, 30) : BK_DIM);
    }
    {   /* the rule, in short */
        int ry = y + 56 + ar_nbaskets * 64 + 16;
        bk_text(x + 18, ry, &FONT_T, 1, "ONLY FREE VOICES:", BK_DIM);
        bk_text(x + 18, ry + 16, &FONT_T, 1, "LIBRIVOX ON ARCHIVE.ORG,", BK_FAINT);
        bk_text(x + 18, ry + 32, &FONT_T, 1, "AUTHORS DEAD 70+ YEARS,", BK_FAINT);
        bk_text(x + 18, ry + 48, &FONT_T, 1, "PUBLIC DOMAIN / CC0 / CC-BY", BK_FAINT);
        bk_text(x + 18, ry + 76, &FONT_T, 1, "FETCHED POEMS GO TO", BK_DIM);
        bk_text(x + 18, ry + 92, &FONT_T, 1, "SAMPLES/ARCHIVE/ WITH", BK_FAINT);
        bk_text(x + 18, ry + 108, &FONT_T, 1, "THEIR CREDIT (.TXT)", BK_FAINT);
    }
    n = ar_list(ar_basket, idx);
    top = ar_top[ar_basket];
    if (ar_pos[ar_basket] < top)
        top = ar_pos[ar_basket];
    if (ar_pos[ar_basket] >= top + rows)
        top = ar_pos[ar_basket] - rows + 1;
    ar_top[ar_basket] = top;
    for (i = top; i < n && i < top + rows; i++) {   /* the poems */
        const ar_entry *e = &ar_e[idx[i]];
        int ry = y + 56 + (i - top) * 40, foc = i == ar_pos[ar_basket], fetching = idx[i] == ar_fetching && ar_pid;
        bk_rrect(x + 218, ry, 494, 36, 10, foc ? BK_ALPHA(col, 60) : ARGB(10, 255, 255, 255));
        if (foc)
            bk_rframe(x + 218, ry, 494, 36, 10, 2, BK_ALPHA(col, 200));
        if (e->have)                              /* fetched: a seedling; else a seed */
            gd_seedling((float)(x + 234), (float)(ry + 16), col);
        else
            bk_disc((float)(x + 234), (float)(ry + 18), fetching ? 5.0f + 2.0f * sinf(t * 8.0f) : 3.0f, fetching ? col : BK_FAINT);
        bk_text_fit(x + 250, ry + 3, &FONT_S, e->title, 300, foc ? BK_INK : BK_DIM);
        snprintf(buf, sizeof buf, "%s  %s-%s", e->author, e->born, e->died);
        bk_text_fit(x + 250, ry + 21, &FONT_T, buf, 300, foc ? BK_DIM : BK_FAINT);
        snprintf(buf, sizeof buf, "%d:%02d", e->secs / 60, e->secs % 60);
        bk_text(x + 700 - bk_text_w(&FONT_S, buf, 1), ry + 3, &FONT_S, 1, buf, foc ? BK_INK : BK_DIM);
        snprintf(buf, sizeof buf, "%d KB", e->bytes / 1024);
        bk_text(x + 700 - bk_text_w(&FONT_T, buf, 1), ry + 21, &FONT_T, 1, buf, BK_FAINT);
    }
    if (n > rows) {                               /* where in the list */
        float a = (float)top / (float)(n - rows > 0 ? n - rows : 1);
        bk_rrect(x + 714, y + 56 + (int)(a * (float)(rows * 40 - 60)), 4, 60, 2, BK_ALPHA(col, 160));
    }
    if (n && y + 56 + (n < rows ? n : rows) * 40 + 190 <= y + 580) {   /* room under the list: the book */
        const ar_entry *e = &ar_e[idx[ar_pos[ar_basket]]];
        float fy = (float)(y + 56 + n * 40 + (580 - 56 - n * 40) / 2);
        ar_book((float)(x + 465), fy, t, e->have && ar_playing((int)(e - ar_e)), e->title, col);
    }
    if (n) {                                      /* the chosen poem: its credit, the fetch */
        const ar_entry *e = &ar_e[idx[ar_pos[ar_basket]]];
        int dy = y + 586;
        bk_rrect(x + 218, dy, 494, 120, 12, ARGB(18, 255, 255, 255));
        snprintf(buf, sizeof buf, "%s (%s-%s)", e->author, e->born, e->died);
        bk_text_fit(x + 232, dy + 8, &FONT_S, buf, 466, BK_INK);
        snprintf(buf, sizeof buf, "READ BY %s FOR LIBRIVOX  /  %s", e->reader[0] ? e->reader : "?",
                 strstr(e->licence, "/zero/") ? "CC0" : strstr(e->licence, "/by/") ? "CC-BY" : "PUBLIC DOMAIN");
        for (char *q = buf; *q; q++)
            *q = (char)toupper((unsigned char)*q);
        bk_text_fit(x + 232, dy + 28, &FONT_T, buf, 466, BK_DIM);
        snprintf(buf, sizeof buf, "ARCHIVE.ORG/DETAILS/%s", e->item);
        for (char *q = buf; *q; q++)
            *q = (char)toupper((unsigned char)*q);
        bk_text_fit(x + 232, dy + 44, &FONT_T, buf, 466, BK_FAINT);
        if (ar_pid && ar_fetching >= 0) {
            float pr = ar_progress();
            pr = pr > 1 ? 1 : pr;
            bk_rrect(x + 232, dy + 66, 466, 12, 6, ARGB(30, 255, 255, 255));
            bk_rrect(x + 232, dy + 66, (int)(466.0f * pr) + 1, 12, 6, col);
            snprintf(buf, sizeof buf, "FETCHING %s  %d %%", ar_e[ar_fetching].title, (int)(pr * 100));
            bk_text_fit(x + 232, dy + 82, &FONT_T, buf, 466, BK_INK);
        } else
        {
            int speaking = e->have && ar_playing((int)(e - ar_e)), said = ar_status[0] && bk_ms() - ar_status_at < 6000u;
            if (speaking) {                       /* speaking: its voice as a breath of bars; what the buttons do with it */
                float tt = (float)bk_ms() / 1000.0f;
                for (int k = 0; k < 18; k++) {
                    float hh = 4.0f + 12.0f * fabsf(sinf(tt * 5.0f + (float)k * 0.7f) * sinf(tt * 1.3f + (float)k));
                    bk_rrect(x + 232 + k * 7, dy + 86 - (int)hh, 4, (int)hh, 2, col);   /* (under the address line) */
                }
                bk_text(x + 372, dy + 66, &FONT_S, 1, "SPEAKING", col);
                bk_text_fit(x + 232, dy + 92, &FONT_T, said ? ar_status : "A: SILENCE   X: SAMPLER   Y: INTO THE CLOUD", 466,
                            said ? ARGB(255, 250, 220, 170) : BK_INK);
            } else if (said)                     /* the last word (FETCHED, a failure ...) in place of what A does, a while */
                bk_text_fit(x + 232, dy + 66, &FONT_S, ar_status, 466, ARGB(255, 250, 220, 170));
            else
                bk_text(x + 232, dy + 66, &FONT_S, 1, e->have ? "HERE.  A: PLAY IT" : "A: FETCH (NEEDS WIFI)", e->have ? col : BK_DIM);
        }
    }
}

/* ---- WORLDS: families, worlds, the chosen one's story and what sounds in it ---- */
static void wd_tags(const wd_world *w, char *out, size_t n)
{
    int rule = -1, eng = -1, voices = 0, poly = 0, on_logic = 0, on_synth = 0;
    for (int k = 0; k < w->nlab; k++) {
        int p = w->lab[k].p;
        if (p == P_LG_RULE) rule = (int)w->lab[k].v;
        if (p == P_LG_ON && w->lab[k].v > 0.5f) on_logic = 1;
        if (p == P_SY_ENGINE) eng = (int)w->lab[k].v;
        if (p == P_SY_LEVEL && w->lab[k].v > 0) on_synth = 1;
        if (p == P_VO_LEVEL && w->lab[k].v > 0) voices = 1;
        if (p == P_PO_LEVEL && w->lab[k].v > 0) poly = 1;
    }
    snprintf(out, n, "%s%s%s%s%s%s", on_logic ? "LOGIC: " : "", on_logic ? LG_RULE_NAMES[(rule < 0 ? 0 : rule) % LG_RULES] : "",
             on_synth ? (on_logic ? "  /  SYNTH: " : "SYNTH: ") : "", on_synth ? SY_NAMES[(eng < 0 ? 0 : eng) % SY_ENGINES] : "",
             poly ? "  /  POLY" : "", voices ? "  /  VOICES" : "");
}

static void bk_draw_worlds(float t)
{
    uint32_t col = ARGB(255, 180, 230, 200);
    int x = BK_SX, y = BK_SY, i, idx[WD_MAX], n, rows = 10, top;
    char buf[200];
    if (wd_n < 0)
        wd_load();
    bk_view_frame("WORLDS", col, "L1/R1 FAMILY  UP/DN WORLD  A ENTER  Y WANDER");
    if (!wd_n) {
        bk_text(x + 30, y + 80, &FONT_B, 1, "No worlds yet", col);
        bk_text(x + 30, y + 112, &FONT_S, 1, "MUTABLEDREAMS/WORLDS/WORLDS.TXT IS MISSING", BK_DIM);
        return;
    }
    const int fstep = wd_nfam > 13 ? 572 / wd_nfam : 44;   /* (0.79: 14 families and more close up, all on the screen) */
    for (i = 0; i < wd_nfam; i++) {              /* the families */
        int by = y + 56 + i * fstep, act = i == wd_fam, here = wd_current >= 0 && !strcmp(wd_w[wd_current].family, wd_fams[i]);
        char nm[40];
        bk_rrect(x + 10, by, 196, fstep - 6, 12, act ? BK_ALPHA(col, 200) : ARGB(16, 255, 255, 255));
        bk_title(wd_fams[i], nm, sizeof nm);
        if (bk_text_w(&FONT_S, nm, 1) <= 166)
            bk_text(x + 22, by + (fstep - 6) / 2 - 9, &FONT_S, 1, nm, act ? ARGB(255, 12, 24, 18) : BK_INK);
        else {                                   /* a long name: two smaller lines, broken at the space nearest its middle */
            int len = (int)strlen(nm), cut = -1;
            for (int k = 0; k < len; k++)
                if (nm[k] == ' ' && (cut < 0 || abs(k - len / 2) < abs(cut - len / 2)))
                    cut = k;
            if (cut > 0) {
                nm[cut] = 0;
                bk_text_fit(x + 22, by + (fstep < 44 ? 0 : 1), &FONT_T, nm, 166, act ? ARGB(255, 12, 24, 18) : BK_INK);
                bk_text_fit(x + 22, by + (fstep < 44 ? 15 : 18), &FONT_T, nm + cut + 1, 166, act ? ARGB(255, 12, 24, 18) : BK_INK);
            } else
                bk_text_fit(x + 22, by + 10, &FONT_T, nm, 166, act ? ARGB(255, 12, 24, 18) : BK_INK);
        }
        if (here)
            gd_seedling((float)(x + 194), (float)(by + (fstep - 6) / 2 - 2), act ? ARGB(255, 12, 24, 18) : col);
    }
    n = wd_list(wd_fam, idx);
    top = wd_top[wd_fam];
    if (wd_pos[wd_fam] < top)
        top = wd_pos[wd_fam];
    if (wd_pos[wd_fam] >= top + rows)
        top = wd_pos[wd_fam] - rows + 1;
    wd_top[wd_fam] = top;
    for (i = top; i < n && i < top + rows; i++) {   /* the worlds */
        const wd_world *w = &wd_w[idx[i]];
        int ry = y + 56 + (i - top) * 48, foc = i == wd_pos[wd_fam], here = idx[i] == wd_current, coming = idx[i] == wd_pending;
        bk_rrect(x + 218, ry, 494, 45, 10, foc ? BK_ALPHA(col, 60) : ARGB(10, 255, 255, 255));
        if (foc)
            bk_rframe(x + 218, ry, 494, 45, 10, 2, BK_ALPHA(col, 200));
        if (here)
            gd_flower((float)(x + 236), (float)(ry + 22), 8, 1, col, BK_INK);
        else
            bk_disc((float)(x + 236), (float)(ry + 22), coming ? 4.0f + 2.0f * sinf(t * 8.0f) : 3.0f, coming ? col : BK_FAINT);
        bk_text_fit(x + 252, ry + 1, &FONT_S, w->name, 330, foc ? BK_INK : BK_DIM);
        snprintf(buf, sizeof buf, "GROWS FROM %s", w->biome >= 0 ? md_biome_name(w->biome) : "?");
        for (char *q = buf; *q; q++)
            *q = (char)toupper((unsigned char)*q);
        bk_text_fit(x + 252, ry + 25, &FONT_T, buf, 330, foc ? BK_DIM : BK_FAINT);
        if (w->tempo > 0) {
            snprintf(buf, sizeof buf, "%d BPM", w->tempo);
            bk_text(x + 700 - bk_text_w(&FONT_T, buf, 1), ry + 14, &FONT_T, 1, buf, BK_FAINT);
        }
    }
    if (n && n < rows && (rows - n) * 48 >= 150) {   /* room under the list: the family's worlds as floating islands */
        float top_y = (float)(y + 56 + n * 48), bot_y = (float)(y + 540), cyy = (top_y + bot_y) * 0.5f - 6.0f;
        int sel = wd_pos[wd_fam];
        for (i = 0; i < n; i++) {                /* the others, small, in an arc; the chosen one large in the middle */
            float a = (float)(i - sel) * 0.55f, ix = (float)(x + 465) + sinf(a) * 200.0f, iy = cyy - 10.0f + (1.0f - cosf(a)) * 60.0f;
            if (i == sel || fabsf(a) > 1.6f)
                continue;
            iy += 3.0f * sinf(t * 0.9f + (float)i);
            bk_line((float)(x + 465), cyy, ix, iy, 1.0f, ARGB(30, 255, 255, 255));
            dm_island(wd_w[idx[i]].biome, ix, iy + 10.0f, 46.0f, t, idx[i] == wd_current);
            bk_biome_sigil(wd_w[idx[i]].biome, ix, iy - 4.0f, 10.0f, BK_ALPHA(col, 150), 0.0f);
        }
        {
            const wd_world *w = &wd_w[idx[sel]];
            float bob = 5.0f * sinf(t * 0.8f);
            bk_disc((float)(x + 465), cyy + bob, 70, BK_ALPHA(col, 12));
            dm_island(w->biome, (float)(x + 465), cyy + 22.0f + bob, 150.0f, t, 1);
            bk_biome_sigil(w->biome, (float)(x + 465), cyy - 12.0f + bob, 24.0f, col, t);
            if (idx[sel] == wd_current)          /* you are here: a little flag */
                gd_flower((float)(x + 520), cyy + 10.0f + bob, 7, 1, col, BK_INK);
        }
    }
    if (n) {                                      /* the chosen world: its story, what sounds in it */
        const wd_world *w = &wd_w[idx[wd_pos[wd_fam]]];
        int dy = y + 548;
        bk_rrect(x + 218, dy, 494, 146, 12, ARGB(18, 255, 255, 255));
        bk_text_fit(x + 232, dy + 6, &FONT_B, w->name, 466, col);
        {   /* the story, up to two lines */
            char line[120];
            int len = (int)strlen(w->story), cut = len;
            if (bk_text_w(&FONT_S, w->story, 1) > 466) {
                for (cut = len / 2; cut < len && w->story[cut] != ' '; cut++)
                    ;
            }
            snprintf(line, sizeof line, "%.*s", cut, w->story);
            bk_text(x + 232, dy + 33, &FONT_S, 1, line, BK_INK);
            if (cut < len)
                bk_text_fit(x + 232, dy + 53, &FONT_S, w->story + cut + 1, 466, BK_INK);
        }
        wd_tags(w, buf, sizeof buf);
        bk_text_fit(x + 232, dy + 78, &FONT_T, buf, 466, BK_DIM);
        if (md_pending_biome() >= 0 && wd_pending >= 0)
            bk_text(x + 232, dy + 98, &FONT_S, 1, "UNSAVED CHANGES: A AGAIN ENTERS NOW", ARGB(255, 250, 220, 170));
        else
            bk_text(x + 232, dy + 98, &FONT_S, 1, idx[wd_pos[wd_fam]] == wd_current ? "YOU ARE HERE" : "A: ENTER THIS WORLD",
                    idx[wd_pos[wd_fam]] == wd_current ? col : BK_DIM);
        snprintf(buf, sizeof buf, wd_wander ? "WANDERING: A NEW WORLD EVERY %d MIN" : "Y: WANDER (EVERY %d MIN)", wd_wander_min);
        bk_text(x + 232, dy + 121, &FONT_T, 1, buf, wd_wander ? col : BK_FAINT);
    }
    {
        int ey = y + 56 + wd_nfam * fstep + 14;
        if (wd_error[0])
            bk_text_fit(x + 18, ey, &FONT_T, wd_error, 186, ARGB(255, 240, 150, 120));
        snprintf(buf, sizeof buf, "%d WORLDS", wd_n);
        bk_text(x + 18, ey + 20, &FONT_T, 1, buf, BK_FAINT);
        if (ey + 36 < y + 548) {                 /* (the file's place, while it stays clear of the story box beside it) */
            bk_text(x + 18, ey + 36, &FONT_T, 1, "MUTABLEDREAMS/WORLDS/", BK_FAINT);
            bk_text(x + 18, ey + 52, &FONT_T, 1, "WORLDS.TXT: ADD YOUR OWN", BK_FAINT);
        }
    }
}

#include "brick_toys_draw.inc"

static void bk_draw_view(float t)
{
    if (bk_view == V_MIXER)
        bk_draw_mixer(t);
    else if (bk_view == V_ATLAS)
        bk_draw_atlas(t);
    else if (bk_view == V_SEQ)
        bk_draw_seq(t);
    else if (bk_view == V_DREAM)
        bk_draw_dream(t);
    else if (bk_view == V_MOD)
        bk_draw_mod(t);
    else if (bk_view == V_VOICE)
        bk_draw_voice(t);
    else if (bk_view == V_SPACE)
        bk_draw_space(t);
    else if (bk_view == V_ARCHIVE)
        bk_draw_archive(t);
    else if (bk_view == V_WORLDS)
        bk_draw_worlds(t);
    else if (bk_view == V_TOYS)
        bk_draw_toys(t);
}
