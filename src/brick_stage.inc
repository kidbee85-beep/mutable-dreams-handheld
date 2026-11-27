/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the stage, 1024 x 768.
 *
 *   top bar      the name, the Biome (and its family), transport, battery
 *   left         the six tracks as living plants: they grow with their level, sway with the sound, bloom on each
 *                hit; the selected one framed in its colour, a muted one asleep
 *   centre       the FM-1 screen, 3x (720 x 720): every one of the 45 pages, unchanged
 *   right        K1..K4 (the one the D-pad turns lit), tempo, the bar's steps, key and chord; in play mode the
 *                eight scale tones under the Brick's buttons
 *   bottom       what the buttons do now (R2 held shows its layer)
 *   overlays     the jump menu (MENU)
 *
 * Drawn from the engine's state only (md_state, md_status, dream_ui): it never changes the sound. */
#include <math.h>

static uint32_t bk_px[BK_W * BK_H];

/* ------------------------------------------------------------ colours --- */
#define ARGB(a, r, g, b) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define BK_ALPHA(c, a) (((c) & 0xFFFFFFu) | ((uint32_t)(a) << 24))
static const uint32_t BK_BG0 = ARGB(255, 6, 9, 16), BK_BG1 = ARGB(255, 14, 22, 36);
static const uint32_t BK_INK = ARGB(255, 255, 252, 244), BK_DIM = ARGB(255, 178, 188, 200),   /* (text contrast on the night: DIM ~9:1, FAINT ~4.6:1) */
                      BK_FAINT = ARGB(255, 118, 128, 146);

static const uint32_t LAB_MOD_COL[M_COUNT] = {ARGB(255, 140, 210, 150), ARGB(255, 120, 200, 210), ARGB(255, 250, 196, 150), ARGB(255, 250, 150, 170), ARGB(255, 200, 236, 170), ARGB(255, 180, 220, 120),
                                              ARGB(255, 170, 200, 255), ARGB(255, 255, 170, 110), ARGB(255, 140, 235, 220), ARGB(255, 235, 150, 235), ARGB(255, 200, 180, 240), ARGB(255, 240, 180, 210),
                                              ARGB(255, 245, 205, 120), ARGB(255, 150, 210, 235), ARGB(255, 240, 150, 110), ARGB(255, 235, 200, 140), ARGB(255, 230, 160, 140), ARGB(255, 220, 225, 235)};

static uint32_t bk_track_col(int i)              /* the app's own six track colours (dream_colors, RGB565) */
{
    uint32_t c = dream_colors[i % 6];
    uint32_t r = (c >> 11) & 31u, g = (c >> 5) & 63u, b = c & 31u;
    return ARGB(255, r * 255u / 31u, g * 255u / 63u, b * 255u / 31u);
}

/* ---------------------------------------------------------- primitives --- */
/* the sound comes first: while the stage draws, every couple of thousand pixels it asks whether the sound thread is
   waiting for the core, and if so lets it in (bk_yield_fn, from the front end); a frame never holds the sound up
   for more than a moment */
static void (*bk_yield_fn)(void);
static int bk_handheld;                          /* a handheld without the Brick's MENU, F1, F2 and FN (MD_KEYS=handheld) */
/* the light picture (0.64): for the weaker handhelds, or by itself when the stage cannot keep up. It leaves out
   what only decorates - the drifting spores and aura, the pond's mirror, the grain of the skies - and draws the
   garden's sky three times less often; everything you use stays (bk_light: brick_core.c, the quick bar sets it) */
static char bk_device[32] = "TRIMUI BRICK";      /* the handheld's name in the title and on the start screen (MD_DEVICE) */
static const char *bk_title_for(void)
{
    static char t[48];
    snprintf(t, sizeof t, "FOR %s", bk_device);
    return t;
}
void bk_set_device(const char *name)
{
    int k;
    for (k = 0; name && name[k] && k < (int)sizeof bk_device - 1; k++)
        bk_device[k] = (char)(name[k] >= 'a' && name[k] <= 'z' ? name[k] - 32 : name[k]);
    if (k)
        bk_device[k] = 0;
}
/* a float as an alpha or a byte, held to 0..255 (a negative float cast to unsigned is undefined in C) */
static inline uint32_t bk_u8(float v) { return v <= 0.0f ? 0u : v >= 255.0f ? 255u : (uint32_t)v; }
static uint32_t bk_poll_n;
/* the time since the last frame, and a glide by it: what eases towards a target moves the same at 12 frames a second
   as at 30 (a fixed share per frame crawled on the Brick and raced on the Mac) */
static float bk_frame_s = 1.0f / 30.0f;
static inline float bk_glide(float rate) { return 1.0f - expf(-bk_frame_s * rate); }
void bk_set_draw_yield(void (*fn)(void)) { bk_yield_fn = fn; }
static void bk_draw_yield(void)
{
    if (bk_yield_fn)
        bk_yield_fn();
}
/* N pixels' worth of work done: the sound's turn whenever the count crosses a step of 2048. (Until 0.60 a single
   look asked for the count to land exactly on a step, so the rows that first added 1024 or 2048 and then looked
   almost never let the sound in: the garden, its reflection and an FM-1 page's light ran through whole, on the
   Brick 25 to 45 ms at a stretch, about all the sound kept ahead. The stutter in 0.60's device log.) */
static inline void bk_draw_count(uint32_t n)
{
    uint32_t before = bk_poll_n;
    bk_poll_n += n;
    if (((before ^ bk_poll_n) & ~2047u) && bk_yield_fn)
        bk_yield_fn();
}
static inline void bk_draw_poll(void) { bk_draw_count(1u); }

/* one pixel p blended with c at coverage cov (the sums of bk_blend, without its checks) */
static inline void bk_blend_px(uint32_t *p, uint32_t c, uint32_t cov)
{
    uint32_t a, d;
    a = (c >> 24) * cov + 128u;
    a = (a + (a >> 8)) >> 8;
    if (!a)
        return;
    if (a >= 255u) {
        *p = c | 0xFF000000u;
        return;
    }
    d = *p;
    {
        uint32_t na = 255u - a, rb = (c & 0xFF00FFu) * a + (d & 0xFF00FFu) * na + 0x800080u, g = (c & 0xFF00u) * a + (d & 0xFF00u) * na + 0x8000u;
        rb = ((rb + ((rb >> 8) & 0xFF00FFu)) >> 8) & 0xFF00FFu;
        g = ((g + ((g >> 8) & 0xFF00u)) >> 8) & 0xFF00u;
        *p = 0xFF000000u | rb | g;
    }
}
/* (1.5.3) a band of columns drawing stays inside (bk_span, bk_blend): the garden's sky is drawn only now and then and
   kept for the centre alone; what of it reached past the stage's edge lit the side columns only on those frames - on
   the RGB20S a flicker beside the stage, left and right. 0..BK_W: everywhere */
static int bk_clip_x0 = 0, bk_clip_x1 = BK_W;
static inline void bk_blend(int x, int y, uint32_t c, uint32_t cov)   /* cov 0..255 more on top of c's alpha */
{
    uint32_t a, *p, d;
    bk_draw_poll();
    if ((unsigned)(x - bk_clip_x0) >= (unsigned)(bk_clip_x1 - bk_clip_x0) || (unsigned)y >= BK_H)
        return;
    a = (c >> 24) * cov + 128u;                  /* (no divisions: the Brick's cores divide slowly; x/255 as (x*257)>>16) */
    a = (a + (a >> 8)) >> 8;
    if (!a)
        return;
    p = &bk_px[y * BK_W + x];
    if (a >= 255u) {
        *p = c | 0xFF000000u;
        return;
    }
    d = *p;
    {   /* red and blue together, then green */
        uint32_t na = 255u - a, rb = (c & 0xFF00FFu) * a + (d & 0xFF00FFu) * na + 0x800080u, g = (c & 0xFF00u) * a + (d & 0xFF00u) * na + 0x8000u;
        rb = ((rb + ((rb >> 8) & 0xFF00FFu)) >> 8) & 0xFF00FFu;
        g = ((g + ((g >> 8) & 0xFF00u)) >> 8) & 0xFF00u;
        *p = 0xFF000000u | rb | g;
    }
}


static void bk_span(int x0, int x1, int y, uint32_t c);
static void bk_rect(int x, int y, int w, int h, uint32_t c)
{
    for (int j = y < 0 ? 0 : y; j < y + h && j < BK_H; j++)
        bk_span(x, x + w, j, c);
}

#ifdef MD_SPANSTAT   /* measuring only (tools/spancost.sh): pixels per caller of bk_span, printed at exit (address relative to main) */
#ifndef MD_SPANSTAT_RA
#define MD_SPANSTAT_RA __builtin_return_address(0)
#endif
int main(int, char **);
static struct { void *at; unsigned long long op, tr, calls; } span_tab[256];
static void span_dump(void)
{
    for (int i = 0; i < 256; i++)
        if (span_tab[i].at)
            fprintf(stderr, "SPAN %ld %llu %llu %llu\n", (long)((char *)span_tab[i].at - (char *)main),
                    span_tab[i].op, span_tab[i].tr, span_tab[i].calls);
}
static void span_stat(void *at, int n, int opaque)
{
    static int reg;
    unsigned h = (unsigned)(((uintptr_t)at >> 2) * 2654435761u) >> 24;
    if (!reg)
        reg = 1, atexit(span_dump);
    while (span_tab[h].at && span_tab[h].at != at)
        h = (h + 1) & 255u;
    span_tab[h].at = at;
    if (opaque) span_tab[h].op += (unsigned)n; else span_tab[h].tr += (unsigned)n;
    span_tab[h].calls++;
}
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
/* (0.64.1) the see-through run four pixels at a time with the ARM's vector unit: the very sums of bk_blend, per byte
   lane: (c*a + 128 + d*(255-a)), then (v + (v >> 8)) >> 8 - bit for bit the same picture. On the Brick's device log
   the see-through spans were nearly half of all drawing (300 000 to 650 000 pixels a frame: the plants' column, the
   right panel, the hint line, the mixer's strips). Every handheld planned has NEON (Brick A53, RGB20S A35, Miyoo A7,
   Android arm64). */
static void bk_span_blend_neon(uint32_t *p, int n, uint32_t c, uint32_t a)
{
    const uint8x8_t na = vdup_n_u8((uint8_t)(255u - a));
    const uint8x8_t cb = vreinterpret_u8_u32(vdup_n_u32(c));
    const uint16x8_t ca = vaddq_u16(vmull_u8(cb, vdup_n_u8((uint8_t)a)), vdupq_n_u16(128));
    const uint8x16_t opaque = vreinterpretq_u8_u32(vdupq_n_u32(0xFF000000u));
    int i = 0;
    for (; i + 8 <= n; i += 8) {                 /* (eight at a time: two chains side by side for the in-order A53 / A35 / A7) */
        uint8x16_t d0 = vld1q_u8((const uint8_t *)(p + i)), d1 = vld1q_u8((const uint8_t *)(p + i + 4));
        uint16x8_t l0 = vmlal_u8(ca, vget_low_u8(d0), na), h0 = vmlal_u8(ca, vget_high_u8(d0), na);
        uint16x8_t l1 = vmlal_u8(ca, vget_low_u8(d1), na), h1 = vmlal_u8(ca, vget_high_u8(d1), na);
        l0 = vsraq_n_u16(l0, l0, 8);
        h0 = vsraq_n_u16(h0, h0, 8);
        l1 = vsraq_n_u16(l1, l1, 8);
        h1 = vsraq_n_u16(h1, h1, 8);
        vst1q_u8((uint8_t *)(p + i), vorrq_u8(vcombine_u8(vshrn_n_u16(l0, 8), vshrn_n_u16(h0, 8)), opaque));
        vst1q_u8((uint8_t *)(p + i + 4), vorrq_u8(vcombine_u8(vshrn_n_u16(l1, 8), vshrn_n_u16(h1, 8)), opaque));
    }
    for (; i + 4 <= n; i += 4) {
        uint8x16_t d = vld1q_u8((const uint8_t *)(p + i));
        uint16x8_t lo = vmlal_u8(ca, vget_low_u8(d), na), hi = vmlal_u8(ca, vget_high_u8(d), na);
        lo = vsraq_n_u16(lo, lo, 8);
        hi = vsraq_n_u16(hi, hi, 8);
        vst1q_u8((uint8_t *)(p + i), vorrq_u8(vcombine_u8(vshrn_n_u16(lo, 8), vshrn_n_u16(hi, 8)), opaque));
    }
    if (i < n) {                                 /* the last one to three: in a register of four, the rest left as is */
        uint32_t t[4];
        int k, m = n - i;
        for (k = 0; k < m; k++)
            t[k] = p[i + k];
        for (; k < 4; k++)
            t[k] = 0;
        {
            uint8x16_t d = vld1q_u8((const uint8_t *)t);
            uint16x8_t lo = vmlal_u8(ca, vget_low_u8(d), na), hi = vmlal_u8(ca, vget_high_u8(d), na);
            lo = vsraq_n_u16(lo, lo, 8);
            hi = vsraq_n_u16(hi, hi, 8);
            vst1q_u8((uint8_t *)t, vorrq_u8(vcombine_u8(vshrn_n_u16(lo, 8), vshrn_n_u16(hi, 8)), opaque));
        }
        for (k = 0; k < m; k++)
            p[i + k] = t[k];
    }
}
#define BK_SPAN_NEON 1
static int getenv_md_scalar_span(void)          /* MD_SCALAR_SPAN=1: the plain C loop again (to compare on a device) */
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("MD_SCALAR_SPAN");
        v = e && *e == '1';
    }
    return v;
}
#endif

/* a run of pixels in one colour (fast path for opaque colours) */
static void bk_span(int x0, int x1, int y, uint32_t c)
{
    int i;
    bk_draw_count(64u);                          /* (a span counts for many pixels) */
    if ((unsigned)y >= BK_H)
        return;
    if (x0 < bk_clip_x0) x0 = bk_clip_x0;
    if (x1 > bk_clip_x1) x1 = bk_clip_x1;
#ifdef MD_SPANSTAT
    if (x1 > x0)
        span_stat(MD_SPANSTAT_RA, x1 - x0, (c >> 24) == 255u);
#endif
    if ((c >> 24) == 255u) {
        uint32_t *row = &bk_px[y * BK_W];
        for (i = x0; i < x1; i++)
            row[i] = c;
    } else if (x1 > x0) {                        /* see-through: bk_blend's own sums, worked out once for the run */
        uint32_t *row = &bk_px[y * BK_W], a = (c >> 24) * 255u + 128u, na, crb, cg, before = bk_poll_n;
        a = (a + (a >> 8)) >> 8;
        if (!a)
            return;
#ifdef BK_SPAN_NEON
        (void)na, (void)crb, (void)cg;
        if (!getenv_md_scalar_span())
            bk_span_blend_neon(row + x0, x1 - x0, c, a);
        else
#endif
        {
        na = 255u - a, crb = (c & 0xFF00FFu) * a + 0x800080u, cg = (c & 0xFF00u) * a + 0x8000u;
        for (i = x0; i < x1; i++) {
            uint32_t d = row[i], rb = crb + (d & 0xFF00FFu) * na, g = cg + (d & 0xFF00u) * na;
            rb = ((rb + ((rb >> 8) & 0xFF00FFu)) >> 8) & 0xFF00FFu;
            g = ((g + ((g >> 8) & 0xFF00u)) >> 8) & 0xFF00u;
            row[i] = 0xFF000000u | rb | g;
        }
        }
        bk_poll_n += (uint32_t)(x1 - x0);         /* (the sound's turn, as often as pixel by pixel) */
        if (((before ^ bk_poll_n) & ~2047u) && bk_yield_fn)
            bk_yield_fn();
    }
}

/* the coverage of pixel (i, j) by a rounded box (x, y, w, h, r): its signed distance, anti-aliased */
static float bk_rbox_dist(int i, int j, int x, int y, int w, int h, int r)
{
    float cx = (float)i + 0.5f, cy = (float)j + 0.5f;
    float qx = fabsf(cx - ((float)x + (float)w / 2)) - ((float)w / 2 - (float)r);
    float qy = fabsf(cy - ((float)y + (float)h / 2)) - ((float)h / 2 - (float)r);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    return sqrtf(ox * ox + oy * oy) + (qx > qy ? (qx < 0 ? qx : 0) : (qy < 0 ? qy : 0)) - (float)r;
}

/* a rounded box: rows clear of the corners are plain spans; only the corners are measured */
static void bk_rrect(int x, int y, int w, int h, int r, uint32_t c)
{
    int i, j;
    if (w <= 0 || h <= 0)
        return;
    if (2 * r > w)
        r = w / 2;                               /* (a box narrower than its corners: rounder, never stuck) */
    if (2 * r > h)
        r = h / 2;
    if (r > 0 && r <= 64) {                      /* the corners from a table for this radius (its coverage, once) */
        static uint8_t *tab[65];
        if (!tab[r] && (tab[r] = malloc((size_t)(r * r)))) {
            for (int m = 0; m < r; m++)          /* m rows from the top edge, k columns from the left edge */
                for (int k = 0; k < r; k++) {
                    float qx = (float)r - ((float)k + 0.5f), qy = (float)r - ((float)m + 0.5f);
                    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
                    float d = sqrtf(ox * ox + oy * oy) + (qx > qy ? (qx < 0 ? qx : 0) : (qy < 0 ? qy : 0)) - (float)r;
                    tab[r][m * r + k] = d < 0.5f ? (d <= -0.5f ? 255u : (uint8_t)((0.5f - d) * 255.0f)) : 0;
                }
        }
        if (tab[r]) {
            for (j = y; j < y + h; j++) {
                const uint8_t *row;
                if (j >= y + r && j < y + h - r) {
                    bk_span(x, x + w, j, c);
                    continue;
                }
                bk_span(x + r, x + w - r, j, c);
                row = tab[r] + (j < y + r ? j - y : y + h - 1 - j) * r;
                for (i = 0; i < r; i++) {
                    if (row[i])
                        bk_blend(x + i, j, c, row[i]);
                }
                for (i = 0; i < r; i++) {
                    int k = r - 1 - i;           /* (the right corner: mirrored) */
                    if (row[k])
                        bk_blend(x + w - r + i, j, c, row[k]);
                }
            }
            return;
        }
    }
    for (j = y; j < y + h; j++) {
        if (j >= y + r && j < y + h - r) {
            bk_span(x, x + w, j, c);
            continue;
        }
        bk_span(x + r, x + w - r, j, c);
        for (i = x; i < x + w; i++) {
            float d;
            if (i == x + r && x + w - r > i)
                i = x + w - r;
            d = bk_rbox_dist(i, j, x, y, w, h, r);
            if (d < 0.5f)
                bk_blend(i, j, c, d <= -0.5f ? 255u : (uint32_t)((0.5f - d) * 255.0f));
        }
    }
}

/* the outline of a rounded box, th thick: only the band along the edge is visited. Along the straight sides the
   coverage depends only on the column (left, right) or the row (top, bottom): worked out once; only the corners
   measure each pixel (the same coverage as measuring all) */
static void bk_rframe_plain(int x, int y, int w, int h, int r, int th, uint32_t c)
{
    if (w <= 0 || h <= 0)
        return;
    int i, j, band, ymid;
    uint8_t lcov[64], rcov[64];
    if (2 * r > w)
        r = w / 2;
    if (2 * r > h)
        r = h / 2;
    band = r + th + 1;
    if (th + 2 > 64)
        th = 62;
    ymid = y + h / 2;
    for (i = 0; i < th + 2; i++) {               /* the sides' columns, as in a middle row */
        float e = fabsf(bk_rbox_dist(x + i, ymid, x, y, w, h, r) + (float)th / 2) - (float)th / 2;
        lcov[i] = e < 0.5f ? (e <= -0.5f ? 255u : (uint8_t)((0.5f - e) * 255.0f)) : 0;
        e = fabsf(bk_rbox_dist(x + w - th - 1 + i, ymid, x, y, w, h, r) + (float)th / 2) - (float)th / 2;
        rcov[i] = e < 0.5f ? (e <= -0.5f ? 255u : (uint8_t)((0.5f - e) * 255.0f)) : 0;
    }
    for (j = y; j < y + h; j++) {
        int edge = j < y + band || j >= y + h - band, near = j < y + th + 2 || j >= y + h - th - 2;
        if (!edge) {                             /* a middle row: the two sides only */
            for (i = 0; i < th + 2 && x + i < x + w - th - 1; i++)
                if (lcov[i])
                    bk_blend(x + i, j, c, lcov[i]);
            for (i = 0; i < th + 2; i++)
                if (rcov[i] && x + w - th - 1 + i >= x + th + 1)
                    bk_blend(x + w - th - 1 + i, j, c, rcov[i]);
            continue;
        }
        {
            float cy = (float)j + 0.5f, qy = fabsf(cy - ((float)y + (float)h / 2)) - ((float)h / 2 - (float)r);
            float hw = (float)w / 2 - (float)r, cxm = (float)x + (float)w / 2;
            uint32_t mid = 0;
            if (near) {                          /* where qx <= qy the distance is qy - r all along the row */
                float e = fabsf(qy - (float)r + (float)th / 2) - (float)th / 2;
                mid = e < 0.5f ? (e <= -0.5f ? 255u : (uint32_t)((0.5f - e) * 255.0f)) : 0u;
            }
            for (i = x; i < x + w; i++) {
                float d, e;
                if (i == x + r + 1 && x + w - r - 1 > i) {
                    if (!near) {
                        i = x + w - r - 1;       /* (the corner rows away from the edge: the corners only) */
                    } else {
                        for (; i < x + w - r - 1; i++) {
                            float qx = fabsf((float)i + 0.5f - cxm) - hw;
                            if (qx <= qy) {
                                if (mid)
                                    bk_blend(i, j, c, mid);
                                continue;
                            }
                            d = bk_rbox_dist(i, j, x, y, w, h, r);
                            e = fabsf(d + (float)th / 2) - (float)th / 2;
                            if (e < 0.5f)
                                bk_blend(i, j, c, e <= -0.5f ? 255u : (uint32_t)((0.5f - e) * 255.0f));
                        }
                    }
                }
                d = bk_rbox_dist(i, j, x, y, w, h, r);
                e = fabsf(d + (float)th / 2) - (float)th / 2;
                if (e < 0.5f)
                    bk_blend(i, j, c, e <= -0.5f ? 255u : (uint32_t)((0.5f - e) * 255.0f));
            }
        }
    }
}

static void bk_disc(float cx, float cy, float r, uint32_t c);
static void bk_line(float x0, float y0, float x1, float y1, float th, uint32_t c);
uint32_t bk_ms(void);
/* a small sprig growing from a frame's corner: a curved twig, two leaves, a drop of dew; swaying (dir: 1 up-right) */
static void bk_sprig(float x, float y, float dir, float size, uint32_t c, float t)
{
    float sw = sinf(t * 1.3f + x * 0.013f + y * 0.007f) * 0.18f, px = x, py = y;
    for (int k = 1; k <= 6; k++) {
        float u = (float)k / 6.0f, a = -0.5f + sw + u * 0.6f;
        float nx = x + dir * size * u * cosf(a), ny = y - dir * size * u * sinf(a + 0.9f) * 0.8f;
        bk_line(px, py, nx, ny, 1.6f, c);
        px = nx, py = ny;
        if (k == 3 || k == 6)
            for (int d = 1; d <= 5; d++) {        /* a leaf */
                float v = (float)d / 5.0f, wd = sinf(v * 3.14f) * size * 0.11f;
                float side = k == 3 ? 1.0f : -1.0f;
                bk_disc(nx + dir * side * size * 0.32f * v * 0.6f, ny - dir * size * 0.32f * v * (k == 3 ? 0.9f : 0.3f), wd + 0.5f, c);
            }
    }
    bk_disc(px, py + 3.0f * dir, 1.8f, BK_ALPHA(BK_INK, 180));   /* dew */
}
/* a frame; a lit one (2 px and more, large enough) grows sprigs at two corners: nothing here is a plain box */
static void bk_rframe(int x, int y, int w, int h, int r, int th, uint32_t c)
{
    bk_rframe_plain(x, y, w, h, r, th, c);
    if (th >= 2 && w >= 80 && h >= 40) {
        float t = (float)bk_ms() / 1000.0f, sz = h < 70 ? 18.0f : 26.0f;
        bk_sprig((float)(x + r), (float)y + 1.0f, 1.0f, sz, c, t);
        bk_sprig((float)(x + w - r), (float)(y + h) - 1.0f, -1.0f, sz, c, t);
    }
}

static void bk_disc(float cx, float cy, float r, uint32_t c)
{
    int i, j;
    float out2 = (r + 0.5f) * (r + 0.5f), in2f = (r - 0.5f) * (r - 0.5f);
    if (r <= 0)
        return;
    for (j = (int)(cy - r - 1); j <= (int)(cy + r + 1); j++) {
        float dy = (float)j + 0.5f - cy, in2 = (r - 0.5f) * (r - 0.5f) - dy * dy;
        int i0 = (int)(cx - r - 1), i1 = (int)(cx + r + 1), s0 = 1, s1 = 0;
        if ((unsigned)j >= BK_H)
            continue;
        if (in2 > 0) {                            /* the solid middle of the row: one span; only its ends measured */
            float in = sqrtf(in2);
            s0 = (int)ceilf(cx - in - 0.5f), s1 = (int)floorf(cx + in - 0.5f);
            if (s1 >= s0)
                bk_span(s0, s1 + 1, j, c);
        }
        for (i = i0; i <= i1; i++) {
            float dx, d, d2;
            if (s1 >= s0 && i == s0)
                i = s1 + 1;
            dx = (float)i + 0.5f - cx;
            d2 = dx * dx + dy * dy;
            if (d2 >= out2)                       /* (clearly outside: no root; clearly inside: full) */
                continue;
            if (r >= 0.5f && d2 <= in2f) {
                bk_blend(i, j, c, 255u);
                continue;
            }
            d = sqrtf(d2) - r;
            if (d < 0.5f)
                bk_blend(i, j, c, d <= -0.5f ? 255u : (uint32_t)((0.5f - d) * 255.0f));
        }
    }
}

/* a ring of thickness th, from angle a0 to a1 (radians, 0 = up, clockwise); a full ring when a1 - a0 >= 2 pi */
static void bk_arc(float cx, float cy, float r, float th, float a0, float a1, uint32_t c)
{
    int i, j;
    float ro = r + th / 2 + 1, ri = r - th / 2 - 1.5f;
    float ring_out2 = (r + th / 2 + 0.5f) * (r + th / 2 + 0.5f), ring_in2 = r - th / 2 - 0.5f > 0 ? (r - th / 2 - 0.5f) * (r - th / 2 - 0.5f) : -1.0f;
    if (!(fabsf(cx) < 20000.0f && fabsf(cy) < 20000.0f && r >= 0 && r < 4000.0f && th >= 0 && th < 4000.0f))
        return;                                  /* (nonsense from a caller: nothing, and never a loop without end) */
    for (j = (int)(cy - ro); j <= (int)(cy + ro); j++) {
        float rdy = (float)j + 0.5f - cy, hole = ri > 0 && ri * ri > rdy * rdy ? sqrtf(ri * ri - rdy * rdy) : -1.0f;
        for (i = (int)(cx - ro); i <= (int)(cx + ro); i++) {
            float dx = (float)i + 0.5f - cx, dy = rdy, d;
            float a;
            if (hole > 0 && fabsf(dx) < hole - 1.0f) {   /* (its hole: nothing to draw; on to the far side - only ever on) */
                int far = (int)ceilf(cx + hole - 1.0f - 0.5f) - 1;
                if (far > i)
                    i = far;
                continue;
            }
            {
                float d2 = dx * dx + dy * dy;    /* (outside the ring's reach: no root) */
                if (d2 >= ring_out2 || d2 <= ring_in2)
                    continue;
                d = fabsf(sqrtf(d2) - r) - th / 2;
            }
            if (d >= 0.5f)
                continue;
            if (a1 - a0 < 6.2831f) {
                a = atan2f(dx, -dy);
                if (a < 0)
                    a += 6.2831853f;
                if (a < a0 || a > a1)
                    continue;
            }
            bk_blend(i, j, c, d <= -0.5f ? 255u : (uint32_t)((0.5f - d) * 255.0f));
        }
    }
}

/* a line th wide, anti-aliased (distance to the segment) */
static void bk_line(float x0, float y0, float x1, float y1, float th, uint32_t c)
{
    int i, j;
    float minx = fminf(x0, x1) - th, maxx = fmaxf(x0, x1) + th, miny = fminf(y0, y1) - th, maxy = fmaxf(y0, y1) + th;
    float vx = x1 - x0, vy = y1 - y0, l2 = vx * vx + vy * vy, R = th / 2 + 0.5f, inv_l2 = l2 > 0 ? 1.0f / l2 : 0;
    float out2 = (th / 2 + 0.5f) * (th / 2 + 0.5f), in2 = th >= 1.0f ? (th / 2 - 0.5f) * (th / 2 - 0.5f) : -1.0f;
    for (j = (int)miny; j <= (int)maxy; j++) {
        /* (only the columns that can lie within reach of the segment in this row: its stretch whose height is within
           R of the row's, widened by R - a slanted line no longer visits its whole box) */
        float rowy = (float)j + 0.5f, t0 = 0, t1 = 1, xa, xb;
        int ia, ib;
        if (vy != 0) {
            float ta = (rowy - R - y0) / vy, tb = (rowy + R - y0) / vy;
            t0 = fminf(ta, tb), t1 = fmaxf(ta, tb);
            t0 = t0 < 0 ? 0 : t0;
            t1 = t1 > 1 ? 1 : t1;
            if (t0 > t1)
                continue;
        } else if (fabsf(rowy - y0) > R)
            continue;
        xa = x0 + t0 * vx, xb = x0 + t1 * vx;
        ia = (int)floorf(fminf(xa, xb) - R) - 1, ib = (int)ceilf(fmaxf(xa, xb) + R) + 1;
        if (ia < (int)minx)
            ia = (int)minx;
        if (ib > (int)maxx)
            ib = (int)maxx;
        {   /* (no division and no root per pixel: the Cortex-A cores of the handhelds take ten cycles and more for
               each; inside and outside are decided on the squared distance, the root only in the soft edge) */
            float py = (float)j + 0.5f - y0, pyv = py * vy;
            for (i = ia; i <= ib; i++) {
                float px = (float)i + 0.5f - x0;
                float t = (px * vx + pyv) * inv_l2, dx, dy, d2, d;
                t = t < 0 ? 0 : t > 1 ? 1 : t;
                dx = px - t * vx;
                dy = py - t * vy;
                d2 = dx * dx + dy * dy;
                if (d2 >= out2)
                    continue;
                if (d2 <= in2) {
                    bk_blend(i, j, c, 255u);
                    continue;
                }
                d = sqrtf(d2) - th / 2;
                if (d < 0.5f)
                    bk_blend(i, j, c, d <= -0.5f ? 255u : (uint32_t)((0.5f - d) * 255.0f));
            }
        }
    }
}

/* ------------------------------------------------- languages (0.80) --- */
/* DEUTSCH / ENGLISH: the stage is written in English; with German chosen every text it draws is looked up in
   mutabledreams/lang/de.txt (pairs of lines "en ..." / "de ..."), whole, as it is drawn. Digits in a text stand for
   a '#' in the table and are put back in order ("CAUGHT 3   BEST 9" finds "CAUGHT #   BEST #"); what is not in the
   table stays English. German letters come in UTF-8 and are drawn from the faces' _X parts (196..252). English
   costs nothing (bk_lang 0: no lookup at all). */
static struct bk_tr_e { uint32_t h; const char *en, *de; } *bk_tr_tab;
static int bk_tr_cap, bk_tr_n, bk_tr_keys;      /* bk_tr_keys: a button's name is being drawn - never translated */
static char *bk_tr_mem;

static uint32_t bk_tr_hash(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++)
        h = (h ^ (uint8_t)*s) * 16777619u;
    return h ? h : 1u;
}
static const struct bk_tr_e *bk_tr_get(const char *s)
{
    uint32_t h, i;
    if (!bk_tr_cap)
        return 0;
    h = bk_tr_hash(s);
    for (i = h & (uint32_t)(bk_tr_cap - 1); bk_tr_tab[i].h; i = (i + 1u) & (uint32_t)(bk_tr_cap - 1))
        if (bk_tr_tab[i].h == h && !strcmp(bk_tr_tab[i].en, s))
            return &bk_tr_tab[i];
    return 0;
}
/* the table from a file (again when the language is chosen); its count, -1 when there is none */
static int bk_lang_load(const char *path, const char *code)
{
    FILE *f = fopen(path, "rb");
    char pre[8];
    snprintf(pre, sizeof pre, "%s ", code);            /* (1.6) the target language's line starts with its code: "de ", "fr ", "es " */
    long n;
    char *en = 0;
    free(bk_tr_tab), free(bk_tr_mem);
    bk_tr_tab = 0, bk_tr_mem = 0, bk_tr_cap = bk_tr_n = 0;
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || n > (1L << 22)) {               /* (unreadable, or no language table) */
        fclose(f);
        return -1;
    }
    bk_tr_mem = (char *)malloc((size_t)n + 1);
    if (!bk_tr_mem || fread(bk_tr_mem, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return -1;
    }
    fclose(f);
    bk_tr_mem[n] = 0;
    {
        int lines = 1;
        for (long k = 0; k < n; k++)
            lines += bk_tr_mem[k] == '\n';
        for (bk_tr_cap = 64; bk_tr_cap < lines * 2; bk_tr_cap *= 2)
            ;
        bk_tr_tab = (struct bk_tr_e *)calloc((size_t)bk_tr_cap, sizeof *bk_tr_tab);
        if (!bk_tr_tab)
            return bk_tr_cap = 0, -1;
    }
    for (char *l = bk_tr_mem; l && *l;) {
        char *e = strchr(l, '\n');
        if (e) {
            *e = 0;
            if (e > l && e[-1] == '\r')
                e[-1] = 0;
        }
        if (!strncmp(l, "en ", 3))
            en = l + 3;
        else if (!strncmp(l, pre, 3) && en) {
            uint32_t h = bk_tr_hash(en), i;
            {   /* (a format: the German must have the same %-fields in the same order, else it is not taken) */
                const char *p = en, *q = l + 3;
                int ok = 1;
                for (;;) {                       /* (a field: % and a letter; a % before anything else is a percent sign) */
                    while (*p && !(*p == '%' && p[1] && strchr("sdiucfx", p[1])))
                        p++;
                    while (*q && !(*q == '%' && q[1] && strchr("sdiucfx", q[1])))
                        q++;
                    if (!*p || !*q) {
                        ok = !*p && !*q;
                        break;
                    }
                    if (p[1] != q[1]) {
                        ok = 0;
                        break;
                    }
                    p += 2, q += 2;
                }
                if (!ok) {
                    fprintf(stderr, "lang: the fields of \"%s\" differ in %s, kept English\n", en, code);
                    en = 0;
                    goto next_line;
                }
            }
            for (i = h & (uint32_t)(bk_tr_cap - 1); bk_tr_tab[i].h && strcmp(bk_tr_tab[i].en, en); i = (i + 1u) & (uint32_t)(bk_tr_cap - 1))
                ;
            if (!bk_tr_tab[i].h)
                bk_tr_n++;
            bk_tr_tab[i].h = h, bk_tr_tab[i].en = en, bk_tr_tab[i].de = l + 3;
            en = 0;
        }
    next_line:
        l = e ? e + 1 : 0;
    }
    return bk_tr_n;
}
/* (1.6) the languages: English (no table) and German, French, Spanish (mutabledreams/lang/de.txt, fr.txt, es.txt, pairs of
   lines "en ..." / "de ..." and so on); the quick bar's stone and the options' row go round them. A table is read
   beside save/, else from the package in the working folder; -1 and the old language when none is found */
static int bk_tr_idx = -1;                       /* the language the table in memory belongs to */
static int bk_lang_choose_idx(int idx)
{
    char path[600];
    int n = -1;
    if (idx <= 0 || idx >= BK_NLANG) {
        bk_lang = bk_lang_idx = 0;
        return 0;
    }
    if (bk_tr_n > 0 && bk_tr_idx == idx) {       /* (read already) */
        bk_lang = 1, bk_lang_idx = idx;
        return bk_tr_n;
    }
    if (bk_dir[0]) {
        snprintf(path, sizeof path, "%s/../lang/%s.txt", bk_dir, BK_LANG_CODE[idx]);
        n = bk_lang_load(path, BK_LANG_CODE[idx]);
    }
    if (n <= 0) {
        snprintf(path, sizeof path, "package/knulli/mutabledreams/lang/%s.txt", BK_LANG_CODE[idx]);
        n = bk_lang_load(path, BK_LANG_CODE[idx]);
    }
    if (n <= 0) {                                /* (no table: English, and the next try reads again) */
        bk_tr_idx = -1;
        bk_lang = bk_lang_idx = 0;
        return -1;
    }
    bk_tr_idx = idx;
    bk_lang = 1, bk_lang_idx = idx;
    return n;
}
static int bk_lang_choose(int on) { return bk_lang_choose_idx(on ? 1 : 0); }
static int bk_lang_by_code(const char *c)
{
    for (int i = 0; i < BK_NLANG; i++)
        if (c && !strncmp(c, BK_LANG_CODE[i], 2))
            return i;
    return 0;
}
/* the next language that has a table (English always has none to need): the stone and the row go round; -1 when none worked */
static int bk_lang_cycle(void)
{
    int from = bk_lang_idx;
    for (int k = 1; k <= BK_NLANG; k++) {
        int idx = (from + k) % BK_NLANG;
        if (bk_lang_choose_idx(idx) >= 0)
            return bk_lang_idx;
    }
    return -1;
}
int bk_set_lang(int on) { bk_lang_forced = 1; return bk_lang_choose(on); }
int bk_set_lang_code(const char *code) { bk_lang_forced = 1; return bk_lang_choose_idx(bk_lang_by_code(code)); }
void bk_lang_default(int on) { if (!bk_lang_forced) bk_lang_choose(on); }   /* (the system's language: the saved choice still wins) */
int bk_lang_on(void) { return bk_lang; }
const char *bk_lang_code(void) { return BK_LANG_CODE[bk_lang_idx]; }
#ifdef BK_LOG_TEXT                               /* (collecting what the stage writes: every text once, digits as #) */
static void bk_tr_log(const char *t)
{
    static uint32_t seen[8192];
    static int nseen;
    uint32_t h = bk_tr_hash(t);
    for (int k = 0; k < nseen; k++)
        if (seen[k] == h)
            return;
    if (nseen < 8192)
        seen[nseen++] = h;
    fprintf(stderr, "TEXT %s\n", t);
}
#endif
/* s in the chosen language (s itself in English, or when the table does not know it) */
const char *bk_touch_text(const char *s);
static const char *bk_tr(const char *s)
{
    static char ring[16][256];
    static int w;
    char tmpl[256], *o;
    const struct bk_tr_e *e;
    int has_digit = 0, k = 0;
#ifndef BK_LOG_TEXT
    if (!bk_lang || !s || !*s || bk_tr_keys)
        return s;
#else
    if (!s || !*s || bk_tr_keys)
        return s;
#endif
    for (const char *q = s; *q && k < 255; q++) {   /* its pattern: each run of digits one '#' */
        if (*q >= '0' && *q <= '9') {
            if (!has_digit || tmpl[k - 1] != '#' || !(q[-1] >= '0' && q[-1] <= '9'))
                tmpl[k++] = '#';
            has_digit = 1;
        } else
            tmpl[k++] = *q;
    }
    tmpl[k] = 0;
#ifdef BK_LOG_TEXT
    bk_tr_log(tmpl);
    if (!bk_lang)
        return s;
#endif
    if (!has_digit) {
        e = bk_tr_get(s);
        return e ? e->de : s;
    }
    if (!(e = bk_tr_get(tmpl)))
        return s;
    o = ring[w++ & 15];                          /* the numbers put back, in their order */
    {
        const char *q = s, *d = e->de;
        int n = 0;
        for (; *d && n < 255; d++) {
            if (*d != '#') {
                o[n++] = *d;
                continue;
            }
            while (*q && !(*q >= '0' && *q <= '9'))
                q++;
            while (*q >= '0' && *q <= '9' && n < 255)
                o[n++] = *q++;
        }
        o[n] = 0;
    }
    return o;
}
const char *bk_touch_text(const char *s) { return bk_tr(s); }   /* (1.6) the touch buttons' labels in the stage's language */
#define T(s) bk_tr(s)

/* the face for a character: German letters (UTF-8, two bytes from 0xC3) from the face's _X part; *s moves on */
static const felucca_font_t *bk_font_x(const felucca_font_t *f)
{
    return f == &FONT_ST_T ? &FONT_ST_T_X : f == &FONT_ST_XS ? &FONT_ST_XS_X : f == &FONT_ST_H ? &FONT_ST_H_X
         : f == &FONT_ST_S ? &FONT_ST_S_X : f == &FONT_ST_B ? &FONT_ST_B_X : f == &FONT_ST_M ? &FONT_ST_M_X
         : f == &FONT_ST_L ? &FONT_ST_L_X : 0;
}
static uint32_t bk_glyph_at(const felucca_font_t **f, const char **s)
{
    uint8_t c = (uint8_t)**s;
    if (c >= 0xC0u && (uint8_t)(*s)[1] >= 0x80u && (uint8_t)(*s)[1] < 0xC0u) {   /* a two-byte letter */
        uint32_t cp = ((uint32_t)(c & 0x1Fu) << 6) | ((uint8_t)(*s)[1] & 0x3Fu);
        const felucca_font_t *fx = bk_font_x(*f);
        (*s)++;
        if (fx && cp >= fx->first && cp <= fx->last) {
            *f = fx;
            return cp - fx->first;
        }
        return glyph(*f, '?');
    }
    if (c >= 0xE0u) {                            /* (three bytes and more: a dash stands for it) */
        while (((uint8_t)(*s)[1] & 0xC0u) == 0x80u)
            (*s)++;
        return glyph(*f, '-');
    }
    return glyph(*f, c);
}
static int bk_tw(const felucca_font_t *f, const char *s)   /* width in pixels, German letters counted once */
{
    int w = 0;
    for (; *s; s++) {
        const felucca_font_t *g = f;
        uint32_t gi = bk_glyph_at(&g, &s);
        w += g->adv[gi];
    }
    return w;
}

/* text in one of the firmware's own faces (felucca_font.h, built unpacked for the host), scaled s x */
/* (1.6) LARGE TEXT: the smallest face is drawn one size up (and measured so); MORE CONTRAST: the dim greys brighter */
static int bk_big_ctx;                           /* (only on the screens with room: menus, help, guide, options, journal) */
static const felucca_font_t *bk_bigf(const felucca_font_t *f) { return bk_big_text && bk_big_ctx && f == &FONT_T ? &FONT_S : f; }
static uint32_t bk_contrast_col(uint32_t c)
{
    uint32_t a = c >> 24;
    if (c == BK_FAINT)
        return ARGB(255, 178, 188, 206);
    if (c == BK_DIM)
        return ARGB(255, 226, 232, 240);
    if (a >= 60u && a < 255u)
        a = a + 90u > 255u ? 255u : a + 90u;
    return a << 24 | (c & 0xFFFFFFu);
}
static int bk_text_w(const felucca_font_t *f, const char *s, int sc)
{
    return bk_tw(bk_bigf(f), bk_tr(s)) * sc;
}
static int bk_text(int x, int y0, const felucca_font_t *f0, int sc, const char *s, uint32_t c)
{
    const int y = y0;
    const felucca_font_t *f;
    f0 = bk_bigf(f0);
    f = f0;
    if (bk_contrast)
        c = bk_contrast_col(c);
    s = bk_tr(s);
#ifdef BK_LOG_FIT                                /* (finding texts off the screen: every one, once, to stderr) */
    {
        static char seen[96][64];
        static int nseen;
        int tw = bk_tw(f, s) * sc, k;
        if ((x < 0 || x + tw > BK_W || y < 0 || y + (int)f->h > BK_H) && (c >> 24) > 40u) {
            for (k = 0; k < nseen && strncmp(seen[k], s, 63); k++)
                ;
            if (k == nseen && nseen < 96) {
                snprintf(seen[nseen++], 64, "%s", s);
                fprintf(stderr, "OFF x %d..%d y %d: %s\n", x, x + tw, y, s);
            }
        }
    }
#endif
    for (; *s; s++) {
        uint32_t gi, w, bpr, gx, gy;
        f = f0;
        gi = bk_glyph_at(&f, &s), w = f->bw[gi], bpr = (w + 1u) / 2u;
        const uint8_t *gd = f->data + f->off[gi];
        int gx0 = x - (int)f->pad, y = y0 - ((int)f->h - (int)f0->h) * sc;   /* (a German capital's dots: above the line) */
        if (sc == 1 && gx0 >= 0 && gx0 + (int)w <= BK_W && y >= 0 && y + (int)f->h <= BK_H) {
            /* wholly on the stage, at its own size: straight into the pixels; the sound's turn once a glyph */
            uint32_t before = bk_poll_n;
            for (gy = 0; gy < f->h; gy++) {
                uint32_t *row = &bk_px[(y + (int)gy) * BK_W + gx0];
                const uint8_t *g = gd + gy * bpr;
                for (gx = 0; gx < w; gx++) {
                    uint32_t a = g[gx / 2u];
                    a = (gx & 1u) ? (a & 15u) : (a >> 4);
                    if (a)
                        bk_blend_px(&row[gx], c, a * 17u);
                }
            }
            bk_poll_n += w * f->h;
            if (((before ^ bk_poll_n) & ~2047u) && bk_yield_fn)
                bk_yield_fn();
            x += f->adv[gi];
            continue;
        }
        for (gy = 0; gy < f->h; gy++)
            for (gx = 0; gx < w; gx++) {
                uint32_t a = gd[gy * bpr + gx / 2u];
                int ox, oy;
                a = (gx & 1u) ? (a & 15u) : (a >> 4);
                if (!a)
                    continue;
                for (oy = 0; oy < sc; oy++)
                    for (ox = 0; ox < sc; ox++)
                        bk_blend(x + (int)(gx - f->pad) * sc + ox, y + (int)gy * sc + oy, c, a * 17u);
            }
        x += f->adv[gi] * sc;
    }
    return x;
}
static void bk_text_c(int cx, int y, const felucca_font_t *f, int sc, const char *s, uint32_t c)
{
    bk_text(cx - bk_text_w(f, s, sc) / 2, y, f, sc, s, c);
}
/* fit: shortened with ".." to w pixels */
/* s broken word by word into at most maxl lines of width w (each line up to 63 chars): their count, or 0 when it
   does not fit so (a word wider than w, or more lines needed) */
static int bk_wrap(const felucca_font_t *f, const char *s, int w, char lines[][64], int maxl)
{
    int n = 0;
    s = bk_tr(s);
    while (*s == ' ')
        s++;
    while (*s) {
        int len = 0, best = -1;
        char tmp[64];
        if (n == maxl)
            return 0;
        for (int k = 0; s[k] && k < 63; k++) {   /* the longest run of whole words that fits */
            if (s[k + 1] == ' ' || s[k + 1] == 0) {
                snprintf(tmp, sizeof tmp, "%.*s", k + 1, s);
                if (bk_tw(f, tmp) <= w)
                    best = k + 1;
                else
                    break;
            }
        }
        if (best < 0)
            return 0;
        len = best;
        snprintf(lines[n++], 64, "%.*s", len, s);
        s += len;
        while (*s == ' ')
            s++;
    }
    return n;
}
static void bk_text_fit(int x, int y, const felucca_font_t *f, const char *s, int w, uint32_t c)
{
    char buf[256];                               /* (48 cut longer lines short without a mark) */
    size_t n;
    s = bk_tr(s);
    n = strlen(s);
    if (n >= sizeof buf)
        n = sizeof buf - 1;
    memcpy(buf, s, n);
    buf[n] = 0;
    if (n > 2 && bk_tw(f, buf) > w) {
#ifdef BK_LOG_FIT                                /* (finding shortened texts: every one, once, to stderr) */
        static char seen[64][96];
        static int nseen;
        int k;
        for (k = 0; k < nseen && strncmp(seen[k], s, 95); k++)
            ;
        if (k == nseen && nseen < 64) {
            snprintf(seen[nseen++], 96, "%s", s);
            fprintf(stderr, "FIT %d px at %d,%d: %s\n", w, x, y, s);
        }
#endif
    }
    while (n > 2 && bk_tw(f, buf) > w) {         /* the last letter goes, the one before becomes '.' (German letters whole) */
        for (int cut = 0; cut < 2 && n > 0; cut++) {
            while (n > 1 && ((uint8_t)buf[n - 1] & 0xC0u) == 0x80u)
                buf[--n] = 0;
            buf[--n] = 0;
        }
        buf[n++] = '.', buf[n] = 0;
    }
    bk_text(x, y, f, 1, buf, c);
}

/* ---------------------------------------------------------- the stage --- */
#define BK_SX 152                                /* the FM-1 screen's place: 3 x 240 */
#define BK_SY 30
#define BK_LEFT_W 152
#define BK_RIGHT_X (BK_SX + 720)

static void gd_flower(float x, float y, float r, float open, uint32_t col, uint32_t heart);   /* brick_garden.inc */
static void gd_seedling(float x, float y, uint32_t col);
static void gd_butterfly(float x, float y, float t, uint32_t col);
static uint32_t gd_mix(uint32_t a, uint32_t b, float k);
static void bk_icon(int id, float cx, float cy, float r, uint32_t c, float t);
static void gd_leaf(float x, float y, float side, float len, float rise, uint32_t col, float a);
static void bk_biome_sigil(int id, float cx, float cy, float r, uint32_t c, float t);

/* NAMES AS THE FIRMWARE WRITES THEM > Names As The Garden Writes Them (FM-1, MIDI, LFO and words with digits kept) */
static void bk_title(const char *src, char *dst, int n)
{
    int i = 0, up = 1;
    while (*src && i < n - 1) {
        const char *w = src;
        int len = 0, keep = 0;
        while (w[len] && w[len] != ' ')
            len++;
        for (int k = 0; k < len; k++)
            keep |= w[k] >= '0' && w[k] <= '9';
        keep |= (len == 4 && !strncmp(w, "MIDI", 4)) || (len == 3 && !strncmp(w, "LFO", 3)) || (len == 2 && !strncmp(w, "OK", 2));
        for (int k = 0; k < len && i < n - 1; k++) {
            char c = w[k];
            if ((uint8_t)c == 0xC3u && k + 1 < len && i < n - 2) {   /* (0.80) a German capital: its small letter */
                uint8_t c2 = (uint8_t)w[k + 1];
                dst[i++] = c;
                dst[i++] = (char)(!keep && !up && c2 >= 0x80u && c2 <= 0x9Eu && c2 != 0x97u ? c2 + 0x20u : c2);
                k++, up = 0;
                continue;
            }
            dst[i++] = keep || up || c < 'A' || c > 'Z' ? c : (char)(c - 'A' + 'a');
            up = c == '(' || c == '/' || c == '-';
        }
        src += len;
        while (*src == ' ' && i < n - 1) {
            dst[i++] = *src++;
            up = 1;
        }
    }
    dst[i] = 0;
}

/* a key hint, garden-like: each button a pebble, its meaning beside it; the mode's name first, in colour.
 * Segments are separated by two or more spaces; a segment's first word (or what stands before ':') is its key. */
/* (0.78) on a PC without a game controller (bk_keyboard) a hint's key is named as on the keyboard, part by part:
   "L1/R1" is "Q/W", "R2+X" is "SHIFT+S" (brick_sdl.c's key_control: X = A, Z = B, S = X, A = Y, Q/W = L1/R1, 1 = L2,
   SHIFT = R2, ENTER = START, BACKSPACE = SELECT, ESC = MENU, TAB = FN; arrows and F1/F2 as they are) */
void bk_set_keyboard(int on) { bk_keyboard = on != 0; }
/* (0.80) the keyboard's names, as keys.txt binds them (brick_sdl.c: bk_set_key_label); these are its defaults */
static char bk_kmap[20][2][16] = {{"A", "X"}, {"B", "Z"}, {"X", "S"}, {"Y", "A"}, {"L1", "Q"}, {"R1", "W"}, {"L2", "1"},
                                  {"R2", "SHIFT"}, {"START", "ENTER"}, {"SELECT", "BKSP"}, {"MENU", "ESC"}, {"FN", "TAB"},
                                  {"D-PAD", "ARROWS"}, {"F1", "F1"}, {"F2", "F2"}, {"UP", "UP"}, {"DOWN", "DOWN"}, {"LEFT", "LEFT"},
                                  {"RIGHT", "RIGHT"}};
void bk_set_key_label(const char *brick, const char *key)
{
    for (int m = 0; m < 20; m++)
        if (!bk_kmap[m][0][0] || !strcmp(bk_kmap[m][0], brick)) {
            snprintf(bk_kmap[m][0], sizeof bk_kmap[m][0], "%s", brick);
            snprintf(bk_kmap[m][1], sizeof bk_kmap[m][1], "%s", key);
            return;
        }
}
static const char *const BK_BNAMES[BK_NBUTTONS] = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y", "L1", "R1", "L2", "R2",
                                                    "START", "SELECT", "MENU", "F1", "F2", "FN"};
/* (1.1) a button's name as the player presses it: its part moved to another button by OPTIONS, that button's name */
static const char *bk_phys_name(const char *logical)
{
    for (int i = 0; i < BK_NBUTTONS; i++)
        if (!strcmp(BK_BNAMES[i], logical))
            return BK_BNAMES[bk_map_phys(i)];
    return logical;
}
static const char *bk_key_label(const char *key)
{
    static char out[48];
    int n = 0;
    if (!bk_keyboard && bk_map_custom()) {      /* (moved buttons: each part of "R2+X", "L1/R1" by its new button) */
        static char mo[48];
        int k = 0;
        while (*key && k < (int)sizeof mo - 8) {
            char part[16];
            int j = 0;
            while (key[j] && key[j] != '/' && key[j] != '+' && j < 15)
                part[j] = key[j], j++;
            part[j] = 0;
            k += snprintf(mo + k, sizeof mo - (size_t)k, "%s", bk_phys_name(part));
            key += j;
            if (*key && k < (int)sizeof mo - 2)
                mo[k++] = *key++, mo[k] = 0;
        }
        return mo;
    }
    if (!bk_keyboard)
        return key;
    out[0] = 0;
    while (*key && n < (int)sizeof out - 8) {
        char part[16];
        int k = 0, m;
        while (key[k] && key[k] != '/' && key[k] != '+' && k < 15)
            part[k] = key[k], k++;
        part[k] = 0;
        snprintf(part, sizeof part, "%s", bk_phys_name(part));   /* (1.1: the button that acts as it, then its key) */
        for (m = 0; m < 20 && strcmp(bk_kmap[m][0], part); m++)
            ;
        n += snprintf(out + n, sizeof out - (size_t)n, "%s", m < 20 ? bk_kmap[m][1] : part);
        key += k;
        if (*key && n < (int)sizeof out - 2)
            out[n++] = *key++, out[n] = 0;
    }
    return out;
}
static int bk_is_key(const char *w, int len)
{
    static const char *const keys[] = {"UP", "DOWN", "UP/DOWN", "UP/DN", "LEFT/RIGHT", "L/R", "L1/R1", "L1", "R1", "L2", "R2",
                                       "R2+", "A", "B", "X", "Y", "START", "SELECT", "MENU", "F1", "F2", "FN", "D-PAD",
                                       "START/F1", "R2+F2", "X/Y", "B/X", "R2+X", "R2+A", 0};
    for (int k = 0; keys[k]; k++)
        if ((int)strlen(keys[k]) == len && !strncmp(keys[k], w, (size_t)len))
            return 1;
    return 0;
}
/* the button bar: each "KEY what" as a pebble with its word, large enough to read on the Brick; what does not fit
   in one row comes in turns, page after page, gliding (SELECT held shows the whole map) */
/* a hint for a handheld without MENU, F1, F2 and FN: their segments left out, F1's place telling of the quick bar
   (L2 reaches the Lab, the mixer, the menu and play mode), "B OR F1 CLOSE" just "B CLOSE" */
static const char *bk_hint_adapt(const char *hint)
{
    static char out[320];
    char menu_seg[96] = "";
    const char *s = hint;
    int n = 0;
    out[0] = 0;
    while (*s) {
        char seg[96], key[24];
        int len = 0, k = 0;
        while (*s == ' ')
            s++;
        while (s[len] && !(s[len] == ' ' && s[len + 1] == ' '))
            len++;
        if (!len)
            break;
        snprintf(seg, sizeof seg, "%.*s", len, s);
        s += len;
        while (seg[k] && seg[k] != ' ' && seg[k] != ':' && k < 23)
            key[k] = seg[k], k++;
        key[k] = 0;
        if (!strcmp(key, "F1") || !strcmp(key, "START/F1")) {
            if (!strstr(out, "L2 LAB"))
                snprintf(seg, sizeof seg, "L2 LAB, MIXER, MENU");
            else
                continue;
        } else if (!strcmp(key, "MENU")) {     /* (1.5.8) the MENU key's part is SELECT's on a handheld (brick_core.c
                                                    bk_release): "MENU WHERE TO" becomes "SELECT MENU", and it goes
                                                    to the front, after the place's name (Kid Bunka did not find it on
                                                    the line's second page) */
            snprintf(menu_seg, sizeof menu_seg, "SELECT MENU");   /* (the word one looks for; German: MENÜ) */
            continue;
        } else if (!strcmp(key, "F2") || !strcmp(key, "R2+F2"))
            continue;
        else if (!strcmp(key, "FN"))
            snprintf(seg, sizeof seg, "L2 PLAY MODE OFF");
        for (char *q; (q = strstr(seg, " OR F1")) || (q = strstr(seg, " OR F2"));)
            memmove(q, q + 6, strlen(q + 6) + 1);
        n += snprintf(out + n, sizeof out - (size_t)n, "%s%s", n ? "   " : "", seg);
        if (n >= (int)sizeof out - 1)
            break;
    }
    if (menu_seg[0]) {                           /* the menu's key right after the first segment (the place's name) */
        char tmp[320];
        const char *cut = strstr(out, "   ");
        if (cut)
            snprintf(tmp, sizeof tmp, "%.*s   %s%s", (int)(cut - out), out, menu_seg, cut);
        else
            snprintf(tmp, sizeof tmp, "%s%s%s", out, out[0] ? "   " : "", menu_seg);
        snprintf(out, sizeof out, "%s", tmp);
    }
    return out;
}
void bk_set_handheld(int on) { bk_handheld = on != 0; }
/* (1.5.8) the Miyoo's compromise: in the light picture the columns around the stage (the tracks on the left, the dials
   and tempo on the right, the top line and the hints at the bottom) are drawn every other frame only; between, they
   come back as they were, copied, and only the stage in the middle is drawn anew. They hardly move by themselves (a
   plant's sway, a meter: 7.5 times a second instead of 15). The frame right after a button changes is always whole,
   so what you press shows at once; while a dial's turn makes the stage's frame breathe, or anything is open over the
   stage (menu, quick bar, help, guide, options, module map), every frame is whole. Saves about a third of a frame on the Miyoo Mini Plus. MD_SIDE_HALF=1 (the Miyoo
   launchers; sidefull.on there: off). */
static int bk_side_half, bk_side_ok;
static float bk_side_dt;
static uint32_t bk_iris_at;                      /* (when the opening of a new place began: 0.42 s whole frames) */                         /* the time the columns have not been drawn: their glides catch up by it */
static uint32_t *bk_side_px;
void bk_set_side_half(int on) { bk_side_half = on != 0; bk_side_ok = 0; }
/* the part drawn anew each frame: the stage inside its frame, above the hint line (the frame's edge, a few pixels wide,
   and the hint line, which reaches 22 pixels up into the stage, are the columns' and come with them) */
#define BK_IN_X0 (BK_SX + 8)
#define BK_IN_X1 (BK_SX + 712)
#define BK_IN_Y0 BK_SY
#define BK_IN_Y1 (BK_SY + 698)
static void bk_side_copy(int save)               /* everything outside that, into the copy or back */
{
    uint32_t *a = save ? bk_side_px : bk_px, *b = save ? bk_px : bk_side_px;
    for (int y = 0; y < BK_H; y++) {
        size_t o = (size_t)y * BK_W;
        if (y < BK_IN_Y0 || y >= BK_IN_Y1)
            memcpy(a + o, b + o, BK_W * 4u);
        else {
            memcpy(a + o, b + o, BK_IN_X0 * 4u);
            memcpy(a + o + BK_IN_X1, b + o + BK_IN_X1, (BK_W - BK_IN_X1) * 4u);
        }
    }
}
void bk_set_light(int on)                        /* 2: by itself, the stage was too slow (and it says so) */
{
    if (on == 2 && !bk_light)
        bk_notice("LIGHT PICTURE", "THE STAGE WAS SLOW - L2: FULL PICTURE"), bk_light_auto = 1;
    else
        bk_light_auto = 0;
    bk_light = on != 0;
}
int bk_light_on(void) { return bk_light; }
/* CALM MOTION (0.75, from the players' wish for clarity): what only moves for decoration holds still - the shapes'
   breathing, the vines' sway, drifting spores and pollen, lifting stones and gliding cards; what shows the music
   (plants, sparks of a note, the playhead) still moves. The quick bar's last stone, calm.on or MD_CALM=1 */
void bk_set_calm(int on) { bk_calm = on != 0; }
/* (0.76) a handheld that shows the stage at 640 x 480 (MD_OUT): the smallest font, 13 px here, would be 8 there -
   the newer parts' small labels (playground, guide, tale, garden hint) take the next size up instead */
void bk_set_small(int on) { bk_small = on != 0; }
#define BK_TINY (bk_small ? &FONT_S : &FONT_T)
int bk_calm_on(void) { return bk_calm; }
/* ---- (1.6) the hint line as touch buttons: every segment that starts with a button names an action of this place; the
   D-pad's ones (drags), START, MENU, F1, F2, SELECT (the dock) and BACK (the dock) are left out ---- */
static bk_chip bk_chips_w[12], bk_chips_pub[12];
static int bk_chips_nw, bk_chips_npub;
static volatile uint32_t bk_chips_ver;
static void bk_chip_add(int btn, const char *label)
{
    char buf[26];
    size_t n;
    if (bk_chips_nw >= 12 || !label[0])
        return;
    for (int i = 0; i < bk_chips_nw; i++)
        if (bk_chips_w[i].btn == btn)
            return;
    snprintf(buf, sizeof buf, "%s", label);
    {   /* (what is in brackets goes; a colon only for HOLD: ...) */
        char *br = strstr(buf, " ("), *co = strchr(buf, ':');
        if (br)
            *br = 0;
        if (co && strncmp(buf, "HOLD:", 5))
            *co = 0;
    }
    n = strlen(buf);
    while (n && buf[n - 1] == ' ')
        buf[--n] = 0;
    bk_chips_w[bk_chips_nw].btn = (int8_t)btn;
    snprintf(bk_chips_w[bk_chips_nw].label, sizeof bk_chips_w[0].label, "%s", buf);
    bk_chips_nw++;
}
static void bk_chips_hint(const char *hint, int shift)
{
    static const struct { const char *t; int b; int n; int shifted; } TK[] = {
        {"A ", BK_A, 2, 0}, {"B ", BK_B, 2, 0}, {"X ", BK_X, 2, 0}, {"Y ", BK_Y, 2, 0}, {"L1 ", BK_L1, 3, 0}, {"R1 ", BK_R1, 3, 0},
        {"START ", BK_START, 6, 1}, {"MENU ", BK_MENU, 5, 1}, {"F1 ", BK_F1, 3, 1}, {"F2 ", BK_F2, 3, 1}, {"SELECT ", BK_SELECT, 7, 1}};
    const char *s = hint;
    while (*s) {
        char seg[100];
        int len = 0;
        const char *r;
        while (*s == ' ')
            s++;
        while (s[len] && !(s[len] == ' ' && s[len + 1] == ' '))
            len++;
        if (!len)
            break;
        snprintf(seg, sizeof seg, "%.*s", len, s);
        s += len;
        r = seg;
        if (!strncmp(r, "L1/R1 ", 6)) {          /* a pair: one each way */
            char a[26], b[26];
            snprintf(a, sizeof a, "< %.20s", r + 6);
            snprintf(b, sizeof b, "%.20s >", r + 6);
            bk_chip_add(BK_L1, a), bk_chip_add(BK_R1, b);
            continue;
        }
        if (!strncmp(r, "L1+R1 ", 6)) {          /* (the Sound Lab: both at once; F2 does the same) */
            bk_chip_add(BK_F2, r + 6);
            continue;
        }
        if (!strncmp(r, "X OR L1/R1 ", 11)) {      /* (SPACE: X or L1/R1 for the next panel) */
            bk_chip_add(BK_X, r + 11);
            continue;
        }
        for (unsigned k = 0; k < sizeof TK / sizeof TK[0]; k++)
            if (!strncmp(r, TK[k].t, (size_t)TK[k].n)) {
                const char *lab = r + TK[k].n;
                if (TK[k].shifted && !shift)
                    break;                       /* (START, MENU, F1, F2, SELECT are the dock's, unless R2 changes what they do) */
                if (!strncmp(lab, "OR ", 3)) {   /* "X OR B DONE", "B OR F2 BACK": the second key's name goes */
                    lab += 3;
                    while (*lab && *lab != ' ')
                        lab++;
                    while (*lab == ' ')
                        lab++;
                }
                if (TK[k].b == BK_B && (!strncmp(lab, "BACK", 4) || !strncmp(lab, "CLOSE", 5) || !strncmp(lab, "DONE", 4)))
                    break;                       /* (the dock's BACK) */
                bk_chip_add(TK[k].b, lab);
                break;
            }
    }
}
static void bk_chips_publish(void)
{
    bk_chips_ver++;                              /* (odd: being written) */
    memcpy(bk_chips_pub, bk_chips_w, sizeof bk_chips_w);
    bk_chips_npub = bk_chips_nw;
    bk_chips_ver++;
}
int bk_touch_chips(bk_chip *out, int max)
{
    for (int tries = 0; tries < 4; tries++) {
        uint32_t v1 = bk_chips_ver;
        int n = bk_chips_npub < max ? bk_chips_npub : max;
        if (v1 & 1u)
            continue;
        memcpy(out, bk_chips_pub, (size_t)n * sizeof(bk_chip));
        if (v1 == bk_chips_ver)
            return n;
    }
    return 0;
}
static void bk_draw_hint_at(const char *hint, uint32_t col, int by)
{
    if (bk_handheld)
        hint = bk_hint_adapt(hint);
    char seg[20][72];
    int n = 0, wseg[20], page_of[20], npages = 1, x, i;
    const char *s = hint;
    static char last[256];
    static uint32_t since;
    uint32_t now = bk_ms(), el;
    float fade = 1.0f;
    const felucca_font_t *f = &FONT_H;
    if (strncmp(last, hint, sizeof last - 1)) {   /* a new set of hints: from its first page */
        snprintf(last, sizeof last, "%s", hint);
        since = now;
    }
    while (*s && n < 20) {                       /* the segments */
        int len = 0;
        while (*s == ' ')
            s++;
        while (s[len] && !(s[len] == ' ' && s[len + 1] == ' '))
            len++;
        if (!len)
            break;
        snprintf(seg[n++], sizeof seg[0], "%.*s", len, s);
        s += len;
    }
    {   /* each segment's width; pages that fit the bar */
        int used = 0;
        for (i = 0; i < n; i++) {
            const char *sp = strchr(seg[i], ' ');
            int kl = sp ? (int)(sp - seg[i]) : (int)strlen(seg[i]);
            if (bk_is_key(seg[i], kl) && sp) {
                char key[24];
                snprintf(key, sizeof key, "%.*s", kl, seg[i]);
                bk_tr_keys = 1;
                wseg[i] = bk_text_w(f, bk_key_label(key), 1) + 18 + 6;
                bk_tr_keys = 0;
                wseg[i] += bk_text_w(f, sp + 1, 1) + 22;
            } else
                wseg[i] = bk_text_w(f, seg[i], 1) + 22;
            if (i > 1 && used + wseg[i] > BK_W - 40) {   /* (the first segment, the context's name, on every page) */
                npages++;
                used = wseg[0];
            }
            if (i == 0)
                used = 0;
            used += wseg[i];
            page_of[i] = npages - 1;
        }
    }
    el = now - since;
    int page = (int)((el / 4500u) % (uint32_t)npages);
    if (npages > 1) {
        uint32_t in = el % 4500u;
        fade = in < 250u ? (float)in / 250.0f : in > 4250u ? (float)(4500u - in) / 250.0f : 1.0f;
    }
    for (int yy = by - 7; yy < by + 33; yy++)    /* the bar: a band of night, darker downwards */
        if (by != BK_H - 33)
            bk_rect(0, yy, BK_W, 1, ARGB((uint32_t)(140 + (yy - (by - 7)) * 2), 3, 5, 12));
        else if (yy < BK_SY + 720)               /* (1.3: the usual place is baked into the night; only where it
                                                    lies over the centre, which draws itself, it is blended here) */
            bk_span(BK_SX, BK_SX + 720, yy, ARGB((uint32_t)(140 + (yy - (by - 7)) * 2), 3, 5, 12));
    {
        int total = 0;
        for (i = 0; i < n; i++)
            if (i == 0 || page_of[i] == page)
                total += wseg[i];
        x = BK_W / 2 - total / 2;
    }
    for (i = 0; i < n; i++) {
        char *sp = strchr(seg[i], ' ');
        int kl = sp ? (int)(sp - seg[i]) : (int)strlen(seg[i]);
        uint32_t a = i == 0 ? 255u : (uint32_t)(255.0f * fade);
        if (i && page_of[i] != page)
            continue;
        if (bk_is_key(seg[i], kl) && sp) {
            int kw;
            *sp = 0;
            bk_tr_keys = 1;                      /* (the button's own name, as printed on it) */
            kw = bk_text_w(f, bk_key_label(seg[i]), 1) + 18;
            bk_rrect(x, by, kw, 28, 14, BK_ALPHA(ARGB(255, 232, 236, 222), (uint32_t)(235 * a / 255)));
            bk_text(x + 9, by + 2, f, 1, bk_key_label(seg[i]), BK_ALPHA(ARGB(255, 14, 20, 28), a));
            bk_tr_keys = 0;
            x += kw + 6;
            bk_text(x, by + 2, f, 1, sp + 1, BK_ALPHA(ARGB(255, 214, 220, 228), a));
            x += bk_text_w(f, sp + 1, 1) + 22;
            *sp = ' ';
        } else {
            bk_text(x, by + 2, f, 1, seg[i], i == 0 ? col : BK_ALPHA(ARGB(255, 214, 220, 228), a));
            x += bk_text_w(f, seg[i], 1) + 22;
        }
    }
    if (npages > 1)                              /* which page: small seeds at the right */
        for (i = 0; i < npages; i++)
            bk_disc((float)(BK_W - 14 - (npages - 1 - i) * 12), (float)(by + 14), i == page ? 4.0f : 2.5f,
                    i == page ? col : ARGB(110, 255, 255, 255));
}

static void bk_draw_hint(const char *hint, uint32_t col) { bk_draw_hint_at(hint, col, BK_H - 33); }

static float bk_level[6], bk_bloom[6], bk_sway[6];
static float bk_kick;                            /* a turn's impulse: everything in sight moves with it, then settles */
static uint32_t bk_kick_seen;
static uint32_t bk_last_events[6], bk_last_draw;
static uint32_t bk_bg_row[BK_H];
static float bk_spore_x[48], bk_spore_y[48], bk_spore_v[48];

/* (1.3) grounds baked into the night: the side columns' quiet boxes (the track cards, the dials' and the Lab list's
   grounds, the tempo box) drawn once into a copy of the background, not blended anew each frame. A frame lists them
   with bk_ground(); one that matches the baked list in order is skipped (it is already there); a frame whose list
   differs draws the rest live, and the next frame bakes the new list. (Measured: about a third of the see-through
   pixels of every screen.) A ground left out at the end shows one frame longer, faint as these grounds are. */
#define BK_GROUNDS 64
typedef struct { int16_t x, y, w, h, r; uint32_t c; } bk_ground_t;
/* two layers: 0 the side columns (over the night, bk_background), 1 the centre of a full view or the Lab (over the
   garden's kept night, gd_backdrop: only in a frame that drew it, bk_gr_centre) */
static bk_ground_t bk_gr_baked[2][BK_GROUNDS], bk_gr_cur[2][BK_GROUNDS];
static int bk_gr_nbaked[2] = {-1, -1}, bk_gr_ncur[2], bk_gr_differs[2], bk_gr_rebake[2] = {1, 1}, bk_gr_centre;
#define BK_SIDE_W (BK_SX + BK_W - BK_RIGHT_X)
static void bk_rrect(int x, int y, int w, int h, int r, uint32_t c);
/* (1.5.5) MD_GROUND_CACHE=0: no baking at all - every ground blended each frame, as until 1.2 (the RGB20S's launcher
   sets it: its picture flickered beside the menus with the caches, and 1.0 there was steady) */
static int bk_gr_off = -1;
static int bk_ground_cache_off(void)
{
    if (bk_gr_off < 0) {
        const char *e = getenv("MD_GROUND_CACHE");
        bk_gr_off = e && e[0] == '0';
    }
    return bk_gr_off;
}
static void bk_ground(int x, int y, int w, int h, int r, uint32_t c)
{
    if (bk_ground_cache_off()) {
        bk_rrect(x, y, w, h, r, c);
        return;
    }
    bk_ground_t g = {(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, (int16_t)r, c};
    int side = (x + w <= BK_SX || x >= BK_RIGHT_X) && y >= 0 && y + h <= BK_H;
    int centre = bk_gr_centre && x >= BK_SX && x + w <= BK_SX + 720 && y >= BK_SY && y + h <= BK_SY + 720;
    int L = side ? 0 : 1, k = bk_gr_ncur[L];
    if ((!side && !centre) || k >= BK_GROUNDS) {
        bk_rrect(x, y, w, h, r, c);              /* (elsewhere: as ever) */
        return;
    }
    bk_gr_cur[L][k] = g, bk_gr_ncur[L] = k + 1;
    if (!bk_gr_differs[L] && k < bk_gr_nbaked[L] && !memcmp(&bk_gr_baked[L][k], &g, sizeof g))
        return;                                  /* (baked: already there) */
    bk_gr_differs[L] = 1;
    bk_rrect(x, y, w, h, r, c);
}
static void bk_ground_frame_end(void)            /* (after a frame: a list that changed is baked for the next) */
{
    for (int L = 0; L < 2; L++) {
        if (bk_gr_differs[L] || bk_gr_ncur[L] != bk_gr_nbaked[L]) {
            memcpy(bk_gr_baked[L], bk_gr_cur[L], sizeof(bk_ground_t) * (size_t)bk_gr_ncur[L]);
            bk_gr_nbaked[L] = bk_gr_ncur[L], bk_gr_rebake[L] = 1;
        }
        bk_gr_ncur[L] = 0, bk_gr_differs[L] = 0;
    }
    bk_gr_centre = 0;
}
static int bk_garden_shown(void);
static int bk_gr_key[2] = {-1, -1};
static void bk_ground_key(int L, int key)        /* (another screen: nothing of the last one's grounds is laid) */
{
    if (key != bk_gr_key[L])
        bk_gr_key[L] = key, bk_gr_nbaked[L] = 0, bk_gr_rebake[L] = 1;
}
static void bk_ground_bake(int L)                /* (the baked list drawn, into what lies in bk_px now) */
{
    for (int i = 0; i < bk_gr_nbaked[L]; i++)
        bk_rrect(bk_gr_baked[L][i].x, bk_gr_baked[L][i].y, bk_gr_baked[L][i].w, bk_gr_baked[L][i].h, bk_gr_baked[L][i].r, bk_gr_baked[L][i].c);
}
static int bk_bg_spores_only;                    /* (1.5.8) a frame whose columns come back copied: only the spores */
static void bk_background(float t, float energy)
{
    int y, i;
    if (!bk_bg_row[0]) {
        for (y = 0; y < BK_H; y++) {
            uint32_t a = (uint32_t)(y * 255 / (BK_H - 1));
            bk_bg_row[y] = ARGB(255, ((BK_BG0 >> 16 & 255u) * (255u - a) + (BK_BG1 >> 16 & 255u) * a) / 255u,
                                ((BK_BG0 >> 8 & 255u) * (255u - a) + (BK_BG1 >> 8 & 255u) * a) / 255u,
                                ((BK_BG0 & 255u) * (255u - a) + (BK_BG1 & 255u) * a) / 255u);
        }
        for (i = 0; i < 48; i++) {
            uint32_t h = (uint32_t)i * 2654435761u;
            bk_spore_x[i] = (float)(h % BK_W);
            bk_spore_y[i] = (float)((h >> 11) % BK_H);
            bk_spore_v[i] = 4.0f + (float)((h >> 21) % 12u);
        }
    }
    if (bk_bg_spores_only)                       /* (the night around the stage is copied back after it anyway) */
        goto spores;
    {   /* the night around the centre: the gradient, stars, hills along the bottom; drawn once and kept */
        static uint32_t bg[BK_W * BK_H];
        static int ready;
        if (!ready) {
            for (y = 0; y < BK_H; y++)
                for (i = 0; i < BK_W; i++)
                    bk_px[y * BK_W + i] = bk_bg_row[y];
            for (i = 0; i < 110; i++) {
                uint32_t h = (uint32_t)i * 2654435761u;
                bk_disc((float)(h % BK_W), (float)((h >> 10) % 620u), 0.6f + (float)(h >> 20 & 3u) * 0.3f, ARGB(40 + (h >> 24 & 63u), 230, 236, 255));
            }
            for (int layer = 0; layer < 2; layer++)
                for (i = 0; i < BK_W; i += 2) {
                    float u = (float)i / (float)BK_W, hy = 704.0f + (float)layer * 22.0f + 16.0f * sinf(u * (3.1f + (float)layer * 2.3f) + (float)layer);
                    bk_rect(i, (int)hy, 2, BK_H - (int)hy, layer ? ARGB(255, 6, 9, 15) : ARGB(255, 11, 16, 27));
                }
            for (int yy = BK_H - 40; yy < BK_H; yy++)   /* (1.3) the hint line's band, baked (bk_draw_hint_at) */
                bk_rect(0, yy, BK_W, 1, ARGB((uint32_t)(140 + (yy - (BK_H - 40)) * 2), 3, 5, 12));
            memcpy(bg, bk_px, sizeof bg);
            ready = 1;
            bk_gr_rebake[0] = 1;
        }
        {   /* (1.3) the side columns from their baked copy (bk_ground); baked anew when their grounds changed */
            static uint32_t side[BK_H * BK_SIDE_W];
            if (bk_ground_cache_off()) {         /* (1.5.5: no cache - the plain night, the grounds drawn each frame) */
                for (y = 0; y < BK_H; y++) {
                    memcpy(&bk_px[y * BK_W], &bg[y * BK_W], BK_SX * 4u);
                    memcpy(&bk_px[y * BK_W + BK_RIGHT_X], &bg[y * BK_W + BK_RIGHT_X], (BK_W - BK_RIGHT_X) * 4u);
                }
                goto sides_done;
            }
            /* (1.5.4) the key holds everything the side columns' grounds follow - the chosen track, how many tracks
               sound, the chosen dial, the Lab's module - so a change is known before the frame and that frame is drawn
               whole; before, the old baked grounds showed one frame beside the new (the RGB20S: a flicker as the
               cursor moved) */
            bk_ground_key(0, lab_open | (int)((bk_held >> BK_FN) & 1u) << 1 | (bk_view != 0) << 2 | bk_garden_shown() << 3 |
                             (dream_ui.track & 7) << 4 | (md_active_tracks() & 7) << 7 | (bk_focus & 3) << 10 | (lab_mod & 31) << 12);
            if (bk_gr_rebake[0]) {
                for (y = 0; y < BK_H; y++) {
                    memcpy(&bk_px[y * BK_W], &bg[y * BK_W], BK_SX * 4u);
                    memcpy(&bk_px[y * BK_W + BK_RIGHT_X], &bg[y * BK_W + BK_RIGHT_X], (BK_W - BK_RIGHT_X) * 4u);
                }
                bk_ground_bake(0);
                for (y = 0; y < BK_H; y++) {
                    memcpy(&side[y * BK_SIDE_W], &bk_px[y * BK_W], BK_SX * 4u);
                    memcpy(&side[y * BK_SIDE_W + BK_SX], &bk_px[y * BK_W + BK_RIGHT_X], (BK_W - BK_RIGHT_X) * 4u);
                }
                bk_gr_rebake[0] = 0;
            }
            for (y = 0; y < BK_H; y++) {
                memcpy(&bk_px[y * BK_W], &side[y * BK_SIDE_W], BK_SX * 4u);
                memcpy(&bk_px[y * BK_W + BK_RIGHT_X], &side[y * BK_SIDE_W + BK_SX], (BK_W - BK_RIGHT_X) * 4u);
            }
        sides_done:;
        }
        for (y = 0; y < BK_H; y++)               /* (the centre draws itself; above and below it, the night) */
            if (y < BK_SY || y >= BK_SY + 720)
                memcpy(&bk_px[y * BK_W + BK_SX], &bg[y * BK_W + BK_SX], 720u * 4u);
    }
spores:
    /* drifting spores, brighter with the sound */
    for (i = 0; i < 48; i++) {
        float y0 = fmodf(bk_spore_y[i] - t * bk_spore_v[i] + BK_H * 8, (float)BK_H);
        float x0 = bk_spore_x[i] + 10.0f * sinf(t * 0.3f + (float)i);
        bk_disc(x0, y0, 1.2f + (float)(i % 3) * 0.5f, BK_ALPHA(bk_track_col(i), 40 + (uint32_t)(150 * energy)));
    }
}

static void bk_garden_frame(uint32_t col);       /* brick_views_draw.inc */

/* the FM-1's picture in the garden's style: Scale3x (EPX) for clean edges, then added as light onto the garden's
 * night: its dark background becomes the sky, its lines and words glow on it */
static void bk_fm1_words(void);
static void bk_fm1_leaves(void);
static void bk_fm1_lines(void);
static void bk_fm1_points(void);
/* the FM-1 page's name and value for dial k (its words "K1 TONE" and the number under them), or 0 */
static int bk_fm1_dial(int k, char *name, int nn, char *value, int nv)
{
    if (!fm1t_hires)
        return 0;
    for (int i = 0; i < fm1t_n; i++) {
        const fm1_word *w = &fm1t[i];
        if (w->hidden || w->s[0] != 'K' || w->s[1] != (char)('1' + k) || (w->s[2] != ' ' && w->s[2] != ':'))
            continue;
        snprintf(name, (size_t)nn, "%s", w->s + 3 + (w->s[2] == ':' && w->s[3] == ' '));
        value[0] = 0;
        for (int j = 0; j < fm1t_n; j++)          /* the value: the next words below it, in the same cell */
            if (j != i && !fm1t[j].hidden && abs(fm1t[j].x - w->x) <= 3 && fm1t[j].y > w->y + 6 && fm1t[j].y < w->y + 26) {
                snprintf(value, (size_t)nv, "%s", fm1t[j].s);
                break;
            }
        return 1;
    }
    return 0;
}
static int gd_defer_life;                        /* (brick_garden.inc: the backdrop's moving part held back) */
static uint32_t gd_bd_ver, gd_bd_col;            /* (counts the backdrop's rebuilds; its colour: brick_garden.inc) */
static void gd_backdrop_life(float t);
/* (1.5.6) the FM-1 page's light in parts: what follows makes the source (the page's pixels above its background), the
   glow (halo) and the light (Scale3x, half smoothed, plus the glow) for a rectangle of the 240 x 240 page only, so a
   page where only a little moves (GRAIN: its grains; the meters) is not made anew whole each frame. The Miyoo Mini
   Plus drew GRAIN at 3.8 pictures a second (259 ms a frame), the whole page rebuilt every frame. */
static uint32_t fm1_src[240 * 240], fm1_light[720 * 720];
static uint16_t fm1_halo[240 * 240 * 3];
static inline uint32_t fm1_rgb(uint16_t px)
{
    uint32_t c = swap16(px);
    return ((c >> 11 & 31u) * 255u / 31u) << 16 | ((c >> 5 & 63u) * 255u / 63u) << 8 | (c & 31u) * 255u / 31u;
}
static void fm1_src_rect(const uint16_t *fb, uint32_t bg, int x0, int y0, int x1, int y1)
{
    for (int y = y0; y <= y1; y++)               /* what stands above the background */
        for (int x = x0; x <= x1; x++) {
            uint32_t c = fm1_rgb(fb[y * 240 + x]);
            int r = (int)(c >> 16 & 255u) - (int)(bg >> 16 & 255u), g = (int)(c >> 8 & 255u) - (int)(bg >> 8 & 255u), b = (int)(c & 255u) - (int)(bg & 255u);
            fm1_src[y * 240 + x] = c == bg ? 0u : (uint32_t)(r < 0 ? 0 : r) << 16 | (uint32_t)(g < 0 ? 0 : g) << 8 | (uint32_t)(b < 0 ? 0 : b);
        }
}
static void fm1_halo_rect(int x0, int y0, int x1, int y1)   /* the glow: each channel blurred (two box passes, 5 wide) */
{
    static uint16_t tmp[240 * 240];
    int ty0 = y0 - 2 < 0 ? 0 : y0 - 2, ty1 = y1 + 2 > 239 ? 239 : y1 + 2;
    for (int ch = 0; ch < 3; ch++) {
        int sh = 16 - ch * 8;
        for (int y = ty0; y <= ty1; y++)
            for (int x = x0; x <= x1; x++) {
                int sum = 0;
                for (int d = -2; d <= 2; d++)
                    sum += (int)(fm1_src[y * 240 + (x + d < 0 ? 0 : x + d > 239 ? 239 : x + d)] >> sh & 255u);
                tmp[y * 240 + x] = (uint16_t)sum;
            }
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                int sum = 0;
                for (int d = -2; d <= 2; d++)
                    sum += tmp[(y + d < 0 ? 0 : y + d > 239 ? 239 : y + d) * 240 + x];
                fm1_halo[(y * 240 + x) * 3 + ch] = (uint16_t)(sum / 25);
            }
        bk_draw_count(2048u);
    }
}
static void fm1_light_rect(int x0, int y0, int x1, int y1)
{
    const uint32_t *src = fm1_src;
    for (int y = y0; y <= y1; y++)
        for (int x = x0, k0 = (bk_draw_count(2048u), 0); x <= x1; x++) {
            uint32_t E = src[y * 240 + x];
            uint32_t B = y > 0 ? src[(y - 1) * 240 + x] : E, H = y < 239 ? src[(y + 1) * 240 + x] : E;
            uint32_t D = x > 0 ? src[y * 240 + x - 1] : E, F = x < 239 ? src[y * 240 + x + 1] : E;
            uint32_t A = y > 0 && x > 0 ? src[(y - 1) * 240 + x - 1] : E, C = y > 0 && x < 239 ? src[(y - 1) * 240 + x + 1] : E;
            uint32_t G = y < 239 && x > 0 ? src[(y + 1) * 240 + x - 1] : E, I = y < 239 && x < 239 ? src[(y + 1) * 240 + x + 1] : E;
            uint32_t o[9];
            int k;
            (void)k0;
            if (B != H && D != F) {               /* Scale3x */
                o[0] = D == B ? D : E;
                o[1] = (D == B && E != C) || (B == F && E != A) ? B : E;
                o[2] = B == F ? F : E;
                o[3] = (D == B && E != G) || (D == H && E != A) ? D : E;
                o[4] = E;
                o[5] = (B == F && E != I) || (H == F && E != C) ? F : E;
                o[6] = D == H ? D : E;
                o[7] = (D == H && E != I) || (H == F && E != G) ? H : E;
                o[8] = H == F ? F : E;
            } else
                for (k = 0; k < 9; k++)
                    o[k] = E;
            for (k = 0; k < 9; k++) {             /* the light: the pixel (Scale3x, half of it smoothed: painted, not
                                                     stepped), and its glow (a little, soft) */
                const uint16_t *hl = &fm1_halo[(y * 240 + x) * 3];
                float fx = (float)(k % 3 - 1) / 3.0f, fy = (float)(k / 3 - 1) / 3.0f;
                uint32_t nx = fx < 0 ? D : F, ny = fy < 0 ? B : H, nd = fx < 0 ? (fy < 0 ? A : G) : (fy < 0 ? C : I);
                float ax = fabsf(fx), ay = fabsf(fy), w0 = (1 - ax) * (1 - ay), w1 = ax * (1 - ay), w2 = (1 - ax) * ay, w3 = ax * ay;
                int sr = (int)(w0 * (float)(E >> 16 & 255u) + w1 * (float)(nx >> 16 & 255u) + w2 * (float)(ny >> 16 & 255u) + w3 * (float)(nd >> 16 & 255u));
                int sg = (int)(w0 * (float)(E >> 8 & 255u) + w1 * (float)(nx >> 8 & 255u) + w2 * (float)(ny >> 8 & 255u) + w3 * (float)(nd >> 8 & 255u));
                int sb = (int)(w0 * (float)(E & 255u) + w1 * (float)(nx & 255u) + w2 * (float)(ny & 255u) + w3 * (float)(nd & 255u));
                int r = ((int)(o[k] >> 16 & 255u) + sr) / 2 + hl[0] * 3 / 5, g = ((int)(o[k] >> 8 & 255u) + sg) / 2 + hl[1] * 3 / 5,
                    b = ((int)(o[k] & 255u) + sb) / 2 + hl[2] * 3 / 5;
                fm1_light[(y * 3 + k / 3) * 720 + x * 3 + k % 3] = (uint32_t)(r > 255 ? 255 : r) << 16 | (uint32_t)(g > 255 ? 255 : g) << 8 | (uint32_t)(b > 255 ? 255 : b);
            }
        }
}
static inline uint32_t fm1_add(uint32_t D, uint32_t L)   /* three channels added at once, each held at 255 (no branches) */
{
    uint32_t s = (D & 0x7F7F7Fu) + (L & 0x7F7F7Fu), hb = (D ^ L) & 0x808080u;
    uint32_t carry = ((D & L) | (s & hb)) & 0x808080u;
    return 0xFF000000u | ((s ^ hb) & 0xFFFFFFu) | ((carry >> 7) * 0xFFu);
}
static void bk_draw_screen_garden(void)
{
    /* the FM-1 page's light: its pixels above its background, made smooth (Scale3x) and given a soft glow round
       them, kept as a layer of light and laid onto the night each frame; made again only where the page changes */
    static uint32_t last_hash, bg_now;
    static uint16_t prev[240 * 240];
    static int made;
    const uint16_t *fb = web_fb();
    uint32_t hsh = 2166136261u;
    int x, y;
    /* the frame, the night and the page's light laid on it are kept as one picture (732 x 732, the frame's whole box)
       while the page, its colour and the night stay the same; the moving stars and fireflies go on top (0.64: the
       light's add over 720 x 720 pixels was a third of every frame on an FM-1 page). The night under the light is kept
       too (1.5.6), so a part of the page that changed is laid anew there alone. */
    static uint32_t box[732 * 732], night[732 * 732], box_hash, box_col, box_ver;
    static int box_ok;
    uint32_t fcol = bk_track_col(dream_ui.track);
    float tl = (float)bk_ms() / 1000.0f;
    for (y = 0; y < 240 * 240; y += 3)
        hsh = (hsh ^ fb[y]) * 16777619u;
    if (box_ok && made && box_col == fcol && box_ver == gd_bd_ver && gd_bd_col == fcol && fm1_rgb(fb[239 * 240]) == bg_now) {
        if (hsh != last_hash) {                   /* (1.5.6) the page changed: only where it did */
            int x0 = 240, y0 = 240, x1 = -1, y1 = -1;
            for (y = 0; y < 240; y++) {
                const uint16_t *a = &fb[y * 240], *b = &prev[y * 240];
                if (!memcmp(a, b, 480))
                    continue;
                y0 = y < y0 ? y : y0, y1 = y;
                for (x = 0; x < 240; x++)
                    if (a[x] != b[x]) {
                        x0 = x < x0 ? x : x0;
                        break;
                    }
                for (x = 239; x >= 0; x--)
                    if (a[x] != b[x]) {
                        x1 = x > x1 ? x : x1;
                        break;
                    }
            }
            if (y1 >= 0) {
                int hx0 = x0 - 2 < 0 ? 0 : x0 - 2, hy0 = y0 - 2 < 0 ? 0 : y0 - 2, hx1 = x1 + 2 > 239 ? 239 : x1 + 2, hy1 = y1 + 2 > 239 ? 239 : y1 + 2;
                fm1_src_rect(fb, bg_now, x0, y0, x1, y1);
                fm1_halo_rect(hx0, hy0, hx1, hy1);   /* (the glow reaches 2 round what changed) */
                fm1_light_rect(hx0, hy0, hx1, hy1);  /* (the light: the glow there, and Scale3x's neighbours) */
                for (y = hy0 * 3; y < hy1 * 3 + 3; y++) {
                    uint32_t *d = &box[(y + 6) * 732 + 6], *n = &night[(y + 6) * 732 + 6];
                    const uint32_t *l = &fm1_light[y * 720];
                    bk_draw_count(256u);
                    for (x = hx0 * 3; x < hx1 * 3 + 3; x++)
                        d[x] = fm1_add(n[x], l[x] & 0xFFFFFFu);
                }
                memcpy(prev, fb, sizeof prev);
            }
            last_hash = hsh;
        }
        for (y = 0; y < 732; y++) {
            memcpy(&bk_px[(BK_SY - 6 + y) * BK_W + BK_SX - 6], &box[y * 732], 732u * 4u);
            bk_draw_count(732u);
        }
        gd_backdrop_life(tl);
        bk_fm1_lines();
        bk_fm1_points();
        bk_fm1_leaves();
        bk_fm1_words();
        return;
    }
    gd_defer_life = 1;
    bk_garden_frame(fcol);
    gd_defer_life = 0;
    for (y = 0; y < 732; y++)                    /* (the night alone, kept: the parts laid anew later lie on it) */
        memcpy(&night[y * 732], &bk_px[(BK_SY - 6 + y) * BK_W + BK_SX - 6], 732u * 4u);
    if (!made || hsh != last_hash || fm1_rgb(fb[239 * 240]) != bg_now) {
        made = 1, last_hash = hsh;
        bg_now = fm1_rgb(fb[239 * 240]);          /* (a corner: the background) */
        fm1_src_rect(fb, bg_now, 0, 0, 239, 239);
        fm1_halo_rect(0, 0, 239, 239);
        fm1_light_rect(0, 0, 239, 239);
        memcpy(prev, fb, sizeof prev);
    }
    for (y = 0; y < 720; y++) {                   /* laid onto the night: added */
        uint32_t *d = &bk_px[(BK_SY + y) * BK_W + BK_SX];
        const uint32_t *l = &fm1_light[y * 720];
        bk_draw_count(1024u);
        for (x = 0; x < 720; x++)
            d[x] = fm1_add(d[x], l[x] & 0xFFFFFFu);
    }
    for (y = 0; y < 732; y++)                    /* (kept for the frames to come) */
        memcpy(&box[y * 732], &bk_px[(BK_SY - 6 + y) * BK_W + BK_SX - 6], 732u * 4u);
    box_ok = 1, box_hash = hsh, box_col = fcol, box_ver = gd_bd_ver;
    gd_backdrop_life(tl);
    bk_fm1_lines();
    bk_fm1_points();
    bk_fm1_leaves();
    bk_fm1_words();
}

/* the FM-1 page's lines (gfx.c kept them), drawn smooth at three times the size. Lines that go on from one another
   (a curve drawn in little steps) are gathered and drawn as one smoothed curve; a lone straight one along a row or
   column is a plain bar (cheap), a lone slanted one anti-aliased */
#define BK_CHAIN 600
static float bk_ch_x[BK_CHAIN], bk_ch_y[BK_CHAIN];
static void bk_fm1_chain(int n, uint32_t col)
{
    float X = (float)BK_SX, Y = (float)BK_SY;
    if (n == 2) {
        float x0 = X + (bk_ch_x[0] + 0.5f) * 3.0f, y0 = Y + (bk_ch_y[0] + 0.5f) * 3.0f;
        float x1 = X + (bk_ch_x[1] + 0.5f) * 3.0f, y1 = Y + (bk_ch_y[1] + 0.5f) * 3.0f;
        if (bk_ch_x[0] == bk_ch_x[1] || bk_ch_y[0] == bk_ch_y[1]) {
            int ax = (int)fminf(x0, x1) - 1, ay = (int)fminf(y0, y1) - 1;
            bk_rect(ax, ay, (int)fabsf(x1 - x0) + 3, (int)fabsf(y1 - y0) + 3, col);
        } else
            bk_line(x0, y0, x1, y1, 3.2f, col);
        return;
    }
    {   /* a curve: each inner point eased towards its neighbours (twice), the ends kept */
        float px = X + (bk_ch_x[0] + 0.5f) * 3.0f, py = Y + (bk_ch_y[0] + 0.5f) * 3.0f;
        for (int k = 1; k < n; k++) {
            float sx = bk_ch_x[k], sy = bk_ch_y[k];
            if (k + 1 < n) {
                float ax = (bk_ch_x[k - 1] + 2.0f * sx + bk_ch_x[k + 1]) * 0.25f, ay = (bk_ch_y[k - 1] + 2.0f * sy + bk_ch_y[k + 1]) * 0.25f;
                if (k >= 2 && k + 2 < n) {
                    ax = (bk_ch_x[k - 2] + 4.0f * bk_ch_x[k - 1] + 6.0f * sx + 4.0f * bk_ch_x[k + 1] + bk_ch_x[k + 2]) / 16.0f;
                    ay = (bk_ch_y[k - 2] + 4.0f * bk_ch_y[k - 1] + 6.0f * sy + 4.0f * bk_ch_y[k + 1] + bk_ch_y[k + 2]) / 16.0f;
                }
                sx = ax, sy = ay;
            }
            {
                float nx = X + (sx + 0.5f) * 3.0f, ny = Y + (sy + 0.5f) * 3.0f;
                bk_line(px, py, nx, ny, 3.2f, col);
                px = nx, py = ny;
            }
        }
    }
}
/* the FM-1 page's points: those going on column by column (a curve plotted, one or two points a column) joined into a
   smoothed curve, the rest little round dots */
static void bk_fm1_points(void)
{
    int n = 0, thick = 0, s = 0;
    uint32_t ccol = 0;
    for (int i = 0; i <= fm1p_n; i++) {
        const fm1_point *q = i < fm1p_n ? &fm1p[i] : 0;
        uint32_t col = 0;
        int ok = 0;
        if (q) {
            uint32_t c = q->col;
            col = 0xFF000000u | ((c >> 11 & 31u) * 255u / 31u) << 16 | ((c >> 5 & 63u) * 255u / 63u) << 8 | (c & 31u) * 255u / 31u;
            ok = !q->hidden && q->y < 222 && q->y >= 0 && q->y < 240;
        }
        if (n && ok && col == ccol) {
            float lx = bk_ch_x[n - 1], ly = bk_ch_y[n - 1];
            if ((float)q->x == lx && fabsf((float)q->y - ly) <= 1.0f) {   /* a second point in its column: a thicker curve */
                thick++;
                continue;
            }
            if ((float)q->x == lx + 1.0f && fabsf((float)q->y - ly) <= 60.0f && n < BK_CHAIN) {
                bk_ch_x[n] = (float)q->x, bk_ch_y[n] = (float)q->y, n++;
                continue;
            }
        }
        if (n >= 4) {                            /* a curve */
            float th = thick * 2 >= n ? 5.0f : 3.2f, X = (float)BK_SX, Y = (float)BK_SY;
            if (th > 4.0f)
                for (int k = 0; k < n; k++)
                    bk_ch_y[k] += 0.5f;          /* (two points a column: the curve between them) */
            {
                float px = X + (bk_ch_x[0] + 0.5f) * 3.0f, py = Y + (bk_ch_y[0] + 0.5f) * 3.0f;
                for (int k = 1; k < n; k++) {
                    float sy = bk_ch_y[k];
                    if (k >= 2 && k + 2 < n)
                        sy = (bk_ch_y[k - 2] + 4.0f * bk_ch_y[k - 1] + 6.0f * sy + 4.0f * bk_ch_y[k + 1] + bk_ch_y[k + 2]) / 16.0f;
                    float nx = X + (bk_ch_x[k] + 0.5f) * 3.0f, ny = Y + (sy + 0.5f) * 3.0f;
                    bk_line(px, py, nx, ny, th, ccol);
                    px = nx, py = ny;
                }
            }
        } else
            for (int k = 0; k < n; k++)          /* single dots: as the FM-1's pixel, three times the size (cheap) */
                bk_rect(BK_SX + (int)bk_ch_x[k] * 3, BK_SY + (int)bk_ch_y[k] * 3, 3, 3, ccol);
        n = 0, thick = 0;
        if (ok)
            bk_ch_x[0] = (float)q->x, bk_ch_y[0] = (float)q->y, n = 1, ccol = col;
        s++;
    }
    (void)s;
}
static void bk_fm1_lines(void)
{
    int n = 0;
    uint32_t ccol = 0;
    for (int i = 0; i <= fm1n_n; i++) {
        const fm1_line *l = i < fm1n_n ? &fm1n[i] : 0;
        uint32_t col = 0;
        int ok = 0;
        if (l) {
            uint32_t c = l->col;
            col = 0xFF000000u | ((c >> 11 & 31u) * 255u / 31u) << 16 | ((c >> 5 & 63u) * 255u / 63u) << 8 | (c & 31u) * 255u / 31u;
            ok = !l->hidden && !(l->y0 >= 222 && l->y1 >= 222) && l->y0 >= -2 && l->y1 >= -2 && l->y0 <= 241 && l->y1 <= 241 &&
                 l->x0 >= -2 && l->x1 >= -2 && l->x0 <= 241 && l->x1 <= 241;
        }
        if (n && ok && col == ccol && n < BK_CHAIN && (float)l->x0 == bk_ch_x[n - 1] && (float)l->y0 == bk_ch_y[n - 1]) {
            bk_ch_x[n] = (float)l->x1, bk_ch_y[n] = (float)l->y1, n++;   /* it goes on */
            continue;
        }
        if (n >= 2)
            bk_fm1_chain(n, ccol);
        n = 0;
        if (ok) {
            bk_ch_x[0] = (float)l->x0, bk_ch_y[0] = (float)l->y0, bk_ch_x[1] = (float)l->x1, bk_ch_y[1] = (float)l->y1;
            n = 2, ccol = col;
        }
    }
}

/* the FM-1's glass leaves (a value with its slider, or a name) as the garden's own cards: a soft pod of night with
   a mossy floor, and the slider a vine that grows to its value, with leaves and a glowing bud at its end (it
   shivers and sparks when that dial turns). The value's place on the slider is read from the FM-1's pixels: its
   slider is drawn in MD_EDGE beyond the value. The words come after, sharp, from bk_fm1_words. */
static void bk_fm1_leaves(void)
{
    const uint16_t *fb = web_fb();
    float t = (float)bk_ms() / 1000.0f;
    for (int e = 0; e < fm1l_n; e++) {
        int lx = fm1l[e].x - 12, ly = fm1l[e].y, k = -1, start = 45, width, fill = -1, has_value = 0, j, f;
        uint32_t c = fm1l[e].col, acc, card;
        float X, Y, focus = 0, kick = 0, ar, ag, ab;
        for (f = 0; f < fm1l_n; f++)               /* its bottom edge: 31 rows down, 8 px to the left */
            if (fm1l[f].x == fm1l[e].x - 8 && fm1l[f].y == ly + 31)
                break;
        if (f == fm1l_n || ly < 0 || ly + 32 > 240 || lx < 0 || lx + 108 > 240)
            continue;
        for (j = 0; j < fm1t_n; j++) {             /* its words: "K1 NAME" at (x+6, y+2), the value at (x+6, y+16) */
            const fm1_word *w = &fm1t[j];
            if (w->x == lx + 6 && w->y == ly + 2 && w->s[0] == 'K' && w->s[1] >= '1' && w->s[1] <= '4')
                k = w->s[1] - '1';
            if (w->x == lx + 6 && w->y == ly + 16 && w->face == 4 && ((w->s[0] >= '0' && w->s[0] <= '9') || ((w->s[0] == '-' || w->s[0] == '+') && w->s[1] >= '0' && w->s[1] <= '9')))
                has_value = 1, start = w->w + 12;
        }
        start = start < 45 ? 45 : start > 78 ? 78 : start;
        width = 100 - start;
        if (has_value) {                           /* the value's place: where MD_EDGE begins along the bent slider */
            fill = width;
            for (j = 0; j <= width; j++) {
                int bend = j * (width - j) * 3 / (width * width / 4 + 1), px = lx + start + j, py = ly + 24 + bend;
                if (py >= 0 && py < 240 && px >= 0 && px < 240 && swap16(fb[py * 240 + px]) == MD_EDGE) {
                    fill = j < 4 ? 0 : j - 4;
                    break;
                }
            }
        }
        ar = (float)((c >> 11 & 31u) * 255u / 31u), ag = (float)((c >> 5 & 63u) * 255u / 63u), ab = (float)((c & 31u) * 255u / 31u);
        /* the edge is 14/32 the leaf's ink over MD_CARD (13, 24, 33): the ink taken back out of it */
        ar = fmaxf(0, (ar - 13.0f * 18 / 32) * 32 / 14), ag = fmaxf(0, (ag - 24.0f * 18 / 32) * 32 / 14), ab = fmaxf(0, (ab - 33.0f * 18 / 32) * 32 / 14);
        {   /* lifted to a clear colour of its own */
            float m = fmaxf(ar, fmaxf(ag, ab)), g = m > 1 ? 225.0f / m : 1;
            acc = ARGB(255, (int)fminf(255, ar * g), (int)fminf(255, ag * g), (int)fminf(255, ab * g));
        }
        if (k >= 0 && k == bk_focus) {
            uint32_t ago = bk_ms() - bk_focus_at[k];
            focus = 1;
            kick = ago < 700u ? 1.0f - (float)ago / 700.0f : 0;
        }
        X = (float)(BK_SX + lx * 3), Y = (float)(BK_SY + ly * 3);
        card = ARGB(255, 6 + (int)(ar * 0.05f), 11 + (int)(ag * 0.06f), 16 + (int)(ab * 0.06f));
        bk_rrect((int)X - 3, (int)Y - 3, 330, 102, 26, card);
        /* its floor: a mossy glow, and tufts along the bottom */
        bk_rrect((int)X + 6, (int)Y + 62, 312, 32, 16, BK_ALPHA(acc, 18 + (int)(18 * focus)));
        for (j = 0; j < 9; j++) {
            uint32_t h = (uint32_t)(lx * 31 + ly * 17 + j) * 2654435761u;
            float tx = X + 20 + (float)j * 36 + (float)(h >> 27), ty = Y + 96;
            float sw = sinf(t * 0.9f + (float)j) * 2.5f, ht = 6.0f + (float)(h >> 28 & 7u);
            bk_line(tx, ty, tx - 3.0f + sw, ty - ht, 1.5f, BK_ALPHA(acc, 70));   /* a tuft: three blades */
            bk_line(tx, ty, tx + sw, ty - ht - 4.0f, 1.5f, BK_ALPHA(acc, 90));
            bk_line(tx, ty, tx + 3.0f + sw, ty - ht + 1.0f, 1.5f, BK_ALPHA(acc, 70));
        }
        bk_rframe((int)X - 3, (int)Y - 3, 330, 102, 26, focus ? 2 : 1, BK_ALPHA(acc, focus ? 200 + (int)(55 * kick) : 80));
        if (fill >= 0) {                           /* the vine, grown to the value */
            float x0 = X + (float)start * 3.0f + 6, x1 = X + 300, vy = Y + 70, fr = width > 0 ? (float)fill / (float)width : 0;
            float xe = x0 + (x1 - x0) * fr, px = x0, py = vy, shake = kick * 3.0f;
            int n = 28;
            for (j = 1; j <= n; j++) {
                float u = (float)j / (float)n, x = x0 + (x1 - x0) * u;
                float y = vy + sinf(u * 9.0f + t * 1.2f) * 2.2f - sinf(u * 3.14159f) * 5.0f + (x <= xe ? sinf(t * 30.0f + u * 20.0f) * shake : 0);
                if (x <= xe)
                    bk_line(px, py, x, y, 4.2f - u * 1.4f, acc);
                else
                    bk_line(px, py, x, y, 1.6f, BK_ALPHA(acc, 70));
                if (x <= xe && j % 4 == 2) {      /* leaves along the grown part, alternating */
                    float sd = (j & 4) ? 1.0f : -1.0f, lw = 9.0f + 2.0f * sinf(t * 1.5f + (float)j);
                    for (int d = 1; d <= 4; d++) {   /* a leaf: thick in the middle */
                        float v = (float)d / 4.0f;
                        bk_disc(x + 6.0f * v, y - sd * lw * v, 1.0f + 2.4f * sinf(v * 3.14f), BK_ALPHA(acc, 190));
                    }
                }
                px = x, py = y;
            }
            {   /* the bud at the value: breathing, brighter when focused, bursting when turned */
                float by = vy + sinf(fr * 9.0f + t * 1.2f) * 2.2f - sinf(fr * 3.14159f) * 5.0f;
                float br = 6.0f + 1.2f * sinf(t * 2.4f + (float)e) + 4.0f * kick;
                bk_disc(xe, by, br + 9.0f, BK_ALPHA(acc, 30 + (int)(40 * focus)));
                bk_disc(xe, by, br, acc);
                bk_disc(xe - 1.5f, by - 1.5f, br * 0.45f, BK_ALPHA(BK_INK, 220));
                for (j = 0; j < 6 && kick > 0.05f; j++) {
                    float a = (float)j * 1.047f + (float)bk_turns * 0.6f, d = (1.0f - kick) * 30.0f + 8.0f;
                    bk_disc(xe + cosf(a) * d, by + sinf(a) * d * 0.7f, 2.2f * kick + 0.6f, BK_ALPHA(BK_INK, (int)(220 * kick)));
                }
            }
        } else {                                   /* a name: a little sprout to the right, swaying */
            float sx = X + 296, sy = Y + 84;
            bk_sprig(sx, sy, -1.0f, 30.0f, BK_ALPHA(acc, 150), t);
        }
    }
}

/* the FM-1 page's words (gfx.c kept them instead of drawing them into its pixels), set sharp at three times the
   size; "K1 " .. "K4 " before a name becomes a small dial: here they are the Brick's four dials */
static void bk_fm1_words(void)
{
    static const felucca_font_t *const parts[7][4] = {
        {&FONT_H3_T}, {&FONT_H3_XS}, {&FONT_H3_H}, {&FONT_H3_S}, {&FONT_H3_B0, &FONT_H3_B1},
        {&FONT_H3_M0, &FONT_H3_M1, &FONT_H3_M2}, {&FONT_H3_L0, &FONT_H3_L1, &FONT_H3_L2, &FONT_H3_L3}};
    for (int i = 0; i < fm1t_n; i++) {
        const fm1_word *w = &fm1t[i];
        const char *str = w->s;
        uint32_t c = w->col, col = 0xFF000000u | ((c >> 11 & 31u) * 255u / 31u) << 16 | ((c >> 5 & 63u) * 255u / 63u) << 8 | (c & 31u) * 255u / 31u;
        int x = BK_SX + w->x * 3, y = BK_SY + w->y * 3, h = parts[w->face][0]->h;
        if (w->hidden || w->y < -4 || w->y >= 222)   /* (its footer of buttons: the Brick's bar says it, large) */
            continue;
        if (str[0] == 'K' && str[1] >= '1' && str[1] <= '4' && (str[2] == ' ' || str[2] == ':')) {
            float r = (float)h * 0.26f, cx = (float)x + r + 2, cy = (float)y + (float)h * 0.52f, a = -0.8f + 0.55f * (float)(str[1] - '1');
            bk_arc(cx, cy, r, 2.5f, 0, 6.3f, col);
            bk_line(cx, cy, cx + sinf(a) * r * 0.8f, cy - cosf(a) * r * 0.8f, 2.5f, col);
            x += (int)(2.0f * r) + 10;
            str += str[2] == ':' ? 3 + (str[3] == ' ') : 3;
        }
        for (; *str; str++) {
            const felucca_font_t *f = parts[w->face][0];
            char one[2] = {*str, 0};
            for (int k = 0; k < 4 && parts[w->face][k]; k++)
                if ((uint8_t)*str >= parts[w->face][k]->first && (uint8_t)*str <= parts[w->face][k]->last)
                    f = parts[w->face][k];
            x = bk_text(x, y, f, 1, one, col);
        }
    }
}

static void bk_draw_screen(void)
{
    const uint16_t *fb = web_fb();
    int x, y;
    {   /* the black bezel: only the 6 px around the picture (the picture covers the rest) */
        int j;
        for (j = BK_SY - 6; j < BK_SY + 726; j++)
            if (j < BK_SY || j >= BK_SY + 720)
                bk_span(BK_SX - 6, BK_SX + 726, j, ARGB(255, 0, 0, 0));
            else {
                bk_span(BK_SX - 6, BK_SX, j, ARGB(255, 0, 0, 0));
                bk_span(BK_SX + 720, BK_SX + 726, j, ARGB(255, 0, 0, 0));
            }
    }
    for (y = 0; y < 240; y++)
        for (x = 0; x < 240; x++) {
            uint32_t c = swap16(fb[y * 240 + x]);
            uint32_t r = (c >> 11) & 31u, g = (c >> 5) & 63u, b = c & 31u;
            uint32_t p = 0xFF000000u | ((r * 255u / 31u) << 16) | ((g * 255u / 63u) << 8) | (b * 255u / 31u);
            uint32_t *o = &bk_px[(BK_SY + y * 3) * BK_W + BK_SX + x * 3];
            o[0] = o[1] = o[2] = p;
            o[BK_W] = o[BK_W + 1] = o[BK_W + 2] = p;
            o[2 * BK_W] = o[2 * BK_W + 1] = o[2 * BK_W + 2] = p;
        }
    bk_rframe(BK_SX - 6, BK_SY - 6, 732, 732, 12, 2, ARGB(255, 40, 48, 62));
}

static void bk_draw_plant(int i, int x, int y, int w, int h, float t, int muted, int selected)
{
    uint32_t col = muted ? BK_FAINT : bk_track_col(i);
    float lv = bk_level[i], grow = 0.3f + 0.7f * lv, bx = (float)x + 40, by = (float)(y + h - 8);
    float sway = bk_sway[i] * (muted ? 0.1f : 1.0f), top = (float)(h - 50) * grow;
    float px = bx, py = by;
    int s;
    (void)w;
    /* the stem, in segments, bending with the sway */
    for (s = 1; s <= 8; s++) {
        float f = (float)s / 8.0f, nx = bx + sway * f * f * 14.0f, ny = by - top * f;
        bk_line(px, py, nx, ny, 3.0f - f * 1.5f, BK_ALPHA(col, muted ? 120 : 220));
        if (s == 3 || s == 5 || s == 7) {    /* leaves, alternating */
            float side = (s & 2) ? 1.0f : -1.0f;
            float lx = nx + side * (9.0f + 6.0f * lv), ly = ny - 3.0f + sinf(t * 1.7f + (float)(s + i)) * 1.5f;
            bk_line(nx, ny, lx, ly, 3.5f, BK_ALPHA(col, muted ? 70 : 150));
        }
        px = nx;
        py = ny;
    }
    /* the bloom: opens on a hit, then closes */
    {
        float b = bk_bloom[i], r = 4.0f + 7.0f * b + 3.0f * lv;
        int k;
        for (k = 0; k < 5; k++) {
            float a = (float)k * 1.2566f + t * 0.4f + (float)i;
            bk_disc(px + sinf(a) * r * 0.8f, py - cosf(a) * r * 0.8f, r * 0.55f, BK_ALPHA(col, muted ? 60 : 110 + (uint32_t)(140 * b)));
        }
        bk_disc(px, py, r * 0.45f, muted ? BK_FAINT : BK_INK);
        if (b > 0.05f && !muted)
            bk_arc(px, py, r + 6.0f + 18.0f * (1.0f - b), 1.5f, 0, 6.3f, BK_ALPHA(col, (uint32_t)(200 * b)));
    }
    (void)selected;
}

static void gd_mini(int i, int x, int y, int h, float t, int muted);   /* brick_garden.inc */
static int bk_garden_shown(void);
static int bk_garden;

static void bk_draw_tracks(float t)
{
    const md_patch *p = md_state();
    int i, n = md_active_tracks();
    for (i = 0; i < 6; i++) {
        int x = 8, y = BK_SY + i * 117, w = BK_LEFT_W - 14, h = 111;
        const md_track *tr = &p->track[i];
        int muted = tr->mute != 0, sel = dream_ui.track == i, kit = p->kit.enabled && i == 3;
        uint32_t col = bk_track_col(i);
        char num[4] = {(char)('1' + i), 0};
        if (i >= n) {                            /* (its ground: baked, bk_ground) */
            bk_ground(x, y, w, h, 10, ARGB(14, 255, 255, 255));
            bk_text(x + 10, y + 8, &FONT_H, 1, num, BK_FAINT);
            continue;
        }
        bk_ground(x, y, w, h, 10, sel ? BK_ALPHA(col, 34) : ARGB(16, 255, 255, 255));
        {   /* the soil glows in its colour, with its sound - (1.5.3) smoothed, so it breathes instead of flickering */
            static float soil[6];
            soil[i] += (bk_level[i] - soil[i]) * bk_glide(5.0f);
            for (int g = 0; g < 26; g += 2)
                bk_rect(x + 6, y + h - 4 - g, w - 12, 2, BK_ALPHA(col, (uint32_t)((float)(26 - g) * (muted ? 0.4f : 1.2f + 3.0f * soil[i]))));
        }
        if (sel)
            bk_rframe(x, y, w, h, 10, 2, col);
        if (bk_garden)
            gd_mini(i, x, y, h, t, muted);   /* the track's own species, as in the garden */
        else
            bk_draw_plant(i, x, y, w, h, t, muted, sel);
        {   /* the number in a seed of its colour; the sound's name: its family small, its own word large */
            const char *nm = kit ? "KIT" : md_model_name(tr->model), *sp = strchr(nm, ' ');
            char fam[24];
            bk_disc((float)(x + 17), (float)(y + 17), 12, muted ? ARGB(255, 60, 66, 78) : col);
            bk_text_c(x + 17, y + 7, &FONT_H, 1, num, ARGB(255, 12, 16, 24));
            if (sp && sp - nm < (int)sizeof fam) {
                snprintf(fam, sizeof fam, "%.*s", (int)(sp - nm), nm);
                bk_text_fit(x + 34, y + 2, &FONT_T, fam, w - 40, muted ? BK_FAINT : BK_DIM);
                bk_text_fit(x + 34, y + 18, &FONT_H, sp + 1, w - 40, muted ? BK_FAINT : BK_INK);
            } else
                bk_text_fit(x + 34, y + 8, &FONT_H, nm, w - 40, muted ? BK_FAINT : BK_INK);
            if (kit)
                bk_text_fit(x + 34, y + 30, &FONT_T, md_kit_name(p->kit.bank), w - 40, BK_DIM);
        }
        if (muted) {
            bk_rrect(x + w - 66, y + h - 26, 46, 20, 10, ARGB(200, 70, 78, 92));
            bk_text_c(x + w - 43, y + h - 26, &FONT_T, 1, "MUTE", BK_INK);
        }
        /* level: a soft column at the right edge */
        {
            int lh = (int)((float)(h - 56) * bk_level[i]);
            bk_rrect(x + w - 12, y + 48, 7, h - 56, 3, ARGB(30, 255, 255, 255));
            if (lh > 0)
                bk_rrect(x + w - 12, y + 48 + (h - 56 - lh), 7, lh, 3, muted ? BK_FAINT : col);
        }
    }
}

static const char *const BK_NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

/* an arc from a0 to a1 (radians, 0 up, clockwise; any range, wrapped) */
static void bk_arc_span(float cx, float cy, float r, float th, float a0, float a1, uint32_t c)
{
    const float tau = 6.2831853f;
    while (a0 < 0)
        a0 += tau, a1 += tau;
    if (a1 - a0 < 0.01f)
        return;
    if (a1 <= tau)
        bk_arc(cx, cy, r, th, a0, a1, c);
    else {
        bk_arc(cx, cy, r, th, a0, tau - 0.0001f, c);
        bk_arc(cx, cy, r, th, 0, a1 - tau, c);
    }
}

static void bk_draw_right(float t)
{
    const md_patch *p = md_state();
    const md_stats *st = md_status();
    int x = BK_RIGHT_X + 8, w = BK_W - BK_RIGHT_X - 14, k, y;
    char buf[48];
    uint32_t now = bk_ms();
    int play = (bk_held >> BK_FN) & 1u;
    (void)t;
    if (lab_open && !play) {
        /* the Sound Lab's six modules at a glance: which sound, and their main amount */
        static const int main_p[M_COUNT] = {-1, P_SA_LEVEL, P_VO_LEVEL, P_PO_LEVEL, P_SY_ENGINE, P_RN_RATE, P_LG_RULE, P_GR_X, P_RA_RES_MIX, P_RB_RES_MIX, P_CL_MIX, P_PA_MIX, P_BE_MIX, P_SH_MIX, P_XM_MIX, -1, P_ENS_MIX, P_GAIN};
        for (k = 0; k < M_COUNT; k++) {
            int ky = BK_SY + k * 22 + (k >= M_SOURCES ? 6 : 0), on = k == M_SENDS ? 0 : lab_active(k);   /* (a gap between the rows) */
            bk_ground(x, ky, w, 20, 7, k == lab_mod ? BK_ALPHA(LAB_MOD_COL[k], 60) : ARGB(14, 255, 255, 255));
            bk_disc((float)(x + 12), (float)(ky + 10), 4, on ? LAB_MOD_COL[k] : BK_FAINT);
            bk_text(x + 22, ky + 3, &FONT_T, 1, LAB_MOD_NAMES[k], on || k == lab_mod ? BK_INK : BK_DIM);
            if (k == M_RACK_A || k == M_RACK_B || k == M_GRIDS) {   /* what goes into a rack; whether GRIDS plays */
                static const char *const sn[4] = {"VOICES", "SAMPLES", "SYNTH", "POLY"};
                int nin = 0, last = 0;
                for (int s2 = 0; s2 < 4 && k != M_GRIDS; s2++)
                    if (fx_routed(k == M_RACK_B, s2))
                        nin++, last = s2;
                snprintf(buf, sizeof buf, "%s", k == M_GRIDS ? (lab_v[P_GR_ON] > 0.5f ? "PLAYS" : "") : nin == 0 ? "" : nin == 1 ? sn[last] : "MIXED");
                if (bk_text_w(&FONT_T, buf, 1) <= w - 30 - bk_text_w(&FONT_T, LAB_MOD_NAMES[k], 1) - 8)
                    bk_text(x + w - 8 - bk_text_w(&FONT_T, buf, 1), ky + 3, &FONT_T, 1, buf, BK_DIM);
            } else if (main_p[k] >= 0 && (k != M_RANDOM || on)) {
                lab_format(main_p[k], buf, sizeof buf);
                if (bk_text_w(&FONT_T, buf, 1) > w - 30 - bk_text_w(&FONT_T, LAB_MOD_NAMES[k], 1) - 8)
                    buf[0] = 0;                  /* (no room: the name alone) */
                bk_text(x + w - 8 - bk_text_w(&FONT_T, buf, 1), ky + 3, &FONT_T, 1, buf, BK_DIM);
            }
        }
        y = BK_SY + M_COUNT * 22 + 12;
        if (lb_fading) {                         /* a Biome change: the Lab gliding over */
            snprintf(buf, sizeof buf, "CHANGING %d%%", (int)(lb_progress * 100));   /* (ran off the screen: shorter, fitted) */
            bk_text_fit(x + 4, y, &FONT_T, buf, w - 8, LAB_MOD_COL[M_MASTER]);
            bk_rrect(x + 4, y + 16, w - 8, 4, 2, ARGB(30, 255, 255, 255));
            bk_rrect(x + 4, y + 16, (int)((float)(w - 8) * lb_progress) + 1, 4, 2, LAB_MOD_COL[M_MASTER]);
        } else
            bk_text(x + 4, y, &FONT_T, 1, lb_own ? "OWN LAB HERE" : "SHARED LAB",
                    lb_own ? LAB_MOD_COL[M_MASTER] : BK_DIM);
        y += 22;
    } else if (!play) {
        /* the four dials (UP/DOWN chooses, LEFT/RIGHT turns): a knob each, its pointer turning with every turn;
           on the garden they are its weather, with their values */
        static float ang[4];
        static uint32_t seen_at[4];
        static const char *const wn[4] = {"FIREFLIES", "WIND", "GLOW", "MIST"};
        static const uint32_t wc[4] = {ARGB(255, 214, 255, 150), ARGB(255, 160, 220, 235), ARGB(255, 250, 170, 120), ARGB(255, 200, 210, 240)};
        int garden = bk_garden_shown();
        if (bk_view) {                           /* (a full view uses the D-pad itself: no dials, but what sounds now) */
            const char *bn = md_biome_name(p->biome_id), *sp2 = strrchr(bn, ' ');
            bk_ground(x, BK_SY, w, 248, 14, ARGB(16, 255, 255, 255));
            bk_text(x + 12, BK_SY + 8, &FONT_T, 1, "NOW SOUNDING", BK_DIM);
            {   /* its whole name: two large lines, or three smaller ones; shortened only when even that fails */
                char ln[3][64];
                bk_tr_keys = 1;                  /* (a Biome's name: its words never translated one by one) */
                int nl = bk_wrap(&FONT_B, bn, w - 20, ln, 2);
                (void)sp2;
                if (nl)
                    for (int q = 0; q < nl; q++)
                        bk_text(x + 12, BK_SY + 28 + q * 26, &FONT_B, 1, ln[q], bk_track_col(p->biome_id));
                else if ((nl = bk_wrap(&FONT_S, bn, w - 20, ln, 3)))
                    for (int q = 0; q < nl; q++)
                        bk_text(x + 12, BK_SY + 28 + q * 22, &FONT_S, 1, ln[q], bk_track_col(p->biome_id));
                else if ((nl = bk_wrap(&FONT_T, bn, w - 20, ln, 3)))
                    for (int q = 0; q < nl; q++)
                        bk_text(x + 12, BK_SY + 30 + q * 20, &FONT_T, 1, ln[q], bk_track_col(p->biome_id));
                else
                    bk_text_fit(x + 12, BK_SY + 28, &FONT_T, bn, w - 20, bk_track_col(p->biome_id));
                bk_tr_keys = 0;
            }
            for (k = 0; k < 6; k++) {             /* six stems, each as loud as its track, blooming on its hits */
                float sx = (float)(x + 16 + k * (w - 30) / 5), foot = (float)(BK_SY + 236), top = foot - 30.0f - 110.0f * bk_level[k];
                uint32_t c = p->track[k].mute ? BK_FAINT : bk_track_col(k);
                float sw = sinf(t * 1.1f + (float)k) * (1.5f + 4.0f * bk_level[k]) + bk_kick * 4.0f * sinf(t * 9.0f + (float)k);
                float mx = sx + sw * 0.4f, my = (foot + top) * 0.5f;
                bk_line(sx, foot, mx, my, 2.6f, BK_ALPHA(c, 200));          /* (a stem that bends, a leaf) */
                bk_line(mx, my, sx + sw, top, 2.2f, BK_ALPHA(c, 200));
                bk_line(mx, my, mx + ((k & 1) ? 8.0f : -8.0f), my - 6.0f + sinf(t * 1.7f + (float)k) * 1.5f, 3.0f, BK_ALPHA(c, 140));
                gd_flower(sx + sw, top, 5.0f + 5.0f * bk_bloom[k], 0.4f + 0.6f * bk_bloom[k], c, BK_INK);
            }
        } else
        for (k = 0; k < 4; k++) {
            int ky = BK_SY + k * 84, foc = bk_focus == k;
            uint32_t age = now - bk_focus_at[k], kc = garden ? wc[k] : bk_track_col(dream_ui.track);
            float fl = age < 500u ? 1.0f - (float)age / 500.0f : 0.0f, v = garden ? (float)p->macro[k] / 1000.0f : -1.0f;
            float cx = (float)(x + 36), cy = (float)(ky + 50), a, R = 26.0f;
            char nm[40], val[24];
            static float shown[4];
            if (bk_focus_at[k] != seen_at[k]) {   /* a turn: the pointer moves on */
                seen_at[k] = bk_focus_at[k];
                ang[k] += 0.35f * (float)bk_focus_dir[k];
            }
            a = v >= 0 ? -2.4f + v * 4.8f : ang[k];
            shown[k] += (a - shown[k]) * bk_glide(13.0f);  /* (it glides there) */
            a = shown[k];
            if (garden)
                snprintf(nm, sizeof nm, "%s", wn[k]), snprintf(val, sizeof val, "%d", p->macro[k] / 10);
            else if (!bk_fm1_dial(k, nm, sizeof nm, val, sizeof val)) {
                static const char *const dn[4] = {"FIRST DIAL", "SECOND DIAL", "THIRD DIAL", "FOURTH DIAL"};
                snprintf(nm, sizeof nm, "%s", dn[k]), val[0] = 0;
            }
            bk_ground(x, ky, w, 78, 16, foc ? BK_ALPHA(kc, 36) : ARGB(14, 255, 255, 255));
            if (foc) {
                float pulse = 0.5f + 0.5f * sinf(t * 2.4f);
                bk_rframe(x, ky, w, 78, 16, 2, BK_ALPHA(kc, 230));
                bk_disc(cx, cy, R + 12 + 3 * pulse, BK_ALPHA(kc, (uint32_t)(20 + 25 * pulse)));   /* a glow, breathing */
            }
            bk_text_fit(x + 10, ky + 3, &FONT_T, nm, w - 30, foc ? kc : BK_DIM);
            /* the knob: a mushroom's cap seen from below - its gills fanning out, a rim, a pale heart */
            bk_disc(cx + 1, cy + 3, R + 1, ARGB(130, 0, 0, 0));
            bk_disc(cx, cy, R, gd_mix(foc ? ARGB(255, 52, 46, 44) : ARGB(255, 38, 36, 38), kc, 0.12f));
            for (int g = 0; g < 22; g++) {
                float ga = (float)g * 0.2856f + a * 0.15f;
                bk_line(cx + sinf(ga) * 7.0f, cy - cosf(ga) * 7.0f, cx + sinf(ga) * (R - 2.0f), cy - cosf(ga) * (R - 2.0f), 1.0f,
                        BK_ALPHA(gd_mix(ARGB(255, 210, 190, 170), kc, 0.3f), 70));
            }
            bk_arc(cx, cy, R, 2.0f, 0, 6.3f, BK_ALPHA(gd_mix(ARGB(255, 200, 180, 160), kc, 0.4f), 120));
            if (v >= 0) {                         /* the value: a vine growing round the rim, leaves along it, a bud at its tip */
                float a0 = -2.4f, a1 = -2.4f + v * 4.8f;
                bk_arc_span(cx, cy, R + 5, 2.4f, a0, a1, BK_ALPHA(kc, foc ? 255 : 170));
                for (float la = a0 + 0.45f; la < a1 - 0.1f; la += 0.55f) {
                    float lx = cx + sinf(la) * (R + 5), ly = cy - cosf(la) * (R + 5), o = ((int)(la * 10.0f) & 1) ? 1.0f : -1.0f;
                    bk_disc(lx + sinf(la + 1.57f * o) * 4.0f, ly - cosf(la + 1.57f * o) * 4.0f, 2.6f, BK_ALPHA(kc, foc ? 230 : 140));
                }
                bk_disc(cx + sinf(a1) * (R + 5), cy - cosf(a1) * (R + 5), 4.5f, foc ? BK_INK : kc);
            }
            /* the pointer: a glowing spore at the rim, a vein to it from the heart */
            bk_line(cx, cy, cx + sinf(a) * (R - 6), cy - cosf(a) * (R - 6), 2.5f, BK_ALPHA(foc ? BK_INK : kc, 200));
            bk_disc(cx + sinf(a) * (R - 6), cy - cosf(a) * (R - 6), 4.0f, foc ? BK_INK : kc);
            bk_disc(cx, cy, 7.0f, gd_mix(ARGB(255, 236, 226, 206), kc, 0.25f));
            bk_disc(cx - 2, cy - 2, 2.5f, ARGB(140, 255, 255, 255));
            if (fl > 0)                           /* the turn: spores fly off the rim the way it went */
                for (int sp = 0; sp < 5; sp++) {
                    float sa = a + (float)bk_focus_dir[k] * (0.3f + (float)sp * 0.25f), d = R + 6.0f + (1.0f - fl) * 14.0f;
                    bk_disc(cx + sinf(sa) * d, cy - cosf(sa) * d, 2.0f * fl + 0.5f, BK_ALPHA(kc, (uint32_t)(230 * fl)));
                }
            if (val[0]) {                        /* its value: large, else smaller, else on two small lines (LP / 12 dB) */
                char ln[2][64];
                int nl;
                uint32_t vc = foc ? BK_INK : BK_DIM;
                if (bk_text_w(&FONT_M, val, 1) <= w - 74)
                    bk_text(x + 70, ky + 32, &FONT_M, 1, val, vc);
                else if (bk_text_w(&FONT_S, val, 1) <= w - 74)
                    bk_text(x + 70, ky + 38, &FONT_S, 1, val, vc);
                else if (bk_text_w(&FONT_T, val, 1) <= w - 74)
                    bk_text(x + 70, ky + 40, &FONT_T, 1, val, vc);
                else if ((nl = bk_wrap(&FONT_T, val, w - 74, ln, 2)))
                    for (int q = 0; q < nl; q++)
                        bk_text(x + 70, ky + 32 + q * 17, &FONT_T, 1, ln[q], vc);
                else
                    bk_text_fit(x + 70, ky + 40, &FONT_T, val, w - 74, vc);
            }
            if (foc) {                            /* LEFT / RIGHT turns it */
                float tx = (float)(x + w - 10), ty = (float)(ky + 12), pu = 2.0f * sinf(t * 5.0f);
                bk_line(tx - 3 - pu, ty, tx + 3 - pu, ty - 5, 2.2f, BK_ALPHA(kc, 200));
                bk_line(tx - 3 - pu, ty, tx + 3 - pu, ty + 5, 2.2f, BK_ALPHA(kc, 200));
            }
        }
        y = BK_SY + 4 * 84 + 4;
    } else {
        /* play mode: the eight scale tones as the Brick's buttons hold them */
        static const char *const keys[8] = {"LEFT", "DOWN", "RIGHT", "UP", "Y", "X", "B", "A"};
        bk_text(x + 4, BK_SY, &FONT_H, 1, lab_v[P_PO_LEVEL] > 0 && lab_v[P_PO_PLAYMODE] > 0.5f ? "PLAY > POLY" : "PLAY MODE",
                bk_track_col(dream_ui.track));
        for (k = 0; k < 8; k++) {
            int kx = x + (k % 2) * (w / 2), ky = BK_SY + 26 + (k / 2) * 62, lit = (bk_notes_lit >> k) & 1u;
            int note = bk_scale_note(k);
            uint32_t tc = bk_track_col(dream_ui.track);
            if (lit)                              /* a struck stone: a ring spreading */
                bk_rrect(kx - 3, ky - 3, w / 2, 62, 13, BK_ALPHA(tc, 70));
            bk_ground(kx, ky, w / 2 - 6, 56, 10, ARGB(18, 255, 255, 255));   /* (the stone's ground baked; lit, over it) */
            if (lit)
                bk_rrect(kx, ky, w / 2 - 6, 56, 10, BK_ALPHA(tc, 220));
            snprintf(buf, sizeof buf, "%s%d", BK_NOTE_NAMES[note % 12], note / 12 - 1);
            bk_text(kx + 8, ky + 2, &FONT_B, 1, buf, lit ? ARGB(255, 10, 12, 18) : BK_INK);
            bk_text(kx + 8, ky + 34, &FONT_T, 1, keys[k], lit ? ARGB(255, 10, 12, 18) : BK_DIM);
        }
        y = BK_SY + 26 + 4 * 62 + 6;
    }
    /* tempo and transport */
    bk_ground(x, y, w, 132, 10, ARGB(14, 255, 255, 255));
    snprintf(buf, sizeof buf, "%d", md_tempo_now());
    bk_text(x + 12, y + 10, &FONT_L, 1, buf, BK_INK);
    bk_text(x + 12 + bk_text_w(&FONT_L, buf, 1) + 6, y + 24, &FONT_T, 1, "BPM", BK_DIM);
    /* (0.75) the transport in words too, not only as a flower: PLAYING in green, or RESTING with what starts it */
    bk_text(x + 12, y + 50, &FONT_T, 1, md_playing() ? "PLAYING" : "RESTING", md_playing() ? ARGB(255, 130, 225, 150) : BK_FAINT);
    if (md_playing())                            /* playing: a flower, open; stopped: a bud */
        gd_flower((float)(x + w - 24), (float)(y + 26), 13, 0.8f + 0.2f * sinf(t * 3.0f), ARGB(255, 130, 225, 150), BK_INK);
    else
        gd_flower((float)(x + w - 24), (float)(y + 26), 11, 0, ARGB(255, 110, 120, 134), ARGB(255, 60, 66, 78));
    /* the bar's sixteen steps as a ring of seeds */
    for (k = 0; k < 16; k++) {
        float a = (float)k * 0.3927f, cx = (float)(x + w / 2), cy = (float)(y + 99), r = 25;   /* (0.76: below the PLAYING line, not over it) */
        int on = md_playing() && (int)(st->step % 16u) == k;
        if (on)
            gd_flower(cx + sinf(a) * r, cy - cosf(a) * r, 7, 1, bk_track_col(dream_ui.track), BK_INK);
        else
            bk_disc(cx + sinf(a) * r, cy - cosf(a) * r, k % 4 ? 2.2f : 3.2f, ARGB(k % 4 ? 70 : 120, 255, 255, 255));
    }
    y += 140;
    if (lab_open && !play) {                     /* (the Lab's list is long: tempo is enough below it) */
        snprintf(buf, sizeof buf, "OCT %+d   CPU %u%%", dream_ui.octave, web_cpu() > 99u ? 99u : (unsigned)web_cpu());   /* (three digits ran off the screen) */
        bk_text_fit(x + 4, y, &FONT_T, buf, BK_W - x - 10, web_cpu() > 99u ? ARGB(255, 230, 140, 120) : BK_FAINT);
        return;
    }
    /* key, scale, chord */
    bk_ground(x, y, w, 106, 10, ARGB(14, 255, 255, 255));
    bk_text(x + 12, y + 8, &FONT_T, 1, "KEY", BK_DIM);
    snprintf(buf, sizeof buf, "%s", BK_NOTE_NAMES[((p->root % 12) + 12) % 12]);
    bk_text(x + 12, y + 22, &FONT_B, 1, buf, BK_INK);
    {   /* the scale beside the key: one line, or two smaller ones (HARM / MINOR) */
        const char *sn = md_scale_name(p->scale);
        char ln[2][64];
        int nl;
        if (bk_text_w(&FONT_XS, sn, 1) <= w - 58)
            bk_text(x + 50, y + 25, &FONT_XS, 1, sn, BK_DIM);
        else if (bk_text_w(&FONT_T, sn, 1) <= w - 50)   /* (one word a little wide: a little more to the left) */
            bk_text(x + 44, y + 26, &FONT_T, 1, sn, BK_DIM);
        else if ((nl = bk_wrap(&FONT_T, sn, w - 58, ln, 2)))
            for (int q = 0; q < nl; q++)
                bk_text(x + 50, y + 18 + q * 17, &FONT_T, 1, ln[q], BK_DIM);
        else
            bk_text_fit(x + 50, y + 26, &FONT_T, sn, w - 58, BK_DIM);
    }
    bk_text(x + 12, y + 52, &FONT_T, 1, "CHORDS", BK_DIM);
    {   /* the chords: large, or two smaller lines */
        const char *cn = md_chord_name(p->chord_pattern);
        char ln[2][64];
        int nl;
        if (bk_text_w(&FONT_S, cn, 1) <= w - 24)
            bk_text(x + 12, y + 68, &FONT_S, 1, cn, BK_INK);
        else if ((nl = bk_wrap(&FONT_T, cn, w - 24, ln, 2)))
            for (int q = 0; q < nl; q++)
                bk_text(x + 12, y + 68 + q * 17, &FONT_T, 1, ln[q], BK_INK);
        else
            bk_text_fit(x + 12, y + 68, &FONT_T, cn, w - 24, BK_INK);
    }
    y += 114;
    /* states worth seeing at a glance */
    {
        int fx = x;
        if (p->freeze) {                         /* a snowflake */
            bk_rrect(fx, y, 74, 24, 12, ARGB(210, 170, 215, 245));
            for (int a = 0; a < 3; a++) {
                float an = (float)a * 1.047f;
                bk_line((float)(fx + 13) - 7.0f * cosf(an), (float)(y + 12) - 7.0f * sinf(an), (float)(fx + 13) + 7.0f * cosf(an), (float)(y + 12) + 7.0f * sinf(an), 1.6f, ARGB(255, 20, 40, 60));
            }
            bk_text(fx + 24, y + 5, &FONT_T, 1, "FREEZE", ARGB(255, 10, 22, 34));
            fx += 80;
        }
        if (p->locked) {                         /* a padlock */
            bk_rrect(fx, y, 64, 24, 12, ARGB(210, 236, 206, 140));
            bk_arc((float)(fx + 13), (float)(y + 10), 4.5f, 1.6f, 4.7f, 7.85f, ARGB(255, 40, 30, 14));
            bk_rrect(fx + 7, y + 10, 12, 9, 2, ARGB(255, 40, 30, 14));
            bk_text(fx + 24, y + 5, &FONT_T, 1, "LOCK", ARGB(255, 40, 30, 14));
        }
    }
    snprintf(buf, sizeof buf, "OCT %+d   CPU %u%%", dream_ui.octave, web_cpu() > 99u ? 99u : (unsigned)web_cpu());   /* (three digits ran off the screen) */
    bk_text_fit(x + 4, BK_SY + 676, &FONT_T, buf, BK_W - x - 10, web_cpu() > 99u ? ARGB(255, 230, 140, 120) : BK_FAINT);
    if (bk_sleep_left_min() > 0) {               /* (1.4) the sleep timer: a crescent and the minutes left */
        bk_disc((float)(x + 12), (float)(BK_SY + 660), 6, ARGB(220, 250, 236, 190));
        bk_disc((float)(x + 15), (float)(BK_SY + 657), 5, ARGB(255, 12, 16, 26));
        snprintf(buf, sizeof buf, "SLEEP %d MIN", bk_sleep_left_min());
        bk_text_fit(x + 24, BK_SY + 652, &FONT_T, buf, BK_W - x - 30, ARGB(255, 230, 220, 180));
    }
}

/* (1.4) resting after the sleep timer: the night alone, a slow moon, the words faint */
static void bk_draw_perf(void)                   /* (1.6) the sound's load in a corner: now and the highest of the last ten seconds */
{
    static uint32_t peak, peak_at;
    uint32_t now = bk_ms(), c = web_cpu();
    char buf[48];
    if (c >= peak || now - peak_at > 10000u)
        peak = c, peak_at = now;
    snprintf(buf, sizeof buf, "SOUND %u%%   PEAK %u%%", (unsigned)c, (unsigned)peak);
    bk_rrect(BK_W - 226, BK_H - 30, 220, 24, 10, ARGB(190, 0, 0, 0));
    bk_text(BK_W - 216, BK_H - 26, &FONT_T, 1, buf, c >= 85u ? ARGB(255, 255, 150, 120) : BK_INK);
}

static void bk_draw_resting(void)
{
    float t = (float)bk_ms() / 1000.0f, br = 0.5f + 0.5f * sinf(t * 0.5f);
    for (int y = 0; y < BK_H; y++)
        for (int i = 0; i < BK_W; i++)
            bk_px[y * BK_W + i] = ARGB(255, 3, 4, 9);
    for (int i = 0; i < 40; i++) {               /* a few stars, slowly twinkling */
        uint32_t h = (uint32_t)(i + 7) * 2654435761u;
        float tw = 0.5f + 0.5f * sinf(t * (0.3f + (float)(h >> 28) * 0.1f) + (float)i);
        bk_disc((float)(h % (uint32_t)BK_W), (float)((h >> 10) % 600u), 1.0f + (float)(h >> 30) * 0.4f, ARGB((uint32_t)(20 + 50 * tw), 220, 226, 240));
    }
    for (int k = 0; k < 4; k++)                  /* the moon's light, soft */
        bk_disc(BK_W / 2.0f, 300.0f, 44.0f + (float)k * 18.0f + 6.0f * br, ARGB((uint32_t)(7 - k), 250, 240, 210));
    bk_disc(BK_W / 2.0f, 300.0f, 36.0f, ARGB(200, 230, 226, 205));
    bk_disc(BK_W / 2.0f + 14.0f, 292.0f, 32.0f, ARGB(255, 3, 4, 9));
    bk_text_c(BK_W / 2, 420, &FONT_B, 1, bk_listen_rest ? "THE GARDEN LISTENS" : "THE GARDEN SLEEPS", ARGB(150, 200, 205, 215));
    bk_text_c(BK_W / 2, 460, &FONT_T, 1, bk_listen_rest ? "THE MUSIC PLAYS ON   ANY BUTTON WAKES IT" : "ANY BUTTON WAKES IT",
              ARGB(110, 160, 166, 180));
}

static void bk_draw_top(void)
{
    const md_patch *p = md_state();
    char buf[64];
    gd_seedling(18, 12, bk_track_col(dream_ui.track));
    bk_text(32, 4, &FONT_B, 1, "Mutable Dreams", BK_INK);
    bk_text(32 + bk_text_w(&FONT_B, "Mutable Dreams ", 1), 8, &FONT_T, 1, bk_title_for(), BK_FAINT);
    snprintf(buf, sizeof buf, "%s%s", md_biome_name(p->biome_id), lb_fading ? "  - LAB CHANGING" : lb_own ? "  - OWN LAB" : "");
    {   /* a vine along the top edge, leaves on it, swaying; the Biome's sign before its name */
        float t = (float)bk_ms() / 1000.0f, px = (float)BK_SX, py = 30.0f;
        uint32_t vc = BK_ALPHA(bk_track_col(p->biome_id), 90);
        for (int k = 1; k <= 36; k++) {
            float nx = (float)BK_SX + (float)k * 20.0f, ny = 29.0f + 2.0f * sinf((float)k * 0.9f + t * 0.6f);
            bk_line(px, py, nx, ny, 1.4f, vc);
            if (k % 3 == 0)
                bk_disc(nx, ny + ((k & 1) ? -3.0f : 3.0f), 2.6f, vc);
            px = nx, py = ny;
        }
        int nw = bk_text_w(&FONT_B, buf, 1);
        bk_biome_sigil(p->biome_id, (float)(BK_W / 2 - nw / 2 - 20), 16.0f, 11.0f, bk_track_col(p->biome_id), t);
    }
    bk_text_c(BK_W / 2, 5, &FONT_B, 1, buf, bk_track_col(p->biome_id));
    bk_title(lab_open ? "SOUND LAB" : bk_garden_shown() ? "GARDEN" : bk_page_name(), buf, sizeof buf);
    {   /* (0.73: "Modulation View" ran off the screen) the place's name, " View" left out, kept clear of the battery */
        size_t L = strlen(buf);
        if (L > 5 && !strcmp(buf + L - 5, " View"))
            buf[L - 5] = 0;
        bk_text_fit(BK_RIGHT_X + 8, 6, &FONT_S, buf, (bk_battery_pct >= 0 ? BK_W - 52 : BK_W - 8) - (BK_RIGHT_X + 8), BK_DIM);
    }
    if (bk_back_name()[0]) {                     /* where B leads: a pebble with B, the place's name (0.62) */
        const char *bn = bk_back_name();
        int bw = bk_text_w(&FONT_T, bn, 1), bx = BK_RIGHT_X - 14 - (bw > 150 ? 150 : bw);
        bk_rrect(bx - 26, 6, 20, 18, 9, ARGB(200, 232, 236, 222));
        bk_text_c(bx - 16, 7, &FONT_T, 1, bk_key_label("B"), ARGB(255, 14, 20, 28));   /* (Z on a keyboard) */
        bk_text_fit(bx, 8, &FONT_T, bn, 150, BK_FAINT);
    }
    if (bk_midi_ndev > 0 || bk_ms() - bk_midi_seen_at < 400u) {   /* MIDI: a device, and a light for what arrives */
        int mx = BK_W - 150, lit = bk_midi_count && bk_ms() - bk_midi_seen_at < 150u;
        bk_disc((float)mx, 14, 4, lit ? ARGB(255, 140, 230, 160) : BK_FAINT);
        bk_text_fit(mx + 8, 8, &FONT_T, bk_midi_ndev > 0 && bk_midi_name[0] ? bk_midi_name : "MIDI", 90, BK_DIM);
    }
    if (bk_battery_pct >= 0) {
        int bx = BK_W - 44;
        bk_rframe(bx, 7, 30, 14, 3, 1, BK_DIM);
        bk_rect(bx + 30, 11, 2, 6, BK_DIM);
        bk_rect(bx + 2, 9, 26 * bk_battery_pct / 100, 10, bk_battery_pct < 15 ? ARGB(255, 230, 110, 90) : BK_DIM);
    }
}

/* R2 held a moment: its layer of the buttons appears around the middle, each where it sits on the Brick, lit while
   pressed; what was hidden behind "R2 + ..." is there to see */
static void bk_draw_r2(float t)
{
    static uint32_t since;
    static const struct { const char *key, *what; int bit, bit2; float x, y; } R2[] = {
        {"L1/R1", "OCTAVE", BK_L1, BK_R1, 0.50f, 0.10f}, {"UP/DOWN", "SOUND MODEL", BK_UP, BK_DOWN, 0.16f, 0.30f},
        {"LEFT/RIGHT", "BIOME (A: TAKE)", BK_LEFT, BK_RIGHT, 0.16f, 0.44f}, {"F2", "BIOME ATLAS", BK_F2, -1, 0.16f, 0.58f},
        {"SELECT", "HOLD: SAVE", BK_SELECT, -1, 0.16f, 0.72f}, {"X", "ENVELOPE", BK_X, -1, 0.84f, 0.30f},
        {"Y", "GLOBAL", BK_Y, -1, 0.84f, 0.40f}, {"A", "LOCK", BK_A, -1, 0.84f, 0.50f}, {"B", "FREEZE", BK_B, -1, 0.84f, 0.60f},
        {"START", "RECORD", BK_START, -1, 0.84f, 0.72f}, {"MENU", "DREAM MAP", BK_MENU, -1, 0.50f, 0.86f}};
    uint32_t now = bk_ms(), col = bk_track_col(dream_ui.track);
    float a;
    int held = (bk_held >> BK_R2) & 1u;
    if (!held || lab_open || bk_view || bk_menu_open || bk_help_open || ((bk_held >> BK_FN) & 1u)) {
        since = 0;
        return;
    }
    if (!since)
        since = bk_stamp(now);
    if (now - since < 280u)
        return;
    a = fminf((float)(now - since - 280u) / 180.0f, 1.0f);
    bk_rrect(BK_SX + 4, BK_SY + 4, 712, 690, 22, BK_ALPHA(ARGB(255, 4, 7, 14), (uint32_t)(225 * a)));
    {
        float cx = (float)(BK_SX + 360), cy = (float)(BK_SY + 345), rr = 70.0f + 6.0f * sinf(t * 2.0f);
        bk_arc(cx, cy, rr, 3, 0, 6.3f, BK_ALPHA(col, (uint32_t)(200 * a)));
        bk_arc(cx, cy, rr + 14 + 10 * fmodf(t, 1.0f), 2, 0, 6.3f, BK_ALPHA(col, (uint32_t)(120 * a * (1 - fmodf(t, 1.0f)))));
        bk_text_c((int)cx, (int)cy - 36, &FONT_L, 1, "R2", BK_ALPHA(BK_INK, (uint32_t)(255 * a)));
        bk_text_c((int)cx, (int)cy + 12, &FONT_T, 1, "HELD: MORE", BK_ALPHA(BK_DIM, (uint32_t)(255 * a)));
        for (unsigned i = 0; i < sizeof R2 / sizeof R2[0]; i++) {
            if (bk_handheld && (R2[i].bit == BK_F2 || R2[i].bit == BK_MENU))
                continue;                        /* (no such key: the quick bar has the atlas and the dream map) */
            int on = ((bk_held >> R2[i].bit) & 1u) || (R2[i].bit2 >= 0 && ((bk_held >> R2[i].bit2) & 1u));
            float px = (float)BK_SX + R2[i].x * 720.0f, py = (float)BK_SY + R2[i].y * 690.0f;
            float in = fminf(fmaxf((a * 1.6f - (float)i * 0.05f), 0.0f), 1.0f);
            int kw = bk_text_w(&FONT_H, R2[i].key, 1) + 20, ww = bk_text_w(&FONT_S, R2[i].what, 1);
            int left = R2[i].x < 0.3f, mid = R2[i].x > 0.4f && R2[i].x < 0.6f;
            int bx = mid ? (int)px - (kw + 8 + ww) / 2 : left ? (int)px - 90 : (int)px + 90 - kw - 8 - ww;
            uint32_t pc = on ? col : ARGB(255, 232, 236, 222);
            bk_line(cx, cy, px, py, 1.2f, BK_ALPHA(col, (uint32_t)(50 * in)));
            if (on)
                bk_rrect(bx - 8, (int)py - 20, kw + ww + 24, 40, 20, BK_ALPHA(col, 80));
            bk_rrect(bx, (int)py - 15, kw, 30, 15, BK_ALPHA(pc, (uint32_t)(240 * in)));
            bk_text(bx + 10, (int)py - 13, &FONT_H, 1, R2[i].key, BK_ALPHA(ARGB(255, 12, 16, 24), (uint32_t)(255 * in)));
            bk_text(bx + kw + 8, (int)py - 13, &FONT_S, 1, R2[i].what, BK_ALPHA(on ? BK_INK : BK_DIM, (uint32_t)(255 * in)));
        }
    }
}

/* the air of the place: spores of light drifting up over everything, slower where it is calm, a breathing aura
   round the stage's middle; the Biome's colours */
static void bk_ambient(float t, float energy)
{
    const md_patch *p = md_state();
    uint32_t a1 = bk_track_col(p->biome_id % 6), a2 = bk_track_col((p->biome_id / 6) % 6);
    float br = 0.5f + 0.5f * sinf(t * 0.6f);
    for (int k = 0; k < 2; k++)                  /* the aura, breathing */
        bk_rframe_plain(BK_SX - 10 - k * 4, BK_SY - 10 - k * 4, 740 + k * 8, 740 + k * 8, 26 + k * 4, 2,
                        BK_ALPHA(gd_mix(a1, a2, br), (uint32_t)((10.0f + 14.0f * br + 30.0f * energy) / (float)(k + 1))));
    for (int i = 0; i < 46; i++) {               /* spores */
        uint32_t h = (uint32_t)(i + 1) * 2654435761u;
        float sp = 0.012f + (float)(h >> 27) * 0.003f + energy * 0.03f, life = fmodf(t * sp + (float)(h & 1023u) / 1023.0f, 1.0f);
        float x = (float)(h % (uint32_t)BK_W) + 22.0f * sinf(t * 0.5f + (float)i), y = (float)BK_H * (1.0f - life);
        float fade = sinf(life * 3.1416f), sz = 1.2f + (float)((h >> 8) % 3u) * 0.7f;
        uint32_t c = (i & 1) ? a1 : a2;
        if (x > BK_SX - 4 && x < BK_SX + 724 && y > BK_SY && y < BK_SY + 720 && !bk_garden_shown() && (lab_open || bk_view))
            fade *= 0.5f;                        /* (over the Lab's and the views' words: fainter) */
        bk_disc(x, y, sz * 3.0f, BK_ALPHA(c, (uint32_t)(22 * fade)));
        bk_disc(x, y, sz, BK_ALPHA(gd_mix(c, BK_INK, 0.5f), (uint32_t)(170 * fade)));
    }
}

/* a turn, large: what was turned and its value, in a pebble over the bottom of the stage, fading after a moment */
static void bk_draw_toast(void)
{
    static char nm[48], val[32];
    static uint32_t col, at;
    uint32_t now = bk_ms(), age;
    int play = (bk_held >> BK_FN) & 1u, k = bk_focus;
    float a;
    if (bk_menu_open || bk_help_open || play)
        return;
    if (lab_open && lab_turned_at && now - lab_turned_at < 1400u) {
        int p = lab_focused_param();
        snprintf(nm, sizeof nm, "%s", LAB_P[p].name);
        lab_format(p, val, sizeof val);
        col = LAB_MOD_COL[lab_mod], at = lab_turned_at;
    } else if (!lab_open && !bk_view && bk_focus_at[k] && now - bk_focus_at[k] < 1400u) {
        if (bk_garden_shown()) {
            static const char *const wn[4] = {"FIREFLIES", "WIND", "GLOW", "MIST"};
            snprintf(nm, sizeof nm, "%s", wn[k]);
            snprintf(val, sizeof val, "%d", md_state()->macro[k] / 10);
        } else
            return;                              /* (an FM-1 page shows its values large itself) */
        col = bk_track_col(dream_ui.track), at = bk_focus_at[k];
    } else if (!at || now - at >= 1400u)
        return;
    age = now - at;
    a = age < 900u ? 1.0f : 1.0f - (float)(age - 900u) / 500.0f;
    if (a <= 0)
        return;
    {
        int wn = bk_text_w(&FONT_B, nm, 1), wv = bk_text_w(&FONT_L, val, 1), w = wn + wv + 70, h = 64;
        int x = BK_SX + 360 - w / 2, y = BK_H - 128 + (int)(8.0f * (1.0f - a));
        bk_rrect(x, y, w, h, 32, BK_ALPHA(ARGB(255, 10, 14, 24), (uint32_t)(252 * a)));
        bk_rframe(x, y, w, h, 32, 2, BK_ALPHA(col, (uint32_t)(220 * a)));
        bk_text(x + 28, y + 18, &FONT_B, 1, nm, BK_ALPHA(col, (uint32_t)(255 * a)));
        bk_text(x + 42 + wn, y + 6, &FONT_L, 1, val, BK_ALPHA(BK_INK, (uint32_t)(255 * a)));
    }
}

static void ty_blob(float x, float y, float r, uint32_t c, float t, int seed, uint32_t alpha);   /* (brick_toys_draw.inc) */
static void bk_vines_at(float t, uint32_t col, float x0, float y0, float x1, float y1);   /* (brick_toys_draw.inc) */
/* the quick bar: the Brick's places as stones in a row along the bottom, the chosen one raised with its name */
/* (0.79) the menus' own little sparks: a puff of pollen where the choice lands, drawn and moved on the drawing side
   (its clock: bk_frame_s); nothing in calm motion */
#define BK_UI_SP 28
static struct { float x, y, vx, vy, life; uint32_t c; } bk_ui_sp[BK_UI_SP];
static int bk_ui_sp_w;
static void bk_ui_burst(float x, float y, uint32_t c, int n)
{
    static uint32_t seed = 12345u;
    if (bk_calm || bk_light)
        return;
    for (int i = 0; i < n; i++) {
        int k = bk_ui_sp_w++ % BK_UI_SP;
        float a, v;
        seed = seed * 1664525u + 1013904223u, a = (float)(seed >> 8) / 16777216.0f * 6.2832f;
        seed = seed * 1664525u + 1013904223u, v = 40.0f + (float)(seed >> 8) / 16777216.0f * 90.0f;
        bk_ui_sp[k].x = x, bk_ui_sp[k].y = y, bk_ui_sp[k].vx = cosf(a) * v, bk_ui_sp[k].vy = sinf(a) * v - 50.0f;
        bk_ui_sp[k].life = 1.0f, bk_ui_sp[k].c = (i & 1) ? c : ARGB(255, 255, 236, 180);
    }
}
static void bk_ui_sparks(void)
{
    float dt = fminf(bk_frame_s, 0.1f);
    for (int k = 0; k < BK_UI_SP; k++) {
        float l = bk_ui_sp[k].life;
        if (l <= 0)
            continue;
        bk_ui_sp[k].life = l - dt * 1.6f;
        bk_ui_sp[k].x += bk_ui_sp[k].vx * dt, bk_ui_sp[k].y += bk_ui_sp[k].vy * dt;
        bk_ui_sp[k].vy += 60.0f * dt, bk_ui_sp[k].vx *= 1.0f - 2.0f * dt;
        bk_disc(bk_ui_sp[k].x, bk_ui_sp[k].y, 2.0f + 3.5f * l, BK_ALPHA(bk_ui_sp[k].c, (uint32_t)(50.0f * l)));
        bk_disc(bk_ui_sp[k].x, bk_ui_sp[k].y, 1.0f + 1.2f * l, BK_ALPHA(bk_ui_sp[k].c, (uint32_t)(230.0f * l)));
    }
}

/* the QUICK BAR (L2): living stones on a swaying vine along the bottom; the chosen one rises, a leaf above it names
   the place and says in a line what it is (0.79), a ring and a puff of pollen where the choice lands */
static void bk_draw_quick(float t)
{
    static const int icon[BK_NQUICK] = {8, 1, 0, 2, 3, 4, 10, 9, 5, 6, 7, 15, 11, 18, 12, 13, 14, 16, 17};
    static const char *const nm[BK_NQUICK] = {"GARDEN", "MIXER", "SOUND LAB", "ATLAS", "SEQUENCE", "DREAM MAP", "WORLDS",
                                              "ARCHIVE", "MODULATION", "VOICE", "SPACE", "PLAYGROUND", "HELP", "OPTIONS", "MENU", "PLAY MODE",
                                              "LIGHT PICTURE", "CALM MOTION", "DEUTSCH / ENGLISH"};
    static const char *const about[BK_NQUICK] = {"Home: the living garden", "Levels, pan and sends", "Synths, modules, effects",
                                                 "All Biomes, by family", "Every track's steps", "Journeys through Biomes",
                                                 "Whole scenes to enter", "Free voices to fetch", "LFOs, links and macros",
                                                 "One track's whole sound", "Echo, room and grains", "Toys and little games",
                                                 "Every button, explained", "Buttons as you like them, repeat, language",
                                                 "All places, and the FM-1's", "Notes on the buttons; SELECT ends it",
                                                 "Less drawing, more speed", "Shapes held still",
                                                 "The garden's language"};
    static float shown = -1;
    static int last_sel = -1;
    static uint32_t moved_at;
    uint32_t col = bk_track_col(dream_ui.track);
    int i, cw = (BK_W - 24) / BK_NQUICK, by = BK_H - 62 - bk_quick_lift();
    float px = 0, py = 0;
    if (shown < 0)
        shown = (float)bk_quick_sel;
    shown += ((float)bk_quick_sel - shown) * bk_glide(13.0f);
    for (int yy = BK_H - 112 - bk_quick_lift(); yy < BK_H; yy++)   /* a band of night along the bottom: the stage above stays in sight */
        bk_rect(0, yy, BK_W, 1, ARGB((uint32_t)(150 + (yy - (BK_H - 112 - bk_quick_lift())) * 9 / 10 > 255 ? 255 : 150 + (yy - (BK_H - 112 - bk_quick_lift())) * 9 / 10), 3, 6, 12));
    for (int x = 4; x <= BK_W - 4; x += 12) {    /* the vine the stones sit on (row fills, cheap), with a leaf now and then */
        float y = (float)by + 22.0f + 4.0f * sinf((float)x * 0.021f + (bk_calm ? 0.0f : t * 0.8f));
        if (x > 4)
            bk_rect((int)px, (int)fminf(py, y) - 1, 13, (int)fabsf(y - py) + 3, ARGB(255, 50, 88, 60));
        if (x % 72 == 40 && !bk_light)
            bk_disc((float)x + 4.0f, y - 5.0f, 4.0f, ARGB(190, 88, 146, 94));
        px = (float)x, py = y;
    }
    {   /* (1.5) the two groups, places and settings (UP/DOWN between them): a stalk between them with a bud */
        int dx = 12 + cw * BK_QUICK_SETTINGS;
        bk_rect(dx - 1, by - 28, 3, 52, ARGB(160, 120, 170, 120));
        bk_disc((float)dx + 0.5f, (float)(by - 30), 4.0f, ARGB(200, 150, 210, 150));
    }
    bk_disc(12.0f + (float)cw * (shown + 0.5f), (float)by, 40, BK_ALPHA(col, 50));   /* the glow gliding to the chosen */
    if (bk_quick_sel != last_sel) {              /* the choice landed: a ring and a puff */
        if (last_sel >= 0)
            bk_ui_burst(12.0f + (float)cw * ((float)bk_quick_sel + 0.5f), (float)by - 10.0f, col, 8);
        last_sel = bk_quick_sel, moved_at = bk_ms();
    }
    for (i = 0; i < BK_NQUICK; i++) {
        int sel = i == bk_quick_sel;
        float bob = bk_calm ? 0.0f : 1.5f * sinf(t * 1.7f + (float)i * 0.8f);
        float cx = 12.0f + (float)cw * ((float)i + 0.5f), cy = (float)by + bob - (sel ? 8.0f + (bk_calm ? 0.0f : 3.0f * sinf(t * 4.0f)) : 0.0f);
        float rr = sel ? 30.0f : 23.0f;
        bk_disc(cx + 2, cy + 4, rr, ARGB(120, 0, 0, 0));
        ty_blob(cx, cy, rr * 1.08f, sel ? col : ARGB(255, 40, 56, 60), t, i, 255);   /* (0.72: living stones, mossy) */
        bk_icon(icon[i], cx, cy, rr * 0.85f, sel ? ARGB(255, 22, 28, 42) : bk_track_col(i % 6), t);
        if ((i == 16 && bk_light) || (i == 17 && bk_calm))   /* (a switch that is on: a small light on its stone) */
            bk_disc(cx + rr * 0.7f, cy - rr * 0.7f, 4.0f, ARGB(255, 255, 230, 150));
        if (sel) {
            float age = (float)(bk_ms() - moved_at) / 1000.0f;
            if (age < 0.6f && !bk_calm)
                bk_arc(cx, cy, rr + 4.0f + 40.0f * age, 2.0f, 0, 6.2832f, BK_ALPHA(col, (uint32_t)(200.0f * (1.0f - age / 0.6f))));
        }
    }
    {   /* the leaf above the chosen: its name and a line on what it is (kept on the screen at the row's ends) */
        char line[48];
        const char *ab = about[bk_quick_sel];
        float cx = 12.0f + (float)cw * (shown + 0.5f);
        if (bk_quick_sel == 16 || bk_quick_sel == 17)
            snprintf(line, sizeof line, "%s - %s", T((bk_quick_sel == 16 ? bk_light : bk_calm) ? "ON" : "OFF"), T(ab)), ab = line;
        else if (bk_quick_sel == 18)
            snprintf(line, sizeof line, "NOW: %s - A: %s", BK_LANG_NAME[bk_lang_idx], BK_LANG_NAME[(bk_lang_idx + 1) % BK_NLANG]), ab = line;
        int w1 = bk_text_w(&FONT_H, nm[bk_quick_sel], 1), w2 = bk_text_w(BK_TINY, ab, 1), w = (w1 > w2 ? w1 : w2) + 32;
        int lx = (int)cx - w / 2, ly = BK_H - 178;
        lx = lx < 6 ? 6 : lx + w > BK_W - 6 ? BK_W - 6 - w : lx;
        bk_rrect(lx + 3, ly + 5, w, 58, 18, ARGB(110, 0, 0, 0));
        bk_rrect(lx, ly, w, 58, 18, ARGB(240, 12, 20, 26));
        bk_rrect(lx, ly, w, 4, 2, BK_ALPHA(col, 200));
        bk_disc(cx, (float)(ly + 62), 6.0f, ARGB(240, 12, 20, 26));   /* (its stalk, down to the stone) */
        bk_disc(cx, (float)(ly + 72), 3.0f, ARGB(220, 12, 20, 26));
        bk_text(lx + 16, ly + 8, &FONT_H, 1, nm[bk_quick_sel], BK_INK);
        bk_text(lx + 16, ly + 36, BK_TINY, 1, ab, BK_DIM);
    }
    bk_ui_sparks();
    bk_text(10, BK_H - 108, &FONT_T, 1, "L2: WHERE TO   LEFT/RIGHT CHOOSE   UP/DOWN PLACES / SETTINGS   A GO   B CLOSE", BK_DIM);
}

static void bk_draw_bottom(void)
{
    bk_chips_nw = 0;
    if (bk_opt_open || bk_journal_on || (lab_mapview && lab_open)) {   /* (what lies over the place has its own few) */
        if (bk_opt_open && bk_opt_capture < 0)
            bk_chip_add(BK_LEFT, "<"), bk_chip_add(BK_RIGHT, ">");
        if (lab_mapview && lab_open && !bk_opt_open)
            bk_chip_add(BK_A, "GO");
        bk_chips_publish();
        return;
    }
    if (bk_quick_open) {
        bk_chip_add(BK_A, "GO");
        bk_chips_publish();
        bk_draw_quick((float)bk_ms() / 1000.0f);
        return;
    }
    const char *hint;
    int shift = (bk_held >> BK_R2) & 1u, play = (bk_held >> BK_FN) & 1u;
    if (bk_help_open || bk_guide_open) {
        bk_chip_add(BK_L1, "< PAGE"), bk_chip_add(BK_R1, "PAGE >");
        bk_chip_add(bk_help_open ? BK_X : BK_Y, bk_help_open ? "GUIDE" : "BUTTON MAP");
        if (bk_help_open)
            bk_chip_add(BK_Y, "OPTIONS");
        bk_chips_publish();
        return;                                  /* (the help says it itself, larger) */
    }
    if (bk_menu_open)
        hint = "MENU   UP/DOWN CHOOSE   LEFT/RIGHT OTHER SIDE   A GO   B CLOSE";
    else if (bk_view == V_TOYS && shift && !play)
        hint = "R2+   X KEEP: THE LAST BAR BECOMES THIS TRACK'S SEQUENCE   A UNDO KEEP";
    else if (bk_view == V_SEQ && sq_edit && shift && !play)
        hint = "R2+  L1 COPY   R1 PASTE   UP/DN TRANSPOSE 1   L/R OCTAVE   A MIRROR   Y STEP/PHRASE   X FILL";
    else if (bk_view && !lab_open && !shift && !play)
        hint = bk_view == V_MIXER ? "MIXER   LEFT/RIGHT TRACK   UP/DOWN VALUE   L1/R1 ROW   A MUTE   B OR F2 BACK   START PLAY"
             : bk_view == V_ATLAS ? (at_col == 0 ? "ATLAS   UP/DOWN FAMILY   RIGHT OR A TO ITS BIOMES   Y SURPRISE   B BACK" : "ATLAS   D-PAD BIOME   LEFT: FAMILIES   A LOAD (AGAIN: NOW)   Y SURPRISE   B BACK")
             : bk_view == V_DREAM ? (dm_fly ? "FREE FLIGHT   D-PAD FLY   A LAND ON THE ISLAND   STARS NUDGE A MACRO   Y JOURNEY   B BACK"
                                             : "DREAM MAP   L1/R1 JOURNEY   UP/DOWN SETTING   L/R VALUE   A DREAM / WAKE   Y FREE FLIGHT   B BACK")
             : bk_view == V_WORLDS ? "WORLDS   L1/R1 FAMILY   UP/DOWN WORLD   A ENTER   Y WANDER   L/R WANDER TIME   B BACK"
             : bk_view == V_TOYS ? (ty_toy == TY_BOUNCE ? "BOUNCE   A THROW   LEFT/RIGHT TILT   UP/DOWN GRAVITY   Y EMPTY   X TRACK   L1/R1 TOY   R2+X KEEP   B BACK"
                                  : ty_toy == TY_ORBITS ? "ORBITS   UP/DOWN PLANET   LEFT/RIGHT SPEED   A ON/OFF   Y NOTE   X TRACK   L1/R1 TOY   R2+X KEEP   B BACK"
                                  : ty_toy == TY_RAIN ? "RAIN   LEFT/RIGHT CLOUD   UP/DOWN RAIN   A THUNDER   Y IN TIME   X TRACK   L1/R1 TOY   R2+X KEEP   B BACK"
                                  : ty_toy == TY_SWING ? "SWING   A RELEASE   LEFT/RIGHT CYCLE   UP/DOWN FASTER/SLOWER   Y ONE WAY   X TRACK   L1/R1 TOY   R2+X KEEP   B BACK"
                                  : ty_toy == TY_FIREFLY ? "FIREFLY   D-PAD FLY   A FORGET THE MELODY   Y QUARTERS/EIGHTHS/16THS   X TRACK   L1/R1 TOY   R2+X KEEP   B BACK"
                                  : ty_toy == TY_WHEEL ? "WHEEL   UP/DOWN RING   LEFT/RIGHT HITS   Y STEPS   A TURN   L1/R1 TOY   R2+X KEEP   B BACK"
                                  : ty_toy == TY_ECHO ? "ECHO   A PLAY   D-PAD SING IT BACK   L1/R1 TOY   R2+X KEEP   B BACK"
                                  : ty_toy == TY_SEED ? "SEED   D-PAD FLY (UP: GLOW, RIGHT: WIND)   A GUST   Y DIALS BACK   L1/R1 TOY   B BACK"
                                  : ty_toy == TY_STARS ? "STARFALL   LEFT/RIGHT THE CUP   A A ROUND   Y GENTLE/STEADY/QUICK   X TRACK   L1/R1 TOY   R2+X KEEP   B BACK"
                                  : "JELLY   A PULSE   LEFT/RIGHT DRIFT   DEEP: MIST   RIGHT: FIREFLIES   Y DIALS BACK   L1/R1 TOY   B BACK")
             : bk_view == V_ARCHIVE && ar_smp ? "SAMPLER   UP/DOWN ROW   L/R TURN   L1/R1 SECTION   A SPEAK / SILENCE   Y GRAINS   B BACK"
             : bk_view == V_ARCHIVE ? "ARCHIVE   A FETCH / PLAY / SILENCE   X SAMPLER   Y INTO THE CLOUD   UP/DOWN POEM   L1/R1 BASKET   L/R PAGE   B BACK"
             : bk_view == V_SPACE ? "SPACE   UP/DOWN FIELD   LEFT/RIGHT VALUE   X OR L1/R1 PANEL   A FREEZE   B BACK   START PLAY"
             : bk_view == V_VOICE ? "VOICE   L1/R1 TRACK   UP/DOWN FIELD   LEFT/RIGHT VALUE   X NEXT PANEL   A HOLD: LISTEN   B BACK"
             : bk_view == V_MOD ? (mf_pond && !lk_mode ? "POND   L1/R1 FIREFLY   UP/DOWN HIGHER   LEFT/RIGHT SLOWER/FASTER   A DANCE   X TOUCHES   Y LFOS"
                                 : lk_mode == 2 ? "DRIFT   L1 CAPTURE A   R1 CAPTURE B   UP/DOWN FIELD   LEFT/RIGHT VALUE   X LFOS   B BACK"
                                 : lk_mode == 3 ? "MACROS   UP/DOWN MACRO   LEFT/RIGHT VALUE   X DRIFT   B BACK"
                                 : lk_mode ? "LINKS   UP/DOWN LINK   L1/R1 FIELD   LEFT/RIGHT VALUE   A ON/OFF   X MACROS   B BACK"
                                           : "MODULATION   UP/DOWN TRACK   L1/R1 FIELD   LEFT/RIGHT VALUE   X LINKS   Y POND   B BACK")
             : sq_edit ? "EDIT   LEFT/RIGHT STEP   UP/DOWN VALUE   L1/R1 FIELD   A STEP ON/OFF   Y GENERATE   X OR B DONE"
                                  : "SEQUENCE   UP/DOWN TRACK   A MUTE   X EDIT   B BACK   START PLAY";
    else if (lab_open && !shift && !play)
        hint = "SOUND LAB   L1/R1 MODULE   L1+R1 ALL MODULES   UP/DN PARAMETER   L/R VALUE (L2 FINE)   A NEUTRAL/BACK   X SILENT/BACK   Y NEXT SOUNDING   B OR F1 CLOSE   START PLAY";
    else if (play && bk_fn_soft)
        hint = shift ? "R2 + NOTE: CHORD   L1/R1 OCTAVE   START PLAY   SELECT END PLAY MODE"
                     : "SELECT END PLAY MODE   D-PAD + ABXY: SCALE TONES   R2: CHORD   L1/R1 TRACK";
    else if (play)
        hint = shift ? "R2 + NOTE: CHORD   L1/R1 OCTAVE   START PLAY   FN OFF: CONTROL"
                     : "D-PAD + ABXY: SCALE TONES   R2: CHORD   L1/R1 TRACK   FN OFF: CONTROL";
    else if (shift) {                            /* R2 held: all it does, in two rows, nothing in the stage covered */
        static const char *const rows[2] = {"R2+   UP/DN SOUND MODEL   L/R BIOME   A LOCK   B FREEZE   X ENVELOPE   Y GLOBAL",
                                            "R2+   L1/R1 OCTAVE   START RECORD   SELECT HOLD: SAVE   MENU DREAM MAP   F2 ATLAS"};
        for (int yy = BK_H - 76; yy < BK_H - 40; yy++)
            bk_rect(0, yy, BK_W, 1, ARGB((uint32_t)(120 + (yy - (BK_H - 76)) * 3), 3, 5, 12));
        bk_chips_hint(rows[0], 1);
        bk_chips_hint(rows[1], 1);
        bk_chips_publish();
        bk_draw_hint_at(rows[0], BK_INK, BK_H - 72);
        bk_draw_hint_at(rows[1], BK_INK, BK_H - 33);
        return;
    }
    else
        hint = bk_garden_shown() ? "GARDEN   UP/DOWN DIAL   L/R TURN   L1/R1 TRACK   START PLAY   F1 SOUND LAB   F2 MIXER   MENU WHERE TO   SELECT HOLD: HELP   A SELECT   B BACK   X EDIT   Y EFFECTS   R2 MORE"
                                 : "PAGE   UP/DOWN DIAL   L/R TURN   L1/R1 TRACK   A SELECT   B BACK   X EDIT   Y EFFECTS   START PLAY   F1 SOUND LAB   F2 MIXER   SELECT HOLD: HELP   MENU WHERE TO";
    bk_chips_hint(hint, shift);
    bk_chips_publish();
    bk_draw_hint(hint, shift ? BK_INK : bk_track_col(dream_ui.track));
}

/* a small drawn sign for each of the Brick's places (the menu's stepping stones) */
static void bk_icon(int id, float cx, float cy, float r, uint32_t c, float t)
{
    int k;
    switch (id) {
    case 0:                                      /* SOUND LAB: a flask, bubbling */
        bk_disc(cx, cy + r * 0.25f, r * 0.55f, c);
        bk_rrect((int)(cx - r * 0.18f), (int)(cy - r * 0.75f), (int)(r * 0.36f), (int)(r * 0.8f), 2, c);
        for (k = 0; k < 3; k++) {
            float u = fmodf(t * 0.8f + (float)k / 3.0f, 1.0f);
            bk_disc(cx + r * 0.2f * sinf((float)k * 2.0f + t), cy - r * 0.8f - u * r * 0.7f, 2.0f + (float)k * 0.5f, BK_ALPHA(c, (uint32_t)(220 * (1 - u))));
        }
        break;
    case 1:                                      /* MIXER: three stems at their levels */
        for (k = 0; k < 3; k++) {
            float sx = cx + ((float)k - 1.0f) * r * 0.5f, top = cy - r * (0.1f + 0.5f * (0.5f + 0.5f * sinf(t * 1.3f + (float)k * 2.1f)));
            bk_line(sx, cy + r * 0.7f, sx, cy - r * 0.7f, 1.5f, BK_ALPHA(c, 90));
            bk_disc(sx, top, r * 0.2f, c);
        }
        break;
    case 2:                                      /* ATLAS: a globe turning */
        bk_arc(cx, cy, r * 0.75f, 2.0f, 0, 6.3f, c);
        for (k = -1; k <= 1; k++)
            bk_line(cx - r * 0.7f, cy + (float)k * r * 0.35f, cx + r * 0.7f, cy + (float)k * r * 0.35f, 1.4f, BK_ALPHA(c, 150));
        {
            float m = sinf(t * 0.9f) * r * 0.5f;
            bk_line(cx + m, cy - r * 0.72f, cx + m * 0.7f, cy + r * 0.72f, 1.6f, c);
        }
        break;
    case 3:                                      /* SEQUENCE: a vine with steps */
        for (k = 0; k < 5; k++) {
            float sx = cx - r * 0.8f + (float)k * r * 0.4f, sy = cy + r * 0.2f * sinf((float)k + t * 2.0f);
            bk_disc(sx, sy, k == ((int)(t * 4.0f) % 5) ? r * 0.22f : r * 0.12f, c);
        }
        break;
    case 4:                                      /* DREAM MAP: a moon and a star */
        bk_disc(cx - r * 0.1f, cy, r * 0.6f, c);
        bk_disc(cx + r * 0.15f, cy - r * 0.15f, r * 0.52f, ARGB(255, 22, 28, 42));
        bk_disc(cx + r * 0.55f, cy - r * 0.45f, 2.0f + sinf(t * 3.0f), c);
        break;
    case 5:                                      /* MODULATION: a wave moving */
        {
            float px = 0, py = 0;
            for (k = 0; k <= 12; k++) {
                float u = (float)k / 12.0f, xx = cx - r * 0.8f + u * r * 1.6f, yy = cy - sinf(u * 6.28f + t * 3.0f) * r * 0.4f;
                if (k)
                    bk_line(px, py, xx, yy, 2.0f, c);
                px = xx, py = yy;
            }
        }
        break;
    case 6:                                      /* VOICE: a sound's spectrum */
        for (k = 0; k < 5; k++) {
            float hh = r * (0.3f + 0.6f * fabsf(sinf(t * 2.0f + (float)k * 1.3f))) / (1.0f + 0.3f * (float)k);
            bk_rrect((int)(cx - r * 0.75f + (float)k * r * 0.33f), (int)(cy + r * 0.5f - hh), (int)(r * 0.22f), (int)hh, 2, c);
        }
        break;
    case 7:                                      /* SPACE: a spiral of echoes */
        for (k = 0; k < 4; k++)
            bk_arc(cx, cy, r * (0.2f + 0.2f * (float)k), 1.8f, fmodf(t + (float)k, 6.28f) * 0.0f + (float)k * 0.8f, (float)k * 0.8f + 3.6f, BK_ALPHA(c, (uint32_t)(255 - k * 45)));
        break;
    case 8:                                      /* GARDEN: a flower */
        gd_flower(cx, cy, r * 0.6f, 0.8f + 0.2f * sinf(t * 2.0f), c, BK_INK);
        break;
    case 9:                                      /* ARCHIVE: an open book */
        bk_rrect((int)(cx - r * 0.75f), (int)(cy - r * 0.45f), (int)(r * 0.7f), (int)(r * 0.9f), 3, c);
        bk_rrect((int)(cx + r * 0.05f), (int)(cy - r * 0.45f), (int)(r * 0.7f), (int)(r * 0.9f), 3, c);
        for (k = 0; k < 3; k++)
            bk_line(cx - r * 0.6f, cy - r * 0.2f + (float)k * r * 0.22f, cx - r * 0.15f, cy - r * 0.2f + (float)k * r * 0.22f, 1.2f, ARGB(255, 22, 28, 42));
        break;
    case 10:                                     /* WORLDS: a planet with its ring */
        bk_disc(cx, cy, r * 0.42f, c);
        for (k = 0; k < 24; k++) {
            float a = (float)k / 24.0f * 6.28f + t * 0.6f;
            if (sinf(a) < 0 || k % 2)
                bk_disc(cx + cosf(a) * r * 0.8f, cy + sinf(a) * r * 0.25f, 1.6f, BK_ALPHA(c, 200));
        }
        break;
    case 11:                                     /* BUTTONS: a question */
        bk_text_c((int)cx, (int)(cy - r * 0.62f), &FONT_B, 1, "?", c);
        break;
    case 12:                                     /* MENU: three stepping stones */
        for (int k = 0; k < 3; k++)
            bk_disc(cx - r * 0.5f + (float)k * r * 0.5f, cy + (k == 1 ? -r * 0.25f : r * 0.2f), r * 0.2f, c);
        break;
    case 14:                                     /* LIGHT PICTURE: a feather, drifting (filled while it is on) */
        {
            float sw = 0.15f * sinf(t * 1.5f);
            bk_line(cx - r * 0.55f, cy + r * 0.6f, cx + r * 0.5f + sw * r, cy - r * 0.65f, 2.2f, c);
            for (k = 0; k < 5; k++) {
                float u = 0.15f + (float)k * 0.17f, px = cx - r * 0.55f + u * (r * 1.05f + sw * r), py = cy + r * 0.6f - u * r * 1.25f;
                bk_line(px, py, px - r * 0.38f, py - r * 0.12f, 1.6f, c);
                bk_line(px, py, px + r * 0.12f, py + r * 0.36f, 1.6f, c);
            }
            if (bk_light)
                bk_disc(cx + r * 0.45f, cy + r * 0.45f, r * 0.18f, c);
        }
        break;
    case 15:                                     /* PLAYGROUND: a ball bouncing on a lily pad */
        {
            float h = fabsf(sinf(t * 3.2f));
            bk_disc(cx, cy + r * 0.55f, r * 0.55f, BK_ALPHA(c, 150));
            bk_disc(cx, cy + r * 0.2f - h * r * 0.7f, r * 0.26f, c);
        }
        break;
    case 16:                                     /* CALM MOTION: a still wave, a dot when calm is on */
        for (k = 0; k < 4; k++)
            bk_line(cx - r * 0.7f + (float)k * r * 0.35f, cy + (bk_calm ? 0.0f : (k % 2 ? -r * 0.18f : r * 0.18f)),
                    cx - r * 0.35f + (float)k * r * 0.35f, cy + (bk_calm ? 0.0f : (k % 2 ? r * 0.18f : -r * 0.18f)), 2.2f, c);
        if (bk_calm)
            bk_disc(cx + r * 0.45f, cy + r * 0.45f, r * 0.18f, c);
        break;
    case 18:                                     /* OPTIONS: a cog of petals, turning slowly */
        for (k = 0; k < 8; k++) {
            float a = (float)k * 0.785f + (bk_calm ? 0.0f : t * 0.4f);
            bk_disc(cx + cosf(a) * r * 0.58f, cy + sinf(a) * r * 0.58f, r * 0.2f, c);
        }
        bk_disc(cx, cy, r * 0.5f, c);
        bk_disc(cx, cy, r * 0.22f, ARGB(255, 22, 28, 42));
        break;
    case 17:                                     /* DEUTSCH / ENGLISH: a speech bubble with the language's two letters */
        bk_disc(cx - r * 0.2f, cy - r * 0.05f, r * 0.62f, BK_ALPHA(c, 230));
        bk_disc(cx + r * 0.2f, cy - r * 0.05f, r * 0.62f, BK_ALPHA(c, 230));
        bk_disc(cx - r * 0.45f, cy + r * 0.55f, r * 0.16f, BK_ALPHA(c, 230));
        bk_text_c((int)cx, (int)(cy - r * 0.42f), &FONT_T, 1, bk_lang_idx == 1 ? "DE" : bk_lang_idx == 2 ? "FR" : bk_lang_idx == 3 ? "ES" : "EN", ARGB(255, 22, 28, 42));
        break;
    case 13:                                     /* PLAY MODE: three keys, one lit in turn */
        for (int k = 0; k < 3; k++)
            bk_rrect((int)(cx - r * 0.6f + (float)k * r * 0.42f), (int)(cy - r * 0.45f), (int)(r * 0.34f), (int)(r * 0.9f), 2,
                     k == (int)(t * 2.0f) % 3 ? c : BK_ALPHA(c, 120));
        break;
    default:                                     /* an FM-1 page: a leaf */
        bk_disc(cx, cy, r * 0.35f, c);
        bk_disc(cx + r * 0.25f, cy - r * 0.25f, r * 0.25f, c);
        break;
    }
}

/* MENU: the Brick's own places as stepping stones along a winding garden path, each with its sign and a line on
   what it is; the FM-1's pages beside them as leaves. UP/DOWN walks the path, LEFT/RIGHT crosses over, A goes */
static float bk_menu_hx[BK_MENU_SPLIT], bk_menu_hy[BK_MENU_SPLIT];   /* (1.6) where the stones were drawn: a finger's targets */
static void bk_draw_menu(void)
{
    static const char *const about[BK_MENU_SPLIT] = {
        "Synths, MI modules, effects, mastering", "Levels, pan and sends of the six tracks", "All Biomes, by family",
        "Every track's steps; X edits the notes", "Journeys through Biomes, for hours", "LFOs, living links, macros, drift",
        "One track's whole sound, with pictures", "Echo, room, grains, tape, ducking", "Home: the garden or the FM-1 screen",
        "Free voices to fetch over WiFi", "Whole scenes to enter, or wander", "Every button, explained",
        "Toys that play: balls, planets, rain"};
    static float fly_x, fly_y;
    static int last_sel = -1;
    int i, x = BK_SX, y = BK_SY;
    uint32_t col = bk_track_col(dream_ui.track);
    float t = (float)bk_ms() / 1000.0f, sx[BK_MENU_SPLIT], sy[BK_MENU_SPLIT];
    char name[48];
    bk_rrect(BK_SX - 2, BK_SY - 2, 724, 700, 18, ARGB(248, 4, 7, 14));
    if (!bk_light) {                             /* (0.79) moonlight over the path, a mossy bed under the FM-1's leaves */
        bk_disc((float)(x + 150), (float)(y + 340), 230.0f, ARGB(11, 120, 200, 170));
        bk_rrect(x + 468, y + 64, 232, 612, 16, ARGB(255, 9, 15, 20));
        bk_vines_at(t, col, (float)(x + 10), (float)(y + 4), (float)(x + 710), (float)(y + 688));
    }
    gd_seedling((float)(x + 26), (float)(y + 28), col);
    bk_text(x + 44, y + 12, &FONT_M, 1, "Where to?", BK_INK);
    for (i = 0; i < BK_MENU_SPLIT; i++) {        /* the path's stones, winding down */
        sx[i] = (float)(x + 62) + 34.0f * sinf((float)i * 1.1f) + 3.0f * sinf(t * 0.7f + (float)i);
        sy[i] = (float)(y + 88) + (float)i * 46.0f;   /* (13 stones since 0.65: a little closer) */
    }
    for (i = 0; i + 1 < BK_MENU_SPLIT; i++)      /* the path between them: a trail of pebbles; walked to the chosen, they glow */
        for (int k = 1; k < 5; k++) {
            float u = (float)k / 5.0f, px = sx[i] + (sx[i + 1] - sx[i]) * u, py = sy[i] + (sy[i + 1] - sy[i]) * u;
            if (i < bk_menu_sel && bk_menu_sel < BK_MENU_SPLIT) {
                float w = bk_calm ? 0.6f : 0.5f + 0.5f * sinf(t * 4.0f - (float)(i * 5 + k) * 0.7f);
                bk_disc(px, py, 4.0f + 2.0f * w, BK_ALPHA(col, (uint32_t)(40.0f * w)));
                bk_disc(px, py, 2.4f, ARGB((uint32_t)(140.0f + 100.0f * w), 255, 236, 180));
            } else
                bk_disc(px, py, k & 1 ? 2.4f : 1.8f, ARGB(80, 170, 200, 170));
        }
    for (i = 0; i < BK_NJUMPS; i++) {
        int sel = i == bk_menu_sel;
        const char *nm = BK_JUMPS[i].name;
        if (BK_JUMPS[i].page == -11)
            nm = bk_garden ? "HOME: GARDEN" : "HOME: FM-1 SCREEN";
        bk_title(nm, name, sizeof name);
        {   /* (" View" says nothing here) */
            size_t L = strlen(name);
            if (L > 5 && !strcmp(name + L - 5, " View"))
                name[L - 5] = 0;
        }
        if (i < BK_MENU_SPLIT) {                 /* a stone, its sign, its name; the chosen one lifted, with its line */
            bk_menu_hx[i] = sx[i], bk_menu_hy[i] = sy[i];
            uint32_t ic = sel ? ARGB(255, 22, 28, 42) : bk_track_col(i % 6);
            float r = sel ? 24.0f + 1.5f * sinf(t * 4.0f) : 19.0f, lift = sel ? -3.0f : 0.0f;
            if (sel) {
                float ring = fmodf(t * 0.9f, 1.0f);
                bk_arc(sx[i], sy[i] + lift, r + 6 + 18 * ring, 2, 0, 6.3f, BK_ALPHA(col, (uint32_t)(180 * (1 - ring))));
                bk_rrect((int)sx[i] + 32, (int)sy[i] - 26, 350, 52, 16, BK_ALPHA(col, 50));
            }
            bk_disc(sx[i] + 2, sy[i] + 4, r, ARGB(110, 0, 0, 0));
            ty_blob(sx[i], sy[i] + lift, r * 1.05f, sel ? col : ARGB(255, 34, 46, 54), t, i + 3, 255);   /* (0.79: living stones) */
            bk_icon(i == 12 ? 15 : i, sx[i], sy[i] + lift, r * 0.85f, ic, t);
            if (sel) {
                bk_text((int)sx[i] + 44, (int)sy[i] - 24, &FONT_B, 1, name, BK_INK);
                bk_text_fit((int)sx[i] + 44, (int)sy[i] + 2, &FONT_T, about[i], 330, BK_DIM);
            } else
                bk_text((int)sx[i] + 32, (int)sy[i] - 12, &FONT_S, 1, name, BK_DIM);
        } else {                                 /* the FM-1's pages: leaves in a column */
            int r2 = i - BK_MENU_SPLIT, rx = x + 486, ry = y + 70 + r2 * 40;
            if (i == BK_MENU_SPLIT)
                bk_text(rx, y + 44, &FONT_T, 1, "THE FM-1'S PAGES", BK_FAINT);
            {   /* (0.79) a name too long for its leaf: what is in brackets goes ("Space (Delay + Re..." no more) */
                char *br = strstr(name, " (");
                if (br && bk_text_w(&FONT_S, name, 1) > 192)
                    *br = 0;
            }
            if (sel)
                bk_rrect(rx - 6, ry - 4, 228, 36, 14, BK_ALPHA(col, 220));
            bk_icon(99, (float)(rx + 10), (float)(ry + 14 + (sel || bk_calm ? 0.0f : 1.5f * sinf(t * 1.6f + (float)r2))), 14,
                    sel ? ARGB(255, 10, 14, 22) : BK_ALPHA(col, 160), t);
            bk_text_fit(rx + 28, ry + 2, &FONT_S, name, 192, sel ? ARGB(255, 10, 14, 22) : BK_INK);
        }
    }
    {   /* a butterfly, flying to what is chosen */
        float tx = bk_menu_sel < BK_MENU_SPLIT ? sx[bk_menu_sel] + 20 : (float)(x + 476);
        float ty = bk_menu_sel < BK_MENU_SPLIT ? sy[bk_menu_sel] - 30 : (float)(y + 70 + (bk_menu_sel - BK_MENU_SPLIT) * 40);
        if (fly_x == 0)
            fly_x = tx, fly_y = ty;
        fly_x += (tx - fly_x) * bk_glide(6.7f), fly_y += (ty - fly_y) * bk_glide(6.7f);
        gd_butterfly(fly_x, fly_y + 36, t, col);
        if (bk_menu_sel != last_sel) {           /* (0.79) where the choice lands: a puff of pollen */
            if (last_sel >= 0)
                bk_ui_burst(bk_menu_sel < BK_MENU_SPLIT ? sx[bk_menu_sel] : (float)(x + 496), ty + 30.0f, col, 8);
            last_sel = bk_menu_sel;
        }
        bk_ui_sparks();
    }
}

/* ------------------------------------------------------- the Sound Lab --- */

static void bk_lab_scope(int x, int y, int w, int h, uint32_t col)
{
    float px = 0, py = 0;
    for (int i = 0; i < w; i += 2) {
        float v = lab_scope[(lab_scope_w + LAB_SCOPE - (uint32_t)w + (uint32_t)i) % LAB_SCOPE] * 2.5f;
        float yy = (float)y + (float)h / 2 - (v > 1 ? 1 : v < -1 ? -1 : v) * (float)h / 2;
        if (i)
            bk_line(px, py, (float)(x + i), yy, 1.6f, col);
        px = (float)(x + i);
        py = yy;
    }
}

/* a line of small text that may be too long for its place: then it breaks at a space and its first part goes one
   line up (never cut off) */
static int bk_viz_mid;                           /* (the middle of the Lab picture: above it, long lines go down) */
static void bk_text_up(int x, int y, const felucca_font_t *f, const char *s, int w, uint32_t c)
{
    char a[160];
    int up = y > bk_viz_mid;
    int n, cut;
    s = bk_tr(s);
    n = (int)strlen(s);
    if (bk_tw(f, s) <= w || n >= (int)sizeof a) {
        bk_text(x, y, f, 1, s, c);
        return;
    }
    for (cut = n - 1; cut > 0; cut--) {           /* the last space that leaves the rest fitting */
        if (s[cut] != ' ')
            continue;
        snprintf(a, sizeof a, "%.*s", cut, s);
        if (bk_tw(f, a) <= w && bk_tw(f, s + cut + 1) <= w)
            break;
    }
    if (cut <= 0) {
        bk_text(x, y, f, 1, s, c);
        return;
    }
    snprintf(a, sizeof a, "%.*s", cut, s);
    bk_text(x, up ? y - (int)f->h - 2 : y, f, 1, a, c);
    bk_text(x, up ? y : y + (int)f->h + 2, f, 1, s + cut + 1, c);
}

static void bk_lab_viz(int m, int x, int y, int w, int h, float t)
{
    uint32_t col = LAB_MOD_COL[m];
    int i;
    char buf[48];
    bk_ground(x, y, w, h, 14, ARGB(255, 10, 14, 24));   /* (1.3: baked while it stands still) */
    bk_viz_mid = y + h / 2;
    {   /* every module's picture lives a little, even at rest: seeds of its colour drifting up from a mossy floor,
           faster and brighter while it sounds (it is active), and a soft glow breathing at the floor */
        int act = lab_active(m);
        float br = 0.5f + 0.5f * sinf(t * 0.9f);
        bk_rrect(x + 10, y + h - 70, w - 20, 34, 16, BK_ALPHA(col, (uint32_t)(10 + 10 * br + (act ? 14 : 0))));
        for (i = 0; i < 16; i++) {
            uint32_t hh = (uint32_t)(m * 31 + i + 1) * 2654435761u;
            float sp = (act ? 0.06f : 0.025f) + 0.02f * (float)(hh >> 29), u = fmodf(t * sp + (float)(hh >> 8 & 1023u) / 1024.0f, 1.0f);
            float px = (float)x + 20.0f + (float)(hh >> 14 & 1023u) / 1024.0f * (float)(w - 40) + sinf(t * 0.5f + (float)i) * 8.0f;
            float py = (float)(y + h - 60) - u * (float)(h - 120);
            bk_disc(px, py, 1.4f + (float)(hh >> 30), BK_ALPHA(col, (uint32_t)((act ? 110.0f : 55.0f) * sinf(u * 3.14159f))));
        }
    }
    switch (m) {
    case M_TEX: {
        static const char *const nm[4] = {"FOREST", "AIR", "WATER", "STARDUST"};
        static const uint32_t cc[4] = {ARGB(255, 110, 200, 120), ARGB(255, 210, 225, 240), ARGB(255, 90, 160, 230), ARGB(255, 240, 210, 120)};
        for (i = 0; i < 4; i++) {                /* four little landscapes, each alive as much as it sounds */
            int qx = x + 10 + (i % 2) * (w / 2), qy = y + 14 + (i / 2) * 290, qw = w / 2 - 20, qh = 250;
            float lv = lab_v[P_FOREST + i] / 100, g = tex.glow[i] > lv ? tex.glow[i] : lv, gx = (float)(qx + qw / 2), base = (float)(qy + qh - 40);
            uint32_t c = lv > 0 ? cc[i] : BK_FAINT;
            bk_rrect(qx, qy, qw, qh, 16, BK_ALPHA(cc[i], (uint32_t)(10 + 30 * g)));
            if (i == 0)                           /* FOREST: firs, more and swaying with its level */
                for (int k = 0; k < 2 + (int)(lv * 5); k++) {
                    float fx = (float)(qx + 20 + (k * 37) % (qw - 40)), hh = 50.0f + (float)((k * 23) % 50), sw = sinf(t * 1.1f + (float)k) * 3.0f * g;
                    for (int l = 0; l < 5; l++) {
                        float ly = base - hh * (float)l / 5.0f, lw = (hh * 0.35f) * (1.0f - (float)l / 5.0f);
                        bk_line(fx - lw + sw * (float)l / 5.0f, ly, fx + sw * (float)(l + 1) / 5.0f, ly - hh / 5.0f - 6.0f, 3.0f, BK_ALPHA(c, 200));
                        bk_line(fx + lw + sw * (float)l / 5.0f, ly, fx + sw * (float)(l + 1) / 5.0f, ly - hh / 5.0f - 6.0f, 3.0f, BK_ALPHA(c, 200));
                    }
                }
            else if (i == 1)                      /* AIR: wind streaks drifting */
                for (int k = 0; k < 3 + (int)(lv * 10); k++) {
                    float u = fmodf(t * (0.15f + 0.4f * lv) + (float)k * 0.137f, 1.0f), wy = (float)(qy + 40 + (k * 29) % (qh - 90));
                    float wx = (float)qx + u * (float)qw, len = 30.0f + 30.0f * g;
                    for (int d = 0; d < 6; d++)
                        bk_line(wx + (float)d * len / 6.0f, wy + 4.0f * sinf((float)d + t * 2.0f), wx + (float)(d + 1) * len / 6.0f, wy + 4.0f * sinf((float)(d + 1) + t * 2.0f),
                                1.5f, BK_ALPHA(c, (uint32_t)(150.0f * sinf(u * 3.14f))));
                }
            else if (i == 2)                      /* WATER: waves, higher with its level */
                for (int l = 0; l < 4; l++)
                    for (int xx = 0; xx < qw - 20; xx += 4) {
                        float wy = base - (float)l * 26.0f + (3.0f + 9.0f * g) * sinf((float)xx * 0.06f + t * (1.0f + 0.3f * (float)l) + (float)l);
                        bk_disc((float)(qx + 10 + xx), wy, 3.0f, BK_ALPHA(c, (uint32_t)(60 + 40 * l)));
                    }
            else                                  /* STARDUST: stars falling, twinkling */
                for (int k = 0; k < 4 + (int)(lv * 18); k++) {
                    float u = fmodf(t * (0.08f + 0.05f * (float)(k % 3)) + (float)k * 0.071f, 1.0f);
                    float sx = (float)(qx + 14 + (k * 41) % (qw - 28)) + 10.0f * sinf(u * 6.0f), sy = (float)qy + 20.0f + u * (float)(qh - 70);
                    float tw = 0.5f + 0.5f * sinf(t * 5.0f + (float)k);
                    bk_disc(sx, sy, 1.5f + 2.0f * tw * g, BK_ALPHA(c, (uint32_t)(120 + 120 * tw)));
                }
            bk_text_c((int)gx, qy + qh - 30, &FONT_H, 1, nm[i], lv > 0 ? BK_INK : BK_DIM);
        }
        for (i = 0; i < TEX_SPARKS; i++)               /* the sparks as they ring */
            if (tex.sparks[i].amp > 0.001f)
                bk_disc((float)x + 20 + tex.sparks[i].pan * (float)(w - 40), (float)(y + h - 70) - 400.0f * tex.sparks[i].step * 20,
                        2 + 60 * tex.sparks[i].amp, ARGB(220, 250, 235, 170));
        if (lab_v[P_PULSE] > 0) {
            float ph = tex.pulse_phase;
            bk_rrect(x + 20, y + h - 30, (int)((float)(w - 40) * (1 - ph)), 6, 3, BK_ALPHA(col, 160));
        }
        break;
    }
    case M_CLOUD: {
        int frozen = lab_v[P_CL_FREEZE] > 0.5f;
        float gx0 = (float)(x + 20), gw = (float)(w - 40), gy0 = (float)(y + 40), gh = (float)(h - 140);
        bk_text_up(x + 20, y + 12, &FONT_T, "NOW", w - 40, BK_DIM);
        bk_text(x + w - 60, y + 12, &FONT_T, 1, "4 S AGO", BK_DIM);
        for (i = -2; i <= 2; i++) {                    /* octave lines */
            float yy = gy0 + gh / 2 - (float)i * gh / 4.4f;
            bk_line(gx0, yy, gx0 + gw, yy, 1, ARGB(i ? 30 : 60, 255, 255, 255));
        }
        {   /* the cloud itself, always there: puffs as large as GRAIN SIZE, as many as DENSITY, drifting from now
               into the past, brighter with what it sounds; SCATTER spreads them up and down */
            float sz = lab_v[P_CL_SIZE] / 1500.0f, dn = lab_v[P_CL_DENS] / 40.0f, sc = lab_v[P_CL_SCATTER] / 24.0f;
            float lv = fminf(cl_level * 8.0f, 1.0f), mix = lab_v[P_CL_MIX] / 100.0f;
            int np = 8 + (int)(dn * 28.0f);
            for (i = 0; i < np; i++) {
                uint32_t hh = (uint32_t)(i + 1) * 2654435761u;
                float u = fmodf(t * (frozen ? 0.0f : 0.03f + 0.02f * (float)(hh >> 29)) + (float)(hh >> 8 & 1023u) / 1024.0f, 1.0f);
                float px = gx0 + u * gw, py = gy0 + gh / 2 + ((float)(hh >> 18 & 255u) / 128.0f - 1.0f) * gh * (0.08f + 0.35f * sc)
                           + sinf(t * 0.4f + (float)i) * 6.0f;
                float r = 6.0f + 30.0f * sz * (0.6f + 0.4f * (float)(hh >> 26 & 7u) / 7.0f);
                uint32_t al = (uint32_t)((22.0f + 20.0f * mix + 34.0f * lv) * (1.0f - u * 0.6f));
                bk_disc(px, py, r, BK_ALPHA(col, al));
            }
        }
        for (i = 0; i < CL_GRAINS; i++) {
            const cl_grain *g = &cl_g[i];
            float age, oct, gx, gy, a;
            if (!g->on)
                continue;
            age = (float)((cl_w + CL_N - (uint32_t)g->pos) % CL_N) / (float)CL_N;
            oct = log2f(fabsf(g->rate));
            gx = gx0 + age * gw;
            gy = gy0 + gh / 2 - oct * gh / 4.4f;
            a = lab_win[(int)(g->t * WIN_N)];
            bk_disc(gx, gy, 3 + 12 * a, BK_ALPHA(g->rate < 0 ? ARGB(255, 240, 170, 150) : col, 60 + (uint32_t)(190 * a)));
        }
        if (frozen) {
            bk_rframe(x + 6, y + 6, w - 12, h - 12, 12, 3, ARGB(255, 150, 200, 240));
            bk_text(x + 20, y + h - 88, &FONT_B, 1, "FROZEN", ARGB(255, 150, 200, 240));
        }
        if (lab_auto_input(P_SC1, P_MIX_CL))
            bk_text_up(x + 20, y + h - 86, &FONT_T, "INPUT: WHOLE MIX (NO SENDS SET)", w - 40, BK_DIM);
        snprintf(buf, sizeof buf, "%d GRAINS / S   %d MS", (int)lab_v[P_CL_DENS], (int)lab_v[P_CL_SIZE]);
        bk_text(x + 20, y + h - 60, &FONT_S, 1, buf, BK_DIM);
        bk_lab_scope(x + 20, y + h - 36, w - 40, 26, BK_ALPHA(col, 160));
        break;
    }
    case M_SHIMMER: {
        float cx = (float)(x + w / 2), cy = (float)(y + h / 2 - 30), e = sh_level * 6;
        float dec = lab_v[P_SH_DECAY] / 100;
        e = e > 1 ? 1 : e;
        for (i = 0; i < 9; i++) {
            float ph = fmodf(t * (0.12f + 0.1f * (1 - dec)) + (float)i / 9.0f, 1.0f);
            float r = 20 + ph * (float)(w / 2 - 30);
            bk_arc(cx, cy, r, 2.0f + 3 * e, 0, 6.3f, BK_ALPHA(col, (uint32_t)((1 - ph) * (40 + 200 * e))));
        }
        for (i = 0; i < (int)(lab_v[P_SH_SHIMMER] / 6); i++) {   /* the octave sparkle */
            float a = (float)i * 0.618f + t * 0.05f, r = 40 + fmodf((float)i * 37.0f, (float)(w / 2 - 50));
            bk_disc(cx + lab_sin(a) * r, cy - lab_sin(a + 0.25f) * r, 1.5f + 2 * e, ARGB(200, 255, 255, 255));
        }
        bk_disc(cx, cy, 10 + 20 * e, BK_ALPHA(col, 200));
        if (lab_auto_input(P_SS1, P_MIX_SH))
            bk_text_up(x + 20, y + h - 86, &FONT_T, "INPUT: WHOLE MIX (NO SENDS SET)", w - 40, BK_DIM);
        snprintf(buf, sizeof buf, "DECAY %.1f S", 0.6f * powf(40.0f, dec));
        bk_text(x + 20, y + h - 60, &FONT_S, 1, buf, BK_DIM);
        bk_lab_scope(x + 20, y + h - 36, w - 40, 26, BK_ALPHA(col, 160));
        break;
    }
    case M_SENDS: {
        int rh = (h - 90) / 10;
        bk_text(x + 84, y + 14, &FONT_T, 1, "CLOUD", LAB_MOD_COL[M_CLOUD]);
        bk_text(x + 146, y + 14, &FONT_T, 1, "PARTIC.", LAB_MOD_COL[M_PARTICLES]);
        bk_text(x + 208, y + 14, &FONT_T, 1, "SEEDS", LAB_MOD_COL[M_SEEDS]);
        bk_text(x + 270, y + 14, &FONT_T, 1, "SHIMMER", LAB_MOD_COL[M_SHIMMER]);
        for (int src = 0; src < 4; src++) {   /* SAMPLES, POLY, VOICES and SYNTH: the seventh to tenth rows */
            int ry = y + 40 + (6 + src) * rh, first = src == 3 ? P_SY_TO_CL : src == 2 ? P_VO_TO_CL : src ? P_PO_TO_CL : P_SA_TO_CL;
            int mod = src == 3 ? M_SYNTH : src == 2 ? M_VOICES : src ? M_POLY : M_SAMPLES;
            float lv = (src == 3 ? sy_level : src == 2 ? vo_level : src ? po_level : sa_level) * 6;
            bk_text(x + 16, ry + 6, &FONT_B, 1, src == 3 ? "Y" : src == 2 ? "V" : src ? "P" : "S", LAB_MOD_COL[mod]);
            bk_disc((float)(x + 60), (float)(ry + 18), 4 + 10 * (lv > 1 ? 1 : lv), BK_ALPHA(LAB_MOD_COL[mod], 200));
            for (int c = 0; c < 4; c++) {
                static const int mods[4] = {M_CLOUD, M_PARTICLES, M_SEEDS, M_SHIMMER};
                float v = lab_v[first + c] / 100;
                int bx = x + 82 + c * 62, bw = 54;
                bk_rrect(bx, ry + 8, bw, 20, 6, ARGB(30, 255, 255, 255));
                if (v > 0)
                    bk_rrect(bx, ry + 8, (int)((float)bw * v), 20, 6, BK_ALPHA(LAB_MOD_COL[mods[c]], 220));
            }
        }
        for (i = 0; i < 6; i++) {
            int ry = y + 40 + i * rh;
            uint32_t tc = bk_track_col(i);
            float lv = lab_stem_level[i] * 3;
            char num[3] = {(char)('1' + i), 0, 0};
            bk_text(x + 16, ry + 6, &FONT_B, 1, num, tc);
            bk_disc((float)(x + 60), (float)(ry + 18), 4 + 10 * (lv > 1 ? 1 : lv), BK_ALPHA(tc, 200));
            for (int c = 0; c < 4; c++) {
                static const int first[4] = {P_SC1, P_SP1, P_SB1, P_SS1}, mods[4] = {M_CLOUD, M_PARTICLES, M_SEEDS, M_SHIMMER};
                float v = lab_v[first[c] + i] / 100;
                int bx = x + 82 + c * 62, bw = 54;
                bk_rrect(bx, ry + 8, bw, 20, 6, ARGB(30, 255, 255, 255));
                if (v > 0)
                    bk_rrect(bx, ry + 8, (int)((float)bw * v), 20, 6, BK_ALPHA(LAB_MOD_COL[mods[c]], 220));
            }
        }
        bk_lab_scope(x + 20, y + h - 36, w - 40, 26, BK_ALPHA(col, 160));
        break;
    }
    case M_PARTICLES: {
        float e = pa_level * 8, pos = lab_v[P_PA_POS] / 100, size = lab_v[P_PA_SIZE] / 100;
        int mode = (int)lab_v[P_PA_MODE] & 3, frozen = lab_v[P_PA_FREEZE] > 0.5f;
        float bx = (float)(x + 20), bw = (float)(w - 40), by = (float)(y + 150);
        e = e > 1 ? 1 : e;
        bk_text(x + 20, y + 16, &FONT_B, 1, LAB_PA_MODES[mode], col);
        bk_text_up(x + 20, y + 44, &FONT_T, "BUFFER (NEW AT LEFT)", w - 40, BK_DIM);
        bk_rrect((int)bx, (int)by, (int)bw, 60, 10, ARGB(24, 255, 255, 255));
        for (i = 0; i < (int)bw; i += 3) {           /* the recent sound, as the buffer holds it */
            float v = fabsf(lab_scope[(lab_scope_w + LAB_SCOPE - 1 - (uint32_t)(i * LAB_SCOPE / (int)bw)) % LAB_SCOPE]) * 60;
            v = v > 26 ? 26 : v;
            bk_rect((int)bx + i, (int)(by + 30 - v), 2, (int)(2 * v) + 1, BK_ALPHA(col, frozen ? 120 : 70));
        }
        bk_rrect((int)(bx + pos * bw * 0.9f), (int)by - 6, (int)(bw * (0.04f + 0.25f * size)), 72, 8, BK_ALPHA(col, 110));
        for (i = 0; i < 24; i++) {                   /* particles rising with the output */
            float a = (float)i * 0.618f, ph = fmodf(t * (0.25f + 0.02f * (float)i) + (float)i * 0.13f, 1.0f);
            float px = bx + fmodf(a * 97.0f, bw), py = (float)(y + h - 120) - ph * (float)(h - 340);
            bk_disc(px + lab_sin(t * 0.3f + a) * 10, py, 1.5f + 5 * e * (1 - ph), BK_ALPHA(col, (uint32_t)((1 - ph) * (60 + 180 * e))));
        }
        if (frozen)
            bk_rframe(x + 6, y + 6, w - 12, h - 12, 12, 3, col);
        snprintf(buf, sizeof buf, "PITCH %+d   %s", (int)lab_v[P_PA_PITCH], frozen ? "FROZEN" : "");
        bk_text(x + 20, y + h - 86, &FONT_S, 1, buf, BK_DIM);
        bk_text_up(x + 20, y + h - 60, &FONT_T, "MUTABLE INSTRUMENTS CLOUDS", w - 40, BK_FAINT);
        bk_lab_scope(x + 20, y + h - 36, w - 40, 26, BK_ALPHA(col, 160));
        break;
    }
    case M_POLY: {
        float px = (float)(x + 20), pw = (float)(w - 40), py = (float)(y + 330), ph = 200;
        int nv = (int)lab_v[P_PO_VOICES];
        lab_format(P_PO_ENGINE, buf, sizeof buf);
        bk_text(x + 20, y + 16, &FONT_B, 1, buf, col);
        {
            char mi[24];
            lab_format(P_PO_MIDI, mi, sizeof mi);
            snprintf(buf, sizeof buf, "%d VOICES  %s  MIDI: %s", nv, lab_v[P_PO_PLAYMODE] > 0.5f ? "PLAY MODE" : "", mi);
            bk_text_up(x + 20, y + 46, &FONT_T, buf, w - 40, BK_DIM);
        }
        bk_rrect(x + 14, y + 236, w - 28, 16, 8, ARGB(60, 30, 44, 36));   /* a bed of soil */
        for (i = 0; i < PO_VOICES; i++) {            /* the voices: a stem each, growing with its envelope, a bloom on the gate */
            float vx = (float)(x + 20 + i * (w - 40) / PO_VOICES + (w - 40) / PO_VOICES / 2), gy = (float)(y + 242);
            float e = po_v[i].on ? po_v[i].env : 0.0f, ht = 24.0f + 140.0f * e, sw = sinf(t * 1.3f + (float)i) * (3.0f + 6.0f * e);
            float qx = vx, qy = gy;
            if (i >= nv) {                           /* (a voice not in use: a sleeping seed) */
                bk_disc(vx, gy - 2, 4, ARGB(60, 255, 255, 255));
                continue;
            }
            for (int k = 1; k <= 7; k++) {
                float u = (float)k / 7.0f, nx = vx + sw * u * u, ny = gy - ht * u;
                bk_line(qx, qy, nx, ny, 3.4f - u * 1.4f, BK_ALPHA(col, e > 0.001f ? 230 : 90));
                if (k == 3 || k == 5)
                    bk_line(nx, ny, nx + (k == 3 ? 10.0f : -10.0f), ny - 6.0f, 3.0f, BK_ALPHA(col, e > 0.001f ? 160 : 60));
                qx = nx, qy = ny;
            }
            if (e > 0.001f) {                        /* the bloom: open while the key is held, closing as it fades */
                float r = 5.0f + 9.0f * e * (po_v[i].gate ? 1.0f : 0.6f);
                for (int k = 0; k < 5; k++) {
                    float a = (float)k * 1.2566f + t * 0.5f;
                    bk_disc(qx + cosf(a) * r * 0.7f, qy + sinf(a) * r * 0.7f, r * 0.55f, BK_ALPHA(col, po_v[i].gate ? 200 : 120));
                }
                bk_disc(qx, qy, r * 0.4f, BK_ALPHA(BK_INK, 220));
            } else
                bk_disc(qx, qy, 4, BK_ALPHA(col, 120));   /* (a closed bud) */
            if (po_v[i].on) {
                int nt = po_v[i].note + 12 * (int)lab_v[P_PO_OCTAVE];
                snprintf(buf, sizeof buf, "%s%d", BK_NOTE_NAMES[((nt % 12) + 12) % 12], nt / 12 - 1);
                bk_text_c((int)vx, y + 258, &FONT_T, 1, buf, BK_INK);
            }
        }
        bk_rrect((int)px, (int)py, (int)pw, (int)ph, 40, ARGB(60, 10, 40, 48));   /* timbre x morph: a pond, harmonics its rings */
        bk_text((int)px + 18, (int)py + 10, &FONT_T, 1, "TIMBRE >", BK_FAINT);
        bk_text((int)px + 18, (int)(py + ph) - 26, &FONT_T, 1, "MORPH ^", BK_FAINT);
        {
            float dx = px + 20 + (pw - 40) * lab_v[P_PO_TIMBRE] / 100, dy = py + 20 + (ph - 40) * (1 - lab_v[P_PO_MORPH] / 100);
            float e = po_level * 10;
            e = e > 1 ? 1 : e;
            for (int k = 0; k < 3; k++) {            /* ripples, faster with the sound */
                float ph2 = fmodf(t * (0.4f + e) + (float)k / 3.0f, 1.0f), rr = 8.0f + (20.0f + 40.0f * lab_v[P_PO_HARM] / 100) * ph2;
                bk_arc(dx, dy, rr, 2, 0, 6.3f, BK_ALPHA(col, (uint32_t)(150 * (1.0f - ph2))));
            }
            bk_disc(dx, dy, 7 + 9 * e, col);         /* a lily pad's flower */
            bk_disc(dx - 2, dy - 2, 3, BK_ALPHA(BK_INK, 200));
        }
        bk_text_up(x + 20, y + h - 60, &FONT_T, "MUTABLE INSTRUMENTS PLAITS", w - 40, BK_FAINT);
        bk_lab_scope(x + 20, y + h - 36, w - 40, 26, BK_ALPHA(col, 160));
        break;
    }
    case M_SAMPLES: {
        float wx = (float)(x + 10), ww = (float)(w - 20), wy = (float)(y + 150), wh = 150;
        float st = lab_v[P_SA_START] / 100, ln = lab_v[P_SA_LENGTH] / 100;
        if (sa_nfiles < 0)
            sa_scan();
        if (sa_nfiles <= 0) {
            bk_text(x + 20, y + 30, &FONT_B, 1, "NO SAMPLES YET", col);
            bk_text(x + 20, y + 70, &FONT_S, 1, "PUT WAV FILES INTO", BK_DIM);
            bk_text(x + 20, y + 94, &FONT_S, 1, "MUTABLEDREAMS/SAMPLES/", BK_INK);
            bk_text(x + 20, y + 118, &FONT_S, 1, "THEN PRESS A ON FILE", BK_DIM);
            {   /* (1.3: one text, broken where it must; two lines ran into each other) */
                char ln[3][64];
                int nl = bk_wrap(&FONT_T, "16 / 24 / 32-BIT OR FLOAT, MONO OR STEREO, ANY RATE, UP TO 90 S EACH", w - 40, ln, 3);
                bk_tr_keys = 1;
                for (int q = 0; q < nl; q++)
                    bk_text(x + 20, y + 160 + q * 18, &FONT_T, 1, ln[q], BK_DIM);
                bk_tr_keys = 0;
            }
            break;
        }
        lab_format(P_SA_FILE, buf, sizeof buf);
        bk_text_fit(x + 20, y + 16, &FONT_B, buf, w - 40, col);
        if (sa_error[0])
            bk_text_up(x + 20, y + 48, &FONT_T, sa_error, w - 40, ARGB(255, 240, 140, 110));
        else if (sa_data) {
            snprintf(buf, sizeof buf, "%.1f S   %u HZ   %u BIT   %s", (double)sa_frames / 44100.0, (unsigned)sa_src_rate,
                     (unsigned)sa_src_bits, sa_src_channels == 1 ? "MONO" : "STEREO");
            bk_text_up(x + 20, y + 48, &FONT_T, buf, w - 40, BK_DIM);
        }
        bk_text(x + 20, y + 72, &FONT_S, 1, (int)lab_v[P_SA_MODE] ? "FOLLOWS TRACK" : "LOOP", BK_INK);
        if ((int)lab_v[P_SA_MODE]) {
            lab_format(P_SA_TRACK, buf, sizeof buf);
            bk_text(x + 160, y + 72, &FONT_S, 1, buf, bk_track_col((int)lab_v[P_SA_TRACK]));
        }
        bk_rrect((int)wx, (int)(wy - 6), (int)ww, (int)wh + 12, 10, ARGB(20, 255, 255, 255));
        bk_rrect((int)(wx + ww * st), (int)(wy - 6), (int)(ww * fminf(ln, 1 - st)) + 1, (int)wh + 12, 8, BK_ALPHA(col, 40));
        for (i = 0; i < SA_PEAKS && i < (int)ww; i++) {   /* the waveform */
            float hi = sa_peak_max[i], lo = sa_peak_min[i], cx = wx + (float)i * ww / SA_PEAKS;
            int in = (float)i / SA_PEAKS >= st && (float)i / SA_PEAKS <= st + ln;
            bk_rect((int)cx, (int)(wy + wh / 2 - hi * wh / 2), 1, (int)((hi - lo) * wh / 2) + 1, BK_ALPHA(col, in ? 220 : 70));
        }
        if (sa_data && sa_level > 0) {           /* where it plays */
            if (!(int)lab_v[P_SA_MODE]) {
                double w0 = (double)sa_frames * st, len = (double)sa_frames * ln;
                double p = lab_v[P_SA_REVERSE] > 0.5f ? w0 + len - sa_loop_pos : w0 + sa_loop_pos;
                float px = wx + ww * (float)(p / sa_frames);
                bk_rect((int)px, (int)(wy - 10), 2, (int)wh + 20, BK_INK);
            } else
                for (i = 0; i < SA_VOICES; i++)
                    if (sa_voice[i].on) {
                        float px = wx + ww * (float)(sa_voice[i].pos / sa_frames);
                        bk_rect((int)px, (int)(wy - 10), 2, (int)wh + 20, BK_ALPHA(BK_INK, (uint32_t)(80 + 175 * sa_voice[i].env)));
                    }
        }
        {
            float e = sa_level * 8;
            e = e > 1 ? 1 : e;
            bk_disc((float)(x + w / 2), (float)(y + h - 170), 14 + 40 * e, BK_ALPHA(col, 60 + (uint32_t)(150 * e)));
        }
        bk_text_up(x + 20, y + h - 60, &FONT_T, "FILES: MUTABLEDREAMS/SAMPLES/   A ON FILE: LOOK AGAIN", w - 40, BK_FAINT);
        bk_lab_scope(x + 20, y + h - 36, w - 40, 26, BK_ALPHA(col, 160));
        break;
    }
    case M_RACK_A: case M_RACK_B: {              /* the chain: what goes in, four stations along a flowing path, what comes out */
        int r = m == M_RACK_B;
        fx_rack *xr = &fx_r[r];
        static const char *const srcn[4] = {"VOICES", "SAMPLES", "SYNTH", "POLY"};
        static const int srcm[4] = {M_VOICES, M_SAMPLES, M_SYNTH, M_POLY};
        static const char *const st[4] = {"FILTER", "DRIVE", "RESONATOR", "ECHO"};
        float lin = fminf(xr->level_in * 8, 1), lout = fminf(xr->level_out * 8, 1);
        int bw = (w - 60) / 2, bh = 150, sx0 = x + 20, sy0 = y + 110;
        float amt[4] = {(int)lab_v[FXP(r, FILTER)] ? 1.0f : 0.0f, fmaxf(lab_v[FXP(r, DRIVE)], lab_v[FXP(r, CRUSH)]) / 100.0f,
                        lab_v[FXP(r, RES_MIX)] / 100.0f, lab_v[FXP(r, ECHO_MIX)] / 100.0f};
        float node[6][2];                        /* the path: in, the four stations (Z order), out */
        bk_text(x + 20, y + 14, &FONT_B, 1, r ? "RACK B" : "RACK A", col);
        bk_text_fit(x + 120, y + 20, &FONT_T, "FILTER > DRIVE > RESONATOR > ECHO", w - 140, BK_DIM);
        for (i = 0; i < 4; i++) {                /* the sources coming in */
            int on = fx_routed(r, i), pw = (w - 52) / 4;
            bk_rrect(x + 20 + i * (pw + 4), y + 50, pw, 24, 12, on ? BK_ALPHA(LAB_MOD_COL[srcm[i]], 210) : ARGB(16, 255, 255, 255));
            bk_text_c(x + 20 + i * (pw + 4) + pw / 2, y + 56, &FONT_T, 1, srcn[i], on ? ARGB(255, 10, 14, 22) : BK_FAINT);
        }
        node[0][0] = (float)(x + w / 2), node[0][1] = (float)(y + 80);
        for (i = 0; i < 4; i++)
            node[i + 1][0] = (float)(sx0 + (i % 2) * (bw + 20) + bw / 2), node[i + 1][1] = (float)(sy0 + (i / 2) * (bh + 26) + bh / 2);
        node[5][0] = (float)(x + w / 2), node[5][1] = (float)(sy0 + 2 * (bh + 26) + 4);
        for (i = 0; i < 5; i++) {                /* the wires, and the sound flowing along them */
            float ax = node[i][0], ay = node[i][1], bx2 = node[i + 1][0], by2 = node[i + 1][1];
            bk_line(ax, ay, bx2, by2, 2.0f, BK_ALPHA(col, (uint32_t)(40 + 120 * lin)));
            for (int d = 0; d < 3; d++) {
                float u = fmodf(t * (0.5f + 1.5f * lin) + (float)d / 3.0f + (float)i * 0.13f, 1.0f);
                bk_disc(ax + (bx2 - ax) * u, ay + (by2 - ay) * u, 2.0f + 3.0f * lin, BK_ALPHA(col, (uint32_t)(60 + 190 * lin)));
            }
        }
        for (i = 0; i < 4; i++) {                /* four stations */
            int bx = sx0 + (i % 2) * (bw + 20), byy = sy0 + (i / 2) * (bh + 26), on = amt[i] > 0;
            float glow = on ? (i == 2 ? fminf(xr->res_level * 12.0f, 1) : i == 3 ? fminf(xr->echo_level * 20.0f, 1) : lin) : 0;
            if (on)
                bk_rrect(bx - 3, byy - 3, bw + 6, bh + 6, 17, BK_ALPHA(col, (uint32_t)(20 + 50 * glow)));
            bk_rrect(bx, byy, bw, bh, 14, on ? ARGB(255, 16, 22, 30) : ARGB(255, 12, 16, 24));
            bk_rframe(bx, byy, bw, bh, 14, on ? 2 : 1, on ? BK_ALPHA(col, 200) : ARGB(40, 255, 255, 255));
            bk_text(bx + 10, byy + 8, &FONT_T, 1, st[i], on ? BK_INK : BK_FAINT);
            if (!on) {                            /* asleep: a dim sign of what it does, breathing */
                float cx = (float)(bx + bw / 2), cy = (float)(byy + bh / 2 + 10), br = 0.5f + 0.5f * sinf(t * 1.2f + (float)i);
                uint32_t sc = ARGB(40 + (uint32_t)(30 * br), 255, 255, 255);
                if (i == 0) {                     /* a hill, the filter's curve */
                    float ox = cx - 50, oy = cy + 10;
                    for (int k = 1; k <= 20; k++) {
                        float u = (float)k / 20.0f, nx = cx - 50 + u * 100, ny = cy + 10 - 26.0f * expf(-(u - 0.4f) * (u - 0.4f) * 12.0f);
                        bk_line(ox, oy, nx, ny, 2.0f, sc);
                        ox = nx, oy = ny;
                    }
                } else if (i == 1) {              /* a small flame */
                    float fl = sinf(t * 5.0f + (float)bx) * 2.0f;
                    bk_disc(cx, cy + 8, 9, sc);
                    bk_disc(cx + fl * 0.5f, cy - 2, 6, sc);
                    bk_disc(cx + fl, cy - 11, 3, sc);
                } else if (i == 2) {              /* a string, quivering a little */
                    float ox = cx - 50, oy = cy;
                    for (int k = 1; k <= 20; k++) {
                        float u = (float)k / 20.0f, nx = cx - 50 + u * 100, ny = cy + sinf(u * 3.14159f) * sinf(t * 7.0f) * 5.0f * br;
                        bk_line(ox, oy, nx, ny, 1.6f, sc);
                        ox = nx, oy = ny;
                    }
                } else                            /* echoes: rings fading out */
                    for (int k = 0; k < 3; k++) {
                        float ph = fmodf(t * 0.3f + (float)k / 3.0f, 1.0f);
                        bk_arc(cx, cy, 6.0f + 30.0f * ph, 1.5f, 0, 6.3f, ARGB((uint32_t)(70 * (1 - ph)), 255, 255, 255));
                    }
                bk_text_c((int)cx, byy + bh - 26, &FONT_T, 1, "OFF", BK_FAINT);
                continue;
            }
            if (i == 0) {                         /* the filter's curve, where its cutoff is now */
                float px = 0, py = 0, fc = (xr->level_in > 0 ? xr->cutoff_now : lab_v[FXP(r, CUTOFF)]) / 100.0f, rs = lab_v[FXP(r, RESONANCE)] / 100.0f;
                int ft = (int)lab_v[FXP(r, FILTER)], cw = bw - 20;
                for (int k = 0; k <= cw; k += 3) {
                    float u = (float)k / (float)cw, dd = (u - fc) * 6.0f, mag;
                    mag = ft == 1 ? (dd < 0 ? 1 : expf(-dd * 1.4f)) : ft == 3 ? (dd > 0 ? 1 : expf(dd * 1.4f)) : expf(-dd * dd * 1.2f);
                    mag = fminf(mag + rs * 1.2f * expf(-dd * dd * 10.0f), 1.9f);
                    if (k) {
                        bk_line(px, py, (float)(bx + 10 + k), (float)(byy + bh - 16) - mag * 52.0f, 2.0f, col);
                        bk_line((float)(bx + 10 + k), (float)(byy + bh - 16), (float)(bx + 10 + k), (float)(byy + bh - 16) - mag * 52.0f, 3.0f, BK_ALPHA(col, 26));
                    }
                    px = (float)(bx + 10 + k), py = (float)(byy + bh - 16) - mag * 52.0f;
                }
                lab_format(FXP(r, FILTER), buf, sizeof buf);
                bk_text(bx + bw - 10 - bk_text_w(&FONT_T, buf, 1), byy + 8, &FONT_T, 1, buf, BK_DIM);
            } else if (i == 1) {                  /* the drive's curve; crushed into steps; a dot riding it with the level */
                float g = 1.0f + lab_v[FXP(r, DRIVE)] / 100.0f * 12.0f, cr = lab_v[FXP(r, CRUSH)] / 100.0f, px = 0, py = 0;
                int cw = bw - 20, my = byy + bh / 2 + 8;
                for (int k = 0; k <= cw; k += 2) {
                    float u = ((float)k / (float)cw) * 2.0f - 1.0f, v = tanhf(u * g * 0.4f) / tanhf(g * 0.4f);
                    if (cr > 0)
                        v = floorf(v * (2.0f + 14.0f * (1.0f - cr)) + 0.5f) / (2.0f + 14.0f * (1.0f - cr));
                    if (k)
                        bk_line(px, py, (float)(bx + 10 + k), (float)my - v * 46.0f, 2.0f, col);
                    px = (float)(bx + 10 + k), py = (float)my - v * 46.0f;
                }
                {
                    float u = sinf(t * 7.0f) * lin, v = tanhf(u * g * 0.4f) / tanhf(g * 0.4f);
                    bk_disc((float)(bx + 10) + (u + 1.0f) * 0.5f * (float)cw, (float)my - v * 46.0f, 5, BK_INK);
                }
            } else if (i == 2) {                  /* the resonator: strings trembling with its sound */
                float e = fminf(xr->res_level * 12.0f, 1.0f) * 0.8f + 0.2f;
                for (int sn = 0; sn < 6; sn++) {
                    float sx = (float)(bx + 20) + (float)sn * (float)(bw - 40) / 5.0f, py0 = (float)(byy + 32), px0 = sx;
                    for (int sg = 1; sg <= 10; sg++) {
                        float yy = (float)(byy + 32) + (float)sg * (float)(bh - 60) / 10.0f;
                        float xx = sx + sinf((float)sg * 0.314f) * e * 8.0f * sinf(t * (22.0f + 3.0f * (float)sn) + (float)sn);
                        bk_line(px0, py0, xx, yy, 1.6f, BK_ALPHA(col, 120 + (uint32_t)(130 * e)));
                        px0 = xx, py0 = yy;
                    }
                }
                bk_text(bx + 10, byy + bh - 20, &FONT_T, 1, FX_RES_MODELS[(int)lab_v[FXP(r, RES_MODEL)] % 6], BK_DIM);
            } else {                              /* the echo: repeats, each quieter, crossing sides */
                float fb = lab_v[FXP(r, ECHO_FEEDBACK)] / 100.0f, a = 1.0f, e = fminf(xr->echo_level * 20.0f, 1.0f);
                for (int k = 0; k < 9 && a > 0.04f; k++) {
                    float ex = (float)(bx + 14) + (float)k * (float)(bw - 28) / 9.0f, hh = a * 70.0f * (0.5f + 0.5f * e);
                    int top = (k & 1) ? 0 : 1;   /* (left, right, left: the ping-pong) */
                    bk_rrect((int)ex, top ? (int)((float)(byy + 92) - hh) : byy + 96, 9, (int)(hh * (top ? 1.0f : 0.5f)) + 1, 3,
                             BK_ALPHA(col, (uint32_t)(70 + 180 * a)));
                    a *= fb * 0.92f;
                }
                bk_text(bx + 10, byy + bh - 20, &FONT_T, 1, FX_ECHO_TIMES[(int)lab_v[FXP(r, ECHO_TIME)] % 6], BK_DIM);
            }
        }
        {   /* out: in and out meters */
            int my = sy0 + 2 * (bh + 26) + 18;
            bk_text_up(x + 20, my, &FONT_T, "IN", w - 40, BK_FAINT);
            bk_rrect(x + 52, my + 2, w - 72, 8, 4, ARGB(20, 255, 255, 255));
            bk_rrect(x + 52, my + 2, (int)((float)(w - 72) * lin) + 2, 8, 4, BK_ALPHA(col, 150));
            bk_text_up(x + 20, my + 18, &FONT_T, "OUT", w - 40, BK_FAINT);
            bk_rrect(x + 52, my + 20, w - 72, 8, 4, ARGB(20, 255, 255, 255));
            bk_rrect(x + 52, my + 20, (int)((float)(w - 72) * lout) + 2, 8, 4, col);
        }
        bk_lab_scope(x + 20, y + h - 36, w - 40, 26, BK_ALPHA(col, 160));
        break;
    }
    case M_GRIDS: {                              /* the topographic map, where we are on it, the three parts */
        int mx = x + 30, my = y + 60, ms = 280;
        float px = (float)mx + lab_v[P_GR_X] / 100.0f * (float)ms, py = (float)my + lab_v[P_GR_Y] / 100.0f * (float)ms;
        static const char *const pn[3] = {"KICK", "SNARE", "HAT"};
        static const uint32_t pc[3] = {ARGB(255, 255, 150, 110), ARGB(255, 250, 220, 140), ARGB(255, 160, 220, 255)};
        bk_text(x + 20, y + 14, &FONT_B, 1, "GRIDS", col);
        bk_text(x + 100, y + 20, &FONT_T, 1, "THE TOPOGRAPHIC DRUM MAP", BK_DIM);
        for (int gy = 0; gy < 5; gy++)           /* the 25 patterns, as how busy each is */
            for (int gx = 0; gx < 5; gx++) {
                const uint8_t *nd = GRIDS_NODES[GRIDS_MAP[gx][gy]];
                int sum = 0;
                for (int k = 0; k < 96; k++)
                    sum += nd[k];
                {
                    float dx = (float)(mx + gx * ms / 4) - px, dy = (float)(my + gy * ms / 4) - py, near = expf(-(dx * dx + dy * dy) / 9000.0f);
                    bk_rrect(mx + gx * ms / 4 - 22, my + gy * ms / 4 - 22, 44, 44, 10, BK_ALPHA(col, (uint32_t)(14 + sum / 160 + 120 * near)));
                }
            }
        for (i = 0; i < 3; i++) {                /* each hit: a ring spreading from where we are */
            float fl = (float)(gr_frames - gr_flash_at[i]) / 44100.0f;
            static const uint32_t rc[3] = {ARGB(255, 255, 150, 110), ARGB(255, 250, 220, 140), ARGB(255, 160, 220, 255)};
            if (fl < 0.6f && gr_hits[i])
                bk_arc(px, py, 12.0f + fl * 110.0f, 2.5f, 0, 6.3f, BK_ALPHA(rc[i], (uint32_t)(220 * (1.0f - fl / 0.6f))));
        }
        bk_rframe(mx - 26, my - 26, ms + 52, ms + 52, 14, 1, ARGB(40, 255, 255, 255));
        bk_disc(px, py, 16, BK_ALPHA(col, 60));
        bk_disc(px, py, 7, BK_INK);
        for (i = 0; i < 3; i++) {                /* each part's 32 steps at this place; the step playing now */
            uint8_t gx = gr_xy(lab_v[P_GR_X]), gy = gr_xy(lab_v[P_GR_Y]);
            int ry = y + 400 + i * 52, now = (int)(gr_step % 32u);
            uint8_t thr = (uint8_t)~(uint8_t)(lab_v[P_GR_KICK_DENSITY + i] / 100.0f * 255.0f);
            float fl = (float)(gr_frames - gr_flash_at[i]) / 44100.0f;
            bk_text(x + 20, ry + 4, &FONT_T, 1, pn[i], pc[i]);
            lab_format(P_GR_KICK_TO + i, buf, sizeof buf);
            bk_text_up(x + 20, ry + 20, &FONT_T, buf, w - 40, BK_FAINT);
            for (int k = 0; k < 32; k++) {
                int lv = gr_level(k, i, gx, gy), on = lv > thr;
                bk_rrect(x + 90 + k * 8, ry + 6, 6, 26, 2, on ? BK_ALPHA(pc[i], k == now && md_playing() ? 255 : 150) : ARGB(20 + lv / 6, 255, 255, 255));
            }
            if (fl < 0.15f)
                bk_disc((float)(x + 72), (float)(ry + 18), 6, pc[i]);
        }
        bk_lab_scope(x + 20, y + h - 36, w - 40, 26, BK_ALPHA(col, 160));
        break;
    }
    case M_SYNTH: {                              /* the instrument, drawn as what it is */
        int eng = (int)lab_v[P_SY_ENGINE] % SY_ENGINES;
        float cx = (float)(x + w / 2), cy = (float)(y + 220), chr = lab_v[P_SY_CHARACTER] / 100.0f;
        bk_text(x + 20, y + 14, &FONT_B, 1, SY_NAMES[eng], col);
        lab_format(P_SY_PLAYED_BY, buf, sizeof buf);
        bk_text_up(x + 20, y + 40, &FONT_T, "PLAYED BY", w - 40, BK_DIM);
        bk_text(x + 30 + bk_text_w(&FONT_T, T("PLAYED BY"), 1), y + 40, &FONT_T, 1, buf, BK_INK);   /* (after the label, in any language) */
        switch (eng) {
        case SY_DRONE: {                         /* the partials, breathing; their ratios under them */
            static const char *const nm[3][SY_PARTIALS] = {{"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"},
                                                          {"1", "5/4", "3/2", "2", "5/2", "3", "15/4", "4", "5", "6"},
                                                          {"1", "7/6", "3/2", "7/4", "2", "21/8", "3", "7/2", "4", "21/4"}};
            int set = chr < 0.34f ? 0 : chr < 0.67f ? 1 : 2, bw = (w - 60) / SY_PARTIALS;
            for (i = 0; i < SY_PARTIALS; i++) {
                float a = dr_amp[i] * 220.0f;
                a = a > 230 ? 230 : a;
                bk_rrect(x + 30 + i * bw, (int)(y + 320 - a), bw - 8, (int)a + 2, 5, BK_ALPHA(col, 120 + (uint32_t)(i == 0 ? 120 : 60)));
                bk_text_c(x + 30 + i * bw + bw / 2 - 4, y + 330, &FONT_T, 1, nm[set][i], BK_DIM);
            }
            bk_text_up(x + 20, y + 60, &FONT_T, set == 0 ? "THE HARMONIC SERIES" : set == 1 ? "4:5:6, STACKED (JUST MAJOR)" : "SEPTIMAL: THE 7TH PARTIAL'S COLOURS", w - 40, BK_DIM);
            break;
        }
        case SY_MODAL: {                         /* eight resonators: rings that ring when struck */
            for (i = 0; i < SY_MODES; i++) {
                float a = (float)i * 0.785f, r = 110.0f, px = cx + cosf(a) * r, py = cy + sinf(a) * r * 0.8f;
                float hit = mg_hit[i], rad = 10.0f + 10.0f * logf(1.0f + mg_f[i] / 200.0f);
                bk_arc(px, py, rad + 18.0f * hit, 2.0f + 3.0f * hit, 0, 6.3f, BK_ALPHA(col, 80 + (uint32_t)(170 * hit)));
                bk_disc(px, py, 4.0f, col);
            }
            bk_text_up(x + 20, y + 60, &FONT_T, chr < 0.5f ? "TUNED TO THE KEY" : "BENDING TOWARD BELL RATIOS", w - 40, BK_DIM);
            break;
        }
        case SY_SHEPARD: {                       /* the bell over log frequency; the components sliding under it */
            float centre = 6.0f + 2.5f * lab_v[P_SY_BRIGHT] / 100.0f, px = 0, py = 0;
            for (i = 0; i <= 200; i++) {
                float oct = (float)i / 20.0f, hgt = expf(-0.5f * (oct - centre) * (oct - centre) / 2.2f) * 200.0f;
                float xx = (float)(x + 30) + (float)i / 200.0f * (float)(w - 60), yy = (float)(y + 330) - hgt;
                if (i)
                    bk_line(px, py, xx, yy, 2.0f, BK_ALPHA(col, 120));
                px = xx, py = yy;
            }
            for (int c = 0; c < 10; c++) {
                float oct = (float)c + sh_pos, hgt = expf(-0.5f * (oct - centre) * (oct - centre) / 2.2f) * 200.0f;
                float xx = (float)(x + 30) + oct / 10.0f * (float)(w - 60);
                bk_line(xx, (float)(y + 330), xx, (float)(y + 330) - hgt, 3.0f, col);
            }
            bk_text_up(x + 20, y + 60, &FONT_T, chr < 0.5f ? "RISING WITHOUT END" : "FALLING WITHOUT END", w - 40, BK_DIM);
            break;
        }
        case SY_KARPLUS: {                       /* eight strings, trembling while they sound */
            for (int st = 0; st < SY_STRINGS; st++) {
                float sx = (float)(x + 50 + st * (w - 100) / (SY_STRINGS - 1)), amp = ks_env[st] * 14.0f, py = (float)(y + 100);
                for (int sg = 1; sg <= 16; sg++) {
                    float yy = (float)(y + 100) + (float)sg * 14.0f, xx = sx + sinf((float)sg * 0.39f) * amp * sinf(t * 40.0f + (float)st);
                    bk_line(sx + sinf((float)(sg - 1) * 0.39f) * amp * sinf(t * 40.0f + (float)st), py, xx, yy, 1.6f, BK_ALPHA(col, 120 + (uint32_t)(130 * ks_env[st])));
                    py = yy;
                }
            }
            bk_text_up(x + 20, y + 60, &FONT_T, "DROPS ON STRINGS, IN THE KEY", w - 40, BK_DIM);
            break;
        }
        case SY_BELLS: {                         /* bells swinging, as loud as they ring */
            static const char *const rn[4] = {"RATIO: SQUARE ROOT OF 2", "RATIO: THE GOLDEN RATIO", "RATIO: PI / 2", "RATIO: E / 2"};
            for (int b = 0; b < SY_NBELLS; b++) {
                float bx = (float)(x + 60 + b * (w - 120) / (SY_NBELLS - 1)), e = be_v[b].on ? be_v[b].env : 0, sw = sinf(t * 3.0f + (float)b) * 10.0f * e;
                bk_line(bx, (float)(y + 110), bx + sw, (float)(y + 170), 1.5f, BK_DIM);
                for (int k = 0; k < 5; k++)
                    bk_disc(bx + sw, (float)(y + 172 + k * 7), 6.0f + (float)k * 3.0f, BK_ALPHA(col, 60 + (uint32_t)(190 * e)));
            }
            bk_text_up(x + 20, y + 60, &FONT_T, rn[(int)(chr * 3.99f)], w - 40, BK_DIM);
            break;
        }
        case SY_RINGS: case SY_ELEMENTS: {       /* a resonant body: rings rippling out with the sound */
            float e = sy_level * 14.0f;
            e = e > 1 ? 1 : e;
            for (i = 0; i < 6; i++) {
                float ph = fmodf(t * 0.4f + (float)i / 6.0f, 1.0f), rad = 20.0f + ph * 130.0f;
                bk_arc(cx, cy, rad, 2.0f + 3.0f * e, 0, 6.3f, BK_ALPHA(col, (uint32_t)((1.0f - ph) * (40.0f + 200.0f * e))));
            }
            bk_disc(cx, cy, 14.0f + 10.0f * e, BK_ALPHA(col, 200));
            lab_format(P_SY_MODEL, buf, sizeof buf);
            bk_text_up(x + 20, y + 60, &FONT_T, eng == SY_RINGS ? "MUTABLE INSTRUMENTS RINGS" : "MUTABLE INSTRUMENTS ELEMENTS", w - 40, BK_DIM);
            bk_text_up(x + 20, y + 78, &FONT_T, buf, w - 40, BK_INK);
            if (eng == SY_ELEMENTS)
                bk_text_up(x + 20, y + 96, &FONT_T, chr < 0.34f ? "BOWED" : chr < 0.67f ? "BLOWN" : "STRUCK", w - 40, BK_DIM);
            break;
        }
        case SY_GLASS: {                         /* (1.2) a row of glass bowls, each glowing while it sings */
            for (int v = 0; v < NV_N; v++) {
                float bx = (float)(x + 50) + (float)v * (float)(w - 100) / (float)(NV_N - 1), e = nv[v].on ? nv[v].env : 0;
                float rw = 19.0f - (nv[v].f > 0 ? 2.5f * log2f(nv[v].f / 110.0f) : 0), by = (float)(y + 200) + (float)(v % 2) * 70.0f;
                rw = rw < 11 ? 11 : rw > 19 ? 19 : rw;
                e = e > 1 ? 1 : e;
                if (e > 0.02f && !bk_calm)      /* (its glow, and a ring of sound going out) */
                    bk_disc(bx, by + rw * 0.4f, rw + 8.0f * e, BK_ALPHA(col, (uint32_t)(60.0f * e))),
                    bk_arc(bx, by + rw * 0.4f, rw + 6.0f + fmodf(t * 30.0f + (float)v * 7.0f, 30.0f), 1.5f, 0, 6.3f,
                           BK_ALPHA(col, (uint32_t)(90.0f * e * (1.0f - fmodf(t * 30.0f + (float)v * 7.0f, 30.0f) / 30.0f))));
                bk_line(bx - rw, by - 14.0f, bx + rw, by - 14.0f, 2.0f, BK_ALPHA(col, 110 + (uint32_t)(140 * e)));
                for (int k = 0; k < 8; k++) {    /* (the bowl: its rim to its foot) */
                    float q = (float)k / 7.0f, ww = rw * (1.0f - q * q * 0.8f);
                    bk_line(bx - ww, by - 14.0f + q * rw * 1.5f, bx + ww, by - 14.0f + q * rw * 1.5f, 1.4f, BK_ALPHA(col, 40 + (uint32_t)(150 * e)));
                }
                bk_line(bx, by - 14.0f + rw * 1.5f, bx, by + rw * 1.5f + 4.0f, 2.0f, BK_ALPHA(col, 90));   /* (its stem) */
                bk_line(bx - rw * 0.5f, by + rw * 1.5f + 4.0f, bx + rw * 0.5f, by + rw * 1.5f + 4.0f, 2.0f, BK_ALPHA(col, 90));
                if (e > 0.05f) {                 /* (the finger going round the rim) */
                    float ang = t * 3.0f + (float)v;
                    bk_disc(bx + cosf(ang) * rw, by - 14.0f + sinf(ang) * 4.0f, 3.0f + 3.0f * e, BK_INK);
                }
            }
            bk_text_up(x + 20, y + 60, &FONT_T, chr < 0.5f ? "SLOW SWELLS, RUBBED GLASS" : "QUICK TOUCHES, RUBBED GLASS", w - 40, BK_DIM);
            break;
        }
        case SY_PAD: {                           /* (1.2) a curtain of light, swaying with the chord */
            for (int c = 0; c < 28; c++) {
                float fx = (float)(x + 30) + (float)c * (float)(w - 60) / 27.0f, px = fx, py = (float)(y + 120);
                int v = c % 3;
                float f = pad_f[v] > 0 ? pad_f[v] : 110.0f, e = sy_level * 10.0f;
                e = e > 1 ? 1 : e;
                for (int sg = 1; sg <= 12; sg++) {
                    float yy = (float)(y + 120) + (float)sg * 19.0f;
                    float xx = fx + sinf((float)sg * 0.5f + t * (0.6f + f / 400.0f) + (float)c * 0.4f) * (6.0f + 10.0f * e) * (float)sg / 12.0f;
                    bk_line(px, py, xx, yy, 2.0f, BK_ALPHA(col, (uint32_t)((40.0f + 120.0f * e) * (1.0f - (float)sg / 16.0f))));
                    px = xx, py = yy;
                }
            }
            bk_text_up(x + 20, y + 60, &FONT_T, "A CHORD OF FIFTEEN SAWS: I IV V VI", w - 40, BK_DIM);
            break;
        }
        case SY_CZ: {                            /* (1.2) the wave as the newest voice shapes it now */
            float r = 1.0f, best = -1, px = 0, py = 0;
            float top = 1.5f + chr * 14.0f * (0.3f + lab_v[P_SY_BRIGHT] / 100.0f);
            for (int v = 0; v < NV_N; v++)
                if (nv[v].on && nv[v].env > best)
                    best = nv[v].env, r = 1.0f + (top - 1.0f) * nv[v].cz;
            for (i = 0; i <= 160; i++) {
                float p = (float)(i % 80) / 80.0f, o = sinf(p * r * 6.2832f) * (1.0f - p) * 2.0f - (1.0f - p);
                float xx = (float)(x + 30) + (float)i / 160.0f * (float)(w - 60), yy = cy - o * 80.0f;
                if (i)
                    bk_line(px, py, xx, yy, 2.4f, BK_ALPHA(col, 110 + (uint32_t)(140.0f * (best > 0 ? (best > 1 ? 1 : best) : 0))));
                px = xx, py = yy;
            }
            snprintf(buf, sizeof buf, "%s %.1f", bk_tr("RESONANCE"), (double)r);
            bk_text_up(x + 20, y + 60, &FONT_T, "PHASE DISTORTION: A WINDOW RINGING", w - 40, BK_DIM);
            bk_text_up(x + 20, y + 78, &FONT_T, buf, w - 40, BK_INK);
            break;
        }
        case SY_FLUTE: {                         /* (1.2) a row of tubes, breath rising from the ones that sound */
            for (int v = 0; v < NV_N; v++) {
                float tx = (float)(x + 50) + (float)v * (float)(w - 100) / (float)(NV_N - 1), e = nv[v].on ? nv[v].env : 0;
                float len = nv[v].f > 0 ? 200.0f - 40.0f * log2f(nv[v].f / 110.0f) : 120.0f;
                len = len < 50 ? 50 : len > 220 ? 220 : len;
                e = e > 1 ? 1 : e;
                bk_rrect((int)tx - 9, y + 330 - (int)len, 18, (int)len, 9, BK_ALPHA(col, 50 + (uint32_t)(150 * e)));
                for (int k = 1; k <= 3; k++)     /* (its finger holes) */
                    bk_disc(tx, (float)(y + 330) - len * (0.25f + 0.17f * (float)k), 3.0f, ARGB(200, 10, 16, 22));
                if (e > 0.03f && !bk_calm)
                    for (int k = 0; k < 5; k++) {   /* (breath: wisps rising and fading) */
                        float ph = fmodf(t * 0.9f + (float)k * 0.2f + (float)v * 0.13f, 1.0f);
                        bk_disc(tx + sinf(ph * 9.0f + (float)k) * 8.0f * ph, (float)(y + 322) - len - ph * 70.0f, 2.0f + 4.0f * ph,
                                BK_ALPHA(col, (uint32_t)(60.0f + 190.0f * e * (1.0f - ph))));
                    }
            }
            bk_text_up(x + 20, y + 60, &FONT_T, chr < 0.5f ? "MORE BREATH THAN TONE" : "MORE TONE THAN BREATH", w - 40, BK_DIM);
            break;
        }
        case SY_CHOIR: {                         /* the vowel triangle, and where the choir sings */
            static const char *const vw[5] = {"A", "E", "I", "O", "U"};
            static const float vx[5] = {0.5f, 0.25f, 0.05f, 0.75f, 0.95f}, vy[5] = {0.95f, 0.45f, 0.05f, 0.45f, 0.05f};
            float px, py, fv = ch_vowel - floorf(ch_vowel);
            int vi = (int)ch_vowel;
            vi = vi < 0 ? 0 : vi > 3 ? 3 : vi;
            for (i = 0; i < 5; i++)
                bk_text(x + 40 + (int)(vx[i] * (float)(w - 80)), y + 90 + (int)(vy[i] * 220.0f), &FONT_B, 1, vw[i], BK_DIM);
            px = (float)(x + 46) + (vx[vi] + (vx[vi + 1] - vx[vi]) * fv) * (float)(w - 80);
            py = (float)(y + 98) + (vy[vi] + (vy[vi + 1] - vy[vi]) * fv) * 220.0f;
            bk_disc(px, py, 14.0f + 6.0f * sy_level * 20.0f, BK_ALPHA(col, 120));
            bk_disc(px, py, 5.0f, BK_INK);
            bk_text_up(x + 20, y + 60, &FONT_T, "THREE VOICES: CHORD I IV V VI", w - 40, BK_DIM);
            break;
        }
        }
        {
            float e = sy_level * 8;
            e = e > 1 ? 1 : e;
            float mx = (float)(x + w / 2), my = (float)(y + h - 110);   /* (1.2) the level: a seed glowing, rings going out */
            bk_disc(mx, my, 12 + 22 * e, BK_ALPHA(col, 40 + (uint32_t)(60 * e)));
            bk_disc(mx, my, 6 + 10 * e, BK_ALPHA(col, 150 + (uint32_t)(100 * e)));
            if (!bk_calm)
                for (i = 0; i < 3; i++) {
                    float ph = fmodf(t * 0.5f + (float)i / 3.0f, 1.0f);
                    bk_arc(mx, my, 14.0f + ph * 40.0f, 1.5f, 0, 6.3f, BK_ALPHA(col, (uint32_t)((1.0f - ph) * 160.0f * e)));
                }
        }
        bk_lab_scope(x + 20, y + h - 36, w - 40, 26, BK_ALPHA(col, 160));
        break;
    }
    case M_LOGIC: {                              /* the rule's principle, drawn; below, the score of the last steps */
        int rule = (int)lab_v[P_LG_RULE] % LG_RULES, per = lg_per_octave();
        float cx = (float)(x + w / 2), cy = (float)(y + 200), fl = lg_flash;
        bk_text(x + 20, y + 14, &FONT_B, 1, LG_RULE_NAMES[rule], col);
        switch (rule) {
        case LG_TINTINNABULI: {                  /* a staff: the M line and its bells */
            for (i = 0; i < 5; i++)
                bk_rect(x + 20, y + 140 + i * 18, w - 40, 1, ARGB(50, 255, 255, 255));
            for (i = 0; i < 24; i++) {
                int a = lg_hist1[(lg_hist_w + LG_HIST - 24 + i) % LG_HIST], b = lg_hist2[(lg_hist_w + LG_HIST - 24 + i) % LG_HIST];
                float px = (float)(x + 30 + i * (w - 60) / 24);
                if (a != LG_REST)
                    bk_disc(px, (float)(y + 230) - (float)a * 9.0f, 6.0f, i == 23 ? BK_INK : col);
                if (b != LG_REST && a != LG_REST) {
                    float by = (float)(y + 230) - (float)b * 9.0f;
                    bk_line(px, (float)(y + 230) - (float)a * 9.0f, px, by, 1.0f, BK_ALPHA(col, 80));
                    bk_arc(px, by, 6, 2, 0, 6.3f, BK_ALPHA(col, 200));   /* the T voice: a bell, hollow */
                }
            }
            bk_text_up(x + 20, y + 44, &FONT_T, "M: STEPS FROM THE CENTRE, GROWING", w - 40, BK_DIM);
            bk_text_up(x + 20, y + 64, &FONT_T, "T: NEAREST TRIAD NOTE (HOLLOW)", w - 40, BK_DIM);
            break;
        }
        case LG_PHASING: {                       /* two rings: L and L + 1 points, their pointers drifting */
            int L = lg_ph_len > 0 ? lg_ph_len : 8;
            for (int ring = 0; ring < 2; ring++) {
                int n = L + ring, pos = (int)(lg_step % (uint32_t)n);
                float r = 60.0f + 34.0f * (float)ring;
                for (i = 0; i < n; i++) {
                    float a = (float)i * 6.2832f / (float)n - 1.5708f;
                    int on = lg_ph_pat[i % L] != LG_REST;
                    bk_disc(cx + cosf(a) * r, cy + sinf(a) * r, i == pos ? 7.0f : on ? 4.0f : 2.0f,
                            i == pos ? BK_INK : on ? BK_ALPHA(col, ring ? 150 : 230) : BK_FAINT);
                }
            }
            snprintf(buf, sizeof buf, "LOOPS OF %d AND %d: TOGETHER AGAIN AFTER %d STEPS", L, L + 1, L * (L + 1));
            bk_text_up(x + 20, y + 44, &FONT_T, buf, w - 40, BK_DIM);
            break;
        }
        case LG_FIBONACCI: {                     /* the golden spiral; the golden word under it */
            float a = 0, r0 = 3.0f, px = cx - 40, py = cy - 10;
            for (i = 0; i < 160; i++) {
                float r = r0 * powf(1.0063f, (float)i * 3.0f), nx = cx - 40 + cosf(a) * r, ny = cy - 10 + sinf(a) * r;
                if (i)
                    bk_line(px, py, nx, ny, 2.0f, BK_ALPHA(col, 60 + (uint32_t)(i * 1.2f)));
                px = nx, py = ny;
                a += 0.0785f;
            }
            for (i = 0; i < 34; i++) {
                uint32_t k = lg_step > 34 ? lg_step - 34 + (uint32_t)i : (uint32_t)i;
                int hit = (int)(floor((double)(k + 2) / 1.6180339887) - floor((double)(k + 1) / 1.6180339887)) == 1;
                bk_rrect(x + 20 + i * ((w - 40) / 34), y + 300, (w - 40) / 34 - 2, 16, 3, hit ? BK_ALPHA(col, i == 33 ? 255 : 160) : ARGB(30, 255, 255, 255));
            }
            bk_text_up(x + 20, y + 44, &FONT_T, "RHYTHM: THE GOLDEN WORD", w - 40, BK_DIM);
            bk_text_up(x + 20, y + 60, &FONT_T, "MELODY: GOLDEN ANGLE, CANON BELOW", w - 40, BK_DIM);
            break;
        }
        case LG_AUTOMATON: {                     /* the generations: the newest on top */
            int cw = (w - 60) / 16, rule_no = LG_CA_RULES[(int)(lab_v[P_LG_SHAPE] / 100.0f * 7.99f)];
            for (int g = 0; g < 18; g++)
                for (int c = 0; c < 16; c++) {
                    int on = (lg_ca[g] >> c) & 1, now = g == 0 && c == (int)((lg_step + 15) % 16u);
                    bk_rrect(x + 30 + c * cw, y + 84 + g * 14, cw - 2, 12, 3,
                             now ? BK_INK : on ? BK_ALPHA(col, (uint32_t)(240 - g * 11)) : ARGB(16, 255, 255, 255));
                }
            snprintf(buf, sizeof buf, "RULE %d: EACH BAR GROWS, LIVING CELLS PLAY", rule_no);
            bk_text_up(x + 20, y + 44, &FONT_T, buf, w - 40, BK_DIM);
            bk_text_up(x + 20, y + 60, &FONT_T, "COLUMNS IN THIRDS, NEWBORN = BASS", w - 40, BK_DIM);
            break;
        }
        case LG_MARKOV: {                        /* where it may go next: the chances from here */
            static const float wt[9] = {0.45f, 0.35f, 0.3f, 1.0f, 0.35f, 1.0f, 0.3f, 0.35f, 0.45f};
            float t2 = 0.3f + lab_v[P_LG_SHAPE] / 100.0f * 2.2f, ws[9], sum = 0;
            int d0 = lg_mk_deg < 0 ? per : lg_mk_deg;
            for (int k = 0; k < 9; k++) {
                int d = d0 + k - 4, m = ((d % per) + per) % per;
                float v = wt[k] * (m == 0 ? 1.6f : m == (per >= 7 ? 4 : 2) ? 1.3f : 1.0f);
                ws[k] = powf(v, 1.0f / t2);
                sum += ws[k];
            }
            for (int k = 0; k < 9; k++) {
                float hgt = ws[k] / sum * 300.0f;
                int bx = x + 40 + k * (w - 80) / 9;
                bk_rrect(bx, (int)(y + 280 - hgt), (w - 80) / 9 - 8, (int)hgt + 1, 4, BK_ALPHA(col, k == 4 ? 255 : 170));
                snprintf(buf, sizeof buf, "%+d", k - 4);
                bk_text(bx + 6, y + 288, &FONT_T, 1, buf, BK_DIM);
            }
            bk_text_up(x + 20, y + 44, &FONT_T, "CHANCES OF THE NEXT STEP", w - 40, BK_DIM);
            bk_text_up(x + 20, y + 60, &FONT_T, "SHAPE: TEMPERATURE (CALM..WILD)", w - 40, BK_DIM);
            break;
        }
        case LG_LSYSTEM: {                       /* the grown string, where it is read now */
            static const char *const gn[3] = {"FIBONACCI  A>AB  B>A", "THUE-MORSE  A>AB  B>BA", "KOCH  A>A+A-A"};
            int start = lg_ls_pos > 20 ? lg_ls_pos - 20 : 0;
            char one[2] = {0, 0};
            snprintf(buf, sizeof buf, "%s   DEPTH %d   LENGTH %d", gn[(int)(lab_v[P_LG_SHAPE] / 34.0f) % 3], lg_ls_depth, lg_ls_len);
            bk_text_up(x + 20, y + 44, &FONT_T, buf, w - 40, BK_DIM);
            for (i = 0; i < 36 && start + i < lg_ls_len; i++) {
                int row = i / 18, colx = i % 18, here = start + i == lg_ls_pos - 1;
                one[0] = lg_ls[start + i];
                if (here)
                    bk_rrect(x + 24 + colx * 24, y + 110 + row * 40, 22, 30, 6, col);
                bk_text(x + 30 + colx * 24, y + 116 + row * 40, &FONT_B, 1, one, here ? ARGB(255, 10, 14, 22) : BK_INK);
            }
            bk_text_up(x + 20, y + 210, &FONT_T, "A: A NOTE, UP A STEP   B: A NOTE BELOW, A BASS   +/-: TURN", w - 40, BK_DIM);
            break;
        }
        case LG_HARMONIC: {                      /* the overtone ladder at its true distances */
            int top = 4 + (int)(lab_v[P_LG_SHAPE] / 100.0f * 12.0f);
            for (int k = 1; k <= 16; k++) {
                float ly = (float)(y + 330) - log2f((float)k) * 62.0f;
                int on = k <= top, now = k == lg_hm_partial;
                bk_rect(x + 60, (int)ly, w - 160, now ? 3 : 1, now ? BK_INK : on ? BK_ALPHA(col, 170) : ARGB(30, 255, 255, 255));
                snprintf(buf, sizeof buf, "%d", k);
                bk_text(x + 30, (int)ly - 6, &FONT_T, 1, buf, on ? BK_DIM : BK_FAINT);
                if (now) {
                    double c = (12.0 * log2((double)k) - floor(12.0 * log2((double)k) + 0.5)) * 100.0;
                    snprintf(buf, sizeof buf, "%+.0f CENT", c);
                    bk_text(x + w - 92, (int)ly - 6, &FONT_T, 1, buf, col);
                    bk_disc((float)(x + 60) + (float)(w - 160) * 0.5f, ly + 1, 6.0f + 6.0f * fl, BK_ALPHA(col, 200));
                }
            }
            bk_text_up(x + 20, y + 44, &FONT_T, "ROOT OVERTONES, JUST INTONATION", w - 40, BK_DIM);
            break;
        }
        }
        {   /* the score: the last 48 steps, voice 1 full, voice 2 hollow */
            int sy = y + h - 150;
            bk_rrect(x + 14, sy - 6, w - 28, 108, 10, ARGB(16, 255, 255, 255));
            for (i = 0; i < LG_HIST; i++) {
                int a = lg_hist1[(lg_hist_w + i) % LG_HIST], b = lg_hist2[(lg_hist_w + i) % LG_HIST];
                float px = (float)(x + 22) + (float)i * (float)(w - 44) / LG_HIST;
                if (rule != LG_HARMONIC) {
                    if (a != LG_REST)
                        bk_disc(px, (float)(sy + 90) - (float)(a + per) * 3.0f, 3.0f, i == LG_HIST - 1 ? BK_INK : col);
                    if (b != LG_REST)
                        bk_arc(px, (float)(sy + 90) - (float)(b + per) * 3.0f, 3.0f, 1.2f, 0, 6.3f, BK_ALPHA(col, 160));
                } else if (a != LG_REST)
                    bk_disc(px, (float)(sy + 90) - log2f((float)a) * 20.0f, 3.0f, col);
            }
            bk_text_up(x + 20, sy - 22, &FONT_T, md_playing() ? "THE LAST 48 STEPS" : "START PLAYS IT", w - 40, BK_FAINT);
        }
        bk_lab_scope(x + 20, y + h - 36, w - 40, 26, BK_ALPHA(col, 160));
        break;
    }
    case M_VOICES: {                             /* the voice: its sentences as a band, where it speaks now */
        const vo_file *vf = vo_cur;
        float wx = (float)(x + 10), ww = (float)(w - 20), wy = (float)(y + 170), wh = 130;
        if (vo_nfiles < 0)
            vo_scan();
        if (vo_nfiles <= 0) {
            static const char *const how[] = {"POEMS, STORIES, CHANTS,", "E.G. FROM LIBRIVOX", "(PUBLIC DOMAIN).", "",
                                              "A .TXT BESIDE A FILE:", "LINE 1 TITLE, LINE 2 CREDIT"};
            bk_text(x + 20, y + 30, &FONT_B, 1, "NO VOICES YET", col);
            bk_text(x + 20, y + 74, &FONT_S, 1, "FETCH THEM IN THE ARCHIVE", BK_INK);
            bk_text(x + 20, y + 98, &FONT_S, 1, "(MENU), OR PUT OGG / WAV", BK_DIM);
            bk_text(x + 20, y + 122, &FONT_S, 1, "FILES INTO THE FOLDER", BK_DIM);
            bk_text(x + 20, y + 152, &FONT_S, 1, "SAMPLES/VOICES/", BK_INK);
            for (i = 0; i < 6; i++)
                bk_text_up(x + 20, y + 196 + i * 22, &FONT_T, how[i], w - 40, BK_DIM);
            {   /* a bird asleep on a branch, waiting for a voice */
                float bx = (float)(x + w - 110), by = (float)(y + h - 190), br = sinf(t * 1.1f) * 1.5f;
                bk_line(bx - 90, by + 26, bx + 80, by + 18, 4.0f, ARGB(200, 90, 70, 52));
                bk_line(bx + 30, by + 21, bx + 60, by + 2, 2.5f, ARGB(200, 90, 70, 52));
                bk_disc(bx + 60, by, 6, BK_ALPHA(col, 120));
                bk_disc(bx, by + 4 + br, 17, BK_ALPHA(col, 230));          /* its round, sleeping body */
                bk_disc(bx - 16, by - 6 + br, 10, BK_ALPHA(col, 230));     /* its head, tucked */
                bk_line(bx - 20, by - 7 + br, bx - 13, by - 7 + br, 1.6f, ARGB(220, 20, 24, 30));   /* a closed eye */
                bk_line(bx + 14, by + 2 + br, bx + 30, by + 10 + br, 4.0f, BK_ALPHA(col, 200));     /* its tail */
                for (i = 0; i < 3; i++) {                                   /* z z z, drifting up */
                    float u = fmodf(t * 0.35f + (float)i / 3.0f, 1.0f);
                    bk_text((int)(bx - 30 + u * 30), (int)(by - 30 - u * 60), &FONT_T, 1, "z", BK_ALPHA(BK_DIM, (uint32_t)(220 * (1.0f - u))));
                }
            }
            break;
        }
        if (vo_busy || !vf || vf->index != (int)lab_v[P_VO_FILE]) {
            float a = t * 4.0f;
            bk_text(x + 20, y + 30, &FONT_B, 1, "LISTENING TO THE FILE ...", col);
            for (i = 0; i < 3; i++)
                bk_disc((float)(x + 40 + i * 18), (float)(y + 80), 4.0f + 2.0f * sinf(a - (float)i), BK_ALPHA(col, 200));
            break;
        }
        bk_text_fit(x + 20, y + 14, &FONT_B, vf->title, w - 40, col);
        if (vf->credit[0])
            bk_text_fit(x + 20, y + 40, &FONT_T, vf->credit, w - 40, BK_DIM);
        if (!vf->ok) {
            bk_text_up(x + 20, y + 64, &FONT_T, vf->error, w - 40, ARGB(255, 240, 140, 110));
            break;
        }
        snprintf(buf, sizeof buf, "%.0f S   %d SENTENCES   %d FRAGMENTS", (double)vf->frames / 44100.0, vf->nph, vf->nfr);
        bk_text_up(x + 20, y + 62, &FONT_T, buf, w - 40, BK_DIM);
        lab_format(P_VO_MODE, buf, sizeof buf);
        bk_text(x + 20, y + 88, &FONT_S, 1, buf, BK_INK);
        lab_format(P_VO_TRIGGER, buf, sizeof buf);
        bk_text(x + 180, y + 88, &FONT_S, 1, buf, (int)lab_v[P_VO_TRIGGER] >= 1 && (int)lab_v[P_VO_TRIGGER] <= 6 ? bk_track_col((int)lab_v[P_VO_TRIGGER] - 1) : BK_DIM);
        bk_rrect((int)wx, (int)(wy - 8), (int)ww, (int)wh + 16, 10, ARGB(20, 255, 255, 255));
        for (i = 0; i < vf->nph; i++) {          /* the sentences: soft bands */
            float a = wx + ww * (float)vf->ph_a[i] / (float)vf->frames, b = wx + ww * (float)vf->ph_b[i] / (float)vf->frames;
            bk_rect((int)a, (int)(wy - 4), (int)(b - a) + 1, (int)wh + 8, BK_ALPHA(col, i == vo_now_phrase ? 110 : 40));
        }
        for (i = 0; i < VO_PEAKS && i < (int)ww; i++) {   /* the waveform */
            float hi = vf->peak_max[i], lo = vf->peak_min[i], cx = wx + (float)i * ww / VO_PEAKS;
            bk_rect((int)cx, (int)(wy + wh / 2 - hi * wh / 2), 1, (int)((hi - lo) * wh / 2) + 1, BK_ALPHA(BK_INK, 120));
        }
        for (i = 0; i < VO_VOICES; i++)          /* where it speaks */
            if (vo_v[i].on && (int)lab_v[P_VO_MODE] != 2) {
                float px = wx + ww * (float)(vo_v[i].pos / vf->frames);
                bk_rect((int)px, (int)(wy - 12), 2, (int)wh + 24, BK_ALPHA(BK_INK, (uint32_t)(90 + 165 * vo_v[i].env)));
            }
        if ((int)lab_v[P_VO_MODE] == 4) {        /* GRAINS: the reading place, its spray, the grains as lights */
            float cx = wx + ww * vo_centre, sw = lab_v[P_VO_SPRAY] / 100 * 44100.0f / (float)vf->frames * ww;
            bk_rect((int)(cx - sw), (int)(wy - 8), (int)(2 * sw) + 2, (int)wh + 16, BK_ALPHA(col, 40));
            bk_rect((int)cx - 1, (int)(wy - 14), 3, (int)wh + 28, col);
            bk_disc(cx, wy - 18, 6, col);
            for (i = 0; i < VO_GRAINS; i++)
                if (vo_g[i].len) {
                    float u = (float)vo_g[i].age / (float)vo_g[i].len, gx = wx + ww * (float)(vo_g[i].pos / (double)vf->frames);
                    float gy = wy + wh * (0.15f + 0.7f * (float)((i * 37) % 100) / 100.0f), e2 = sinf(u * 3.14159f);
                    bk_disc(gx, gy, 2.0f + 6.0f * e2, BK_ALPHA(BK_INK, (uint32_t)(60 + 190 * e2)));
                }
            if (lab_v[P_VO_TIDE_DEPTH] > 0) {     /* the tide, a firefly over the band */
                float fx = wx + ww * vo_tide_ph, fy = wy + wh + 30 - vo_tide * 30;
                bk_disc(fx, fy, 10, BK_ALPHA(col, 50));
                bk_disc(fx, fy, 4, BK_INK);
            }
        }
        if ((int)lab_v[P_VO_MODE] == 2) {        /* the vowel's window */
            float a = wx + ww * (float)vo_v[0].a / (float)vf->frames, b = wx + ww * (float)vo_v[0].b / (float)vf->frames;
            bk_rframe((int)a - 2, (int)(wy - 12), (int)(b - a) + 4, (int)wh + 24, 3, 2, BK_INK);
        }
        {
            float e = vo_env;
            bk_disc((float)(x + w / 2), (float)(y + h - 150), 14 + 40 * e, BK_ALPHA(col, 60 + (uint32_t)(150 * e)));
        }
        bk_text_up(x + 20, y + h - 60, &FONT_T, "FILES: SAMPLES/VOICES/ AND SAMPLES/ARCHIVE/", w - 40, BK_FAINT);
        bk_lab_scope(x + 20, y + h - 36, w - 40, 26, BK_ALPHA(col, 160));
        break;
    }
    case M_SEEDS: {
        int media = (int)lab_v[P_BE_MEDIA] & 3, frozen = lab_v[P_BE_FREEZE] > 0.5f, mono = lab_v[P_BE_MONO] > 0.5f;
        float mem = LAB_BE_MEMORY[media] * (mono ? 2.0f : 1.0f), fill = be_filled / mem, e = be_level * 8;
        float bx = (float)(x + 20), bw = (float)(w - 40), by = (float)(y + 120), tpos = lab_v[P_BE_TIME] / 100;
        fill = fill > 1 ? 1 : fill;
        e = e > 1 ? 1 : e;
        bk_text(x + 20, y + 16, &FONT_B, 1, LAB_BE_MEDIA[media], col);
        snprintf(buf, sizeof buf, "MEMORY %.1f S %s", mem, mono ? "(MONO)" : "(STEREO)");
        bk_text_up(x + 20, y + 44, &FONT_T, buf, w - 40, BK_DIM);
        bk_rrect((int)bx, (int)by, (int)bw, 50, 10, ARGB(24, 255, 255, 255));
        if (fill > 0)                                 /* the memory filling from the right (now) to the left */
            bk_rrect((int)(bx + bw * (1 - fill)), (int)by, (int)(bw * fill), 50, 10, BK_ALPHA(col, frozen ? 140 : 70));
        bk_rect((int)(bx + bw * (1 - tpos) - 2), (int)by - 8, 4, 66, col);   /* TIME: how far back the seeds land */
        if (fill < 1 && tpos > fill)
            bk_text_up(x + 20, (int)by + 60, &FONT_T, "FILLING: SEEDS SOUND ONCE THE MEMORY REACHES TIME", w - 40, BK_DIM);
        for (i = 0; i < 30; i++) {                   /* seeds drifting from the marker, brighter with the output */
            float a = (float)i * 0.618f, ph = fmodf(t * (0.15f + 0.015f * (float)i) + (float)i * 0.11f, 1.0f);
            float sx = bx + bw * (1 - tpos) + lab_sin(a + t * 0.05f) * 120 * ph;
            float sy = by + 140 + ph * (float)(h - 330) + lab_sin(a * 3) * 20;
            bk_disc(sx, sy, 2 + 6 * e * (1 - ph), BK_ALPHA(col, (uint32_t)((1 - ph) * (50 + 200 * e))));
        }
        if (frozen)
            bk_rframe(x + 6, y + 6, w - 12, h - 12, 12, 3, col);
        snprintf(buf, sizeof buf, "PITCH %+d   %s", (int)lab_v[P_BE_PITCH], lab_v[P_BE_SIZE] >= 99 ? "DELAY MODE" : frozen ? "FROZEN" : "");
        bk_text(x + 20, y + h - 86, &FONT_S, 1, buf, BK_DIM);
        bk_text_up(x + 20, y + h - 60, &FONT_T, "MUTABLE INSTRUMENTS BEADS", w - 40, BK_FAINT);
        bk_lab_scope(x + 20, y + h - 36, w - 40, 26, BK_ALPHA(col, 160));
        break;
    }
    case M_CROSSMOD: {
        static const char *const algos[7] = {"CROSSFADE", "FOLD", "DIODE", "RING", "XOR", "COMPARE", "VOCODER"};
        float cx = (float)(x + w / 2), cy = (float)(y + 230), a = lab_v[P_XM_ALGO] / 100;
        int ai = a >= 0.72f ? 6 : (int)(a / 0.72f * 6.0f);
        for (i = 0; i < 2; i++) {                    /* the two inputs */
            float lv = xm_in_level[i] * 6;
            lv = lv > 1 ? 1 : lv;
            bk_text_up(x + 20, y + 16 + i * 30, &FONT_T, i ? "MODULATOR" : "CARRIER", w - 40, BK_DIM);
            lab_format(i ? P_XM_MOD : P_XM_CARRIER, buf, sizeof buf);
            bk_text(x + 100, y + 16 + i * 30, &FONT_T, 1, buf, BK_INK);
            bk_rrect(x + 250, y + 18 + i * 30, 80, 10, 4, ARGB(30, 255, 255, 255));
            bk_rrect(x + 250, y + 18 + i * 30, (int)(80 * lv), 10, 4, col);
        }
        bk_arc(cx, cy, 110, 10, 0, 6.3f, ARGB(26, 255, 255, 255));
        bk_arc(cx, cy, 110, 10, 0, 0.1f + a * 6.1f, BK_ALPHA(col, 200));
        bk_disc(cx, cy, 20 + 60 * (xm_level * 5 > 1 ? 1 : xm_level * 5), BK_ALPHA(col, 90));
        bk_text_c((int)cx, (int)cy - 10, &FONT_B, 1, algos[ai], BK_INK);
        bk_text_up(x + 20, y + h - 60, &FONT_T, "MUTABLE INSTRUMENTS WARPS", w - 40, BK_FAINT);
        bk_lab_scope(x + 20, y + h - 36, w - 40, 26, BK_ALPHA(col, 160));
        break;
    }
    case M_RANDOM: {
        int len = (int)lab_v[P_RN_LENGTH], lo = 127, hi = 0;
        float sx = (float)(x + 30), sw = (float)(w - 60), sy = (float)(y + 80), sh = (float)(h - 240);
        for (i = 0; i < RN_HIST; i++)
            if (rn_hist[i] >= 0 && rn_hist_on[i]) {
                if (rn_hist[i] < lo) lo = rn_hist[i];
                if (rn_hist[i] > hi) hi = rn_hist[i];
            }
        if (hi < lo) { lo = 48; hi = 72; }
        if (hi - lo < 12) hi = lo + 12;
        for (i = 0; i <= 4; i++)
            bk_line(sx, sy + sh * (float)i / 4, sx + sw, sy + sh * (float)i / 4, 1, ARGB(24, 255, 255, 255));
        for (i = 0; i < RN_HIST; i++) {              /* the last sixteen steps as marbles, newest at right */
            int idx = (rn_hist_w + i) % RN_HIST, nt = rn_hist[idx];
            float mx = sx + sw * ((float)i + 0.5f) / RN_HIST;
            if (rn_hist_w + i < RN_HIST)
                continue;
            if (nt < 0 || !rn_hist_on[idx]) {
                bk_disc(mx, sy + sh + 20, 3, ARGB(60, 255, 255, 255));
                continue;
            }
            float my = sy + sh - sh * (float)(nt - lo) / (float)(hi - lo);
            bk_disc(mx, my, i == RN_HIST - 1 ? 13.0f : 9.0f, BK_ALPHA(bk_track_col((int)lab_v[P_RN_TRACK]), i == RN_HIST - 1 ? 255 : 150));
            bk_disc(mx - 3, my - 3, 3, ARGB(160, 255, 255, 255));
        }
        for (i = 0; i < len; i++)                    /* the deja vu loop's length */
            bk_disc((float)(x + 30 + i * 18), (float)(y + h - 96), 4, (int)(rn_step_count % (uint32_t)len) == i ? BK_INK : BK_ALPHA(col, 120));
        snprintf(buf, sizeof buf, "%s  %s   DEJA VU %d %%", lab_v[P_RN_ON] > 0.5f ? (md_playing() ? "PLAYING" : "WAITS FOR PLAY") : "OFF",
                 LAB_RATES[(int)lab_v[P_RN_RATE] % 6], (int)lab_v[P_RN_DEJAVU]);
        bk_text(x + 20, y + 20, &FONT_S, 1, buf, BK_DIM);
        bk_text_up(x + 20, y + h - 60, &FONT_T, "MUTABLE INSTRUMENTS MARBLES", w - 40, BK_FAINT);
        break;
    }
    case M_COLOUR:
    case M_MASTER: {
        float pk = lab_peak > 1 ? 1 : lab_peak;
        bk_lab_scope(x + 20, y + 40, w - 40, m == M_MASTER ? h / 2 - 70 : h / 2 - 40, BK_ALPHA(col, 220));
        if (m == M_COLOUR) {
            for (i = 0; i < 3; i++) {                  /* the ensemble's three voices */
                float a = en_lfo + (float)i / 3.0f;
                bk_disc((float)(x + w / 2) + lab_sin(a) * 70, (float)(y + h / 2 + 90) - lab_sin(a + 0.25f) * 30,
                        6 + 10 * lab_v[P_ENS_MIX] / 100, BK_ALPHA(col, 200));
            }
            snprintf(buf, sizeof buf, "TAPE %d  WOW %d  AGE %d", (int)lab_v[P_TAPE_DRIVE], (int)lab_v[P_TAPE_WOW], (int)lab_v[P_TAPE_AGE]);
            bk_text(x + 20, y + h - 40, &FONT_S, 1, buf, BK_DIM);
        } else {
            /* the chain at a glance: the EQ's curve (30 Hz to 16 kHz, +-12 dB), then three meters: what comes out,
               what the glue compressor and what the limiter take away */
            int mx = x + 20, my = y + h / 2 - 10, mw = w - 40, eh = 110;
            float px = 0, py = 0;
            uint32_t warm = ARGB(255, 240, 190, 110);
            bk_text(mx, my, &FONT_T, 1, "EQ", BK_DIM);
            bk_rrect(mx, my + 18, mw, eh, 8, ARGB(26, 255, 255, 255));
            bk_rect(mx + 6, my + 18 + eh / 2, mw - 12, 1, ARGB(50, 255, 255, 255));
            for (i = 0; i <= 48; i++) {
                float hz = 30.0f * powf(16000.0f / 30.0f, (float)i / 48.0f), db = ms_eq_db(hz);
                float cx = (float)mx + 6 + (float)(mw - 12) * (float)i / 48.0f;
                float cy = (float)(my + 18 + eh / 2) - fmaxf(-12, fminf(12, db)) / 12.0f * (float)(eh / 2 - 8);
                if (i)
                    bk_line(px, py, cx, cy, 2.4f, col);
                px = cx, py = cy;
            }
            my += eh + 30;
            bk_text(mx, my, &FONT_T, 1, "OUTPUT", BK_DIM);
            bk_rrect(mx, my + 18, mw, 14, 6, ARGB(30, 255, 255, 255));
            bk_rrect(mx, my + 18, (int)((float)mw * pk), 14, 6, pk > 0.95f ? ARGB(255, 240, 110, 90) : col);
            bk_text(mx, my + 42, &FONT_T, 1, "GLUE", BK_DIM);
            bk_rrect(mx, my + 60, mw, 14, 6, ARGB(30, 255, 255, 255));
            if (comp_gr_db > 0.05f)
                bk_rrect(mx + mw - (int)((float)mw * fminf(comp_gr_db / 12, 1)), my + 60, (int)((float)mw * fminf(comp_gr_db / 12, 1)), 14, 6, warm);
            bk_text(mx, my + 84, &FONT_T, 1, "LIMITER", BK_DIM);
            bk_rrect(mx, my + 102, mw, 14, 6, ARGB(30, 255, 255, 255));
            if (lim_gr_db > 0.05f)
                bk_rrect(mx + mw - (int)((float)mw * fminf(lim_gr_db / 12, 1)), my + 102, (int)((float)mw * fminf(lim_gr_db / 12, 1)), 14, 6, warm);
            snprintf(buf, sizeof buf, "PEAK %.1f  GLUE -%.1f  LIMIT -%.1f DB", 20 * log10f(lab_peak > 1e-5f ? lab_peak : 1e-5f),
                     comp_gr_db, lim_gr_db);
            bk_text(mx, my + 126, &FONT_S, 1, buf, BK_DIM);
        }
        break;
    }
    }
}

static int lab_list_top;
static void bk_garden_frame(uint32_t col);       /* brick_views_draw.inc */

/* a value as a vine instead of a bar: a faint tendril the whole way, the value's part grown thick from where it
   starts (0, or the middle for a value either side of it) with a bud at its end; the chosen row has leaves and
   sways a little more */
static void bk_vine_bar(int x, int y, int w, float from, float to, uint32_t col, int sel, float t, int seed)
{
    float a = fminf(from, to), b = fmaxf(from, to), px = (float)x, py = (float)y, sw = sel ? 2.2f : 1.2f;
    int n = 12, k;
    for (k = 1; k <= n; k++) {
        float u = (float)k / (float)n, u0 = (float)(k - 1) / (float)n;
        float nx = (float)x + (float)w * u, ny = (float)y + sinf(u * 7.0f + t * 1.1f + (float)seed) * sw;
        int lit = u0 >= a - 0.001f && u <= b + 0.001f && b > a;
        if (lit)
            bk_line(px, py, nx, ny, sel ? 3.6f : 3.0f, col);
        else
            bk_line(px, py, nx, ny, 1.3f, ARGB(40, 255, 255, 255));
        if (lit && sel && (k & 1)) {               /* leaves on the chosen one */
            float sd = (k & 2) ? 1.0f : -1.0f;
            bk_line(nx - 6.0f, ny, nx - 1.0f, ny - sd * 7.0f, 2.6f, BK_ALPHA(col, 170));
        }
        px = nx, py = ny;
    }
    if (b > a || to > 0) {                          /* the bud at the value */
        float ex = (float)x + (float)w * to, ey = (float)y + sinf(to * 7.0f + t * 1.1f + (float)seed) * sw;
        if (sel)
            bk_disc(ex, ey, 9.0f + sinf(t * 3.0f), BK_ALPHA(col, 50));
        bk_disc(ex, ey, sel ? 5.0f : 3.6f, col);
        bk_disc(ex - 1.0f, ey - 1.0f, sel ? 2.0f : 1.4f, BK_ALPHA(BK_INK, 200));
    }
}

#include "brick_lab_about.inc"

/* (1.2) the chosen parameter's card: its name and what it does, for a few seconds after the choice, over the
   picture's floor; then it fades and the picture is whole again. Two lines at most, broken at a space. */
static void bk_lab_about_card(int p, int x, int y, int w, uint32_t col)
{
    static int last = -1;
    static uint32_t at;
    uint32_t now = bk_ms(), age;
    const char *s = lab_about(p);
    char a[160], b[160];
    float al;
    int n, cut, lines;
    if (p != last)
        last = p, at = now;
    age = now - at;
    if (!s || age > 4600u)
        return;
    al = age < 180u ? (float)age / 180.0f : age > 4000u ? 1.0f - (float)(age - 4000u) / 600.0f : 1.0f;
    s = bk_tr(s);
    n = (int)strlen(s);
    snprintf(a, sizeof a, "%s", s), b[0] = 0;
    if (bk_tw(&FONT_T, s) > w - 28)              /* (the last space that leaves the first line fitting) */
        for (cut = n - 1; cut > 0; cut--)
            if (s[cut] == ' ') {
                snprintf(a, sizeof a, "%.*s", cut, s);
                if (bk_tw(&FONT_T, a) <= w - 28) {
                    snprintf(b, sizeof b, "%s", s + cut + 1);
                    break;
                }
            }
    lines = b[0] ? 2 : 1;
    {
        int ch = 30 + lines * 18, cy = y - ch, lift = (int)((1.0f - al) * 8.0f);
        bk_rrect(x + 3, cy + 4 + lift, w, ch, 14, ARGB((uint32_t)(120 * al), 0, 0, 0));
        bk_rrect(x, cy + lift, w, ch, 14, ARGB((uint32_t)(245 * al), 16, 24, 34));
        bk_rrect(x, cy + lift, 4, ch, 2, BK_ALPHA(col, (uint32_t)(255 * al)));
        bk_text_fit(x + 14, cy + 6 + lift, &FONT_T, LAB_P[p].name, w - 28, BK_ALPHA(col, (uint32_t)(255 * al)));
        bk_tr_keys = 1;                          /* (already in its language) */
        bk_text_fit(x + 14, cy + 26 + lift, &FONT_T, a, w - 28, BK_ALPHA(BK_INK, (uint32_t)(255 * al)));
        if (b[0])
            bk_text_fit(x + 14, cy + 44 + lift, &FONT_T, b, w - 28, BK_ALPHA(BK_INK, (uint32_t)(255 * al)));
        bk_tr_keys = 0;
    }
}

static void bk_draw_lab(float t)
{
    int x = BK_SX, y = BK_SY, i, ps[P_COUNT], np = lab_params(lab_mod, ps), foc = clamp(lab_focus[lab_mod], 0, np - 1);
    uint32_t col = LAB_MOD_COL[lab_mod];
    char buf[32];
    bk_garden_frame(col);                            /* the garden's night behind the Lab too */
    {   /* where we are among the modules: the one before (L1) and after (R1), this one large; under them a row of
           seeds, the sources then the effects, the sounding ones in colour */
        int pv = (lab_mod + M_COUNT - 1) % M_COUNT, nx = (lab_mod + 1) % M_COUNT, cw2;
        float bob = 2.0f * sinf(t * 3.0f);
        bk_rrect(x + 8, y + 6, 26, 24, 12, ARGB(230, 232, 236, 222));
        bk_text_c(x + 21, y + 8, &FONT_T, 1, "L1", ARGB(255, 14, 20, 28));
        bk_text_fit(x + 40, y + 8, &FONT_H, LAB_MOD_NAMES[pv], 150, BK_DIM);
        bk_rrect(x + 686, y + 6, 26, 24, 12, ARGB(230, 232, 236, 222));
        bk_text_c(x + 699, y + 8, &FONT_T, 1, "R1", ARGB(255, 14, 20, 28));
        cw2 = bk_text_w(&FONT_H, LAB_MOD_NAMES[nx], 1);
        bk_text(x + 680 - (cw2 > 150 ? 150 : cw2), y + 8, &FONT_H, 1, LAB_MOD_NAMES[nx], BK_DIM);
        gd_seedling((float)(x + 360 - bk_text_w(&FONT_M, LAB_MOD_NAMES[lab_mod], 1) / 2 - 18), (float)(y + 20) + bob, col);
        bk_text_c(x + 360, y + 2, &FONT_M, 1, LAB_MOD_NAMES[lab_mod], col);
        if (bk_lab_hushed()) {                   /* (1.3) silenced with X: a pill says so, and how it comes back */
            int px = x + 360 + bk_text_w(&FONT_M, LAB_MOD_NAMES[lab_mod], 1) / 2 + 14, pw = bk_text_w(&FONT_T, "SILENT", 1) + 44;
            bk_rrect(px, y + 8, pw, 22, 11, BK_ALPHA(col, 200));
            bk_text(px + 10, y + 11, &FONT_T, 1, "SILENT", ARGB(255, 12, 18, 26));
            bk_tr_keys = 1;
            bk_text(px + pw - 22, y + 11, &FONT_T, 1, bk_key_label("X"), ARGB(255, 12, 18, 26));
            bk_tr_keys = 0;
        }
        for (i = 0; i < M_COUNT; i++) {
            float sx = (float)(x + 360 - (M_COUNT * 22 + 14) / 2 + i * 22 + (i >= M_SOURCES ? 14 : 0) + 11), sy = (float)(y + 50);
            int on = i != M_SENDS && lab_active(i), cur = i == lab_mod;
            if (cur)
                bk_disc(sx, sy, 9, BK_ALPHA(LAB_MOD_COL[i], 70));
            bk_disc(sx, sy, cur ? 6.0f : on ? 4.5f : 3.0f, cur || on ? LAB_MOD_COL[i] : ARGB(70, 255, 255, 255));
        }
        bk_text(x + 8, y + 42, &FONT_T, 1, lab_mod < M_SOURCES ? "SOURCE" : "EFFECT", BK_FAINT);
    }
    {   /* fourteen rows fit; a longer list scrolls with the focus */
        static int top[M_COUNT];
        if (foc < top[lab_mod]) top[lab_mod] = foc;
        if (foc > top[lab_mod] + 13) top[lab_mod] = foc - 13;
        if (top[lab_mod] > np - 14) top[lab_mod] = np - 14 > 0 ? np - 14 : 0;
        lab_list_top = top[lab_mod];
        if (lab_list_top > 0)
            bk_text_c(x + 178, y + 60, &FONT_T, 1, "...", BK_DIM);
        if (lab_list_top + 14 < np)
            bk_text_c(x + 178, y + 704, &FONT_T, 1, "...", BK_DIM);
    }
    /* the chosen row's frame glides to it, and its value's vine follows the value: eased by the clock, so it moves
       the same at any frame rate (0.62; before, both jumped) */
    static float glide_y = -1, glide_f;
    static int glide_p = -1, glide_mod = -1;
    static uint32_t glide_at;
    float ease;
    {
        uint32_t nowm = bk_ms();
        float dts = glide_at ? (float)(nowm - glide_at) / 1000.0f : 1.0f;
        glide_at = nowm;
        ease = 1.0f - expf(-(dts > 0.25f ? 0.25f : dts) * 22.0f);
        float ty = (float)(y + 72 + (foc - lab_list_top) * 45);
        if (glide_y < 0 || glide_mod != lab_mod)
            glide_y = ty, glide_mod = lab_mod;
        else
            glide_y += (ty - glide_y) * ease;
        bk_rrect(x + 8, (int)lrintf(glide_y) - 4, 340, 44, 10, BK_ALPHA(col, 40));
        bk_rframe(x + 8, (int)lrintf(glide_y) - 4, 340, 44, 10, 2, col);
    }
    for (i = lab_list_top; i < np && i < lab_list_top + 14; i++) {
        int p = ps[i], ry = y + 72 + (i - lab_list_top) * 45, sel = i == foc;
        float f = (lab_v[p] - LAB_P[p].lo) / (LAB_P[p].hi - LAB_P[p].lo);
        if (sel) {
            if (glide_p != p)
                glide_p = p, glide_f = f;
            else
                glide_f += (f - glide_f) * ease;
            f = glide_f;
        }
        if ((p >= P_SC1 && p <= P_SS6) || (p >= P_SP1 && p <= P_SP6) || (p >= P_SB1 && p <= P_SB6)) {   /* a send: by its track's model */
            int tr = p >= P_SB1 ? p - P_SB1 : p >= P_SP1 ? p - P_SP1 : (p - P_SC1) % 6;
            char nm[48];
            snprintf(nm, sizeof nm, "%d %s > %s", tr + 1, md_model_name(md_state()->track[tr].model),
                     p >= P_SB1 ? "SEEDS" : p >= P_SP1 ? "PARTICLES" : p < P_SS1 ? "CLOUD" : "SHIMMER");
            lab_format(p, buf, sizeof buf);
            bk_text_fit(x + 20, ry + 2, &FONT_S, nm, 300 - bk_text_w(&FONT_S, buf, 1), sel ? BK_INK : BK_DIM);
        } else {
            lab_format(p, buf, sizeof buf);
            if (bk_text_w(&FONT_S, buf, 1) > 200)   /* (a long value: the name gives way) */
                bk_text_fit(x + 20, ry + 2, &FONT_T, LAB_P[p].name, 100, sel ? BK_INK : BK_DIM);
            else
                bk_text_fit(x + 20, ry + 2, &FONT_S, LAB_P[p].name, 300 - bk_text_w(&FONT_S, buf, 1), sel ? BK_INK : BK_DIM);
        }
        {
            int vw = bk_text_w(&FONT_S, buf, 1);
            if (vw > 210 && bk_text_w(&FONT_T, buf, 1) <= 210)   /* (a long value: smaller before shorter) */
                bk_text(x + 336 - bk_text_w(&FONT_T, buf, 1), ry + 4, &FONT_T, 1, buf, lab_v[p] != LAB_P[p].def ? BK_INK : BK_DIM);
            else if (vw > 210)
                bk_text_fit(x + 126, ry + 4, &FONT_T, buf, 210, lab_v[p] != LAB_P[p].def ? BK_INK : BK_DIM);
            else
                bk_text(x + 336 - vw, ry + 2, &FONT_S, 1, buf, lab_v[p] != LAB_P[p].def ? BK_INK : BK_DIM);
        }
        if (LAB_P[p].unit == U_BIPCT || LAB_P[p].unit == U_SEMI) {
            float z = (0 - LAB_P[p].lo) / (LAB_P[p].hi - LAB_P[p].lo);
            bk_vine_bar(x + 20, ry + 28, 316, z, f, col, sel, t, i);
        } else
            bk_vine_bar(x + 20, ry + 28, 316, 0, f, col, sel, t, i);
        if (sel && bk_kick > 0.05f) {            /* a turn: sparks fly from the value's end */
            float ex = (float)(x + 20) + 316.0f * f, ey = (float)(ry + 28), kk = fminf(bk_kick, 1.0f);
            bk_disc(ex, ey, 6.0f + 10.0f * kk, BK_ALPHA(col, (uint32_t)(90 * kk)));
            for (int sp = 0; sp < 7; sp++) {
                float a = (float)sp * 0.9f + (float)bk_turns * 0.7f, d = (1.0f - kk) * 34.0f + 6.0f;
                bk_disc(ex + cosf(a) * d, ey + sinf(a) * d * 0.7f, 2.5f * kk + 0.5f, BK_ALPHA(BK_INK, (uint32_t)(220 * kk)));
            }
        }
    }
    {   /* a module change: the new picture sliding in from the side it came from, a veil lifting off it */
        static int last = -1, dir = 1;
        static uint32_t at;
        uint32_t now = bk_ms();
        float u;
        if (lab_mod != last) {
            dir = last < 0 || lab_mod > last ? 1 : -1;
            last = lab_mod, at = now;
        }
        u = fminf((float)(now - at) / 260.0f, 1.0f);
        u = 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u);
        bk_lab_viz(lab_mod, x + 362 + (int)((1.0f - u) * 28.0f * (float)dir), y + 66, 350, 644, t);
        if (u < 1.0f) {
            bk_rrect(x + 362, y + 66, 350, 644, 14, ARGB((uint32_t)(230 * (1.0f - u)), 10, 14, 24));
            bk_rect(x + 362 + (int)(u * 350.0f), y + 66, 2, 644, BK_ALPHA(LAB_MOD_COL[lab_mod], (uint32_t)(200 * (1.0f - u))));
        }
        if (np > 0)
            bk_lab_about_card(ps[foc], x + 372, y + 66 + 644 - 44, 330, LAB_MOD_COL[lab_mod]);
    }
}

#include "brick_views_draw.inc"
#include "brick_splash.inc"
#include "brick_garden.inc"
#include "brick_help.inc"
#include "brick_guide.inc"
#include "brick_options.inc"

/* (1.3) the notice card (bk_notice): low over the stage, 1.6 s, or 3.2 s with a second line; it lifts in, fades */
static void bk_draw_notice(void)
{
    uint32_t now = plat_ms(), age = now - bk_nt_at, life = bk_nt_b[0] ? 3200u : 1600u;
    float a;
    int wa, wb, w, h, x, y;
    static int last_screen = -1;
    int screen = bk_view | lab_open << 6 | bk_menu_open << 7 | bk_help_open << 8 | bk_guide_open << 9 | bk_opt_open << 10 |
                 bk_quick_open << 11 | bk_journal_on << 12;
    if (screen != last_screen) {                 /* another screen: what was said about the last one goes (a notice
                                                    made by the change itself, within 250 ms, stays) */
        if (last_screen >= 0 && age > 250u)
            bk_nt_at = 0;
        last_screen = screen;
    }
    if (!bk_nt_at || age >= life || !bk_nt_a[0])
        return;
    a = age < 160u ? (float)age / 160.0f : age > life - 400u ? (float)(life - age) / 400.0f : 1.0f;
    wa = bk_text_w(&FONT_B, bk_nt_a, 1), wb = bk_nt_b[0] ? bk_text_w(&FONT_T, bk_nt_b, 1) : 0;
    w = (wa > wb ? wa : wb) + 48;
    w = w > 700 ? 700 : w;
    h = bk_nt_b[0] ? 70 : 46;
    x = BK_SX + 360 - w / 2, y = BK_H - 142 - h + (int)((1.0f - a) * 10.0f);   /* (above the value's toast) */
    bk_rrect(x + 3, y + 5, w, h, 18, ARGB((uint32_t)(130 * a), 0, 0, 0));
    bk_rrect(x, y, w, h, 18, ARGB((uint32_t)(240 * a), 236, 232, 214));
    bk_text_c(BK_SX + 360, y + 9, &FONT_B, 1, bk_nt_a, ARGB((uint32_t)(255 * a), 18, 22, 32));
    if (bk_nt_b[0])
        bk_text_c(BK_SX + 360, y + 42, &FONT_T, 1, bk_nt_b, ARGB((uint32_t)(255 * a), 70, 76, 90));
}
/* (1.5) the Sound Lab's module map (brick_core.c lab_map_*): every module a tile - its colour, its name, what it does
   now - the sources above, the effects below; the chosen one lifted and glowing, the sounding ones breathing */
static void bk_draw_lab_map(float t)
{
    static const int main_p[M_COUNT] = {-1, P_SA_LEVEL, P_VO_LEVEL, P_PO_LEVEL, P_SY_ENGINE, P_RN_RATE, P_LG_RULE, P_GR_X, P_RA_RES_MIX, P_RB_RES_MIX,
                                        P_CL_MIX, P_PA_MIX, P_BE_MIX, P_SH_MIX, P_XM_MIX, -1, P_ENS_MIX, P_GAIN};
    static float gx = -1, gy;
    const int tw = 168, th = 96, gap = 14, x0 = (BK_W - (LAB_MAP_COLS * tw + (LAB_MAP_COLS - 1) * gap)) / 2;
    static const int rowy[4] = {112, 222, 386, 496};
    char buf[48];
    bk_rect(0, 0, BK_W, BK_H, ARGB(252, 4, 7, 14));
    gd_seedling(30, 30, LAB_MOD_COL[lab_map_sel]);
    bk_text(50, 14, &FONT_M, 1, "Sound Lab: all modules", BK_INK);
    bk_text(x0, rowy[0] - 26, BK_TINY, 1, "SOURCES", BK_FAINT);
    bk_text(x0, rowy[2] - 26, BK_TINY, 1, "EFFECTS AND THE MIX", BK_FAINT);
    for (int m = 0; m < M_COUNT; m++) {
        int col, row = lab_map_rowcol(m, &col), x = x0 + col * (tw + gap), y = rowy[row], sel = m == lab_map_sel;
        int on = m != M_SENDS && lab_active(m), here = m == lab_mod;
        uint32_t mc = LAB_MOD_COL[m];
        if (sel) {
            float tx = (float)x, ty = (float)y;
            if (gx < 0)
                gx = tx, gy = ty;
            gx += (tx - gx) * bk_glide(14.0f), gy += (ty - gy) * bk_glide(14.0f);
            bk_rrect((int)gx - 6, (int)gy - 6, tw + 12, th + 12, 20, BK_ALPHA(mc, (uint32_t)(50 + 30 * sinf(t * 3.0f))));
        }
        bk_rrect(x, y - (sel ? 3 : 0), tw, th, 16, sel ? ARGB(255, 26, 34, 46) : ARGB(255, 14, 20, 30));
        if (sel)
            bk_rframe(x, y - 3, tw, th, 16, 2, mc);
        bk_disc((float)(x + 22), (float)(y + 24 - (sel ? 3 : 0)), on ? 8.0f + 2.0f * sinf(t * 2.5f + (float)m) : 6.0f, on ? mc : ARGB(90, 255, 255, 255));
        bk_text_fit(x + 38, y + 12 - (sel ? 3 : 0), bk_text_w(&FONT_S, LAB_MOD_NAMES[m], 1) <= tw - 46 ? &FONT_S : &FONT_H,
                    LAB_MOD_NAMES[m], tw - 46, sel || on ? BK_INK : BK_DIM);   /* (a long name: smaller, not cut) */
        if (main_p[m] >= 0 && (on || m == M_SYNTH || m == M_LOGIC))
            lab_format(main_p[m], buf, sizeof buf);
        else
            snprintf(buf, sizeof buf, "%s", m == M_SENDS ? "WHERE IT GOES" : on ? "ON" : "SILENT");
        bk_text_fit(x + 14, y + 50 - (sel ? 3 : 0), bk_text_w(BK_TINY, buf, 1) <= tw - 28 ? BK_TINY : &FONT_T, buf, tw - 28,
                    on ? BK_ALPHA(mc, 240) : BK_FAINT);
        if (here) {                              /* (where the Lab is now) */
            int hw = bk_text_w(&FONT_T, "HERE", 1) + 14;
            bk_rrect(x + tw - hw - 10, y + th - 28 - (sel ? 3 : 0), hw, 20, 10, ARGB(60, 255, 255, 255));
            bk_text(x + tw - hw - 3, y + th - 26 - (sel ? 3 : 0), &FONT_T, 1, "HERE", BK_INK);
        }
    }
    {
        char ka[16], kb[16], line[160];
        snprintf(ka, sizeof ka, "%s", bk_key_label("A"));
        snprintf(kb, sizeof kb, "%s", bk_key_label("B"));
        snprintf(line, sizeof line, T("D-PAD: CHOOSE   %s: GO THERE   %s: CLOSE"), ka, kb);
        bk_text_c(BK_W / 2, BK_H - 60, BK_TINY, 1, line, BK_DIM);
    }
}
static void st_draw_card(void);
static void bk_draw_guide(float t);
static void bk_draw_options(float t);
const uint32_t *bk_draw(void)
{
    const md_stats *st = md_status();
    uint32_t now = bk_ms(), dt = bk_last_draw ? now - bk_last_draw : 16u;
    float t = (float)now / 1000.0f, k = dt > 200u ? 1.0f : (float)dt / 200.0f, energy = 0;
    int i, side_keep = 0, side_skip = 0;
    bk_last_draw = now;
    bk_frame_s = (float)(dt > 250u ? 250u : dt) / 1000.0f;
    if (fm1t_hires != (bk_garden != 0)) {        /* the FM-1 page's words: sharp from the stage, or its own pixels */
        fm1t_hires = bk_garden != 0;
        display_repaint = 1;
        dream_ui.dirty = 1;
    }
    if (bk_splash_frame())                       /* the start screen, alone */
        return bk_px;
    if (bk_resting) {                            /* (1.4) after the sleep timer */
        bk_draw_resting();
        return bk_px;
    }
    for (i = 0; i < 6; i++) {
        /* level in dB: -48 dBFS .. -6 dBFS fills the plant */
        float pk = st->track_peak[i], target = pk > 1e-5f ? (20.0f * log10f(pk) + 48.0f) / 42.0f : 0.0f;
        target = target > 1 ? 1 : target < 0 ? 0 : target;
        bk_level[i] += (target - bk_level[i]) * (target > bk_level[i] ? 0.6f : k * 0.6f);
        if (st->events[i] != bk_last_events[i]) {
            bk_last_events[i] = st->events[i];
            bk_bloom[i] = 1.0f;
        } else
            bk_bloom[i] *= 1.0f - k * 0.9f;
        bk_sway[i] = sinf(t * (0.6f + 0.15f * (float)i) + (float)i) * (0.3f + bk_level[i]);
        energy += bk_level[i] / 6.0f;
    }
    if (bk_turns != bk_kick_seen) {              /* a turn: a kick, more the faster one turns */
        bk_kick_seen = bk_turns;
        bk_kick = fminf(bk_kick + 0.5f, 1.6f);
    }
    bk_kick *= 1.0f - k * 0.55f;
    for (i = 0; i < 6; i++) {                    /* the plants shiver with it, the chosen track's most */
        float w = i == dream_ui.track ? 1.6f : 0.7f;
        bk_sway[i] += bk_kick * w * (sinf(t * 11.0f + (float)i * 1.3f) * 0.8f + 0.6f * (float)bk_turn_dir);
    }
    if (bk_kick > 0.05f && bk_bloom[dream_ui.track] < bk_kick * 0.7f)
        bk_bloom[dream_ui.track] = bk_kick * 0.7f;
    {   /* (1.5.8) the columns at half pace: quiet, and the last frame was whole and kept them */
        static unsigned n;
        static uint32_t seen_input;
        int fresh = bk_input_at != seen_input;   /* (a button changed since the last frame: this one whole, at once) */
        int quiet = bk_side_half && bk_light && !bk_menu_open && !bk_help_open && !bk_guide_open && !bk_opt_open && !bk_quick_open &&
                    !(lab_open && lab_mapview) && bk_kick <= 0.03f && !(bk_iris_at && bk_ms() - bk_iris_at < 450u);
        if (quiet && !bk_side_px)
            bk_side_px = (uint32_t *)malloc((size_t)BK_W * BK_H * 4u), bk_side_ok = 0;
        side_keep = quiet && bk_side_px;
        seen_input = bk_input_at;
        side_skip = side_keep && bk_side_ok && !fresh && (++n & 1u);
        if (!quiet)
            bk_side_ok = 0;
    }
    if (side_skip) {                             /* the sky only behind the stage (the columns come back after it) */
        bk_clip_x0 = BK_IN_X0, bk_clip_x1 = BK_IN_X1, bk_bg_spores_only = 1;
        bk_background(t, energy);
        bk_clip_x0 = 0, bk_clip_x1 = BK_W, bk_bg_spores_only = 0;
        bk_side_dt += bk_frame_s;
        bk_draw_yield();
    } else {
        float fs = bk_frame_s;
        bk_background(t, energy);
        bk_draw_yield();
        bk_frame_s = fminf(fs + bk_side_dt, 0.25f);   /* (the columns' glides over both frames) */
        bk_draw_top();
        bk_draw_tracks(t);
        bk_frame_s = fs;
        bk_draw_yield();
    }
    if (lab_open)
        bk_draw_lab(t);
    else if (bk_view)
        bk_draw_view(t);
    else if (bk_garden_shown())
        bk_draw_garden(t);                       /* the Brick's own home: the garden */
    else if (bk_garden)
        bk_draw_screen_garden();                 /* an FM-1 page, in the garden's style */
    else
        bk_draw_screen();                        /* the FM-1's own pixels (MENU > HOME: FM-1 SCREEN) */
    if (side_skip)                               /* (the columns back over the stage's edges, as a whole frame has them) */
        bk_side_copy(0);
    {   /* a new place (a view, the Lab, the garden or a page, the sampler): it opens like an iris, a ring of leaves
           turning at its edge (opaque night outside: plain spans, cheap) */
        static uint32_t last_key = 0xFFFFFFFFu;
        uint32_t at = bk_iris_at;
        uint32_t key = (uint32_t)lab_open << 12 | (uint32_t)bk_view << 4 | (uint32_t)(bk_garden_shown() ? 1 : 0) << 2 | (uint32_t)ar_smp << 1;
        uint32_t now = bk_ms();
        if (key != last_key) {
            if (last_key != 0xFFFFFFFFu)
                at = now ? now : 1u;
            last_key = key;
        }
        bk_iris_at = at;
        if (at && now - at < 420u) {
            float u = (float)(now - at) / 420.0f, e = u * u * (3 - 2 * u), r = 18.0f + e * 540.0f;
            float cx = (float)(BK_SX + 360), cy = (float)(BK_SY + 360);
            uint32_t veil = ARGB(255, 6, 9, 16), lc = lab_open ? LAB_MOD_COL[lab_mod] : bk_track_col(dream_ui.track);
            for (int yy = 0; yy < 720; yy++) {
                float dy = (float)yy + 0.5f - 360.0f;
                int row = BK_SY + yy;
                if (fabsf(dy) >= r)
                    bk_span(BK_SX, BK_SX + 720, row, veil);
                else {
                    float hw = sqrtf(r * r - dy * dy);
                    int a0 = (int)(cx - hw), a1 = (int)(cx + hw);
                    if (a0 > BK_SX)
                        bk_span(BK_SX, a0, row, veil);
                    if (a1 < BK_SX + 720)
                        bk_span(a1, BK_SX + 720, row, veil);
                }
            }
            for (int l = 0; l < 28; l++) {        /* the leaves at the edge */
                float a = (float)l * 0.2244f + u * 2.0f, lx = cx + cosf(a) * r, ly = cy + sinf(a) * r;
                if (lx < BK_SX || lx > BK_SX + 720 || ly < BK_SY || ly > BK_SY + 720)
                    continue;
                bk_line(lx, ly, lx + cosf(a + 1.2f) * 12.0f, ly + sinf(a + 1.2f) * 12.0f, 4.0f, BK_ALPHA(lc, (uint32_t)(220 * (1 - u))));
                bk_disc(lx, ly, 2.5f, BK_ALPHA(BK_INK, (uint32_t)(200 * (1 - u))));
            }
        } else
            at = 0;
    }
    if (bk_kick > 0.03f && !bk_menu_open && !bk_help_open) {   /* the stage's frame breathes with the turn */
        uint32_t kc = lab_open ? LAB_MOD_COL[lab_mod] : bk_track_col(dream_ui.track);
        float kk = fminf(bk_kick, 1.0f);
        bk_rframe(BK_SX - 7, BK_SY - 7, 734, 734, 20, 3, BK_ALPHA(kc, (uint32_t)(200 * kk)));
        bk_rframe(BK_SX - 7 - (int)(10 * (1 - kk)), BK_SY - 7 - (int)(10 * (1 - kk)), 734 + (int)(20 * (1 - kk)), 734 + (int)(20 * (1 - kk)), 24, 2,
                  BK_ALPHA(kc, (uint32_t)(90 * kk)));
    }
    if (!side_skip) {
        float fs = bk_frame_s;
        bk_frame_s = fminf(fs + bk_side_dt, 0.25f);
        bk_draw_right(t);
        bk_frame_s = fs;
    }
    bk_draw_yield();
    if (!bk_menu_open && !bk_help_open && !bk_light && !bk_calm)
        bk_ambient(t, energy);
    bk_draw_toast();                             /* (toast, card and notice: all inside the stage's 720 x 720) */
    st_draw_card();                              /* DREAM TALES: a chapter's card (brick_toys_draw.inc) */
    bk_draw_notice();                            /* (1.3) what was just said, on every screen */
    if (bk_perf)
        bk_draw_perf();
    if (!side_skip) {
        bk_draw_bottom();
        if (side_keep)                           /* (a whole frame: its columns kept for the next) */
            bk_side_copy(1), bk_side_ok = 1;
        bk_side_dt = 0;
    }
    bk_draw_yield();
    if (lab_open && lab_mapview)
        bk_draw_lab_map(t);                      /* (1.5) the Sound Lab's module map */
    bk_big_ctx = 1;
    if (bk_menu_open)
        bk_draw_menu();
    if (bk_help_open)
        bk_draw_help(t);                         /* the button map, over everything */
    if (bk_guide_open)
        bk_draw_guide(t);                        /* THE GUIDE (0.73) */
    if (bk_opt_open)
        bk_draw_options(t);                      /* OPTIONS (1.1) */
    if (bk_journal_on)
        bk_draw_journal(t);                      /* THE GARDEN JOURNAL (1.6) */
    bk_big_ctx = 0;
    bk_splash_over();                            /* (its fade, over the stage) */
    bk_ground_frame_end();                       /* (1.3: grounds that changed are baked for the next frame) */
    bk_draw_gen++;                               /* (buffers let go of before this frame may now be freed) */
    return bk_px;
}
