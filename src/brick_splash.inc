/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the start screen.
 *
 * A night garden wakes: stars, an aurora in the six track colours, six plants that grow from the horizon, unfold
 * their leaves and bloom, spores that rise from the blooms; then MUTABLE DREAMS rises letter by letter, BY KID BUNKA
 * under it. About six seconds; it waits (breathing PRESS ANY BUTTON) until a button or ten seconds, then fades into
 * the stage over 0.7 s. The engine boots and runs behind it. The button that ends it does nothing else.
 * (Included by brick_stage.inc after the drawing primitives.) */
#include "splash_font.h"

#define BK_VERSION "1.5.10"
#define SP_FADE_MS 700u
#define SP_AUTO_MS 10000u
#ifndef SP_BG_MS
#define SP_BG_MS 100u                           /* how often the sky and the aurora are drawn anew */
#endif

static int sp_on;                                /* 1 shown, 2 fading out */
static uint32_t sp_t0, sp_fade_t0;
static float sp_a = 1.0f;                        /* the whole screen's opacity (the fade) */

void bk_splash(int on)
{
    sp_on = on ? 1 : 0;
    sp_t0 = bk_ms();
    sp_a = 1.0f;
}
int bk_splash_active(void) { return sp_on != 0; }

static void sp_end(void)
{
    if (sp_on == 1) {
        sp_on = 2;
        sp_fade_t0 = bk_ms();
    }
}

static uint32_t sp_c(uint32_t c, float a)        /* c at a times its alpha, times the fade */
{
    float k = a * sp_a;
    uint32_t al = (uint32_t)((float)(c >> 24) * (k < 0 ? 0 : k > 1 ? 1 : k));
    return (c & 0x00FFFFFFu) | al << 24;
}
static float sp_ease(float x)                    /* 0..1, smooth */
{
    x = x < 0 ? 0 : x > 1 ? 1 : x;
    return x * x * (3.0f - 2.0f * x);
}
static uint32_t sp_mix(uint32_t a, uint32_t b, float k)
{
    uint32_t r = (uint32_t)((float)(a >> 16 & 255u) * (1 - k) + (float)(b >> 16 & 255u) * k);
    uint32_t g = (uint32_t)((float)(a >> 8 & 255u) * (1 - k) + (float)(b >> 8 & 255u) * k);
    uint32_t bl = (uint32_t)((float)(a & 255u) * (1 - k) + (float)(b & 255u) * k);
    return 0xFF000000u | r << 16 | g << 8 | bl;
}
static uint32_t sp_hue(float u)                  /* along the six track colours, round */
{
    float f = u - floorf(u);
    int i = (int)(f * 6.0f);
    return sp_mix(bk_track_col(i % 6), bk_track_col((i + 1) % 6), f * 6.0f - (float)i);
}

/* the sky: a deep gradient, darker than the stage's */
static void sp_sky(float t)
{
    static uint32_t row[BK_H];
    int y, x;
    if (!row[0])
        for (y = 0; y < BK_H; y++) {
            float k = (float)y / (float)(BK_H - 1);
            row[y] = sp_mix(ARGB(255, 3, 4, 12), ARGB(255, 12, 18, 34), k * k);
        }
    for (y = 0; y < BK_H; y++) {
        uint32_t *r = &bk_px[y * BK_W], c = row[y];
        for (x = 0; x < BK_W; x++)
            r[x] = c;
    }
    for (x = 0; x < 140; x++) {                  /* stars, twinkling */
        uint32_t h = (uint32_t)x * 2654435761u;
        float sx = (float)(h % BK_W), sy = (float)((h >> 10) % 520u);
        float tw = 0.45f + 0.55f * sinf(t * (0.7f + (float)(h >> 24 & 7u) * 0.25f) + (float)x);
        bk_disc(sx, sy, 0.7f + (float)(h >> 20 & 3u) * 0.35f, sp_c(ARGB(255, 230, 236, 255), 0.25f + 0.6f * tw * tw));
    }
}

/* the aurora: six slow ribbons, one per track colour, wide and faint with a brighter core */
static void sp_aurora(float t, float on)
{
    for (int i = 0; i < 6; i++) {
        uint32_t col = bk_track_col(i);
        float base = 250.0f + (float)i * 26.0f, px = 0, py = 0;
        for (int x = -24; x <= BK_W + 24; x += 24) {
            float u = (float)x / (float)BK_W;
            float y = base + 46.0f * sinf(u * 5.1f + t * 0.23f + (float)i * 1.3f) +
                      22.0f * sinf(u * 11.7f - t * 0.37f + (float)i * 0.7f);
            float lift = 0.6f + 0.4f * sinf(u * 3.0f + t * 0.5f + (float)i);
            if (x > -24) {
                bk_line(px, py, (float)x, y, 14.0f, sp_c(col, 0.06f * on * lift));
                bk_line(px, py, (float)x, y, 2.0f, sp_c(col, 0.22f * on * lift));
            }
            px = (float)x, py = y;
        }
    }
}

/* the hills: two soft layers at the bottom, the garden's ground */
static void sp_ground(float t)
{
    for (int layer = 0; layer < 2; layer++)
        for (int x = 0; x < BK_W; x += 2) {
            float u = (float)x / (float)BK_W, y = layer ? 712.0f + 18.0f * sinf(u * 4.0f + 1.0f) + 8.0f * sinf(u * 13.0f)
                                                        : 690.0f + 26.0f * sinf(u * 2.6f + 0.4f) + 10.0f * sinf(u * 9.0f + 2.0f);
            uint32_t c = layer ? ARGB(255, 5, 8, 14) : ARGB(255, 9, 14, 24);
            (void)t;
            bk_rect(x, (int)y, 2, BK_H - (int)y, sp_c(c, 1));
        }
}

/* a plant: x its foot, h its height; g 0..1 its growth, b 0..1 its bloom, i its track */
static float sp_spore_t[6];
static void sp_plant(int i, float x, float foot, float h, float g, float b, float t)
{
    uint32_t col = bk_track_col(i);
    float sway = sinf(t * (0.5f + 0.09f * (float)i) + (float)i * 1.7f) * (10.0f + 4.0f * (float)(i % 3));
    float lean = ((float)(i % 2) * 2.0f - 1.0f) * 18.0f;
    float px = x, py = foot, tipx = x, tipy = foot;
    int segs = 28;
    if (g <= 0)
        return;
    for (int s = 1; s <= segs; s++) {            /* the stem, drawn as far as it has grown */
        float f = (float)s / (float)segs, nx, ny;
        if (f > g)
            break;
        nx = x + lean * f * f + sway * f * f * f;
        ny = foot - h * f;
        bk_line(px, py, nx, ny, 3.8f * (1.0f - 0.55f * f), sp_c(col, 1.0f));
        px = nx, py = ny;
        tipx = nx, tipy = ny;
        if (s == 8 || s == 13 || s == 18 || s == 22) {   /* the leaves unfold once the stem passed them */
            float side = (s == 8 || s == 18) ? -1.0f : 1.0f, open = sp_ease((g - f) * 5.0f);
            float len = (34.0f - (float)s * 0.8f) * open, rise = 14.0f * open;
            float flut = sinf(t * 1.3f + (float)(s + i)) * 2.0f;
            for (int d = 1; d <= 9; d++) {        /* a blade: discs along a curve, widest a third along */
                float u = (float)d / 9.0f, wdt = sinf(u * 3.1416f) * (1.0f - 0.35f * u) * 6.5f * open;
                float lx = nx + side * len * u, ly = ny - rise * u * u + flut * u - 5.0f * sinf(u * 3.1416f) * open;
                bk_disc(lx, ly, wdt + 0.6f, sp_c(col, 0.38f));
            }
            bk_line(nx, ny, nx + side * len, ny - rise + flut, 1.0f, sp_c(col, 0.7f * open));
        }
    }
    if (b > 0) {                                  /* the bloom: petals open, a heart, a ring that breathes out */
        float r = 9.0f + 14.0f * b, pulse = fmodf(t * 0.42f + (float)i * 0.37f, 1.0f);
        for (int k = 0; k < 7; k++) {
            float a = (float)k * 0.8976f + t * 0.25f + (float)i;
            bk_disc(tipx + sinf(a) * r * 0.85f * b, tipy - cosf(a) * r * 0.85f * b, r * 0.55f, sp_c(col, 0.18f + 0.5f * b));
        }
        bk_disc(tipx, tipy, r * 1.9f, sp_c(col, 0.06f * b));
        bk_disc(tipx, tipy, r * 0.42f, sp_c(BK_INK, b));
        bk_arc(tipx, tipy, r + 6.0f + 60.0f * pulse, 2.0f, 0, 6.2832f, sp_c(col, 0.5f * b * (1.0f - pulse)));
        if (b > 0.9f)
            sp_spore_t[i] = t;
    }
}

/* spores rising from the open blooms */
static void sp_spores(float t, const float *bx, const float *by, float open)
{
    for (int k = 0; k < 60; k++) {
        uint32_t h = (uint32_t)k * 2246822519u;
        int i = (int)(h % 6u);
        float life = fmodf(t * (0.12f + (float)(h >> 8 & 15u) * 0.012f) + (float)(h >> 16 & 255u) / 255.0f, 1.0f);
        float sx = bx[i] + sinf(life * 9.0f + (float)k) * 26.0f * life, sy = by[i] - life * 260.0f;
        bk_disc(sx, sy, 1.3f + (float)(k % 3) * 0.6f, sp_c(bk_track_col(i), open * (1.0f - life) * 0.9f));
    }
}

/* the title, grown rather than printed: dark letters of moss with a bioluminescent rim, rising one after the other;
   threads of mycelium join them below, small mushrooms grow on a few, spores rise from them */
static void sp_word(const char *w, int cx, int y, float t, float t_first, float hue0)
{
    const felucca_font_t *f = &FONT_XL;
    int n = (int)strlen(w), track = 12, total = 0, x, xs[16];
    char one[2] = {0, 0};
    const uint32_t glow = ARGB(255, 150, 240, 200), moss = ARGB(255, 30, 46, 40), cap = ARGB(255, 214, 160, 120);
    (void)hue0;
    for (int j = 0; j < n; j++) {
        one[0] = w[j];
        total += bk_text_w(f, one, 1) + (j < n - 1 ? track : 0);
    }
    x = cx - total / 2;
    for (int j = 0; j < n && j < 16; j++) {
        one[0] = w[j];
        xs[j] = x;
        x += bk_text_w(f, one, 1) + track;
    }
    for (int j = 0; j + 1 < n && j < 15; j++) {   /* the mycelium below: threads from letter to letter, growing in */
        float k = sp_ease((t - t_first - (float)j * 0.085f - 0.3f) / 0.8f), px = 0, py = 0;
        one[0] = w[j];
        float x0 = (float)xs[j] + (float)bk_text_w(f, one, 1) * 0.5f, x1 = (float)xs[j + 1] + 20.0f;
        for (int s2 = 0; s2 <= 16 && k > 0; s2++) {
            float u = (float)s2 / 16.0f * k, xx = x0 + (x1 - x0) * u, yy = (float)(y + 78) + 6.0f * sinf(u * 9.0f + (float)j * 2.0f) + 4.0f * sinf(t * 0.8f + u * 5.0f);
            if (s2)
                bk_line(px, py, xx, yy, 1.2f, sp_c(glow, 0.35f));
            px = xx, py = yy;
        }
    }
    for (int j = 0; j < n && j < 16; j++) {
        float k = sp_ease((t - t_first - (float)j * 0.085f) / 0.55f);
        int rise = (int)(16.0f * (1.0f - k)), wj, yy;
        one[0] = w[j];
        wj = bk_text_w(f, one, 1);
        yy = y + rise + (int)(2.0f * sinf(t * 0.9f + (float)j * 1.7f) * k) + ((j * 5) % 3) - 1;   /* (barely uneven: grown) */
        if (k <= 0)
            continue;
        for (int g = 0; g < 8; g++)              /* the bioluminescent rim, wide and soft, breathing */
            bk_text(xs[j] + (int)(6.0f * cosf((float)g * 0.785f)), yy + (int)(6.0f * sinf((float)g * 0.785f)), f, 1, one,
                    sp_c(glow, (0.07f + 0.03f * sinf(t * 1.5f + (float)j)) * k));
        for (int g = 0; g < 8; g++)              /* a thin bright edge */
            bk_text(xs[j] + (int)(2.0f * cosf((float)g * 0.785f)), yy + (int)(2.0f * sinf((float)g * 0.785f)), f, 1, one, sp_c(glow, 0.55f * k));
        bk_text(xs[j], yy, f, 1, one, sp_c(moss, k));                                        /* the moss */
        bk_text(xs[j], yy + 3, f, 1, one, sp_c(sp_mix(moss, ARGB(255, 0, 0, 0), 0.5f), 0.5f * k));
        for (int d = 0; d < 6; d++) {            /* moss texture: specks of green light in it */
            uint32_t h = (uint32_t)(j * 31 + d) * 2654435761u;
            bk_disc((float)(xs[j] + 8 + (int)(h % (uint32_t)(wj > 16 ? wj - 16 : 1))), (float)(yy + 14 + (int)((h >> 8) % 52u)), 1.6f,
                    sp_c(glow, (0.25f + 0.25f * sinf(t * 2.0f + (float)d)) * k));
        }
        for (int hg = 0; hg < 2 && k > 0.5f; hg++) {   /* threads of moss hang from it, a drop at each end */
            float hx = (float)(xs[j] + wj / 3 + hg * wj / 3), hl = (10.0f + (float)(((j + hg) * 17) % 16)) * sp_ease((k - 0.5f) * 2.0f);
            float swy = sinf(t * 1.1f + (float)(j + hg)) * 2.0f;
            bk_line(hx, (float)(yy + 74), hx + swy, (float)(yy + 74) + hl, 1.2f, sp_c(glow, 0.45f));
            bk_disc(hx + swy, (float)(yy + 76) + hl, 2.0f, sp_c(glow, 0.5f + 0.4f * sinf(t * 3.0f + (float)j)));
        }
        if ((j * 7 + 3) % 3 == 0 && k > 0.6f) {  /* mushrooms grow on top of some */
            float g2 = sp_ease((k - 0.6f) / 0.4f), mx = (float)(xs[j] + wj / 2 + ((j * 13) % 20) - 10), my = (float)(yy + 6);
            for (int m = 0; m < 2; m++) {
                float ox = (float)(m * 12 - 4), hh = (14.0f - (float)m * 5.0f) * g2, cr = (8.0f - (float)m * 3.0f) * g2;
                bk_line(mx + ox, my, mx + ox, my - hh, 2.5f - (float)m * 0.8f, sp_c(ARGB(255, 230, 220, 200), g2));
                for (int c2 = -3; c2 <= 3; c2++)
                    bk_disc(mx + ox + (float)c2 * cr * 0.28f, my - hh - (3.0f - fabsf((float)c2)) * cr * 0.18f, cr * 0.45f, sp_c(cap, g2));
                bk_disc(mx + ox - cr * 0.2f, my - hh - cr * 0.35f, cr * 0.18f + 0.5f, sp_c(ARGB(255, 255, 240, 220), 0.8f * g2));
                {   /* a spore rising from it */
                    float life = fmodf(t * 0.3f + (float)(j + m), 1.0f);
                    bk_disc(mx + ox + 8.0f * sinf(life * 6.0f), my - hh - 6.0f - life * 50.0f, 1.5f, sp_c(glow, (1.0f - life) * 0.8f * g2));
                }
            }
        }
    }
}

static void sp_text_c(const felucca_font_t *f, int cy, const char *s, uint32_t c, float a)
{
    bk_text(BK_W / 2 - bk_text_w(f, s, 1) / 2, cy, f, 1, s, sp_c(c, a));
}

static void bk_draw_splash(float t)           /* (over the sky, the aurora and the ground) */
{
    static const float foot_x[6] = {176, 316, 448, 576, 708, 848};
    static const float hgt[6] = {196, 246, 176, 262, 214, 236};
    float bx[6], by[6], open = 0;
    int i;
    for (i = 0; i < 6; i++) {                     /* the garden: they grow one after another, then bloom */
        float g = sp_ease((t - 0.35f - 0.16f * (float)i) / 1.9f);
        float b = sp_ease((t - 2.1f - 0.13f * (float)i) / 0.9f);
        float foot = 700.0f + 10.0f * sinf((float)i * 1.9f);
        sp_plant(i, foot_x[i], foot, hgt[i], g, b, t);
        bx[i] = foot_x[i] + ((float)(i % 2) * 2.0f - 1.0f) * 18.0f;
        by[i] = foot - hgt[i];
        open += b / 6.0f;
    }
    sp_spores(t, bx, by, open);
    sp_word("MUTABLE", BK_W / 2, 96, t, 1.0f, 0.0f);
    sp_word("DREAMS", BK_W / 2, 186, t, 1.45f, 0.35f);
    {
        float k = sp_ease((t - 2.6f) / 0.8f);
        int lw = (int)(220.0f * k);
        bk_rect(BK_W / 2 - lw, 300, 2 * lw, 1, sp_c(BK_INK, 0.35f * k));
        sp_text_c(&FONT_XM, 316, "BY KID BUNKA", BK_INK, k);
    }
    {
        char line[80];
        snprintf(line, sizeof line, "A GENERATIVE GARDEN OF SOUND  /  %s", bk_device);
        sp_text_c(&FONT_H, 362, line, BK_DIM, sp_ease((t - 3.2f) / 0.8f));
    }
    if (t > 4.4f) {
        float br = 0.35f + 0.45f * (0.5f + 0.5f * sinf((t - 4.4f) * 2.4f));
        sp_text_c(&FONT_H, 742, "PRESS ANY BUTTON", BK_INK, br * sp_ease((t - 4.4f) / 0.6f));
    }
    bk_text(BK_W - 16 - bk_text_w(&FONT_T, "VERSION " BK_VERSION "  /  ENGINE 2.1", 1), 16, &FONT_T, 1,
            "VERSION " BK_VERSION "  /  ENGINE 2.1", sp_c(BK_FAINT, sp_ease((t - 3.2f) / 0.8f)));
}

/* the start screen in place of the stage, or over it while it fades; 1 when it drew the whole frame */
static int bk_splash_frame(void)
{
    uint32_t now = bk_ms();
    float t = (float)(now - sp_t0) / 1000.0f;
    if (!sp_on)
        return 0;
    if (sp_on == 1 && now - sp_t0 >= SP_AUTO_MS)
        sp_end();
    if (sp_on == 2) {
        float u = (float)(now - sp_fade_t0) / (float)SP_FADE_MS;
        if (u >= 1.0f) {
            sp_on = 0;
            sp_a = 1.0f;
            return 0;
        }
        sp_a = 1.0f - sp_ease(u);
        return 0;                                /* the stage is drawn first, then the screen over it */
    }
    sp_a = 1.0f;
    {   /* the sky, the aurora and the ground change slowly: drawn ten times a second and kept (the wide, faint
         * aurora lines are most of the screen's cost) */
        static uint32_t bg[BK_W * BK_H], bg_at;
        static int bg_ok;
        if (!bg_ok || now - bg_at >= SP_BG_MS) {
            sp_sky(t);
            sp_aurora(t, sp_ease(t / 1.2f));
            sp_ground(t);
            memcpy(bg, bk_px, sizeof bg);
            bg_at = now;
            bg_ok = 1;
        } else
            memcpy(bk_px, bg, sizeof bg);
    }
    bk_draw_splash(t);
    return 1;
}

static void bk_splash_over(void)                /* while fading: the night and the garden over the stage */
{
    float t = (float)(bk_ms() - sp_t0) / 1000.0f;
    if (sp_on != 2)
        return;
    bk_rect(0, 0, BK_W, BK_H, sp_c(ARGB(255, 3, 4, 12), 1));
    sp_ground(t);
    bk_draw_splash(t);
}
