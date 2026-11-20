/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the GARDEN, the Brick's own home screen.
 *
 * On the LIVE page (its atlas view) the centre shows a living night garden in place of the FM-1's screen. Everything
 * in it is the engine, drawn as nature:
 *   - each track is a plant whose species follows its sound model (tulips for the analog voices, ferns for FM,
 *     bells for the resonators, dandelions for the airy ones, reeds for strings and breath, mushrooms for drums
 *     and the kit); it grows with its level, blooms on every hit, sends a spark up with the note, sleeps when muted
 *   - the four macros are the weather: DENSITY fireflies, MOTION wind, COLOUR the sky's glow at the horizon,
 *     SPACE mist and the pond that mirrors the garden
 *   - the moon crosses the sky with the bar; the chosen track has a butterfly; the Biome's name is written above
 *   - turning K1..K4 shows the weather's name and value; the app's messages float as a little banner
 * The FM-1 screen comes back for help, questions, the intro and every other page; MENU > GARDEN / FM-1 SCREEN
 * switches the home screen. The controls stay those of the FM-1's LIVE page.
 * (Included by brick_stage.inc after the drawing primitives.) */

static int bk_garden = 1;                        /* the garden as the home screen (MENU toggles) */
static float gd_k = 1.0f;                        /* the plants' size: 1 in the garden, smaller on the track cards */
enum { SP_TULIP, SP_FERN, SP_BELLS, SP_DANDELION, SP_REEDS, SP_MUSHROOM, SP_THISTLE, SP_GRASS, SP_SUNFLOWER, SP_LANTERN,
       SP_LOTUS, SP_SPIRAL, SP_CRYSTAL, SP_ORCHID };
static const char *const GD_WEATHER[4] = {"FIREFLIES  /  DENSITY", "WIND  /  MOTION", "GLOW  /  COLOUR", "MIST  /  SPACE"};

#define GD_X BK_SX
#define GD_Y BK_SY
#define GD_W 720
#define GD_H 720
#define GD_HORIZON 520                           /* (inside the garden) */
#ifndef GD_BG_MS
#define GD_BG_MS 200u                            /* how often the sky is drawn anew (textured: five times a second) */
#endif

static int gd_species(int track)
{
    const md_patch *p = md_state();
    if (p->kit.enabled && track == 3)
        return SP_MUSHROOM;
    switch (p->track[track].model) {            /* fourteen kinds for the 24 sounds: each family its own plant */
    case MD_KICK: case MD_SKIN: return SP_MUSHROOM;
    case MD_SNARE: return SP_THISTLE;
    case MD_HAT: return SP_GRASS;
    case MD_FM: return SP_FERN;
    case MD_COPPER: case MD_SPECTRAL: return SP_CRYSTAL;
    case MD_RINGS: case MD_CEDAR: return SP_BELLS;
    case MD_FELT: case MD_ELEMENTS: return SP_LANTERN;
    case MD_HARMONIC: case MD_BREATH: case MD_AERO: return SP_DANDELION;
    case MD_CHOIR: case MD_VOWEL: return SP_ORCHID;
    case MD_REED: case MD_BOWED: case MD_VELVET_KEYS: return SP_REEDS;
    case MD_SILK: case MD_SATIN: return SP_SPIRAL;
    case MD_MELLOW: return SP_LOTUS;
    case MD_BASS: return SP_SUNFLOWER;
    default: return SP_TULIP;
    }
}

static int bk_garden_shown(void)
{
    return bk_garden && !lab_open && !bk_view && dream_ui.page == LIVE && dream_ui.live_view == LIVE_ATLAS &&
           dream_ui.help < 0 && !intro_active && md_pending_biome() < 0 && bank_pending < 0;
}

int bk_garden_visible(void) { return bk_garden_shown(); }

static uint32_t gd_mix(uint32_t a, uint32_t b, float k)
{
    k = k < 0 ? 0 : k > 1 ? 1 : k;
    return 0xFF000000u | (uint32_t)((float)(a >> 16 & 255u) * (1 - k) + (float)(b >> 16 & 255u) * k) << 16 |
           (uint32_t)((float)(a >> 8 & 255u) * (1 - k) + (float)(b >> 8 & 255u) * k) << 8 |
           (uint32_t)((float)(a & 255u) * (1 - k) + (float)(b & 255u) * k);
}

/* ---- sparks: what a hit sends up (and the dandelions' seeds) ---- */
#define GD_SPARKS 96
static struct { float x, y, vx, vy, life, size; uint32_t col; } gd_spark[GD_SPARKS];
static int gd_spark_n;
static float gd_beat_t = -10;
static float gd_mute_f[6];                       /* each plant's sleepiness (0 awake .. 1 asleep) */                    /* when the last beat fell (the fireflies light up) */
static void gd_emit(float x, float y, float vx, float vy, float size, uint32_t col)
{
    int i = gd_spark_n++ % GD_SPARKS;
    gd_spark[i].x = x, gd_spark[i].y = y, gd_spark[i].vx = vx, gd_spark[i].vy = vy;
    gd_spark[i].life = 1.0f, gd_spark[i].size = size, gd_spark[i].col = col;
}

/* the Biome drift as the time of day: 0 night (scene A, or no drift) .. 1 dawn (scene B, at full depth) */
static float gd_dawn(const md_patch *p)
{
    if (!p->drift.mode || !p->drift.depth || p->drift.captured != 3)
        return 0;
    return (float)md_drift_position() / 1000.0f * (float)p->drift.depth / 1000.0f;
}
static const uint32_t GD_ICE = ARGB(255, 205, 235, 255);

/* ---- ripples: every hit stirs the pond ---- */
#define GD_RIPPLES 24
static struct { float x, y, t0; uint32_t col; } gd_rip[GD_RIPPLES];
static int gd_rip_n;
static void gd_ripple(float x, float y, float t, uint32_t col)
{
    int i = gd_rip_n++ % GD_RIPPLES;
    gd_rip[i].x = x, gd_rip[i].y = y, gd_rip[i].t0 = t, gd_rip[i].col = col;
}
static void gd_ripples(float t)
{
    for (int i = 0; i < GD_RIPPLES; i++) {
        float age = t - gd_rip[i].t0, r, a, px = 0, py = 0;
        if (gd_rip[i].t0 <= 0 || age < 0 || age > 1.6f)
            continue;
        r = 8.0f + 90.0f * age / 1.6f;
        a = (1.0f - age / 1.6f) * 0.7f;
        for (int k = 0; k <= 28; k++) {           /* an ellipse: a ring seen from the side */
            float an = (float)k * 0.2244f, x = gd_rip[i].x + cosf(an) * r, y = gd_rip[i].y + sinf(an) * r * 0.22f;
            if (k && x > (float)GD_X && x < (float)(GD_X + GD_W) && px > (float)GD_X && px < (float)(GD_X + GD_W))
                bk_line(px, py, x, y, 1.4f, BK_ALPHA(gd_rip[i].col, (uint32_t)(255 * a)));
            px = x, py = y;
        }
    }
}

/* ---- texture: a tile of soft noise (made once), laid over the kept layers (sky, backdrop) as grain, drifting veils
   of colour and a vignette; it costs only when those layers are drawn again, never per frame ---- */
static uint8_t gd_noise[128 * 128];
static void gd_noise_init(void)
{
    static int done;
    static uint8_t tmp[128 * 128];
    uint32_t h = 0x2545F491u;
    if (done)
        return;
    done = 1;
    for (int i = 0; i < 128 * 128; i++)
        h = h * 1664525u + 1013904223u, gd_noise[i] = (uint8_t)(h >> 24);
    for (int pass = 0; pass < 3; pass++) {       /* softened: box blurs, wrapping */
        for (int y = 0; y < 128; y++)
            for (int x = 0; x < 128; x++) {
                int sum = 0;
                for (int dy = -2; dy <= 2; dy++)
                    for (int dx = -2; dx <= 2; dx++)
                        sum += gd_noise[((y + dy) & 127) * 128 + ((x + dx) & 127)];
                tmp[y * 128 + x] = (uint8_t)(sum / 25);
            }
        memcpy(gd_noise, tmp, sizeof tmp);
    }
    {   /* stretched to the full range */
        int lo = 255, hi = 0;
        for (int i = 0; i < 128 * 128; i++)
            lo = gd_noise[i] < lo ? gd_noise[i] : lo, hi = gd_noise[i] > hi ? gd_noise[i] : hi;
        for (int i = 0; i < 128 * 128; i++)
            gd_noise[i] = (uint8_t)((gd_noise[i] - lo) * 255 / (hi > lo ? hi - lo : 1));
    }
}
static inline int gd_nz(int x, int y, int sh) { return gd_noise[((y >> sh) & 127) * 128 + ((x >> sh) & 127)]; }
/* rows y0..y1 of the stage area (GD_X..): grain (grain/256 strength), veils of tint drifting with t, a vignette */
/* the grain and the vignette never change: worked out once per grain strength (two are used) and kept, so a texture
   pass is a lookup and a multiply per pixel (0.63; it was the dearest part of every sky redraw) */
static uint16_t gd_kmap[2][GD_W * GD_H];
static int gd_kmap_grain[2] = {-1, -1};
static const uint16_t *gd_kmap_for(int grain)
{
    int slot = gd_kmap_grain[0] == grain ? 0 : gd_kmap_grain[1] == grain ? 1 : gd_kmap_grain[0] < 0 ? 0 : 1;
    if (gd_kmap_grain[slot] != grain) {
        gd_noise_init();
        for (int y = 0; y < GD_H; y++) {
            int vy = y - GD_H / 2;
            for (int x = 0; x < GD_W; x++) {
                int n = (gd_nz(x, y, 1) * 3 + gd_nz(x + 37, y + 11, 3) * 5) / 8;   /* two scales */
                int vx = x - GD_W / 2, vig = 256 - ((vx * vx + vy * vy) >> 11);      /* darker towards the corners */
                gd_kmap[slot][y * GD_W + x] = (uint16_t)((256 + (n - 128) * grain / 128) * (vig < 150 ? 150 : vig) >> 8);
            }
        }
        gd_kmap_grain[slot] = grain;
    }
    return gd_kmap[slot];
}
static void gd_texture(int y0, int y1, int grain, uint32_t tint, int veil, float t)
{
    int ox = (int)(t * 6.0f), tr = (int)(tint >> 16 & 255u), tg = (int)(tint >> 8 & 255u), tb = (int)(tint & 255u);
    const uint16_t *km;
    if (bk_light)
        return;                                  /* (the light picture: no grain) */
    km = gd_kmap_for(grain);
    for (int y = y0; y < y1; y++) {
        uint32_t *row = &bk_px[(GD_Y + y) * BK_W + GD_X];
        const uint16_t *kr = &km[y * GD_W];
        const uint8_t *vrow = &gd_noise[(((y + (ox >> 1)) >> 3) & 127) * 128];
        bk_draw_count(2048u);
        for (int x = 0; x < GD_W; x++) {
            int v = vrow[((x + ox) >> 3) & 127], w = v > 140 ? (v - 140) * veil / 115 : 0;
            int k = kr[x];
            uint32_t c = row[x];
            int r = ((int)(c >> 16 & 255u) * k >> 8) + (tr * w >> 8), g = ((int)(c >> 8 & 255u) * k >> 8) + (tg * w >> 8), b = ((int)(c & 255u) * k >> 8) + (tb * w >> 8);
            row[x] = 0xFF000000u | (uint32_t)(r > 255 ? 255 : r) << 16 | (uint32_t)(g > 255 ? 255 : g) << 8 | (uint32_t)(b > 255 ? 255 : b);
        }
    }
}

/* ---- the sky, the hills, the mist: slow, so drawn ten times a second into a kept layer ---- */
static uint32_t gd_bg[GD_W * GD_H];
static void gd_sky(float t, const md_patch *p)
{
    float colour = (float)p->macro[2] / 1000.0f, space = (float)p->macro[3] / 1000.0f;
    float dawn = gd_dawn(p);
    /* each Biome its own place: a sky of its own colours, its own land, something of its own in the sky */
    static const uint32_t SKY[8][2] = {
        {ARGB(255, 4, 6, 16), ARGB(255, 70, 40, 104)},   {ARGB(255, 2, 12, 16), ARGB(255, 16, 96, 92)},
        {ARGB(255, 10, 6, 14), ARGB(255, 130, 64, 48)},  {ARGB(255, 8, 4, 18), ARGB(255, 120, 44, 96)},
        {ARGB(255, 4, 10, 22), ARGB(255, 60, 110, 150)}, {ARGB(255, 3, 12, 8), ARGB(255, 40, 100, 56)},
        {ARGB(255, 12, 8, 6), ARGB(255, 120, 92, 40)},   {ARGB(255, 2, 3, 8), ARGB(255, 34, 46, 90)}};
    uint32_t bh = (uint32_t)(p->biome_id + 7) * 2654435761u, pal = (bh >> 3) % 8u, land = (bh >> 9) % 6u, extra = (bh >> 15) % 5u;
    uint32_t top = gd_mix(SKY[pal][0], ARGB(255, 30, 36, 70), dawn * 0.8f);
    uint32_t glow = gd_mix(SKY[pal][1], colour < 0.5f ? ARGB(255, 20, 70, 86) : ARGB(255, 128, 70, 46), fabsf(colour - 0.5f) * 0.8f);
    glow = gd_mix(glow, ARGB(255, 222, 134, 96), dawn * 0.8f);   /* the drift towards B: dawn */
    if (p->freeze)
        glow = gd_mix(glow, ARGB(255, 70, 110, 150), 0.5f);    /* frozen: a cold glow */
    int x, y;
    for (y = 0; y < GD_HORIZON; y++) {
        float k = (float)y / (float)GD_HORIZON;
        uint32_t c = gd_mix(top, glow, powf(k, 2.2f) * 0.85f), *r = &bk_px[(GD_Y + y) * BK_W + GD_X];
        for (x = 0; x < GD_W; x++)
            r[x] = c;
    }
    for (x = 0; x < 90; x++) {                   /* stars, fewer near the glow */
        uint32_t h = (uint32_t)x * 2654435761u;
        float sx = (float)(h % GD_W), sy = (float)((h >> 10) % (GD_HORIZON - 120));
        float tw = 0.5f + 0.5f * sinf(t * (0.6f + (float)(h >> 24 & 7u) * 0.2f) + (float)x);
        bk_disc((float)GD_X + sx, (float)GD_Y + sy, 0.7f + (float)(h >> 20 & 3u) * 0.3f,
                BK_ALPHA(ARGB(255, 235, 240, 255), (uint32_t)((40 + 150 * tw * (1.0f - sy / (float)GD_HORIZON)) * (1.0f - 0.85f * dawn))));
    }
    {   /* something of its own in the sky */
        float ex = (float)GD_X + 120.0f + (float)((bh >> 20) % 480u), ey = (float)GD_Y + 90.0f + (float)((bh >> 12) % 120u);
        if (extra == 1)                           /* a nebula */
            for (int k = 0; k < 7; k++)
                bk_disc(ex + 60.0f * sinf((float)k * 2.1f), ey + 30.0f * cosf((float)k * 1.7f), 50.0f + (float)(k * 9),
                        BK_ALPHA(gd_mix(glow, ARGB(255, 200, 120, 220), (float)k / 7.0f), 12));
        else if (extra == 2) {                    /* a ringed planet */
            bk_disc(ex, ey, 16, gd_mix(glow, ARGB(255, 230, 200, 160), 0.5f));
            bk_disc(ex - 5, ey - 5, 7, ARGB(40, 255, 255, 255));
            for (int k = 0; k < 40; k++) {
                float a = (float)k / 40.0f * 6.283f;
                if (sinf(a) > 0 || k % 2 == 0)
                    bk_disc(ex + cosf(a) * 30.0f, ey + sinf(a) * 8.0f, 1.4f, ARGB(170, 230, 220, 200));
            }
        } else if (extra == 3) {                  /* a second, small moon */
            bk_disc(ex, ey, 9, ARGB(220, 220, 225, 240));
            bk_disc(ex + 3, ey - 2, 8, BK_ALPHA(top, 230));
        } else if (extra == 4)                    /* a band of aurora */
            for (x = 0; x < GD_W; x += 6) {
                float u = (float)x / (float)GD_W, ay = ey - 40.0f + 26.0f * sinf(u * 5.0f + t * 0.3f);
                for (int k = 0; k < 5; k++)
                    bk_disc((float)(GD_X + x), ay + (float)k * 9.0f, 7.0f, BK_ALPHA(gd_mix(ARGB(255, 90, 220, 170), glow, (float)k / 5.0f), (uint32_t)(10 + 4 * (4 - k))));
            }
    }
    for (int layer = 0; layer < 3; layer++)       /* its land, far to near: hills, mountains, dunes, a forest, mesas, spires */
        for (x = 0; x < GD_W; x += 2) {
            float u = (float)x / (float)GD_W, base = (float)GD_HORIZON - 70.0f + (float)layer * 34.0f, ph = (float)(bh >> 24) * 0.05f, hy;
            switch (land) {
            case 1:                               /* mountains: sharp peaks */
                hy = base - 40.0f * fabsf(sinf(u * (3.0f + (float)layer) + ph + (float)layer)) + 12.0f * fabsf(sinf(u * 13.0f + ph)) + 20.0f;
                break;
            case 2:                               /* dunes: long, low */
                hy = base + 14.0f * sinf(u * (1.4f + 0.5f * (float)layer) + ph + (float)layer * 2.0f) + 10.0f;
                break;
            case 3: {                             /* a forest: firs along the hills */
                float tri = fabsf(fmodf(u * (34.0f + 10.0f * (float)layer) + ph, 1.0f) - 0.5f) * 2.0f;
                hy = base + 16.0f * sinf(u * 2.5f + (float)layer + ph) - (layer < 2 ? (1.0f - tri) * (26.0f - 6.0f * (float)layer) : 0.0f);
                break;
            }
            case 4:                               /* mesas: flat tops, steep sides */
                hy = base + 26.0f * floorf(2.0f * sinf(u * (2.5f + (float)layer) + ph + (float)layer)) * 0.5f;
                break;
            case 5: {                             /* spires of rock, here and there */
                float sp = sinf(u * (7.0f + 3.0f * (float)layer) + ph * 3.0f);
                hy = base + 12.0f * sinf(u * 3.0f + (float)layer) - (sp > 0.85f ? (sp - 0.85f) * 520.0f : 0.0f);
                break;
            }
            default:                              /* rolling hills */
                hy = base + 22.0f * sinf(u * (2.0f + (float)layer) + (float)layer * 1.7f + ph) + 9.0f * sinf(u * 9.0f + (float)layer);
            }
            if (hy < 40.0f)
                hy = 40.0f;
            uint32_t c = gd_mix(gd_mix(top, glow, 0.30f - 0.09f * (float)layer), ARGB(255, 6, 10, 18), 0.45f + 0.25f * (float)layer);
            bk_rect(GD_X + x, GD_Y + (int)hy, 2, GD_HORIZON - (int)hy, c);
        }
    for (int b = 0; b < 3 && space > 0.05f; b++) {   /* mist, more with SPACE */
        float my = (float)GD_HORIZON - 60.0f + (float)b * 26.0f;
        for (x = 0; x <= GD_W; x += 24) {
            float u = (float)x / (float)GD_W, y0 = my + 8.0f * sinf(u * 6.0f + t * 0.2f + (float)b);
            bk_disc((float)(GD_X + x), (float)GD_Y + y0, 26.0f, BK_ALPHA(ARGB(255, 200, 215, 235), (uint32_t)(10.0f * space)));
        }
    }
    gd_texture(0, GD_HORIZON, 40, gd_mix(glow, ARGB(255, 180, 160, 255), 0.3f), 36, t);   /* grain, drifting veils of the sky's colour */
    bk_rect(GD_X, GD_Y + GD_HORIZON, GD_W, GD_H - GD_HORIZON, ARGB(255, 5, 9, 16));   /* the pond's bed */
}

/* ---- the plants ---- */
typedef struct { float x, foot, h, sway, bloom, lv; uint32_t col; int muted, i; float tipx, tipy; } gd_plant;

static void gd_stem(gd_plant *pl, float bend, float th)
{
    float px = pl->x, py = pl->foot;
    const int n = bk_light ? 7 : 14;             /* (1.5.7: the light picture, half the segments - at 640 x 480 the same curve) */
    for (int s = 1; s <= n; s++) {
        float f = (float)s / (float)n, nx = pl->x + (bend + pl->sway) * f * f, ny = pl->foot - pl->h * f;
        bk_line(px, py, nx, ny, th * (0.45f + 0.55f * gd_k) * (1.0f - 0.5f * f), pl->col);
        px = nx, py = ny;
    }
    pl->tipx = px, pl->tipy = py;
}
static void gd_leaf(float x, float y, float side, float len, float rise, uint32_t col, float a)
{
    const int n = bk_light ? 5 : 8;
    for (int d = 1; d <= n; d++) {
        float u = (float)d / (float)n, w = sinf(u * 3.1416f) * (1.0f - 0.3f * u) * len * 0.17f;
        bk_disc(x + side * len * u, y - rise * u * u - len * 0.12f * sinf(u * 3.1416f), w + 0.6f, BK_ALPHA(col, (uint32_t)(255.0f * a)));
    }
}

static void gd_tulip(gd_plant *pl, float t)
{
    float b = pl->bloom, open = pl->muted ? 0 : 0.25f + 0.75f * b;
    gd_stem(pl, 6, 5.0f);
    gd_leaf(pl->x, pl->foot - 10 * gd_k, -1, pl->h * 0.30f, pl->h * 0.55f, pl->col, 0.45f);
    gd_leaf(pl->x, pl->foot - 24 * gd_k, 1, pl->h * 0.26f, pl->h * 0.45f, pl->col, 0.4f);
    for (int k = -1; k <= 1; k++) {               /* a cup of three petals, opening on the hit */
        float a = (float)k * (0.35f + 0.5f * open);
        bk_disc(pl->tipx + sinf(a) * 15.0f * gd_k, pl->tipy - (cosf(a) * 15.0f + 5.0f) * gd_k, (14.0f + 4.0f * b) * gd_k, BK_ALPHA(pl->col, 150 + (uint32_t)(100 * b)));
    }
    bk_disc(pl->tipx, pl->tipy - 6.0f * gd_k, (5.0f + 3.0f * b) * gd_k, pl->muted ? BK_FAINT : BK_INK);
    (void)t;
}

static void gd_fern(gd_plant *pl, float t)
{
    float unfurl = 0.35f + 0.25f * pl->lv + 0.4f * pl->bloom, a = 0, r = 22.0f * gd_k, cx, cy, px, py;
    gd_stem(pl, -10, 5.0f);
    for (int s = 2; s <= 12; s += 2) {           /* leaflets along the stem */
        float f = (float)s / 14.0f, nx = pl->x + (-10 + pl->sway) * f * f, ny = pl->foot - pl->h * f;
        gd_leaf(nx, ny, (s & 2) ? 1 : -1, 26.0f * gd_k * (1.0f - f * 0.6f), 8 * gd_k, pl->col, 0.5f);
    }
    cx = pl->tipx + 16.0f * gd_k, cy = pl->tipy;         /* the koru: a spiral that opens with sound */
    px = pl->tipx, py = pl->tipy;
    for (int k = 0; k < 40; k++) {
        float nx = cx - cosf(a) * r, ny = cy - sinf(a) * r;
        bk_line(px, py, nx, ny, (3.2f * (1.0f - (float)k / 50.0f)) * (0.5f + 0.5f * gd_k), pl->col);
        px = nx, py = ny;
        a += 0.22f;
        r *= 0.97f - 0.04f * (1.0f - unfurl);
    }
    bk_disc(px, py, (3.0f + 4.0f * pl->bloom) * gd_k, pl->muted ? BK_FAINT : BK_INK);
    pl->tipx = cx, pl->tipy = cy - 10 * gd_k;
    (void)t;
}

static void gd_bells(gd_plant *pl, float t)
{
    float px = pl->x, py = pl->foot;
    gd_leaf(pl->x, pl->foot - 6 * gd_k, -1, 40 * gd_k, 10 * gd_k, pl->col, 0.4f);
    for (int s = 1; s <= 14; s++) {               /* an arch */
        float f = (float)s / 14.0f, nx = pl->x + (46.0f * gd_k + pl->sway) * f, ny = pl->foot - pl->h * sinf(f * 2.2f) / sinf(2.2f * 0.75f) * 0.8f;
        bk_line(px, py, nx, ny, 3.6f * (1.0f - 0.5f * f), pl->col);
        if (s == 6 || s == 10 || s == 14) {       /* bells hang from it, swinging on the hit */
            float sw = sinf(t * 6.0f + (float)s) * 10.0f * gd_k * pl->bloom, bx = nx + sw, by = ny + 30.0f * gd_k;
            bk_line(nx, ny, bx, by - 10.0f * gd_k, 1.5f, pl->col);
            for (int k = 0; k < 6; k++)           /* a bell: narrow at the top, wide at the lip */
                bk_disc(bx, by + (-10.0f + (float)k * 3.6f) * gd_k, (4.0f + (float)k * 1.6f + 1.5f * pl->bloom) * gd_k, BK_ALPHA(pl->col, 210));
            bk_line(bx - (13.0f + pl->bloom) * gd_k, by + 9.0f * gd_k, bx + (13.0f + pl->bloom) * gd_k, by + 9.0f * gd_k, 2.5f * gd_k, pl->col);
            bk_disc(bx + sw * 0.4f, by + 14.0f * gd_k, 3.0f * gd_k, pl->muted ? BK_FAINT : BK_INK);
            pl->tipx = bx, pl->tipy = by;
        }
        px = nx, py = ny;
    }
}

static void gd_dandelion(gd_plant *pl, float t)
{
    float r = (28.0f + 12.0f * pl->lv) * gd_k;
    gd_stem(pl, 4, 3.4f);
    gd_leaf(pl->x, pl->foot - 4, 1, 34 * gd_k, 6 * gd_k, pl->col, 0.4f);
    gd_leaf(pl->x, pl->foot - 4, -1, 30 * gd_k, 6 * gd_k, pl->col, 0.4f);
    for (int k = 0; k < 18; k++) {               /* the clock: rays with seeds */
        float a = (float)k * 0.349f + t * 0.1f, rr = r * (0.85f + 0.15f * sinf((float)k * 2.1f));
        float ex = pl->tipx + cosf(a) * rr, ey = pl->tipy + sinf(a) * rr;
        bk_line(pl->tipx, pl->tipy, ex, ey, 1.0f, BK_ALPHA(pl->col, 140));
        bk_disc(ex, ey, (2.0f + 1.2f * pl->bloom) * (0.5f + 0.5f * gd_k), BK_ALPHA(pl->muted ? BK_FAINT : BK_INK, 200));
    }
    bk_disc(pl->tipx, pl->tipy, 4.0f, pl->col);
}

static void gd_reeds(gd_plant *pl, float t)
{
    float base = pl->x;
    for (int k = 0; k < 5; k++) {                /* blades, bending in the wind */
        float lean = (float)(k - 2) * 9.0f * gd_k, hh = pl->h * (0.55f + 0.1f * (float)((k * 7) % 4)), px = base + (float)(k - 2) * 5.0f * gd_k, py = pl->foot;
        for (int s = 1, n = bk_light ? 5 : 10; s <= n; s++) {
            float f = (float)s / (float)n, nx = base + (float)(k - 2) * 5.0f * gd_k + (lean + pl->sway * 1.3f) * f * f, ny = pl->foot - hh * f;
            bk_line(px, py, nx, ny, 3.0f * (1.0f - 0.8f * f) + 0.5f, BK_ALPHA(pl->col, 170));
            px = nx, py = ny;
        }
    }
    gd_stem(pl, 3, 2.4f);
    for (int k = 0; k < 6; k++)                   /* the cattail */
        bk_disc(pl->tipx, pl->tipy + (6.0f + (float)k * 4.0f) * gd_k, (5.0f + 1.5f * pl->bloom) * gd_k, BK_ALPHA(gd_mix(pl->col, ARGB(255, 120, 80, 50), 0.5f), 230));
    (void)t;
}

static void gd_mushroom(gd_plant *pl, float t)
{
    static const float off[3] = {-34, 8, 40}, sz[3] = {1.1f, 1.4f, 0.85f};
    for (int m = 0; m < 3; m++) {                /* a little family; they jump on the hit */
        float s = sz[m] * gd_k * (0.7f + 0.5f * pl->lv), jump = pl->bloom * 18.0f * sz[m] * (m == 1 ? 1.0f : 0.6f);
        float x = pl->x + off[m] * gd_k, stem = 40.0f * s, top = pl->foot - stem - jump, squash = 1.0f + 0.25f * pl->bloom;
        bk_line(x, pl->foot, x, top + 6, 9.0f * s, BK_ALPHA(ARGB(255, 230, 222, 205), 230));
        for (int k = -4; k <= 4; k++)             /* the cap: a dome of discs */
            bk_disc(x + (float)k * 5.0f * s * squash, top - (16.0f - (float)(k * k)) * s * 0.8f, 9.0f * s, pl->col);
        for (int d = 0; d < 3; d++)
            bk_disc(x + (float)(d - 1) * 9.0f * s, top - 8.0f * s, 2.2f * s, BK_ALPHA(BK_INK, 200));
        if (m == 1)
            pl->tipx = x, pl->tipy = top - 14.0f * s;
    }
    (void)t;
}


static void gd_thistle(gd_plant *pl, float t)      /* SNARE: a spiky globe that bristles on the hit */
{
    float r = (13.0f + 4.0f * pl->lv) * gd_k, sp = 1.0f + 0.5f * pl->bloom;
    gd_stem(pl, -5, 4.0f);
    for (int k = 0; k < 4; k++)
        gd_leaf(pl->x, pl->foot - (float)(8 + k * 16) * gd_k, (k & 1) ? 1 : -1, 22 * gd_k, 10 * gd_k, pl->col, 0.45f);
    for (int k = 0; k < 22; k++) {
        float a = (float)k * 0.2856f - 1.57f, len = r * (1.5f + 0.4f * sinf((float)k * 3.7f)) * sp;
        if (sinf(a) > 0.5f)
            continue;
        bk_line(pl->tipx, pl->tipy, pl->tipx + cosf(a) * len, pl->tipy + sinf(a) * len, 1.6f, BK_ALPHA(pl->col, 220));
    }
    bk_disc(pl->tipx, pl->tipy + 3 * gd_k, r, BK_ALPHA(gd_mix(pl->col, ARGB(255, 60, 70, 90), 0.4f), 255));
    bk_disc(pl->tipx - r * 0.3f, pl->tipy, r * 0.35f, BK_ALPHA(BK_INK, 60));
    (void)t;
}

static void gd_grass(gd_plant *pl, float t)        /* HAT: a tuft of fine blades, a spark on each tip */
{
    for (int k = 0; k < 9; k++) {
        float lean = ((float)k - 4.0f) * 6.0f * gd_k, hh = pl->h * (0.5f + 0.5f * fabsf(sinf((float)k * 1.7f + (float)pl->i)));
        float px = pl->x + ((float)k - 4.0f) * 3.0f * gd_k, py = pl->foot, nx = px, ny = py;
        for (int s = 1, n = bk_light ? 4 : 8; s <= n; s++) {
            float f = (float)s / (float)n;
            nx = pl->x + ((float)k - 4.0f) * 3.0f * gd_k + (lean + pl->sway * 1.6f + sinf(t * 3.0f + (float)k) * 2.0f) * f * f;
            ny = pl->foot - hh * f;
            bk_line(px, py, nx, ny, 2.2f * (1.0f - 0.7f * f) + 0.4f, BK_ALPHA(pl->col, 190));
            px = nx, py = ny;
        }
        if (pl->bloom > 0.1f && (k % 2 == 0))
            bk_disc(nx, ny, 2.0f + 2.5f * pl->bloom, BK_ALPHA(BK_INK, (uint32_t)(255 * pl->bloom)));
        if (k == 4)
            pl->tipx = nx, pl->tipy = ny;
    }
}

static void gd_sunflower(gd_plant *pl, float t)    /* BASS: a heavy round head, nodding with the low notes */
{
    float r = (16.0f + 5.0f * pl->lv) * gd_k, nod = 6.0f * pl->bloom * gd_k;
    gd_stem(pl, 10, 6.0f);
    gd_leaf(pl->x, pl->foot - 20 * gd_k, -1, 38 * gd_k, 14 * gd_k, pl->col, 0.5f);
    gd_leaf(pl->x, pl->foot - 40 * gd_k, 1, 34 * gd_k, 12 * gd_k, pl->col, 0.45f);
    for (int k = 0; k < 14; k++) {
        float a = (float)k * 0.4488f + t * 0.05f;
        bk_disc(pl->tipx + cosf(a) * r * 1.25f, pl->tipy + nod + sinf(a) * r * 1.25f, r * 0.42f, BK_ALPHA(pl->col, 200));
    }
    bk_disc(pl->tipx, pl->tipy + nod, r, gd_mix(pl->col, ARGB(255, 60, 40, 30), 0.6f));
    for (int k = 0; k < 8; k++) {                 /* its seeds in a spiral */
        float a = (float)k * 2.3999f, rr = r * 0.8f * sqrtf((float)k / 8.0f);
        bk_disc(pl->tipx + cosf(a) * rr, pl->tipy + nod + sinf(a) * rr, 1.6f * gd_k + 0.6f, BK_ALPHA(BK_INK, 150));
    }
}

static void gd_lantern(gd_plant *pl, float t)      /* FELT, ELEMENTS: paper lanterns that glow on the hit */
{
    float px = pl->x, py = pl->foot;
    for (int s = 1; s <= 12; s++) {               /* a bowing stem */
        float f = (float)s / 12.0f, nx = pl->x + (30.0f * gd_k + pl->sway) * f * f, ny = pl->foot - pl->h * f;
        bk_line(px, py, nx, ny, 3.0f * (1.0f - 0.5f * f), pl->col);
        if (s == 5 || s == 8 || s == 12) {
            float sw = sinf(t * 1.5f + (float)s) * 3.0f * gd_k, lx = nx + sw, ly = ny + 18.0f * gd_k;
            float g = pl->muted ? 0.0f : 0.3f + 0.7f * pl->bloom, rr = (8.0f + (float)s * 0.4f) * gd_k;
            bk_line(nx, ny, lx, ly - rr, 1.2f, pl->col);
            bk_disc(lx, ly, rr * 2.2f, BK_ALPHA(ARGB(255, 255, 200, 120), (uint32_t)(40 * g)));
            for (int k = -1; k <= 1; k++)         /* ribbed */
                bk_disc(lx + (float)k * rr * 0.45f, ly, rr * (1.0f - 0.25f * fabsf((float)k)), BK_ALPHA(gd_mix(pl->col, ARGB(255, 255, 190, 110), g), 230));
            bk_disc(lx, ly, rr * 0.35f, BK_ALPHA(ARGB(255, 255, 240, 190), (uint32_t)(255 * g)));
            pl->tipx = lx, pl->tipy = ly;
        }
        px = nx, py = ny;
    }
}

static void gd_lotus(gd_plant *pl, float t)        /* MELLOW: a lotus on a pad, opening wide on the hit */
{
    float open = pl->muted ? 0.2f : 0.45f + 0.55f * pl->bloom, cx = pl->x, cy = pl->foot - pl->h * 0.35f;
    bk_line(cx, pl->foot, cx, cy, 3.0f, pl->col);
    for (int k = -6; k <= 6; k++)                 /* the pad */
        bk_disc(cx + (float)k * 4.0f * gd_k, pl->foot - 2.0f, (9.0f - fabsf((float)k)) * gd_k + 1.5f, BK_ALPHA(gd_mix(pl->col, ARGB(255, 60, 120, 90), 0.6f), 200));
    for (int ring = 0; ring < 2; ring++)
        for (int k = 0; k < 7; k++) {
            float a = -1.57f + ((float)k - 3.0f) * (0.28f + 0.22f * open) * (ring ? 0.7f : 1.0f);
            float len = (24.0f - (float)ring * 6.0f) * gd_k;
            for (int d = 1; d <= 5; d++) {
                float u = (float)d / 5.0f;
                bk_disc(cx + cosf(a) * len * u, cy + sinf(a) * len * u, (2.0f + 4.0f * sinf(u * 3.14f)) * gd_k + 0.5f,
                        BK_ALPHA(gd_mix(pl->col, BK_INK, (float)ring * 0.3f), 170));
            }
        }
    bk_disc(cx, cy - 3 * gd_k, 4.0f * gd_k, BK_INK);
    pl->tipx = cx, pl->tipy = cy - 20 * gd_k;
    (void)t;
}

static void gd_spiral(gd_plant *pl, float t)       /* SILK, SATIN: a vine curling up in loops */
{
    float px = pl->x, py = pl->foot;
    const int n = bk_light ? 30 : 60;
    for (int s = 1; s <= n; s++) {
        float f = (float)s / (float)n, a = f * 12.0f + t * 0.4f, rad = (10.0f + 6.0f * pl->bloom) * gd_k * (1.0f - 0.5f * f);
        float nx = pl->x + sinf(a) * rad + pl->sway * f, ny = pl->foot - pl->h * f - cosf(a) * rad * 0.3f;
        bk_line(px, py, nx, ny, 2.6f * (1.0f - 0.6f * f) + 0.5f, BK_ALPHA(pl->col, (uint32_t)(140 + 100 * (cosf(a) > 0))));
        if (s % (n / 5) == n / 10)
            gd_leaf(nx, ny, sinf(a) > 0 ? 1 : -1, 14 * gd_k, 4 * gd_k, pl->col, 0.5f);
        px = nx, py = ny;
    }
    bk_disc(px, py, (3.5f + 3.0f * pl->bloom) * gd_k + 1, pl->muted ? BK_FAINT : BK_INK);
    pl->tipx = px, pl->tipy = py;
}

static void gd_crystal(gd_plant *pl, float t)      /* COPPER, SPECTRAL: glass shards growing, glinting on the hit */
{
    static const float ang[5] = {-0.45f, -0.18f, 0.0f, 0.2f, 0.42f}, len[5] = {0.55f, 0.8f, 1.0f, 0.7f, 0.5f};
    for (int k = 0; k < 5; k++) {
        float a = ang[k] + pl->sway * 0.004f, L = pl->h * len[k] * (0.85f + 0.15f * pl->lv), w = (7.0f - (float)abs(k - 2)) * gd_k;
        float bx = pl->x + ((float)k - 2.0f) * 5.0f * gd_k, ex = bx + sinf(a) * L, ey = pl->foot - cosf(a) * L;
        for (int d = 0, n = bk_light ? 6 : 10; d <= n; d++) {   /* a shard: widest low, a point above */
            float u = (float)d / (float)n;
            bk_disc(bx + (ex - bx) * u, pl->foot + (ey - pl->foot) * u, w * (1.0f - u) + 0.8f,
                    BK_ALPHA(gd_mix(pl->col, BK_INK, 0.25f * u), (uint32_t)(130 + 80 * u)));
        }
        bk_line(bx, pl->foot, ex, ey, 1.0f, BK_ALPHA(BK_INK, 110));
        if (k == 2) {
            pl->tipx = ex, pl->tipy = ey;
            if (pl->bloom > 0.05f) {               /* a glint */
                float g = pl->bloom * 12.0f * gd_k;
                bk_line(ex - g, ey, ex + g, ey, 1.6f, BK_ALPHA(BK_INK, (uint32_t)(255 * pl->bloom)));
                bk_line(ex, ey - g, ex, ey + g, 1.6f, BK_ALPHA(BK_INK, (uint32_t)(255 * pl->bloom)));
            }
        }
    }
    (void)t;
}

static void gd_orchid(gd_plant *pl, float t)       /* CHOIR, VOWEL: an arching spray of open flowers, singing */
{
    float px = pl->x, py = pl->foot;
    gd_leaf(pl->x, pl->foot - 4, -1, 36 * gd_k, 4 * gd_k, pl->col, 0.5f);
    gd_leaf(pl->x, pl->foot - 4, 1, 32 * gd_k, 4 * gd_k, pl->col, 0.5f);
    for (int s = 1; s <= 12; s++) {
        float f = (float)s / 12.0f, nx = pl->x + (-36.0f * gd_k + pl->sway) * f * f, ny = pl->foot - pl->h * (f * 1.15f - 0.25f * f * f);
        bk_line(px, py, nx, ny, 2.4f * (1.0f - 0.5f * f), pl->col);
        if (s >= 6 && s % 2 == 0) {               /* a flower: five petals and a lip, opening as it sings */
            float o = (0.6f + 0.4f * pl->bloom) * gd_k * (0.8f + 0.04f * (float)s), wob = sinf(t * 2.0f + (float)s) * 0.1f;
            for (int k = 0; k < 5; k++) {
                float a = (float)k * 1.2566f + wob;
                bk_disc(nx + cosf(a) * 9.0f * o, ny + sinf(a) * 9.0f * o, 6.0f * o + 0.5f, BK_ALPHA(pl->col, 200));
            }
            bk_disc(nx, ny + 6.0f * o, 5.0f * o + 0.5f, BK_ALPHA(gd_mix(pl->col, BK_INK, 0.5f), 230));
            bk_disc(nx, ny, 2.2f * o + 0.5f, BK_INK);
            pl->tipx = nx, pl->tipy = ny;
        }
        px = nx, py = ny;
    }
}

/* the same plant, small, on a track card (the stage's left column) */
static void gd_draw_species(gd_plant *pl, int species, float t)
{
    switch (species) {
    case SP_FERN: gd_fern(pl, t); break;
    case SP_BELLS: gd_bells(pl, t); break;
    case SP_DANDELION: gd_dandelion(pl, t); break;
    case SP_REEDS: gd_reeds(pl, t); break;
    case SP_MUSHROOM: gd_mushroom(pl, t); break;
    case SP_THISTLE: gd_thistle(pl, t); break;
    case SP_GRASS: gd_grass(pl, t); break;
    case SP_SUNFLOWER: gd_sunflower(pl, t); break;
    case SP_LANTERN: gd_lantern(pl, t); break;
    case SP_LOTUS: gd_lotus(pl, t); break;
    case SP_SPIRAL: gd_spiral(pl, t); break;
    case SP_CRYSTAL: gd_crystal(pl, t); break;
    case SP_ORCHID: gd_orchid(pl, t); break;
    default: gd_tulip(pl, t);
    }
}
static void gd_mini(int i, int x, int y, int h, float t, int muted)
{
    gd_plant pl;
    pl.i = i, pl.muted = muted, pl.col = muted ? ARGB(255, 80, 88, 102) : bk_track_col(i);
    pl.x = (float)x + 62.0f, pl.foot = (float)(y + h) - 6.0f, pl.lv = bk_level[i];
    pl.h = 40.0f + 26.0f * bk_level[i];
    pl.sway = muted ? 0 : bk_sway[i] * 8.0f;
    pl.bloom = muted ? 0 : bk_bloom[i];
    gd_k = 0.6f;
    gd_draw_species(&pl, gd_species(i), t);
    gd_k = 1.0f;
}

static void gd_butterfly(float x, float y, float t, uint32_t col)
{
    float flap = fabsf(sinf(t * 9.0f)), fx = x + 18.0f * sinf(t * 0.9f), fy = y - 36.0f + 8.0f * sinf(t * 1.7f);
    for (int s = -1; s <= 1; s += 2) {
        bk_disc(fx + (float)s * (6.0f + 4.0f * flap), fy - 3.0f, 6.0f * (0.4f + 0.6f * flap) + 1, BK_ALPHA(col, 220));
        bk_disc(fx + (float)s * (5.0f + 2.0f * flap), fy + 5.0f, 4.0f * (0.4f + 0.6f * flap) + 1, BK_ALPHA(col, 180));
    }
    bk_line(fx, fy - 6, fx, fy + 7, 2.0f, BK_INK);
}

/* the pond: the garden mirrored, rippling with the hits, clearer with SPACE */
static void gd_pond(float t, float space, float energy)
{
    int y, x;
    for (y = GD_HORIZON; y < GD_H; y++) {
        int d = y - GD_HORIZON, sy = GD_HORIZON - 1 - (int)((float)d * 1.25f);
        float fade = (0.22f + 0.4f * space) * (1.0f - (float)d / (float)(GD_H - GD_HORIZON));
        int shift = (int)(sinf((float)d * 0.35f - t * 2.2f) * (1.0f + (float)d * 0.03f) * (1.0f + 3.0f * energy));
        uint32_t *o = &bk_px[(GD_Y + y) * BK_W + GD_X], *s;
        bk_draw_count(1024u);
        if (sy < 0)
            continue;
        s = &bk_px[(GD_Y + sy) * BK_W + GD_X];
        {   /* (red and blue in one sum, green in another: the same numbers as channel by channel; the weight once a
               row, the edge clamp only where the shifted row runs out) */
            uint32_t k = (uint32_t)(fade * 256.0f), nk = 256u - k;
            int x0 = shift < 0 ? -shift : 0, x1 = shift > 0 ? GD_W - shift : GD_W;   /* (where the shifted row is whole) */
            const uint32_t *ss = s + shift;
            if (x0 > GD_W)
                x0 = GD_W;
            if (x1 < x0)
                x1 = x0;
            for (x = 0; x < x0; x++) {           /* the left end: its first pixel, repeated */
                uint32_t c = s[0], b = o[x];
                uint32_t rb = ((c & 0xFF00FFu) * k + (b & 0xFF00FFu) * nk) >> 8, g = ((c & 0xFF00u) * k + (b & 0xFF00u) * nk) >> 8;
                o[x] = 0xFF000000u | (rb & 0xFF00FFu) | (g & 0xFF00u);
            }
            for (x = x0; x < x1; x++) {          /* the middle: no branches (the compiler can take several at once) */
                uint32_t c = ss[x], b = o[x];
                uint32_t rb = ((c & 0xFF00FFu) * k + (b & 0xFF00FFu) * nk) >> 8, g = ((c & 0xFF00u) * k + (b & 0xFF00u) * nk) >> 8;
                o[x] = 0xFF000000u | (rb & 0xFF00FFu) | (g & 0xFF00u);
            }
            for (x = x1; x < GD_W; x++) {        /* the right end: its last pixel, repeated */
                uint32_t c = s[GD_W - 1], b = o[x];
                uint32_t rb = ((c & 0xFF00FFu) * k + (b & 0xFF00FFu) * nk) >> 8, g = ((c & 0xFF00u) * k + (b & 0xFF00u) * nk) >> 8;
                o[x] = 0xFF000000u | (rb & 0xFF00FFu) | (g & 0xFF00u);
            }
        }
    }
    bk_rect(GD_X, GD_Y + GD_HORIZON, GD_W, 2, ARGB(60, 200, 220, 255));   /* the water's edge */
}

/* ---- the Sound Lab and the links, as creatures of the garden ---- */

/* living links: a glowing vine from the source track's plant to the destination's, arching over; a light runs
 * along it, brighter with the link's live value */
static void gd_vines(const md_patch *p, const gd_plant *pl, float t)
{
    for (unsigned k = 0; k < MD_LINKS; k++) {
        const md_link *l = &p->links[k];
        int a = l->source % 6, b = l->destination % 6;
        float live = fabsf(md_link_value(k)), ax, ay, bx, by, cx, cy, px = 0, py = 0;
        uint32_t col = bk_track_col(a);
        if (!l->depth)
            continue;
        ax = pl[a].tipx, ay = pl[a].tipy, bx = pl[b].tipx, by = pl[b].tipy;
        if (a == b)                              /* a link onto itself: a small loop */
            bx += 60.0f;
        cx = (ax + bx) * 0.5f, cy = (ay < by ? ay : by) - 70.0f - 20.0f * (float)k;
        for (int s = 0; s <= 24; s++) {
            float u = (float)s / 24.0f, x = (1 - u) * (1 - u) * ax + 2 * u * (1 - u) * cx + u * u * bx;
            float y = (1 - u) * (1 - u) * ay + 2 * u * (1 - u) * cy + u * u * by;
            if (s) {
                bk_line(px, py, x, y, 5.0f, BK_ALPHA(col, (uint32_t)(20 + 40 * live)));
                bk_line(px, py, x, y, 1.6f, BK_ALPHA(col, (uint32_t)(120 + 120 * live)));
            }
            if (s % 6 == 3)                       /* little leaves along it */
                bk_disc(x + ((s & 4) ? 4.0f : -4.0f), y - 3.0f, 3.0f, BK_ALPHA(col, 120));
            px = x, py = y;
        }
        {   /* the light travelling from source to destination */
            float u = fmodf(t * (0.35f + 0.3f * (float)k), 1.0f);
            float x = (1 - u) * (1 - u) * ax + 2 * u * (1 - u) * cx + u * u * bx, y = (1 - u) * (1 - u) * ay + 2 * u * (1 - u) * cy + u * u * by;
            bk_disc(x, y, 6.0f + 6.0f * live, BK_ALPHA(col, (uint32_t)(40 + 60 * live)));
            bk_disc(x, y, 2.4f, BK_ALPHA(BK_INK, 230));
        }
    }
}

/* (1.3) SYNTH in the sky, as its instrument is: GLASS HARMONICA glass bubbles rising, CZ CRYSTAL crystals that
   flash and fall, WIND FLUTE breath curling up, each voice placed by its note and as bright as it sounds; AURORA
   PAD bands of light breathing with the chord; the other instruments a spark where a note is struck */
static void gd_synth_sky(float t)
{
    int eng = (int)lab_v[P_SY_ENGINE] % SY_ENGINES;
    uint32_t col = LAB_MOD_COL[M_SYNTH];
    static float last_flash;
    if (lab_v[P_SY_LEVEL] <= 0 || bk_light)
        return;
    if (eng == SY_GLASS || eng == SY_CZ || eng == SY_FLUTE) {
        for (int v = 0; v < NV_N; v++) {
            float e = nv[v].on ? nv[v].env : 0, x, y, g;
            if (e < 0.01f)
                continue;
            e = e > 1 ? 1 : e;
            g = 0.35f + 0.65f * e;
            x = (float)GD_X + 60.0f + fmodf(log2f(nv[v].f / 55.0f) / 5.0f, 1.0f) * 600.0f;
            if (eng == SY_GLASS) {               /* a bubble of glass, rising as it fades */
                y = (float)GD_Y + 120.0f + 260.0f * e + (bk_calm ? 0 : 4.0f * sinf(t * 1.3f + (float)v));
                bk_disc(x, y, 16.0f + 10.0f * e, BK_ALPHA(col, (uint32_t)(26 * g)));
                bk_arc(x, y, 12.0f + 6.0f * e, 1.6f, 0, 6.3f, BK_ALPHA(ARGB(255, 230, 245, 255), (uint32_t)(170 * g)));
                bk_arc(x - 3, y - 3, 6.0f + 3.0f * e, 1.4f, 3.6f, 4.8f, BK_ALPHA(ARGB(255, 255, 255, 255), (uint32_t)(200 * g)));
            } else if (eng == SY_CZ) {           /* a crystal: four points, falling and turning as it fades */
                float a = bk_calm ? 0.4f : t * 0.8f + (float)v, r = 8.0f + 10.0f * e;
                y = (float)GD_Y + 380.0f - 240.0f * e;
                bk_disc(x, y, r * 1.8f, BK_ALPHA(col, (uint32_t)(30 * g)));
                for (int k = 0; k < 4; k++) {
                    float a0 = a + (float)k * 1.5708f;
                    bk_line(x, y, x + cosf(a0) * r, y + sinf(a0) * r, 2.0f, BK_ALPHA(ARGB(255, 225, 240, 255), (uint32_t)(220 * g)));
                }
                bk_disc(x, y, 2.5f, BK_ALPHA(BK_INK, (uint32_t)(255 * g)));
            } else {                             /* breath: wisps curling up from a low point */
                float bx = x, by = (float)(GD_Y + GD_HORIZON) - 40.0f;
                bk_disc(bx, by, 10.0f + 8.0f * e, BK_ALPHA(col, (uint32_t)(40 * g)));   /* (where the breath comes from) */
                for (int k = 0; k < 9; k++) {
                    float ph = bk_calm ? (float)k / 9.0f : fmodf(t * 0.45f + (float)k / 9.0f + (float)v * 0.17f, 1.0f);
                    bk_disc(bx + sinf(ph * 7.0f + (float)v) * 18.0f * ph, by - ph * 240.0f, 3.0f + 7.0f * ph,
                            BK_ALPHA(gd_mix(col, BK_INK, 0.5f), (uint32_t)((50.0f + 150.0f * e) * (1.0f - ph))));
                }
            }
        }
    } else if (eng == SY_PAD) {                  /* three bands of light high in the sky, breathing with the chord */
        float lv = sy_level * 14.0f;
        lv = lv > 1 ? 1 : lv;
        for (int b = 0; b < 3; b++) {
            float px = 0, py = 0;
            for (int i = 0; i <= 36; i++) {
                float u = (float)i / 36.0f, xx = (float)GD_X + 30.0f + u * 660.0f;
                float yy = (float)GD_Y + 120.0f + (float)b * 26.0f + 18.0f * sinf(u * 5.0f + (bk_calm ? 0 : t * 0.4f) + (float)b * 1.7f);
                if (i)
                    bk_line(px, py, xx, yy, 7.0f - (float)b * 1.5f, BK_ALPHA(gd_mix(col, ARGB(255, 150, 240, 200), (float)b * 0.4f),
                                                                    (uint32_t)((45.0f + 120.0f * lv) * (0.6f + 0.4f * sinf(u * 9.0f + t)))));
                px = xx, py = yy;
            }
        }
    } else if (sy_flash > 0.95f && last_flash <= 0.95f && !bk_calm) {   /* a note struck: a spark rises */
        float x = (float)GD_X + 60.0f + (float)(rand() % 600), y = (float)GD_Y + 200.0f + (float)(rand() % 200);
        for (int k = 0; k < 6; k++)
            gd_emit(x, y, sinf((float)k * 1.05f) * 40.0f, cosf((float)k * 1.05f) * 40.0f - 20.0f, 2.0f, col);
    }
    last_flash = sy_flash;
}

/* POLY: each sounding voice a lantern floating on the pond, placed by its note, as bright as its envelope */
static void gd_lanterns(float t)
{
    uint32_t col = LAB_MOD_COL[M_POLY];
    if (lab_v[P_PO_LEVEL] <= 0)
        return;
    for (int v = 0; v < PO_VOICES; v++) {
        float e = po_v[v].env, x, y;
        if (!po_v[v].on || e < 0.01f)
            continue;
        x = (float)GD_X + 50.0f + (float)(po_v[v].note - 36) / 60.0f * 620.0f;
        y = (float)(GD_Y + GD_HORIZON) + 60.0f + (float)(v % 3) * 26.0f + 3.0f * sinf(t * 1.6f + (float)v);
        x = x < (float)(GD_X + 20) ? (float)(GD_X + 20) : x > (float)(GD_X + GD_W - 20) ? (float)(GD_X + GD_W - 20) : x;
        float g = 0.45f + 0.55f * e;
        bk_disc(x, y, 34.0f * g, BK_ALPHA(ARGB(255, 255, 196, 120), (uint32_t)(36 * g)));
        bk_disc(x, y, 18.0f * g, BK_ALPHA(ARGB(255, 255, 210, 140), (uint32_t)(60 * g)));
        bk_rrect((int)x - 9, (int)y - 12, 18, 23, 8, gd_mix(gd_mix(col, ARGB(255, 255, 214, 150), 0.6f), ARGB(255, 120, 80, 50), 1.0f - g));
        bk_disc(x, y, 4.0f, BK_ALPHA(ARGB(255, 255, 250, 225), (uint32_t)(255 * g)));   /* the flame */
        bk_line(x - 6, y - 13, x + 6, y - 13, 2.0f, ARGB(220, 70, 52, 36));
        bk_line(x - 6, y + 11, x + 6, y + 11, 2.0f, ARGB(220, 70, 52, 36));
        bk_line(x, y - 13, x, y - 19, 1.2f, ARGB(200, 70, 52, 36));
        for (int r = 0; r < 3; r++)              /* its reflection, broken by the water */
            bk_rect((int)x - 6 + r * 2, (int)y + 20 + r * 6, 12 - r * 4, 2, BK_ALPHA(ARGB(255, 255, 214, 150), (uint32_t)(110 * g / (float)(r + 1))));
    }
}

/* RANDOM: a frog on a lily pad below the track it plays; it jumps when RANDOM plays a note */
static void gd_frog(const md_patch *p, const gd_plant *pl, float t)
{
    static uint32_t last_w;
    static float jump_t = -10;
    int tr = (int)lab_v[P_RN_TRACK];
    float x, y, h, age;
    uint32_t green = ARGB(255, 120, 200, 110), pad = ARGB(255, 50, 120, 70);
    (void)p;
    if (lab_v[P_RN_ON] <= 0.5f)
        return;
    if (rn_hist_w != last_w) {
        if (rn_hist_on[(rn_hist_w - 1) % RN_HIST])
            jump_t = t;
        last_w = rn_hist_w;
    }
    x = tr < 6 ? pl[tr].x + 34.0f : (float)(GD_X + 30);   /* (POLY: the left bank) */
    y = (float)(GD_Y + GD_HORIZON) + 34.0f;
    for (int k = -3; k <= 3; k++)                 /* the pad */
        bk_disc(x + (float)k * 6.0f, y + 2.0f, 12.0f - (float)(k * k) * 0.8f, pad);
    bk_line(x, y + 2, x + 14, y - 4, 2.0f, ARGB(255, 8, 12, 20));   /* its notch */
    age = t - jump_t;
    h = age >= 0 && age < 0.4f ? sinf(age / 0.4f * 3.1416f) * 26.0f : 0;
    bk_disc(x, y - 6.0f - h, 9.0f, green);         /* the frog: body, eyes */
    bk_disc(x - 5.0f, y - 14.0f - h, 3.6f, green);
    bk_disc(x + 5.0f, y - 14.0f - h, 3.6f, green);
    bk_disc(x - 5.0f, y - 14.5f - h, 1.6f, ARGB(255, 10, 14, 20));
    bk_disc(x + 5.0f, y - 14.5f - h, 1.6f, ARGB(255, 10, 14, 20));
    if (h > 0)                                    /* legs out in the jump */
        for (int s = -1; s <= 1; s += 2)
            bk_line(x + (float)s * 6.0f, y - 2.0f - h, x + (float)s * 13.0f, y + 4.0f - h * 0.6f, 2.4f, green);
}

/* SHIMMER: a band of light high in the sky, sparkling, as strong as its mix */
static void gd_shimmer(float t, float mix)
{
    float px = 0, py = 0;
    for (int x = 0; x <= GD_W; x += 20) {
        float u = (float)x / (float)GD_W, y = (float)GD_Y + 150.0f + 30.0f * sinf(u * 4.0f + t * 0.3f) + 12.0f * sinf(u * 11.0f - t * 0.5f);
        if (x) {
            uint32_t c = gd_mix(ARGB(255, 170, 230, 255), ARGB(255, 230, 190, 255), 0.5f + 0.5f * sinf(u * 6.0f + t * 0.4f));
            bk_line(px, py, (float)(GD_X + x), y, 24.0f, BK_ALPHA(c, (uint32_t)(22 * mix)));
            bk_line(px, py, (float)(GD_X + x), y, 8.0f, BK_ALPHA(c, (uint32_t)(40 * mix)));
            bk_line(px, py, (float)(GD_X + x), y, 2.0f, BK_ALPHA(c, (uint32_t)(150 * mix)));
        }
        if ((x / 20) % 3 == 0) {
            float tw = 0.5f + 0.5f * sinf(t * 5.0f + (float)x);
            bk_disc((float)(GD_X + x), y + 6.0f * sinf((float)x), 1.5f + tw, ARGB((uint32_t)(200 * mix * tw), 255, 255, 255));
        }
        px = (float)(GD_X + x), py = y;
    }
}

/* LOCK: a lantern on a post on the bank: the garden is kept as it is */
static void gd_lock_lantern(float t)
{
    float x = (float)(GD_X + GD_W - 8), y = (float)(GD_Y + GD_HORIZON) - 70.0f, fl = 0.85f + 0.15f * sinf(t * 7.0f) * sinf(t * 3.1f);
    bk_line(x, y + 10, x, (float)(GD_Y + GD_HORIZON), 3.0f, ARGB(255, 90, 70, 50));
    bk_line(x, y - 4, x - 14, y - 4, 2.0f, ARGB(255, 90, 70, 50));
    bk_disc(x - 14, y + 12, 30.0f * fl, ARGB(30, 255, 200, 120));
    bk_rrect((int)x - 22, (int)y + 2, 16, 20, 5, ARGB(230, 255, 210, 140));
    bk_text((int)x - 34, (int)y + 28, &FONT_T, 1, "LOCK", ARGB(160, 255, 220, 160));
}

/* VOICES: a bird on a branch at the garden's left edge opens its beak as the voice speaks; when a new sentence
 * starts, the voice's title and credit float by on a little scroll */
static void gd_bird(float t)
{
    static int last_phrase = -2;
    static float scroll_t = -10;
    float bx = (float)GD_X + 70.0f, by = (float)GD_Y + 150.0f, open = vo_env, bob = sinf(t * 2.0f) * 1.5f;
    uint32_t col = LAB_MOD_COL[M_VOICES], dark = ARGB(255, 60, 44, 36);
    if (!lab_active(M_VOICES) || !vo_cur || !vo_cur->ok)
        return;
    bk_line((float)GD_X, by + 22.0f, bx + 34.0f, by + 14.0f, 5.0f, ARGB(255, 70, 52, 40));   /* the branch */
    bk_line(bx + 10.0f, by + 18.0f, bx + 24.0f, by + 2.0f, 2.5f, ARGB(255, 70, 52, 40));
    gd_leaf(bx + 24.0f, by + 2.0f, 1, 18, 6, ARGB(255, 110, 170, 110), 0.7f);
    bk_disc(bx, by + bob, 13.0f, col);           /* body, head, wing, eye */
    bk_disc(bx + 11.0f, by - 9.0f + bob, 8.5f, col);
    bk_disc(bx - 3.0f, by + 2.0f + bob, 7.0f, gd_mix(col, dark, 0.35f));
    bk_line(bx - 12.0f, by + 4.0f + bob, bx - 24.0f, by + 10.0f + bob, 4.0f, gd_mix(col, dark, 0.3f));   /* tail */
    bk_disc(bx + 14.0f, by - 11.0f + bob, 1.8f, ARGB(255, 10, 14, 20));
    {   /* the beak: two lines, as wide open as the voice is loud */
        float a = 0.08f + 0.45f * open;
        bk_line(bx + 18.0f, by - 9.0f + bob, bx + 27.0f, by - 9.0f + bob - a * 10.0f, 2.6f, ARGB(255, 240, 180, 80));
        bk_line(bx + 18.0f, by - 7.0f + bob, bx + 27.0f, by - 7.0f + bob + a * 10.0f, 2.6f, ARGB(255, 240, 180, 80));
    }
    if (open > 0.15f)                            /* notes of the voice drifting out */
        for (int k = 0; k < 3; k++) {
            float u = fmodf(t * 0.8f + (float)k * 0.33f, 1.0f);
            bk_disc(bx + 32.0f + u * 60.0f, by - 12.0f - u * 30.0f + sinf(u * 9.0f + (float)k) * 6.0f, 2.5f, BK_ALPHA(col, (uint32_t)(200 * (1 - u) * open)));
        }
    if (vo_now_phrase != last_phrase) {
        if (last_phrase != -2 && vo_now_phrase >= 0)
            scroll_t = t;
        last_phrase = vo_now_phrase;
    }
    if (t - scroll_t < 4.0f) {                   /* the scroll with the title */
        float u = (t - scroll_t) / 4.0f, a = u < 0.15f ? u / 0.15f : u > 0.8f ? (1.0f - u) / 0.2f : 1.0f;
        int w = (bk_text_w(&FONT_H, vo_cur->title, 1) > bk_text_w(&FONT_T, vo_cur->credit, 1) ? bk_text_w(&FONT_H, vo_cur->title, 1) : bk_text_w(&FONT_T, vo_cur->credit, 1)) + 28;
        int sx = GD_X + 120, sy = GD_Y + 104 - (int)(u * 14.0f);
        bk_rrect(sx, sy, w, vo_cur->credit[0] ? 40 : 24, 8, BK_ALPHA(ARGB(255, 240, 226, 196), (uint32_t)(220 * a)));
        bk_text(sx + 14, sy + 4, &FONT_H, 1, vo_cur->title, BK_ALPHA(ARGB(255, 40, 30, 20), (uint32_t)(255 * a)));
        if (vo_cur->credit[0])
            bk_text_fit(sx + 14, sy + 22, &FONT_T, vo_cur->credit, w - 24, BK_ALPHA(ARGB(255, 90, 70, 50), (uint32_t)(255 * a)));
    }
}

/* ------------------------------------------- musical sky and rain (0.74) --- */
/* Asked for by Yimiblú through Codex: animations that follow the music. Both take what the engine already reports
   for each track (its note events and the pitch of its last note, md_status), never an analysis of the sound; the
   picture follows the note in the frame it is heard, the sound never waits for a picture.
   AURORA (the journey Aurora Polaris, and every Biome whose sky has an aurora): six ribbons, one per track in its
   colour; a ribbon flares on each of its notes and floats higher the higher the note, its glow grows with the level.
   TERRA PLUVIA (the journey of that name): it rains in the garden; each note splashes at the foot of its plant - a
   crown of drops and a ring on the water - while a streak shows where the drop came from. At most 12 splashes.
   MD_GARDEN_FX=aurora|rain shows either in any Biome (to look at, and for the tests) */
enum { GFX_NONE, GFX_AURORA, GFX_RAIN };
static float gfx_glow[6], gfx_y[6] = {-1, -1, -1, -1, -1, -1};
#define GFX_SPLASH 12
static struct { float x, y, age; uint32_t col; int on; } gfx_sp[GFX_SPLASH];
static int gfx_sp_w;
static uint32_t gfx_notes;                       /* (how many notes the sky or the rain answered: the tests) */
uint32_t bk_gfx_notes(void) { return gfx_notes; }

static int gfx_forced = -1;
void bk_garden_fx(int mode) { gfx_forced = mode; }   /* (0 none forced, 1 aurora, 2 rain: the tests) */
static int gfx_mode(const md_patch *p)
{
    int forced = gfx_forced;
    md_itinera_view iv;
    if (forced < 0) {
        const char *e = getenv("MD_GARDEN_FX");
        forced = gfx_forced = !e ? 0 : !strcmp(e, "aurora") ? GFX_AURORA : !strcmp(e, "rain") ? GFX_RAIN : 0;
    }
    if (forced)
        return forced;
    md_itinera_read(&iv);
    if (iv.active && iv.preset == 5)             /* (Terra Pluvia: the sixth journey) */
        return GFX_RAIN;
    if ((iv.active && iv.preset == 6) || ((uint32_t)(p->biome_id + 7) * 2654435761u >> 15) % 5u == 4u)   /* Aurora Polaris, or a sky with an aurora */
        return GFX_AURORA;
    return GFX_NONE;
}
/* a note of track i (pitch note): the aurora flares, or a splash at the plant's foot */
static void gfx_note(int mode, int i, float x, float foot, float note, uint32_t col)
{
    gfx_notes++;
    if (mode == GFX_AURORA)
        gfx_glow[i] = 1.0f;
    else if (mode == GFX_RAIN) {
        int k = gfx_sp_w++ % GFX_SPLASH;
        gfx_sp[k].x = x, gfx_sp[k].y = foot, gfx_sp[k].age = 0, gfx_sp[k].col = col, gfx_sp[k].on = 1;
    }
    (void)note;
}
static void gfx_aurora(float t, float dt, const md_stats *st, int n)
{
    for (int i = 0; i < 6; i++) {
        float note = (float)(st->last_note[i] > 0 ? st->last_note[i] : 60), target = (float)GD_Y + 230.0f - (note - 48.0f) * 3.2f;
        float lv = bk_level[i], g = gfx_glow[i];
        uint32_t c = bk_track_col(i);
        target = target < (float)GD_Y + 70.0f ? (float)GD_Y + 70.0f : target > (float)GD_Y + 300.0f ? (float)GD_Y + 300.0f : target;
        if (gfx_y[i] < 0)
            gfx_y[i] = target;
        gfx_y[i] += (target - gfx_y[i]) * (1.0f - expf(-dt * 3.0f));   /* (it floats there, it does not jump) */
        gfx_glow[i] *= expf(-dt * 2.5f);
        if (i >= n)
            continue;
        for (int x = 0; x < 140; x += bk_light ? 20 : 14) {   /* the ribbon: soft columns, overlapping its neighbours */
            float fx = (float)GD_X + (float)i * 120.0f - 10.0f + (float)x, u = (float)x / 140.0f;
            float y = gfx_y[i] + 14.0f * sinf(u * 4.0f + t * 0.6f + (float)i * 1.3f), fade = sinf(u * 3.1416f);
            uint32_t a = (uint32_t)((18.0f + 44.0f * lv + 120.0f * g) * fade);
            if (fx < (float)GD_X || fx > (float)(GD_X + GD_W))
                continue;
            bk_disc(fx, y, 10.0f + 6.0f * g, BK_ALPHA(c, a / 2u));
            if (lv > 0.2f || g > 0.1f)           /* (its lower veil only while the track sounds: cheaper) */
                bk_disc(fx, y + 14.0f + 10.0f * g, 7.0f, BK_ALPHA(c, a / 3u));
            if (g > 0.15f)
                bk_disc(fx, y - 4.0f, 3.0f + 2.0f * g, BK_ALPHA(gd_mix(c, BK_INK, 0.5f), (uint32_t)(200.0f * g * fade)));
        }
    }
}
static void gfx_rain(float t, float dt)
{
    if (!bk_light)
        for (int k = 0; k < 18; k++) {           /* the rain itself, light and steady (short streaks: cheap) */
            uint32_t h = (uint32_t)(k + 3) * 2654435761u;
            float x = (float)GD_X + (float)(h % 700u) + 10.0f, life = fmodf(t * (0.9f + (float)((h >> 8) & 7u) * 0.08f) + (float)(h & 255u) / 255.0f, 1.0f);
            float y = (float)GD_Y + 40.0f + life * (float)(GD_HORIZON - 40);
            bk_line(x, y - 12.0f, x - 2.0f, y, 1.4f, ARGB(70, 180, 210, 255));
        }
    for (int k = 0; k < GFX_SPLASH; k++) {
        float a, u;
        if (!gfx_sp[k].on)
            continue;
        a = gfx_sp[k].age;
        if ((gfx_sp[k].age += dt) > 0.9f) {
            gfx_sp[k].on = 0;
            continue;
        }
        u = a / 0.9f;
        if (a < 0.18f)                           /* where it came from: a streak, already landed */
            bk_line(gfx_sp[k].x, gfx_sp[k].y - 120.0f * (1.0f - a / 0.18f) - 20.0f, gfx_sp[k].x, gfx_sp[k].y - 4.0f, 2.0f, ARGB((uint32_t)(200.0f * (1.0f - a / 0.18f)), 200, 230, 255));
        bk_arc(gfx_sp[k].x, gfx_sp[k].y + 4.0f, 6.0f + 34.0f * u, 1.6f, 3.4f, 6.0f, BK_ALPHA(gfx_sp[k].col, (uint32_t)(180.0f * (1.0f - u))));   /* the ring */
        for (int d = 0; d < 6; d++) {            /* the crown: drops thrown up and out, falling back */
            float ang = 3.1416f + 0.45f + (float)d * 0.45f, sp = 70.0f + (float)(d % 3) * 18.0f;
            float dx = cosf(ang) * sp * a, dy = sinf(ang) * sp * a + 220.0f * a * a;
            if (dy > 6.0f)
                continue;
            bk_disc(gfx_sp[k].x + dx, gfx_sp[k].y + dy, 2.4f * (1.0f - u) + 0.8f, ARGB((uint32_t)(230.0f * (1.0f - u)), 210, 235, 255));
        }
    }
    (void)t;
}

static void bk_draw_garden(float t)
{
    const md_patch *p = md_state();
    const md_stats *st = md_status();
    static uint32_t bg_at, last_ev[6];
    static int bg_ok;
    static float last_t;
    float dt = t - last_t, density = (float)p->macro[0] / 1000.0f, motion = (float)p->macro[1] / 1000.0f;
    float space = (float)p->macro[3] / 1000.0f, energy = 0;
    uint32_t now = bk_ms();
    gd_plant pl[6];
    int n = md_active_tracks(), i, y;
    char buf[64];
    static int last_biome = -2;
    static float sprout_t = -10;
    float sprout;
    last_t = t;
    dt = dt < 0 || dt > 0.25f ? 0.033f : dt;
    if (p->biome_id != last_biome) {             /* another Biome: its garden sprouts from the ground */
        if (last_biome != -2) {
            sprout_t = t;
            for (int k = 0; k < 24; k++)          /* and seeds fly up from where it grows */
                gd_emit((float)GD_X + 30.0f + (float)k * 28.0f, (float)(GD_Y + GD_HORIZON - 6), sinf((float)k * 2.3f) * 30.0f,
                        -60.0f - (float)((k * 37) % 50), 2.2f, bk_track_col(k % 6));
        }
        last_biome = p->biome_id;
        bg_ok = 0;
    }
    sprout = t - sprout_t < 1.8f ? (t - sprout_t) / 1.8f : 1.0f;
    sprout = sprout * sprout * (3.0f - 2.0f * sprout);
    /* (1.5.8) on the Miyoo (the columns at half pace) the light picture's sky every 1.2 s: its stars and glow drift slowly */
    if (!bg_ok || now - bg_at >= (bk_light ? GD_BG_MS * (bk_side_half ? 6u : 3u) : GD_BG_MS)) {   /* the sky, kept */
        bk_clip_x0 = GD_X, bk_clip_x1 = GD_X + GD_W;   /* (1.5.3: the sky only inside the stage - it is kept for it alone) */
        gd_sky(t, p);
        bk_clip_x0 = 0, bk_clip_x1 = BK_W;
        bk_draw_yield();
        for (y = 0; y < GD_H; y++)
            memcpy(&gd_bg[y * GD_W], &bk_px[(GD_Y + y) * BK_W + GD_X], GD_W * 4u);
        bg_at = now, bg_ok = 1;
    } else
        for (y = 0; y < GD_H; y++)
            memcpy(&bk_px[(GD_Y + y) * BK_W + GD_X], &gd_bg[y * GD_W], GD_W * 4u);

    {   /* the moon crosses the sky with four bars */
        float u = (float)(st->step % 64u) / 64.0f + (md_playing() ? 0 : 0), a = 3.1416f * (0.1f + 0.8f * u);
        float mx = (float)GD_X + 360.0f - cosf(a) * 300.0f, my = (float)GD_Y + 300.0f - sinf(a) * 230.0f;
        float dawn = gd_dawn(p);                 /* towards B the moon becomes the sun */
        uint32_t body = gd_mix(ARGB(255, 250, 246, 228), ARGB(255, 255, 200, 110), dawn);
        bk_disc(mx, my, 54 + 30 * dawn, BK_ALPHA(body, (uint32_t)(8 + 14 * dawn)));
        bk_disc(mx, my, 34 + 10 * dawn, BK_ALPHA(body, (uint32_t)(16 + 20 * dawn)));
        if (dawn > 0.25f)
            for (int r = 0; r < 12; r++) {          /* rays */
                float ra = (float)r * 0.5236f + t * 0.1f;
                bk_line(mx + cosf(ra) * 28.0f, my + sinf(ra) * 28.0f, mx + cosf(ra) * (40.0f + 8.0f * dawn), my + sinf(ra) * (40.0f + 8.0f * dawn),
                        2.4f, BK_ALPHA(body, (uint32_t)(200.0f * (dawn - 0.25f) / 0.75f)));
            }
        bk_disc(mx, my, 20, BK_ALPHA(body, 245));
        if (dawn < 0.6f) {
            uint32_t sea = ARGB((uint32_t)(40.0f * (1.0f - dawn / 0.6f)), 120, 120, 140);
            bk_disc(mx - 6, my - 5, 4.5f, sea);  /* seas */
            bk_disc(mx + 7, my + 4, 3.5f, sea);
            bk_disc(mx + 2, my - 10, 2.5f, sea);
        }
    }
    if (lab_active(M_SHIMMER))
        gd_shimmer(t, lab_v[P_SH_MIX] / 100.0f);
    int gfx = gfx_mode(p);                       /* (0.74) the musical sky / rain */
    if (gfx == GFX_AURORA)
        gfx_aurora(t, dt, st, n);
    for (i = 0; i < 6; i++) {                     /* the plants: species by model, size by level, place by pan */
        const md_track *tr = &p->track[i];
        float lv = bk_level[i];
        pl[i].i = i;
        pl[i].muted = i >= n || tr->mute;
        {   /* muting is a slow falling asleep (and waking): colour, height and posture follow */
            static float mf[6];
            mf[i] += ((pl[i].muted ? 1.0f : 0.0f) - mf[i]) * (1.0f - expf(-dt * 2.2f));
            gd_mute_f[i] = mf[i];
            pl[i].col = gd_mix(bk_track_col(i), ARGB(255, 70, 78, 92), mf[i]);
            pl[i].muted = mf[i] > 0.5f;
        }
        if (p->freeze)                            /* frozen: frosted */
            pl[i].col = gd_mix(pl[i].col, GD_ICE, 0.5f);
        pl[i].x = (float)GD_X + 70.0f + (float)i * 116.0f + ((float)tr->pan - 500.0f) * 0.05f;
        pl[i].foot = (float)(GD_Y + GD_HORIZON) - 4.0f - (float)((i * 5) % 3) * 6.0f;
        pl[i].h = (170.0f + 200.0f * (float)tr->level / 1000.0f) * (0.84f + 0.16f * lv) * (i >= n ? 0.5f : 1.0f) * (1.0f - 0.16f * gd_mute_f[i]);
        {   /* sprouting after a Biome change: one after the other */
            float g = (sprout * 1.6f - (float)i * 0.1f);
            g = g < 0.02f ? 0.02f : g > 1 ? 1 : g;
            pl[i].h *= g;
        }
        pl[i].sway = pl[i].muted ? 0 : bk_sway[i] * (8.0f + 26.0f * motion) + sinf(t * (0.8f + motion) + (float)i) * 16.0f * motion;
        if (p->freeze)
            pl[i].sway *= 0.12f;
        pl[i].sway += gd_mute_f[i] * ((i & 1) ? 22.0f : -22.0f);   /* asleep: it bows */
        pl[i].bloom = (1.0f - gd_mute_f[i]) * (sprout < 1.0f ? sprout * bk_bloom[i] : bk_bloom[i]);
        pl[i].lv = lv;
        energy += lv / 6.0f;
        gd_draw_species(&pl[i], gd_species(i), t);
        {   /* a turn: the chosen plant throws up sparks, its pond trembles */
            static uint32_t turn_seen;
            if (i == dream_ui.track && bk_kick_seen != turn_seen) {
                turn_seen = bk_kick_seen;
                for (int sp = 0; sp < 7; sp++)
                    gd_emit(pl[i].tipx, pl[i].tipy, sinf((float)(sp * 7 + (int)turn_seen)) * 70.0f, -50.0f - (float)((sp * 23) % 60), 2.4f, pl[i].col);
                if (!p->freeze)
                    gd_ripple(pl[i].x, (float)(GD_Y + GD_HORIZON + 26), t, pl[i].col);
            }
        }
        if (st->events[i] != last_ev[i]) {        /* a hit: a spark goes up, higher for higher notes */
            float note = (float)(st->last_note[i] > 0 ? st->last_note[i] : 60);
            last_ev[i] = st->events[i];
            if (!pl[i].muted && gfx)
                gfx_note(gfx, i, pl[i].x, pl[i].foot, note, pl[i].col);
            if (!pl[i].muted && !p->freeze)
                gd_ripple(pl[i].x, (float)(GD_Y + GD_HORIZON + 22 + (i % 3) * 16), t, pl[i].col);
            if (!pl[i].muted) {
                gd_emit(pl[i].tipx, pl[i].tipy, sinf(t * 7.0f + (float)i) * 14.0f, -40.0f - (note - 36.0f) * 1.6f, 3.0f, pl[i].col);
                if (gd_species(i) == SP_DANDELION)
                    for (int k = 0; k < 3; k++)
                        gd_emit(pl[i].tipx, pl[i].tipy, 20.0f + 30.0f * motion + (float)k * 8.0f, -10.0f - (float)k * 6.0f, 1.6f, BK_INK);
            }
        }
        if (pl[i].muted && i < n) {               /* asleep */
            float z = fmodf(t * 0.5f + (float)i * 0.3f, 1.0f);
            bk_text((int)pl[i].tipx + 10 + (int)(z * 10), (int)(pl[i].tipy - 20 - z * 30), &FONT_H, 1, "z", BK_ALPHA(BK_DIM, (uint32_t)(220 * (1 - z))));
        }
    }
    if (gfx == GFX_RAIN)
        gfx_rain(t, dt);
    {   /* the butterfly flies over to the chosen track's plant */
        static float bx = -1, by = -1;
        float tx = pl[dream_ui.track % 6].tipx, ty = pl[dream_ui.track % 6].tipy, k = 1.0f - expf(-dt * 3.5f), dist;
        if (bx < 0)
            bx = tx, by = ty;
        dist = fabsf(tx - bx);
        bx += (tx - bx) * k;
        by += (ty - by) * k - dist * 0.04f * k * 6.0f;   /* (a little arc upwards on the way) */
        gd_butterfly(bx, by, t, ARGB(255, 255, 236, 170));
    }
    gd_vines(p, pl, t);
    for (i = 0; i < GD_SPARKS; i++)               /* the sparks rise, drift with the wind and fade */
        if (gd_spark[i].life > 0) {
            gd_spark[i].x += (gd_spark[i].vx + 20.0f * motion) * dt;
            gd_spark[i].y += gd_spark[i].vy * dt;
            gd_spark[i].vy *= 1.0f - 0.6f * dt;
            gd_spark[i].life -= dt * 0.45f;
            if (gd_spark[i].y > (float)(GD_Y + 8) && gd_spark[i].x > (float)GD_X && gd_spark[i].x < (float)(GD_X + GD_W)) {
                bk_disc(gd_spark[i].x, gd_spark[i].y, gd_spark[i].size * 2.6f, BK_ALPHA(gd_spark[i].col, bk_u8(40 * gd_spark[i].life)));
                bk_disc(gd_spark[i].x, gd_spark[i].y, gd_spark[i].size, BK_ALPHA(gd_spark[i].col, bk_u8(230 * gd_spark[i].life)));
            }
        }
    {   /* the beat: every fourth step the fireflies light up together */
        static uint32_t last_step;
        if (st->step != last_step) {
            last_step = st->step;
            if (md_playing() && st->step % 4u == 0)
                gd_beat_t = t;
        }
    }
    for (i = 0; i < 6 + (int)(density * 40.0f); i++) {   /* fireflies: DENSITY */
        uint32_t h = (uint32_t)i * 2246822519u;
        float fx = (float)GD_X + (float)(h % GD_W) + 30.0f * sinf(t * 0.3f + (float)i), blink = sinf(t * (1.0f + (float)(h >> 28) * 0.2f) + (float)i);
        float fy = (float)GD_Y + 180.0f + (float)((h >> 9) % 320u) + 20.0f * sinf(t * 0.5f + (float)i * 1.3f);
        float pulse = md_playing() ? expf(-(t - gd_beat_t) * 5.0f) : 0;
        blink = (blink > 0 ? blink * 0.65f : 0) + pulse * 0.8f;
        blink = blink > 1 ? 1 : blink;
        if (blink > 0.2f) {
            bk_disc(fx, fy, 6.0f + 3.0f * pulse, BK_ALPHA(ARGB(255, 210, 255, 150), (uint32_t)(30 * blink)));
            bk_disc(fx, fy, 1.8f, BK_ALPHA(ARGB(255, 230, 255, 190), (uint32_t)(230 * blink)));
        }
    }
    bk_draw_yield();
    if (!bk_light)
        gd_pond(t, space, energy);
    if (!p->freeze)
        gd_ripples(t);
    else {                                        /* frozen: ice on the pond, cracks, snow falling */
        bk_rect(GD_X, GD_Y + GD_HORIZON + 2, GD_W, GD_H - GD_HORIZON - 2, ARGB(60, 190, 225, 255));
        for (i = 0; i < 14; i++) {
            uint32_t h = (uint32_t)i * 2654435761u;
            float cx = (float)(GD_X + (int)(h % GD_W)), cy = (float)(GD_Y + GD_HORIZON + 20 + (int)((h >> 9) % 170u));
            for (int k = 0; k < 3; k++) {
                float a = (float)((h >> (12 + k * 3)) & 7u) * 0.785f;
                bk_line(cx, cy, cx + cosf(a) * (14.0f + (float)k * 6.0f), cy + sinf(a) * 5.0f, 1.0f, ARGB(90, 235, 248, 255));
            }
        }
        for (i = 0; i < 60; i++) {
            uint32_t h = (uint32_t)i * 2246822519u;
            float sx = (float)(GD_X + (int)(h % GD_W)) + 12.0f * sinf(t * 0.7f + (float)i);
            float sy = (float)GD_Y + fmodf((float)((h >> 8) % 720u) + t * (14.0f + (float)(i % 4) * 6.0f), 720.0f);
            bk_disc(sx, sy, 1.2f + (float)(i % 3) * 0.6f, ARGB(200, 240, 248, 255));
        }
    }

    gd_lanterns(t);
    gd_synth_sky(t);                             /* (1.3) */
    gd_frog(p, pl, t);
    gd_bird(t);
    if (p->locked)
        gd_lock_lantern(t);
    if ((bk_held >> BK_FN) & 1u) {               /* play mode: eight stepping stones over the pond, one per scale tone */
        static const char *const keys[8] = {"LEFT", "DOWN", "RIGHT", "UP", "Y", "X", "B", "A"};
        static uint32_t last_lit;
        int poly = lab_v[P_PO_LEVEL] > 0 && lab_v[P_PO_PLAYMODE] > 0.5f;
        uint32_t col = poly ? LAB_MOD_COL[M_POLY] : bk_track_col(dream_ui.track % 6);
        for (i = 0; i < 8; i++) {
            float sx = (float)GD_X + 80.0f + (float)i * 80.0f, sy = (float)(GD_Y + GD_HORIZON) + 96.0f + ((i & 1) ? 14.0f : 0.0f);
            int lit = (int)((bk_notes_lit >> i) & 1u), note = bk_scale_note(i);
            if (lit && !((last_lit >> i) & 1u) && !p->freeze)
                gd_ripple(sx, sy + 8.0f, t, col);
            for (int k = -3; k <= 3; k++)         /* a flat stone */
                bk_disc(sx + (float)k * 7.0f, sy + 2.0f - (float)(k * k) * 0.4f, 13.0f - (float)(k * k) * 0.6f,
                        lit ? col : ARGB(255, 70, 78, 92));
            if (lit)
                bk_disc(sx, sy, 34, BK_ALPHA(col, 40));
            snprintf(buf, sizeof buf, "%s%d", BK_NOTE_NAMES[note % 12], note / 12 - 1);
            bk_text((int)sx - bk_text_w(&FONT_H, buf, 1) / 2, (int)sy - 6, &FONT_H, 1, buf, lit ? ARGB(255, 10, 14, 22) : BK_INK);
            bk_text((int)sx - bk_text_w(&FONT_T, keys[i], 1) / 2, (int)sy + 18, &FONT_T, 1, keys[i], BK_FAINT);
        }
        last_lit = bk_notes_lit;
        bk_text(GD_X + 24, GD_Y + GD_H - 28, &FONT_H, 1, poly ? "PLAY MODE  /  POLY" : "PLAY MODE", col);
    }
    /* the words: the Biome's name, the tempo; the weather when K turns; the app's messages */
    bk_disc((float)(GD_X + 46), (float)(GD_Y + 44), 26, BK_ALPHA(bk_track_col(p->biome_id), (uint32_t)(40 * sprout)));   /* its sign */
    bk_biome_sigil(p->biome_id, (float)(GD_X + 46), (float)(GD_Y + 44), 20.0f * sprout + 1.0f, bk_track_col(p->biome_id), t);
    bk_text(GD_X + 82, GD_Y + 20 + (int)(10.0f * (1.0f - sprout)), &FONT_XM, 1, md_biome_name(p->biome_id), BK_ALPHA(BK_INK, (uint32_t)(230 * sprout)));
    if (wd_current >= 0) {                        /* a signpost: the world we are in, and whether we wander */
        int sx = GD_X + 82, sy = GD_Y + 80, w;
        snprintf(buf, sizeof buf, "%s%s", wd_w[wd_current].name, wd_wander ? "  >" : "");
        w = bk_text_w(&FONT_H, buf, 1) + 24;
        bk_line((float)(sx + 14), (float)(sy + 22), (float)(sx + 14), (float)(sy + 40), 3.0f, ARGB(255, 110, 82, 56));
        bk_rrect(sx, sy, w, 24, 6, ARGB(235, 132, 100, 68));
        bk_rframe(sx, sy, w, 24, 6, 1, ARGB(255, 168, 132, 92));
        bk_text(sx + 12, sy + 5, &FONT_H, 1, buf, ARGB(255, 250, 238, 214));
        if (wd_wander) {                          /* how long until the next world */
            uint32_t left = (uint32_t)wd_wander_min * 60000u - (bk_ms() - wd_entered_at);
            snprintf(buf, sizeof buf, "WANDERING ON IN %u:%02u", left / 60000u, left / 1000u % 60u);
            bk_text(sx + w + 10, sy + 6, &FONT_T, 1, buf, BK_FAINT);
        }
    }
    snprintf(buf, sizeof buf, "%d BPM  %s%s%s", md_tempo_now(), md_playing() ? "PLAYING" : "RESTING", p->freeze ? "  /  FROZEN" : "",
             gd_dawn(p) > 0 ? (gd_dawn(p) > 0.5f ? "  /  DAWN" : "  /  NIGHT TURNING") : "");
    bk_text(GD_X + 84, GD_Y + 54, &FONT_H, 1, buf, md_playing() ? BK_DIM : BK_FAINT);
    for (i = 0; i < 4; i++)
        if (now - bk_focus_at[i] < 1400u && bk_focus_at[i]) {
            float a = 1.0f - (float)(now - bk_focus_at[i]) / 1400.0f;
            snprintf(buf, sizeof buf, "%s  %d", GD_WEATHER[i], p->macro[i] / 10);
            int w = bk_text_w(&FONT_H, buf, 1);   /* top right, beside the name */
            bk_rrect(GD_X + GD_W - 24 - w - 32, GD_Y + 22, w + 32, 30, 15, BK_ALPHA(ARGB(255, 10, 14, 26), (uint32_t)(210 * a)));
            bk_text(GD_X + GD_W - 24 - w - 16, GD_Y + 29, &FONT_H, 1, buf, BK_ALPHA(BK_INK, (uint32_t)(255 * a)));
        }
    if (dream_ui.msg[0] && (uint32_t)(plat_ms() - dream_ui.msg_start) < 1400u && plat_ms() - bk_nt_at > 3200u) {   /* (1.3: a notice says it) */
        int w = bk_text_w(&FONT_H, dream_ui.msg, 1);
        bk_rrect(GD_X + GD_W - 24 - w - 36, GD_Y + 60, w + 36, 30, 15, ARGB(230, 250, 240, 220));
        bk_text(GD_X + GD_W - 24 - w - 18, GD_Y + 67, &FONT_H, 1, dream_ui.msg, ARGB(255, 20, 24, 36));
    }
    bk_rframe(GD_X - 6, GD_Y - 6, GD_W + 12, GD_H + 12, 18, 3, BK_ALPHA(gd_mix(bk_track_col(dream_ui.track % 6), BK_INK, 0.3f), 120));
}

/* ---- the garden's night behind every full view and the Sound Lab ----
 * the sky (its glow in the view's colour), stars, two dark hills; kept and drawn anew four times a second; over it
 * a few fireflies, sprigs at the corners. The views' panels are translucent, so they read as glass over the night. */
static uint32_t gd_bd[GD_W * GD_H], gd_bd_col, gd_bd_at;
static void gd_backdrop(uint32_t col)
{
    uint32_t now = bk_ms();
    float t = (float)now / 1000.0f;
    int x, y;
    if (gd_bd_col != col) {                      /* (made anew only for another colour: no hitch once a second) */
        uint32_t top = ARGB(255, 5, 7, 15), glow = gd_mix(ARGB(255, 10, 14, 26), col, 0.16f);
        for (y = 0; y < GD_H; y++) {
            uint32_t c = gd_mix(top, glow, powf((float)y / (float)GD_H, 1.6f)), *r = &bk_px[(GD_Y + y) * BK_W + GD_X];
            for (x = 0; x < GD_W; x++)
                r[x] = c;
        }
        for (int layer = 0; layer < 2; layer++)
            for (x = 0; x < GD_W; x += 2) {
                float u = (float)x / (float)GD_W, hy = 646.0f + (float)layer * 26.0f + 18.0f * sinf(u * (2.3f + (float)layer * 1.7f) + (float)layer * 2.0f);
                bk_rect(GD_X + x, GD_Y + (int)hy, 2, GD_H - (int)hy, gd_mix(gd_mix(top, col, 0.10f - 0.04f * (float)layer), ARGB(255, 4, 6, 12), 0.35f + 0.3f * (float)layer));
            }
        gd_texture(0, GD_H, 46, col, 30, t);   /* stone-like grain, veils of the view's colour, a vignette */
        for (int v = 0; v < 1; v++)               /* a canopy: vines hanging in at the top right corner, leaves on them */
            for (int k = 0; k < 4; k++) {
                float vx = (float)(GD_X + GD_W - 10 - k * 13), len = 34.0f + (float)((k * 37) % 46);
                float px2 = vx, py2 = (float)GD_Y;
                uint32_t vc = gd_mix(ARGB(255, 30, 60, 44), col, 0.25f);
                for (int sgm = 1; sgm <= 10; sgm++) {
                    float f = (float)sgm / 10.0f, nx = vx + 8.0f * sinf(f * 4.0f + (float)k), ny = (float)GD_Y + len * f;
                    bk_line(px2, py2, nx, ny, 2.2f * (1.0f - 0.5f * f), vc);
                    if (sgm % 3 == 0)
                        bk_disc(nx + ((sgm & 1) ? 5.0f : -5.0f), ny, 4.0f, gd_mix(vc, col, 0.3f));
                    px2 = nx, py2 = ny;
                }
            }
        for (y = 0; y < GD_H; y++)
            memcpy(&gd_bd[y * GD_W], &bk_px[(GD_Y + y) * BK_W + GD_X], GD_W * 4u);
        gd_bd_col = col, gd_bd_at = now, gd_bd_ver++;
        bk_gr_rebake[1] = 1;
    }
    {   /* (1.3) the view's quiet grounds baked over the kept night (bk_ground, layer 1) */
        static uint32_t baked[GD_W * GD_H];
        if (bk_ground_cache_off()) {             /* (1.5.5: no cache - the kept night alone, grounds drawn live) */
            for (y = 0; y < GD_H; y++)
                memcpy(&bk_px[(GD_Y + y) * BK_W + GD_X], &gd_bd[y * GD_W], GD_W * 4u);
        } else if (bk_gr_rebake[1]) {
            for (y = 0; y < GD_H; y++)
                memcpy(&bk_px[(GD_Y + y) * BK_W + GD_X], &gd_bd[y * GD_W], GD_W * 4u);
            bk_ground_bake(1);
            for (y = 0; y < GD_H; y++)
                memcpy(&baked[y * GD_W], &bk_px[(GD_Y + y) * BK_W + GD_X], GD_W * 4u);
            bk_gr_rebake[1] = 0;
        } else
            for (y = 0; y < GD_H; y++)
                memcpy(&bk_px[(GD_Y + y) * BK_W + GD_X], &baked[y * GD_W], GD_W * 4u);
        bk_gr_centre = !bk_ground_cache_off();
    }
    if (!gd_defer_life)
        gd_backdrop_life(t);
}
/* what moves over the kept night: the stars twinkling, a few fireflies (after an FM-1 page's light when that is kept,
   see bk_draw_screen_garden) */
static void gd_backdrop_life(float t)
{
    int x;
    for (x = 0; x < 70; x++) {                   /* the stars twinkle over the kept night, every frame */
        uint32_t h = (uint32_t)x * 2654435761u;
        float tw = 0.5f + 0.5f * sinf(t * (0.5f + (float)(h >> 24 & 7u) * 0.2f) + (float)x);
        bk_disc((float)(GD_X + (int)(h % GD_W)), (float)(GD_Y + (int)((h >> 10) % 600u)), 0.7f + (float)(h >> 20 & 3u) * 0.25f,
                BK_ALPHA(ARGB(255, 230, 236, 255), (uint32_t)(30 + 90 * tw)));
    }
    for (int i = 0; i < 9; i++) {                 /* a few fireflies */
        uint32_t h = (uint32_t)i * 2246822519u;
        float fx = (float)GD_X + (float)(h % GD_W) + 40.0f * sinf(t * 0.21f + (float)i), blink = sinf(t * (0.8f + (float)(h >> 28) * 0.15f) + (float)i);
        float fy = (float)GD_Y + 80.0f + (float)((h >> 9) % 560u) + 26.0f * sinf(t * 0.33f + (float)i * 1.3f);
        if (blink > 0.3f) {
            bk_disc(fx, fy, 5.0f, BK_ALPHA(ARGB(255, 210, 255, 150), (uint32_t)(22 * blink)));
            bk_disc(fx, fy, 1.6f, BK_ALPHA(ARGB(255, 230, 255, 190), (uint32_t)(170 * blink)));
        }
    }
}

/* a sprig: a curved twig with three leaves, from (x, y) towards dx, dy (each +-1) */
static void gd_sprig(float x, float y, float dx, float dy, uint32_t col)
{
    float px = x, py = y;
    for (int s = 1; s <= 8; s++) {
        float f = (float)s / 8.0f, nx = x + dx * 46.0f * f, ny = y + dy * (14.0f * f + 20.0f * f * f);
        bk_line(px, py, nx, ny, 2.2f * (1.0f - 0.6f * f), BK_ALPHA(col, 160));
        if (s == 3 || s == 5 || s == 7)
            gd_leaf(nx, ny, (s == 5 ? -1.0f : 1.0f) * dx, 16.0f - (float)s, dy * -6.0f, col, 0.35f);
        px = nx, py = ny;
    }
}

/* a seedling, for the titles */
static void gd_seedling(float x, float y, uint32_t col)
{
    bk_line(x, y + 12, x, y - 2, 2.0f, col);
    gd_leaf(x, y + 2, -1, 11, 6, col, 0.8f);
    gd_leaf(x, y - 1, 1, 13, 8, col, 0.8f);
}

/* a flower knob: five petals that open with open (0..1) */
static void gd_flower(float x, float y, float r, float open, uint32_t col, uint32_t heart)
{
    for (int k = 0; k < 5; k++) {
        float a = (float)k * 1.2566f;
        bk_disc(x + sinf(a) * r * (0.45f + 0.4f * open), y - cosf(a) * r * (0.45f + 0.4f * open), r * 0.55f, BK_ALPHA(col, 150 + (uint32_t)(90 * open)));
    }
    bk_disc(x, y, r * 0.42f, heart);
}
