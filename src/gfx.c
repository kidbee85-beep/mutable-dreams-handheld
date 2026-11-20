/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the FM-1's canvas (base/device/gfx.c, unchanged and included
 * whole), with its text listened to.
 *
 * plat_host.c includes "gfx.c"; the port's directory comes first in the include path, so this file is found and
 * brings in the original. After it, the FM-1's pages (app.c and the rest, which come later in the one translation
 * unit) call bk_cv_text / bk_cv_rect / bk_cv_begin in place of cv_text / cv_rect / cv_begin. While the stage asks
 * for it (fm1t_hires: the FM-1 pages in the garden's style), their words are not drawn into the 240 x 240 pixels
 * but kept (place, face, colour, words) and the stage sets them sharp, in faces three times the size
 * (brick_stage.inc bk_fm1_words). A box drawn later over words hides them, as it would on the pixels. */
#include "../../FM-1/Release-2.1/Source/base/device/gfx.c"

#define FM1T_MAX 192
typedef struct {
    int16_t x, y, seq, w, h;
    uint8_t face, hidden;
    uint16_t col;
    char s[46];
} fm1_word;
static fm1_word fm1t[FM1T_MAX];
static int fm1t_n, fm1t_last_oy = 1, fm1t_seq, fm1t_hires;
/* the FM-1's glass leaves (md_glass_leaf, 108 x 32: a value or a name under "K1 TONE"): known by their two edge
   lines (92 x 1 at x+12, y and at x+4, y+31); the stage draws its own card where they stand */
#define FM1L_MAX 32
typedef struct { int16_t x, y; uint16_t col; } fm1_leaf;   /* (an edge line) */
static fm1_leaf fm1l[FM1L_MAX];
static int fm1l_n;
static uint32_t fm1t_begins, fm1t_last_begins, fm1t_pass;
/* the FM-1's lines (cv_line: waves, envelopes, paths, frames): kept like its words and drawn by the stage at three
   times the size, smooth, instead of as stepped pixels; a hash finds the same line again in the next band */
#define FM1N_MAX 2048
#define FM1N_HASH 4096
typedef struct { int16_t x0, y0, x1, y1, seq; uint16_t col; uint8_t hidden; uint32_t band; } fm1_line;   /* band: the band it was last drawn in */
static fm1_line fm1n[FM1N_MAX];
static int fm1n_n;
static uint16_t fm1n_at[FM1N_HASH];              /* index + 1 into fm1n, valid while fm1n_gen[] is this pass */
static uint32_t fm1n_gen[FM1N_HASH];
/* its single points (cv_pset: curves plotted a column at a time, marks): kept too; the stage joins those that go on
   column by column into smooth curves. MD_EDGE's points stay pixels (the stage reads the leaves' sliders there) */
#define FM1P_MAX 4096
#define FM1P_HASH 8192
typedef struct { int16_t x, y, seq; uint16_t col; uint8_t hidden; uint32_t band; } fm1_point;
static fm1_point fm1p[FM1P_MAX];
static int fm1p_n;
static uint16_t fm1p_at[FM1P_HASH];
static uint32_t fm1p_gen[FM1P_HASH];
#define FM1P_KEEP_PIXEL RGB(106, 135, 145)           /* (app.c's MD_EDGE) */

static int fm1t_face(const felucca_font_t *f)
{
    return f == &FONT_T ? 0 : f == &FONT_XS ? 1 : f == &FONT_H ? 2 : f == &FONT_S ? 3 : f == &FONT_B ? 4 : f == &FONT_M ? 5 : f == &FONT_L ? 6 : -1;
}

/* a pass over the screen begins again: at its first band (offset 0, a new canvas) or when the bands go back up */
static void fm1t_track(void)
{
    int oy = -(int)cv_oy;
    if (fm1t_begins != fm1t_last_begins) {
        fm1t_seq = 0;                            /* (each band draws the same things in the same order) */
        if (oy == 0 || oy < fm1t_last_oy) {
            fm1t_n = 0, fm1l_n = 0, fm1n_n = 0, fm1p_n = 0;
            fm1t_pass++;
        }
        fm1t_last_begins = fm1t_begins;
    }
    fm1t_last_oy = oy;
}

static void bk_cv_begin(uint32_t w, uint32_t h, uint16_t bg)
{
    cv_begin(w, h, bg);
    fm1t_begins++;
}

static int32_t bk_cv_text(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    int face = fm1t_face(f);
    if (!fm1t_hires || face < 0 || cv_w != 240u)
        return cv_text(x, y, f, s, c);
    fm1t_track();
    fm1t_seq++;
    for (int i = 0; i < fm1t_n; i++)             /* (the same words in the next band: once) */
        if (fm1t[i].x == x && fm1t[i].y == y && fm1t[i].face == face && !strncmp(fm1t[i].s, s, sizeof fm1t[i].s - 1))
            return x + text_w(f, s);
    if (fm1t_n < FM1T_MAX) {
        fm1_word *w = &fm1t[fm1t_n++];
        w->x = (int16_t)x, w->y = (int16_t)y, w->seq = (int16_t)fm1t_seq, w->face = (uint8_t)face, w->hidden = 0, w->col = c;
        w->w = (int16_t)text_w(f, s), w->h = (int16_t)f->h;
        snprintf(w->s, sizeof w->s, "%s", s);
    }
    return x + text_w(f, s);
}

static void bk_cv_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    cv_rect(x, y, w, h, c);
    if (!fm1t_hires || cv_w != 240u || w * h < 60)
        return;
    fm1t_track();
    fm1t_seq++;
    if (w == 92 && h == 1) {                     /* an edge of a glass leaf (the stage pairs them) */
        for (int i = 0; i < fm1l_n; i++)          /* (the same in the next band: once) */
            if (fm1l[i].x == x && fm1l[i].y == y)
                return;
        if (fm1l_n < FM1L_MAX)
            fm1l[fm1l_n].x = (int16_t)x, fm1l[fm1l_n].y = (int16_t)y, fm1l[fm1l_n].col = c, fm1l_n++;
        return;
    }
    for (int i = 0; i < fm1p_n; i++) {           /* the points it lies over */
        fm1_point *q = &fm1p[i];
        if (q->band == fm1t_begins && q->x >= x && q->x < x + w && q->y >= y && q->y < y + h)   /* (drawn before it in this band) */
            q->hidden = 1;
    }
    for (int i = 0; i < fm1n_n; i++) {           /* and the lines it lies over (their middle under it) */
        fm1_line *l = &fm1n[i];
        int cx = (l->x0 + l->x1) / 2, cy = (l->y0 + l->y1) / 2;
        if (l->band == fm1t_begins && cx >= x && cx < x + w && cy >= y && cy < y + h)
            l->hidden = 1;
    }
    for (int i = 0; i < fm1t_n; i++) {           /* a box drawn after words, over most of them, hides them */
        fm1_word *t = &fm1t[i];
        int cx = t->x + t->w / 2, cy = t->y + t->h / 2;   /* (its middle) */
        if (t->seq < fm1t_seq && cx >= x && cx < x + w && cy >= y && cy < y + h)
            t->hidden = 1;
    }
}

static void bk_cv_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    uint32_t h;
#ifdef BK_PIXEL_LINES                           /* (for comparing: the FM-1's own pixels) */
    if (1) {
#else
    if (!fm1t_hires || cv_w != 240u) {
#endif
        cv_line(x0, y0, x1, y1, c);
        return;
    }
    fm1t_track();                                /* (no step of its own: the words' and boxes' order stays as it was;
                                                    a box drawn after it has a higher number and hides it) */
    h = (((uint32_t)(x0 & 511) * 73856093u) ^ ((uint32_t)(y0 & 511) * 19349663u) ^ ((uint32_t)(x1 & 511) * 83492791u) ^
         ((uint32_t)(y1 & 511) * 2654435761u) ^ c) % FM1N_HASH;
    for (int probe = 0; probe < 8; probe++, h = (h + 1) % FM1N_HASH) {
        if (fm1n_gen[h] != fm1t_pass) {          /* a free place: a new line */
            if (fm1n_n >= FM1N_MAX) {
                cv_line(x0, y0, x1, y1, c);      /* (too many: the rest as pixels) */
                return;
            }
            fm1_line *l = &fm1n[fm1n_n];
            l->x0 = (int16_t)x0, l->y0 = (int16_t)y0, l->x1 = (int16_t)x1, l->y1 = (int16_t)y1, l->col = c;
            l->seq = (int16_t)fm1t_seq, l->hidden = 0, l->band = fm1t_begins;
            fm1n_gen[h] = fm1t_pass, fm1n_at[h] = (uint16_t)++fm1n_n;
            return;
        }
        {
            fm1_line *l = &fm1n[fm1n_at[h] - 1];
            if (l->x0 == x0 && l->y0 == y0 && l->x1 == x1 && l->y1 == y1 && l->col == c) {
                l->band = fm1t_begins;           /* (the same line in the next band: once; drawn in this band now) */
                return;
            }
        }
    }
    cv_line(x0, y0, x1, y1, c);                  /* (the hash crowded: as pixels) */
}

static void bk_cv_pset(int32_t x, int32_t y, uint16_t c)
{
    uint32_t h;
#ifdef BK_PIXEL_LINES
    if (1) {
#else
    if (!fm1t_hires || cv_w != 240u || c == FM1P_KEEP_PIXEL || x < 0 || x >= 240 || y < -2 || y > 241) {
#endif
        cv_pset(x, y, c);
        return;
    }
    fm1t_track();                                /* (no step of its own, as with lines) */
    h = (((uint32_t)x * 73856093u) ^ ((uint32_t)(y & 1023) * 19349663u) ^ ((uint32_t)c * 83492791u)) % FM1P_HASH;
    for (int probe = 0; probe < 8; probe++, h = (h + 1) % FM1P_HASH) {
        if (fm1p_gen[h] != fm1t_pass) {
            if (fm1p_n >= FM1P_MAX) {
                cv_pset(x, y, c);
                return;
            }
            fm1_point *q = &fm1p[fm1p_n];
            q->x = (int16_t)x, q->y = (int16_t)y, q->col = c, q->seq = (int16_t)fm1t_seq, q->hidden = 0, q->band = fm1t_begins;
            fm1p_gen[h] = fm1t_pass, fm1p_at[h] = (uint16_t)++fm1p_n;
            return;
        }
        {
            fm1_point *q = &fm1p[fm1p_at[h] - 1];
            if (q->x == x && q->y == y && q->col == c) {
                q->band = fm1t_begins;
                return;
            }
        }
    }
    cv_pset(x, y, c);
}

#define cv_begin bk_cv_begin
#define cv_pset bk_cv_pset
#define cv_line bk_cv_line
#define cv_text bk_cv_text
#define cv_rect bk_cv_rect
