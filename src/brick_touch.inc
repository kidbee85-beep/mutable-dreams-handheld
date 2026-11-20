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

static int touch_on, touch_shown = 1;            /* the layer at all; shown now (hidden after a controller's button) */
static uint32_t held_touch;
static int tw_w, tw_h;                           /* the renderer's output, in pixels */
static SDL_Rect stage_dst;                       /* where the stage goes in it */
typedef struct { float x, y, w, h; int bit, round; const char *lab; uint8_t r, g, b; } tbtn;
static tbtn tb[12];
static int tbn;
static float dp_x, dp_y, dp_r;                   /* the D-pad: centre, radius */
#define TFINGERS 10
static struct { SDL_FingerID id; int used; float x, y; } fing[TFINGERS];
static uint32_t press_at[BK_NBUTTONS];           /* (the tap's ring) */
static float ring_x[BK_NBUTTONS], ring_y[BK_NBUTTONS];

static void touch_layout(int W, int H)
{
    float bw_min = H * 0.30f, s, sw, sh, bw, u;
    if (W == tw_w && H == tw_h && tbn)
        return;
    tw_w = W, tw_h = H;
    if (!touch_shown) {                          /* hidden: the stage as large as it fits */
        s = fminf((float)W / BK_W, (float)H / BK_H);
    } else {
        s = fminf((float)W / BK_W, (float)H / BK_H);
        if (W - BK_W * s < 2.0f * bw_min)        /* too square for the bars: the stage smaller */
            s = fmaxf(0.2f, (W - 2.0f * bw_min) / BK_W);
    }
    sw = BK_W * s, sh = BK_H * s;
    stage_dst.x = (int)((W - sw) / 2), stage_dst.y = (int)((H - sh) / 2);
    stage_dst.w = (int)sw, stage_dst.h = (int)sh;
    bw = (W - sw) / 2.0f;
    u = fminf(bw, H * 0.62f);                    /* the size everything is measured in */
    tbn = 0;
    {   /* left bar */
        float cx = bw / 2.0f, sh_w = u * 0.40f, sh_h = H * 0.11f, top = H * 0.04f;
        tb[tbn++] = (tbtn){cx - sh_w - u * 0.03f, top, sh_w, sh_h, BK_L2, 0, "L2", 150, 170, 255};
        tb[tbn++] = (tbtn){cx + u * 0.03f, top, sh_w, sh_h, BK_L1, 0, "L1", 150, 170, 255};
        dp_x = cx, dp_y = H * 0.52f, dp_r = fminf(u * 0.40f, H * 0.24f);
        tb[tbn++] = (tbtn){cx - u * 0.30f, H * 0.85f, u * 0.60f, H * 0.09f, BK_SELECT, 0, "SELECT", 190, 190, 210};
    }
    {   /* right bar */
        float ox = W - bw, cx = ox + bw / 2.0f, sh_w = u * 0.40f, sh_h = H * 0.11f, top = H * 0.04f;
        float cy = H * 0.52f, d = fminf(u * 0.40f, H * 0.24f) * 0.66f, r = d * 0.62f;
        tb[tbn++] = (tbtn){cx - sh_w - u * 0.03f, top, sh_w, sh_h, BK_R1, 0, "R1", 150, 170, 255};
        tb[tbn++] = (tbtn){cx + u * 0.03f, top, sh_w, sh_h, BK_R2, 0, "R2", 150, 170, 255};
        tb[tbn++] = (tbtn){cx + d - r, cy - r, 2 * r, 2 * r, BK_A, 1, "A", 110, 230, 190};
        tb[tbn++] = (tbtn){cx - r, cy + d - r, 2 * r, 2 * r, BK_B, 1, "B", 255, 140, 130};
        tb[tbn++] = (tbtn){cx - r, cy - d - r, 2 * r, 2 * r, BK_X, 1, "X", 120, 190, 255};
        tb[tbn++] = (tbtn){cx - d - r, cy - r, 2 * r, 2 * r, BK_Y, 1, "Y", 255, 210, 110};
        tb[tbn++] = (tbtn){cx - u * 0.45f, H * 0.85f, u * 0.42f, H * 0.09f, BK_START, 0, "START", 190, 190, 210};
        tb[tbn++] = (tbtn){cx + u * 0.03f, H * 0.85f, u * 0.42f, H * 0.09f, BK_MENU, 0, "MENU", 255, 190, 240};
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
        if (b->round) {
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

static void touch_recount(void)
{
    uint32_t m = 0, was = held_touch, now = SDL_GetTicks();
    for (int i = 0; i < TFINGERS; i++)
        if (fing[i].used) {
            uint32_t f = touch_mask_at(fing[i].x, fing[i].y);
            for (int b = 0; b < BK_NBUTTONS; b++)
                if ((f & ~was & ~m) >> b & 1u)
                    press_at[b] = now ? now : 1u, ring_x[b] = fing[i].x, ring_y[b] = fing[i].y;
            m |= f;
        }
    held_touch = m;
}

static void touch_release_all(void)
{
    memset(fing, 0, sizeof fing);
    held_touch = 0;
}

static void touch_finger(SDL_FingerID id, float nx, float ny, int phase)   /* phase 0 down, 1 move, 2 up */
{
    int k = -1, fr = -1;
    for (int i = 0; i < TFINGERS; i++) {
        if (fing[i].used && fing[i].id == id)
            k = i;
        if (!fing[i].used && fr < 0)
            fr = i;
    }
    if (phase == 0 && !touch_shown) {            /* the first touch only brings the pad back */
        touch_shown = 1, tbn = 0;
        return;
    }
    if (phase == 2) {
        if (k >= 0)
            fing[k].used = 0;
    } else {
        if (k < 0) {
            if (phase != 0 || fr < 0)
                return;
            k = fr, fing[k].used = 1, fing[k].id = id;
        }
        fing[k].x = nx * tw_w, fing[k].y = ny * tw_h;
    }
    touch_recount();
}

static void touch_hide(void)                     /* a controller's or keyboard's button: the pad away */
{
    if (touch_shown) {
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
/* a 5 x 7 font: just the letters the buttons need */
static const struct { char c; uint8_t row[7]; } TFONT[] = {
    {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}}, {'C', {14, 17, 16, 16, 16, 17, 14}},
    {'E', {31, 16, 16, 30, 16, 16, 31}}, {'L', {16, 16, 16, 16, 16, 16, 31}}, {'M', {17, 27, 21, 21, 17, 17, 17}},
    {'N', {17, 25, 21, 19, 17, 17, 17}}, {'R', {30, 17, 17, 30, 20, 18, 17}}, {'S', {15, 16, 16, 14, 1, 1, 30}},
    {'T', {31, 4, 4, 4, 4, 4, 4}},       {'U', {17, 17, 17, 17, 17, 17, 14}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'1', {4, 12, 4, 4, 4, 4, 14}},      {'2', {14, 17, 1, 2, 4, 8, 31}}};
static void t_text(SDL_Renderer *r, float cx, float cy, float px, const char *s, SDL_Color c)
{
    SDL_FRect q[35 * 8];
    int n = 0, len = (int)strlen(s);
    float x = cx - (len * 6 - 1) * px / 2.0f, y = cy - 3.5f * px;
    for (; *s; s++, x += 6 * px)
        for (unsigned g = 0; g < sizeof TFONT / sizeof TFONT[0]; g++)
            if (TFONT[g].c == *s)
                for (int row = 0; row < 7; row++)
                    for (int col = 0; col < 5; col++)
                        if (TFONT[g].row[row] >> (4 - col) & 1 && n < (int)(sizeof q / sizeof q[0]))
                            q[n++] = (SDL_FRect){x + col * px, y + row * px, px * 0.92f, px * 0.92f};
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRectsF(r, q, n);
}

static void touch_draw(SDL_Renderer *r)
{
    uint32_t now = SDL_GetTicks();
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    {   /* the D-pad: a soft disc, the cross on it, the way pressed lit */
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
        int on = held_touch >> b->bit & 1u;
        SDL_Color fill = on ? (SDL_Color){b->r, b->g, b->b, 170} : (SDL_Color){b->r, b->g, b->b, 34};
        SDL_Color edge = {b->r, b->g, b->b, on ? 255 : 120};
        SDL_Color txt = on ? (SDL_Color){20, 22, 34, 255} : (SDL_Color){235, 240, 255, 215};
        float len = (float)strlen(b->lab);
        if (b->round) {
            float rad = b->w / 2.0f;
            t_disc(r, b->x + rad, b->y + rad, rad, fill);
            t_ring(r, b->x + rad, b->y + rad, rad, fmaxf(1.5f, rad * 0.07f), edge);
            t_text(r, b->x + rad, b->y + rad, rad * 0.16f, b->lab, txt);
        } else {
            float px = fminf(b->h * 0.075f, b->w * 0.80f / (len * 6.0f));
            t_pill(r, b->x, b->y, b->w, b->h, fill);
            t_text(r, b->x + b->w / 2.0f, b->y + b->h / 2.0f, px, b->lab, txt);
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
    fprintf(stderr, "touch: layout %dx%d, stage %d,%d %dx%d, %d wrong\n", tw_w, tw_h, stage_dst.x, stage_dst.y, stage_dst.w,
            stage_dst.h, bad);
}
#endif
