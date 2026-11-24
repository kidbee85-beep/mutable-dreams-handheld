/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the SDL2 front end (the Brick under Knulli, and a Mac or Linux
 * desktop for development).
 *
 *   mutable-dreams [--window] [--keytest] [SAVE_DIR]
 *
 * Sound: SDL's queue, kept a little ahead (MD_AUDIO_AHEAD frames, default 1024: 23 ms) by a render thread of its own,
 * so the sound and the stage run on two of the Brick's four cores. One mutex: the core is never entered by both.
 * Picture: the stage, 1024 x 768, at MD_FPS (default 30) frames a second.
 * Controls: a game controller (SDL's mapping), the raw joystick buttons named in brick-buttons.txt beside the
 * program (lines "F1 = 9": the Brick's extra keys, found with --keytest), and on a desktop the keyboard:
 *   arrows D-pad · X = A · Z = B · S = X · A = Y · Q = L1 · W = R1 · 1 = L2 · 2 or SHIFT = R2 · RETURN = START
 *   BACKSPACE = SELECT · ESC = MENU · F1 · F2 · TAB = FN switch (toggles)
 * MIDI (Linux): every ALSA raw MIDI port (/dev/snd/midiC*D*, a USB keyboard or controller, plugged in at any time)
 * is read directly, no library: notes onto POLY or into the engine (bk_midi_in). Read in the render thread, so a
 * note lands in the next block; a device unplugged lets go of what it held (bk_midi_lost). Written to them is only
 * what the FM-1 engine itself sends out (bk_midi_out): Mutable Dreams 2.1 sends no notes and no clock of its own
 * (its MIDI is input, and THRU between USB and Bluetooth), so there is no per-track MIDI out here (checked 0.73).
 * MD_RUN_SECONDS=N: quit after N seconds and print what ran (sound device, renderer, frames, underruns): a self-test.
 * MD_AUTOPLAY=1: start the transport at once (for that self-test; no start screen then). MD_NO_SPLASH=1: no start
 * screen. */
#include <ctype.h>
#include <strings.h>
#include "brick_core.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

static uint32_t held_kbd, held_pad, fn_latched;

/* MD_OUT=WxH (a 640 x 480 or 752 x 560 handheld): the stage made smaller here, each new pixel the average of the
   area of the stage it covers (so thin lines and small letters stay smooth), first along the rows, then down the
   columns; the texture handed to SDL is then the screen's own size (the Miyoo's SDL keeps its textures small) */
static int out_w = BK_W, out_h = BK_H;
static uint16_t *sx_at, *sy_at;                  /* for each new column / row: its first stage pixel */
static uint8_t (*sx_w)[3], (*sy_w)[3];           /* and the weights of up to three, summing to 256 */
static uint32_t *small, *sc_line[2];            /* (1.5.7) one stage-wide line per thread, in place of a 2.5 MB half-way picture */
static int hx_58;                                /* (1.5.7) 1024 to 640: every 8 stage pixels make 5, the same 5 weights over */
static uint8_t hx_w[5][3];
static void scale_weights(int src, int dst, uint16_t *at, uint8_t (*w)[3])
{
    for (int d = 0; d < dst; d++) {              /* destination pixel d covers [d * r, (d + 1) * r) of the source */
        double r = (double)src / dst, a = d * r, b = a + r;
        int i0 = (int)a, k;
        int sum = 0;
        at[d] = (uint16_t)i0;
        for (k = 0; k < 3; k++) {
            double lo = i0 + k > a ? i0 + k : a, hi = i0 + k + 1 < b ? i0 + k + 1 : b;
            int v = hi > lo && i0 + k < src ? (int)((hi - lo) / r * 256.0 + 0.5) : 0;
            w[d][k] = (uint8_t)(v > 255 ? 255 : v);
            sum += w[d][k];
        }
        w[d][0] = (uint8_t)(w[d][0] + 256 - sum > 255 ? 255 : w[d][0] + 256 - sum);   /* (rounding: the sum exactly 256) */
    }
}
static int scale_init(int w, int h)
{
    if (w <= 0 || h <= 0 || w > BK_W || h > BK_H || (w == BK_W && h == BK_H))
        return 0;
    out_w = w, out_h = h;
    sx_at = malloc(sizeof *sx_at * (size_t)w), sy_at = malloc(sizeof *sy_at * (size_t)h);
    sx_w = malloc(sizeof *sx_w * (size_t)w), sy_w = malloc(sizeof *sy_w * (size_t)h);
    small = malloc(sizeof *small * (size_t)w * (size_t)h);
    sc_line[0] = malloc(sizeof(uint32_t) * BK_W), sc_line[1] = malloc(sizeof(uint32_t) * BK_W);
    if (!sx_at || !sy_at || !sx_w || !sy_w || !small || !sc_line[0] || !sc_line[1])
        return out_w = BK_W, out_h = BK_H, 0;
    scale_weights(BK_W, w, sx_at, sx_w);
    scale_weights(BK_H, h, sy_at, sy_w);
    if (BK_W * 5 == w * 8 && !getenv("MD_SCALE_TABLE")) {   /* the pattern checked, not assumed */
        static const int at[5] = {0, 1, 3, 4, 6}, three[5] = {0, 1, 0, 1, 0};
        hx_58 = 1;
        for (int x = 0; x < w; x++) {
            int k = x % 5;
            if (sx_at[x] != (x / 5) * 8 + at[k] || (sx_w[x][2] != 0) != three[k] || memcmp(sx_w[x], sx_w[k], 3))
                hx_58 = 0;
        }
        memcpy(hx_w, sx_w, sizeof hx_w);
    }
    return 1;
}
static inline uint32_t mix3(uint32_t a, uint32_t b, uint32_t c, const uint8_t *w)
{
    uint32_t rb = (a & 0xFF00FFu) * w[0] + (b & 0xFF00FFu) * w[1] + (c & 0xFF00FFu) * w[2];
    uint32_t g = (a & 0xFF00u) * w[0] + (b & 0xFF00u) * w[1] + (c & 0xFF00u) * w[2];
    return 0xFF000000u | ((rb >> 8) & 0xFF00FFu) | ((g >> 8) & 0xFF00u);
}
static inline uint32_t mix2(uint32_t a, uint32_t b, const uint8_t *w)
{
    uint32_t rb = (a & 0xFF00FFu) * w[0] + (b & 0xFF00FFu) * w[1];
    uint32_t g = (a & 0xFF00u) * w[0] + (b & 0xFF00u) * w[1];
    return 0xFF000000u | ((rb >> 8) & 0xFF00FFu) | ((g >> 8) & 0xFF00u);
}
/* (1.5.7) one output row: first down the columns (two or three stage rows into one stage-wide line, the same weights
   all along: the vector unit's work, four pixels at a time), then along the line. Row by row the line stays in the
   cache; before, the whole stage went along the rows into a 2.5 MB picture, and that down the columns - on the
   Miyoo's Cortex-A7 a third of each frame. Each channel floor(sum / 256) as before, only the order of the two passes
   turned: a picture within one step of brightness of 1.5.6's */
static void scale_down(const uint32_t *r0, const uint32_t *r1, const uint32_t *r2, const uint8_t *w, uint32_t *d)
{
    int x = 0;
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    {
        const uint8x8_t w0 = vdup_n_u8(w[0]), w1 = vdup_n_u8(w[1]), w2 = vdup_n_u8(w[2]);
        const uint8x16_t op = vreinterpretq_u8_u32(vdupq_n_u32(0xFF000000u));
        if (w[2])
            for (; x + 4 <= BK_W; x += 4) {
                uint8x16_t a = vld1q_u8((const uint8_t *)(r0 + x)), b = vld1q_u8((const uint8_t *)(r1 + x)), c = vld1q_u8((const uint8_t *)(r2 + x));
                uint16x8_t lo = vmull_u8(vget_low_u8(a), w0), hi = vmull_u8(vget_high_u8(a), w0);
                lo = vmlal_u8(lo, vget_low_u8(b), w1), hi = vmlal_u8(hi, vget_high_u8(b), w1);
                lo = vmlal_u8(lo, vget_low_u8(c), w2), hi = vmlal_u8(hi, vget_high_u8(c), w2);
                vst1q_u8((uint8_t *)(d + x), vorrq_u8(vcombine_u8(vshrn_n_u16(lo, 8), vshrn_n_u16(hi, 8)), op));
            }
        else
            for (; x + 4 <= BK_W; x += 4) {
                uint8x16_t a = vld1q_u8((const uint8_t *)(r0 + x)), b = vld1q_u8((const uint8_t *)(r1 + x));
                uint16x8_t lo = vmull_u8(vget_low_u8(a), w0), hi = vmull_u8(vget_high_u8(a), w0);
                lo = vmlal_u8(lo, vget_low_u8(b), w1), hi = vmlal_u8(hi, vget_high_u8(b), w1);
                vst1q_u8((uint8_t *)(d + x), vorrq_u8(vcombine_u8(vshrn_n_u16(lo, 8), vshrn_n_u16(hi, 8)), op));
            }
    }
#endif
    if (w[2])
        for (; x < BK_W; x++)
            d[x] = mix3(r0[x], r1[x], r2[x], w);
    else
        for (; x < BK_W; x++)
            d[x] = mix2(r0[x], r1[x], w);
}
static void scale_along(const uint32_t *s, uint32_t *d)
{
    if (hx_58) {                                 /* 8 in, 5 out: no tables, no tests */
        const uint8_t *w0 = hx_w[0], *w1 = hx_w[1], *w2 = hx_w[2], *w3 = hx_w[3], *w4 = hx_w[4];
        for (int b = 0; b < out_w / 5; b++, s += 8, d += 5) {
            d[0] = mix2(s[0], s[1], w0);
            d[1] = mix3(s[1], s[2], s[3], w1);
            d[2] = mix2(s[3], s[4], w2);
            d[3] = mix3(s[4], s[5], s[6], w3);
            d[4] = mix2(s[6], s[7], w4);
        }
        return;
    }
    for (int x = 0; x < out_w; x++) {
        int i = sx_at[x];
        const uint8_t *w = sx_w[x];
        d[x] = w[2] ? mix3(s[i], s[i + 1 < BK_W ? i + 1 : i], s[i + 2 < BK_W ? i + 2 : BK_W - 1], w)
                    : mix2(s[i], s[i + 1 < BK_W ? i + 1 : i], w);
    }
}
static void scale_out_rows(const uint32_t *px, int o0, int o1, uint32_t *line)
{
    for (int y = o0; y < o1; y++) {
        int j = sy_at[y];
        scale_down(px + j * BK_W, px + (j + 1 < BK_H ? j + 1 : j) * BK_W, px + (j + 2 < BK_H ? j + 2 : BK_H - 1) * BK_W,
                   sy_w[y], line);
        scale_along(line, small + y * out_w);
    }
}
/* a helper thread takes the lower half of the output rows (1.5.7: on two cores too - the halves no longer wait for
   each other half-way, and the sound, at real-time priority, goes first anyway; MD_SCALE_ONE=1: one thread) */
static SDL_sem *sc_go, *sc_done;
static const uint32_t *sc_px;
static int sc_split_out;
static int scale_worker(void *arg)
{
    (void)arg;
    for (;;) {
        SDL_SemWait(sc_go);
        if (!sc_px)
            return 0;
        scale_out_rows(sc_px, sc_split_out, out_h, sc_line[1]);
        SDL_SemPost(sc_done);
    }
}
static void scale_threads_init(void)
{
    if (SDL_GetCPUCount() < 2 || getenv("MD_SCALE_ONE"))
        return;
    sc_go = SDL_CreateSemaphore(0), sc_done = SDL_CreateSemaphore(0);
    sc_split_out = out_h / 2;
    if (sc_go && sc_done && SDL_CreateThread(scale_worker, "scale", 0))
        fprintf(stderr, "video: the scaling shared with a second thread%s\n", hx_58 ? " (8 to 5 along the rows)" : "");
    else
        sc_go = 0;
}
static const uint32_t *scale_frame(const uint32_t *px)
{
    if (!sc_go) {
        scale_out_rows(px, 0, out_h, sc_line[0]);
        return small;
    }
    sc_px = px;
    SDL_SemPost(sc_go);
    scale_out_rows(px, 0, sc_split_out, sc_line[0]);
    SDL_SemWait(sc_done);
    return small;
}
static int keytest;

/* ------------------------------------------------------------- MIDI --- */
#ifdef __linux__
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#define MIDI_MAX 4
static struct { int fd, out, sysex, need, got; uint8_t msg[3]; char path[64], name[40]; } mdev[MIDI_MAX];
static int mdev_n;
static uint32_t mdev_scan_at;

static void midi_name(int card, char *out, size_t n)
{
    char p[64];
    FILE *f;
    snprintf(out, n, "MIDI %d", card);
    snprintf(p, sizeof p, "/proc/asound/card%d/id", card);
    if ((f = fopen(p, "r"))) {
        if (fgets(out, (int)n, f)) {
            size_t l = strlen(out);
            while (l && (out[l - 1] == '\n' || out[l - 1] == ' '))
                out[--l] = 0;
            for (char *c = out; *c; c++)
                if (*c >= 'a' && *c <= 'z')
                    *c = (char)(*c - 32);
        }
        fclose(f);
    }
}

static void midi_scan(void)
{
    DIR *d;
    struct dirent *e;
    if ((int32_t)(SDL_GetTicks() - mdev_scan_at) < 0)
        return;
    mdev_scan_at = SDL_GetTicks() + 2000u;
    if (!(d = opendir("/dev/snd")))
        return;
    while ((e = readdir(d)) && mdev_n < MIDI_MAX) {
        int card, dev, i, fd, out = 1;
        char path[64];
        if (sscanf(e->d_name, "midiC%dD%d", &card, &dev) != 2)
            continue;
        snprintf(path, sizeof path, "/dev/snd/%s", e->d_name);
        for (i = 0; i < mdev_n; i++)
            if (!strcmp(mdev[i].path, path))
                break;
        if (i < mdev_n)
            continue;
        if ((fd = open(path, O_RDWR | O_NONBLOCK)) < 0) {
            out = 0;
            if ((fd = open(path, O_RDONLY | O_NONBLOCK)) < 0)
                continue;
        }
        memset(&mdev[mdev_n], 0, sizeof mdev[0]);
        mdev[mdev_n].fd = fd;
        mdev[mdev_n].out = out;
        snprintf(mdev[mdev_n].path, sizeof mdev[0].path, "%s", path);
        midi_name(card, mdev[mdev_n].name, sizeof mdev[0].name);
        fprintf(stderr, "midi: %s (%s)%s\n", path, mdev[mdev_n].name, out ? "" : ", in only");
        mdev_n++;
    }
    closedir(d);
}

static void midi_emit(const uint8_t *m, int n)
{
    if (keytest)
        printf("midi in %02X %02X %02X\n", m[0], n > 1 ? m[1] : 0, n > 2 ? m[2] : 0);
    bk_midi_in(m, n);
}

/* everything waiting on every port into the core (core lock held by the caller) */
static void midi_poll(void)
{
    uint8_t buf[256];
    for (int i = 0; i < mdev_n; i++) {
        ssize_t n = read(mdev[i].fd, buf, sizeof buf);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            continue;
        if (n <= 0) {                            /* unplugged: what it held is let go (0.73) */
            fprintf(stderr, "midi: %s gone\n", mdev[i].path);
            bk_midi_lost();
            close(mdev[i].fd);
            mdev[i] = mdev[--mdev_n];
            i--;
            continue;
        }
        for (ssize_t k = 0; k < n; k++) {
            uint8_t b = buf[k];
            if (b >= 0xF8) {                     /* real time, even inside other messages */
                midi_emit(&b, 1);
            } else if (b == 0xF0) {
                mdev[i].sysex = 1;
            } else if (b == 0xF7) {
                mdev[i].sysex = 0;
            } else if (b & 0x80) {               /* a status: its data to come */
                mdev[i].sysex = 0;
                mdev[i].msg[0] = b;
                mdev[i].got = 0;
                mdev[i].need = (b & 0xE0) == 0xC0 || b == 0xF1 || b == 0xF3 ? 1 : b == 0xF6 ? 0 : b >= 0xF0 && b != 0xF2 ? -1 : 2;
                if (mdev[i].need == 0)
                    midi_emit(mdev[i].msg, 1);
            } else if (!mdev[i].sysex && mdev[i].msg[0] && mdev[i].need > 0) {
                mdev[i].msg[1 + mdev[i].got++] = b;
                if (mdev[i].got == mdev[i].need) {
                    midi_emit(mdev[i].msg, 1 + mdev[i].need);
                    mdev[i].got = 0;             /* (running status: the same status again) */
                }
            }
        }
    }
}

static void midi_send(const uint8_t *b, int n)
{
    for (int i = 0; i < mdev_n && n > 0; i++)
        if (mdev[i].out && write(mdev[i].fd, b, (size_t)n) < 0 && errno != EAGAIN)
            fprintf(stderr, "midi: %s write failed\n", mdev[i].path);
}
#else
static int mdev_n;
static void midi_scan(void) { }
static void midi_poll(void) { }
static void midi_send(const uint8_t *b, int n) { (void)b; (void)n; }
#endif
static SDL_mutex *core_lock;
static SDL_AudioDeviceID audio_dev;
/* the engine sounds at 44.1 kHz; a card opened at another rate (the Brick's codec at 48 kHz) gets it converted here,
   by SDL's own resampler. (0.38 to 0.60 queued the 44.1 kHz blocks as 48 kHz ones: 1.5 semitones high, 9 % fast.) */
static SDL_AudioStream *audio_conv;
#include "brick_resample.h"
static rs_state *audio_rs;                       /* 44.1 to 48 kHz ourselves, in float (see brick_resample.h) */
static uint32_t audio_ahead = 1536u, underruns, underrun_at[8];
static uint32_t ahead_max = 4096u;               /* (it grows by itself after a gap, up to this) */
static int ahead_fixed;                          /* MD_AUDIO_AHEAD given: kept as it is */
/* (1.5.7) the sound made ahead while the core is free: up to ahead_burst frames above the mark, only when no one else
   holds the core (the stage, the buttons) - between frames, while a frame is scaled and shown. The stage then seldom
   has to stop for the sound in the middle of a frame. On the Miyoo's two cores the sound and the stage took turns: of
   a 73 ms frame, 15 ms were the stage waiting for the sound (Miyoo Mini Plus 1.5.5 log). MD_AUDIO_BURST=frames, 0 off;
   by itself 2048 on two cores and less */
static uint32_t ahead_burst, sound_slow_us;
static SDL_atomic_t stage_waiting;               /* the stage wants the core for a frame: no sound ahead now */
static int dev_rate = BK_RATE, dev_frames = 1024;
/* (0.80) USB-C AUDIO: a class-compliant USB sound card or DAC plugged into the Brick takes the sound (SDL lists it as
   an output whose name has "USB" in it); unplugged, the Brick's own codec has it again. MD_AUDIO_DEV=part of a name
   chooses one by name; MD_USB_AUDIO=0 (the launcher: usbaudio.off) leaves the sound on the Brick. The switch: the
   render thread's conversion and the ring are held (out_lock) while the card changes. */
static SDL_mutex *out_lock;
static SDL_AudioSpec audio_want, audio_have;
static char audio_name[128];                     /* the card in use ("" the system's default) */
static uint32_t ring_live_from;                  /* (gaps are counted from a second after the card started) */
/* (1.0.2) a card whose clock is not what it says: the Miyoo's MI_AO, opened at 48 kHz, takes about 10 % more a second
   (the music too fast and too high). The render thread measures what the card really takes, from a second after
   it started, and after 7 s (again after 30 s, finer) has the converter make that many frames a second instead -
   the engine then keeps the clock's time. Within 5 % nothing changes (the Brick, a desktop). Since 1.5.1 only with
   MD_RATE_TRIM=1 (else measured and written to log.txt only) */
static double rate_true;
static uint32_t warm_ms;                         /* (the self-test's warm-up: engine time before the session's clock) */                         /* the rate the converter makes for (0: the card's own) */

/* the sound card's side: our own ring at the card's rate, emptied by SDL's callback. The callback never waits for
   anything; the render thread keeps the ring filled. A gap (the callback finding less than it needs) is counted
   exactly here, also a half one (0.60 counted only an empty queue). */
#define RING_FRAMES 32768u
static int16_t ring[RING_FRAMES * 2];
static SDL_atomic_t ring_w, ring_r, ring_gaps, ring_live;
static SDL_atomic_t dev_taken;                   /* (1.1) every frame the card asked for, the silence of a gap too */
static void audio_cb(void *u, Uint8 *stream, int len)
{
    int16_t *o = (int16_t *)stream;
    uint32_t need = (uint32_t)len / 4u, w = (uint32_t)SDL_AtomicGet(&ring_w), r = (uint32_t)SDL_AtomicGet(&ring_r);
    uint32_t have = w - r, n = have < need ? have : need, i;
    (void)u;
    SDL_AtomicAdd(&dev_taken, (int)need);       /* (the card's true pace: what it takes, filled or not) */
    for (i = 0; i < n; i++) {
        uint32_t k = (r + i) % RING_FRAMES;
        o[2 * i] = ring[2 * k], o[2 * i + 1] = ring[2 * k + 1];
    }
    if (n < need) {
        memset(o + 2 * n, 0, (need - n) * 4u);
        if (SDL_AtomicGet(&ring_live))
            SDL_AtomicIncRef(&ring_gaps);
    }
    SDL_AtomicSet(&ring_r, (int)(r + n));
}
static uint32_t ring_level(void) { return (uint32_t)SDL_AtomicGet(&ring_w) - (uint32_t)SDL_AtomicGet(&ring_r); }
static void ring_put(const int16_t *pcm, uint32_t frames)
{
    uint32_t w = (uint32_t)SDL_AtomicGet(&ring_w), i;
    if (frames > RING_FRAMES - ring_level())
        frames = RING_FRAMES - ring_level();
    for (i = 0; i < frames; i++) {
        uint32_t k = (w + i) % RING_FRAMES;
        ring[2 * k] = pcm[2 * i], ring[2 * k + 1] = pcm[2 * i + 1];
    }
    SDL_AtomicSet(&ring_w, (int)(w + frames));
}
static uint32_t draw_slow_ms;                    /* MD_DRAW_SLOW_MS: a slow device's drawing, simulated (tests) */

/* the stage lets go of the core between its parts, so the sound thread gets in during a long frame */
static SDL_atomic_t audio_waiting;               /* the sound thread wants the core */
static SDL_atomic_t input_waiting;               /* (1.1) the buttons want the core: the stage lets them in at once */
/* (1.1) the stage drawn on a thread of its own, into two buffers by turns: while the main thread shows one frame
   the next is being drawn - the picture's time and the screen's time no longer add up. fb_ready: the frame to
   show (index + 1, 0 none); fb_shown: the one being shown (index + 1). MD_DRAW_THREAD=0: drawn on the main thread
   as before 1.1 */
static uint32_t *fbuf[2];
static SDL_atomic_t fb_ready, fb_shown, draw_run;
static SDL_Thread *draw_thread;
static SDL_mutex *fb_lock;                       /* (who shows and who draws which buffer: decided under it) */
static uint32_t draw_fps = 30u;
static uint64_t draw_us_sum_t;                   /* (the draw thread's own sums, read by the status lines) */
static uint32_t draw_us_max_t, frames_made;
static SDL_atomic_t draw_wait_us;                /* (the stage's time spent letting the sound in, for log.txt) */
/* MD_PROFILE_DRAW (the launcher: profile.on): where the stage's time goes on the device itself. Each stretch between
   two of the stage's looks for the sound is put down to the place of the second look (its caller's address); every
   30 s the dearest twelve go to log.txt as offsets from main, for tools/symbolize_profile.py */
#define DP_SLOTS 1024
int main(int argc, char **argv);
static int dprof;
static int light_auto;                           /* (MD_LIGHT: see main) */
static uintptr_t dp_at[DP_SLOTS];
static uint64_t dp_us[DP_SLOTS], dp_t0;
static void dp_add(uintptr_t at, uint64_t now)
{
    uint64_t us = (now - dp_t0) * 1000000u / SDL_GetPerformanceFrequency();
    unsigned h = (unsigned)((at >> 2) % DP_SLOTS), n = 0;
    while (dp_at[h] && dp_at[h] != at && n++ < DP_SLOTS)
        h = (h + 1) % DP_SLOTS;
    if (n < DP_SLOTS)
        dp_at[h] = at, dp_us[h] += us;
}
static void dp_report(void)
{
    uint64_t tot = 0;
    for (int i = 0; i < DP_SLOTS; i++)
        tot += dp_us[i];
    if (!tot)
        return;
    fprintf(stderr, "draw profile (main at %p):", (void *)&main);
    for (int k = 0; k < 12; k++) {
        int b = -1;
        for (int i = 0; i < DP_SLOTS; i++)
            if (dp_at[i] && (b < 0 || dp_us[i] > dp_us[b]))
                b = i;
        if (b < 0 || !dp_us[b])
            break;
        fprintf(stderr, " %.1f%%@%+lld", 100.0 * (double)dp_us[b] / (double)tot, (long long)((intptr_t)dp_at[b] - (intptr_t)&main));
        dp_us[b] = 0;
    }
    fprintf(stderr, "\n");
    memset(dp_at, 0, sizeof dp_at);
    memset(dp_us, 0, sizeof dp_us);
}
static void draw_yield(void)
{
    if (dprof) {
        uint64_t now = SDL_GetPerformanceCounter();
        dp_add((uintptr_t)__builtin_return_address(0), now);
        dp_t0 = now;
    }
    if (draw_slow_ms) {                          /* (a test: a slow device's drawing, the core held all the while: */
        uint64_t until = SDL_GetPerformanceCounter() + SDL_GetPerformanceFrequency() * draw_slow_ms / 100000u;
        while (SDL_GetPerformanceCounter() < until)   /*  MD_DRAW_SLOW_MS hundredths of a ms per look) */
            ;
    }
    if (!SDL_AtomicGet(&audio_waiting) && !SDL_AtomicGet(&input_waiting))
        return;
    uint64_t w0 = SDL_GetPerformanceCounter();
    SDL_UnlockMutex(core_lock);                  /* the sound (or a button) in first: wait until it has the core */
    for (int spin = 0; (SDL_AtomicGet(&audio_waiting) || SDL_AtomicGet(&input_waiting)) && spin < 2000; spin++)
        SDL_Delay(0);
    SDL_LockMutex(core_lock);
    SDL_AtomicAdd(&draw_wait_us, (int)((SDL_GetPerformanceCounter() - w0) * 1000000u / SDL_GetPerformanceFrequency()));
    if (dprof)
        dp_t0 = SDL_GetPerformanceCounter();
}
static volatile int render_run = 1;

/* the render thread: the ring ahead of the sound card, nothing else. Kept audio_ahead frames ahead (in the engine's
   44.1 kHz); after a gap the mark rises by 512 (up to ahead_max), so a device that needs more gets it by itself,
   and one that does not keeps the short way from button to sound */
static int env_is(const char *name, const char *value);
static int render_loop(void *arg)
{
    static float buf[2 * 256];
    static int16_t pcm[2 * 300];
    static uint8_t mout[1024];
    uint32_t dith = 0x9E3779B9u, gaps_seen = 0;
    uint32_t started = SDL_GetTicks();
    (void)arg;
    ring_live_from = started;
    {   /* tiny numbers (denormals: the end of every reverb and filter tail) flushed to zero in this thread, so a fading
           tail never costs many times more than a loud one */
#if defined(__aarch64__)
        uint64_t fpcr;
        __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
        __asm__ volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
#elif defined(__arm__) && defined(__ARM_PCS_VFP)
        uint32_t fpscr;
        __asm__ volatile("vmrs %0, fpscr" : "=r"(fpscr));
        __asm__ volatile("vmsr fpscr, %0" : : "r"(fpscr | (1u << 24)));
#elif defined(__SSE__)
        __builtin_ia32_ldmxcsr(__builtin_ia32_stmxcsr() | 0x8040u);
#endif
    }
    if (SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL) != 0)   /* the sound first (Knulli runs as root) */
        fprintf(stderr, "audio: no real-time priority: %s\n", SDL_GetError());
    else
        fprintf(stderr, "audio: render thread at real-time priority\n");
    while (render_run) {
        uint32_t gaps;
        midi_scan();
        if (!SDL_AtomicGet(&ring_live) && SDL_GetTicks() - ring_live_from > 1000u)
            SDL_AtomicSet(&ring_live, 1);       /* (the first second: the device settling, not counted) */
        {   /* (1.0.2) the card's true rate: frames taken since it settled, against the clock */
            static uint32_t m_t0, m_r0, m_step, m_from;
            uint32_t now = SDL_GetTicks();
            if (!SDL_AtomicGet(&ring_live) || m_from != ring_live_from) {   /* (a new card: measure anew) */
                if (m_from != ring_live_from)
                    m_from = ring_live_from, m_t0 = 0, m_step = 0, rate_true = 0;
            } else if (!m_t0)
                m_t0 = now, m_r0 = (uint32_t)SDL_AtomicGet(&dev_taken);
            else if (m_step < 2 && now - m_t0 >= (m_step ? 30000u : 6000u)) {
                double got = (double)((uint32_t)SDL_AtomicGet(&dev_taken) - m_r0) * 1000.0 / (double)(now - m_t0);
                double cur = rate_true > 0 ? rate_true : (double)dev_rate, dev = (double)dev_rate;
                m_step++;
                /* (1.5.1) measured always, acted on only with MD_RATE_TRIM=1: no card ever needed it (all near 48 000),
                   and on the RGB20S a stall made the measure 38 883 - the sound then made a fifth too fast */
                if (!env_is("MD_RATE_TRIM", "1") && got > dev * 0.67 && got < dev * 1.5 && fabs(got / dev - 1.0) > 0.05) {
                    fprintf(stderr, "audio: the card seemed to take %.0f frames a second, not %d - left as it is (MD_RATE_TRIM=1 would follow it)\n",
                            got, dev_rate);
                    fflush(stderr);
                } else if (got > dev * 0.67 && got < dev * 1.5 && fabs(got / dev - 1.0) > 0.05 && fabs(got / cur - 1.0) > 0.003) {
                    SDL_AudioStream *nc;
                    SDL_LockMutex(out_lock);
                    nc = SDL_NewAudioStream(AUDIO_S16SYS, 2, BK_RATE, audio_have.format, audio_have.channels, (int)lrint(got));
                    if (nc) {
                        if (audio_conv)
                            SDL_FreeAudioStream(audio_conv);
                        audio_conv = nc, audio_rs = 0, rate_true = got;
                    }
                    SDL_UnlockMutex(out_lock);
                    fprintf(stderr, "audio: the card takes %.0f frames a second, not %d: the sound is made for that now%s\n",
                            got, dev_rate, nc ? "" : " (no converter: left as it was)");
                    fflush(stderr);
                } else if (m_step == 1)
                    fprintf(stderr, "audio: the card takes %.0f frames a second (as it says, %d)\n", got, dev_rate);
            }
        }
        gaps = (uint32_t)SDL_AtomicGet(&ring_gaps);
        if (gaps != gaps_seen) {                  /* the card found the ring short */
            uint32_t at = SDL_GetTicks() - started;
            for (uint32_t g = gaps_seen; g < gaps; g++) {
                if (underruns < 8)
                    underrun_at[underruns] = at;
                underruns++;
                if (underruns <= 30 || underruns % 100 == 0)
                    fprintf(stderr, "audio: underrun %u at %.1f s, page %s\n", (unsigned)underruns, at / 1000.0, bk_page_name());
            }
            gaps_seen = gaps;
            if (!ahead_fixed && audio_ahead < ahead_max) {
                audio_ahead = audio_ahead + 512u > ahead_max ? ahead_max : audio_ahead + 512u;
                fprintf(stderr, "audio: now kept %u frames ahead (%.0f ms)\n", (unsigned)audio_ahead, audio_ahead * 1000.0 / BK_RATE);
            }
            fflush(stderr);
        }
        for (;;) {                               /* the ring kept full; a block whenever it is below the mark */
            int mo;
            uint32_t lvl = ring_level();
            if (!render_run || lvl >= (uint32_t)((uint64_t)(audio_ahead + ahead_burst) * (uint32_t)dev_rate / BK_RATE))
                break;
            if (lvl >= (uint32_t)((uint64_t)audio_ahead * (uint32_t)dev_rate / BK_RATE)) {
                /* above the mark: ahead only while the core is free (1.5.7) */
                if (SDL_AtomicGet(&stage_waiting) || SDL_AtomicGet(&input_waiting) || SDL_TryLockMutex(core_lock) != 0)
                    break;
            } else {
                SDL_AtomicSet(&audio_waiting, 1);
                SDL_LockMutex(core_lock);
                SDL_AtomicSet(&audio_waiting, 0);
            }
            midi_poll();
            bk_render(buf, 256);
            if (sound_slow_us) {                 /* (a test: a slow device's sound, MD_SOUND_SLOW_US per block, core held) */
                uint64_t until = SDL_GetPerformanceCounter() + SDL_GetPerformanceFrequency() * sound_slow_us / 1000000u;
                while (SDL_GetPerformanceCounter() < until)
                    ;
            }
            mo = bk_midi_out(mout, sizeof mout);
#ifdef __linux__
            bk_midi_devices(mdev_n, mdev_n ? mdev[0].name : "");
#endif
            SDL_UnlockMutex(core_lock);
            midi_send(mout, mo);
            {   /* at the card's rate first, in float; then to 16 bits ourselves, with TPDF dither: quiet tails fade
                   into soft noise, not into truncation's grit */
                static float rsbuf[2 * 300];
                const float *src = buf;
                int nf = 256;
                SDL_LockMutex(out_lock);         /* (the card may be changing: USB-C audio) */
                if (audio_rs)
                    nf = rs_run(audio_rs, buf, 256, rsbuf), src = rsbuf;
                for (int i = 0; i < 2 * nf; i++) {
                    float v = src[i] * 32767.0f;
                    uint32_t r1, r2;
                    dith = dith * 1664525u + 1013904223u, r1 = dith >> 16;
                    dith = dith * 1664525u + 1013904223u, r2 = dith >> 16;
                    v += ((float)r1 - (float)r2) / 65536.0f;
                    v = v > 32767.0f ? 32767.0f : v < -32768.0f ? -32768.0f : v;
                    pcm[i] = (int16_t)lrintf(v);
                }
                if (audio_conv) {                /* (another rate than 44.1 or 48 kHz: SDL's converter) */
                    static int16_t conv[2 * 600 + 64];
                    int got;
                    SDL_AudioStreamPut(audio_conv, pcm, (int)(nf * 4));
                    while ((got = SDL_AudioStreamGet(audio_conv, conv, sizeof conv)) > 0)
                        ring_put(conv, (uint32_t)got / 4u);
                } else
                    ring_put(pcm, (uint32_t)nf);
                SDL_UnlockMutex(out_lock);
            }
        }
        SDL_Delay(1);
    }
    return 0;
}
static int usb_name(const char *n)               /* a USB sound card's name (SDL's, from ALSA's card description) */
{
    for (; n && *n; n++)
        if ((n[0] | 32) == 'u' && (n[1] | 32) == 's' && (n[2] | 32) == 'b')
            return 1;
    return 0;
}
/* the output to use: MD_AUDIO_DEV's match, else the first USB card (unless MD_USB_AUDIO=0), else the default (0) */
static const char *audio_pick(char *out, size_t n)
{
    const char *want_name = getenv("MD_AUDIO_DEV");
    int usb_ok = !env_is("MD_USB_AUDIO", "0");
    int count = SDL_GetNumAudioDevices(0);
    for (int i = 0; i < count; i++) {
        const char *nm = SDL_GetAudioDeviceName(i, 0);
        if (!nm)
            continue;
        fprintf(stderr, "audio: output %d: %s%s\n", i, nm, usb_name(nm) ? " (USB)" : "");   /* (log.txt: what the Brick sees) */
        if (want_name && *want_name ? strstr(nm, want_name) != 0 : usb_ok && usb_name(nm)) {
            snprintf(out, n, "%s", nm);
            return out;
        }
    }
    return 0;
}
/* open a card by name (0: the default) at the engine's terms; the converters set for its rate */
static SDL_AudioDeviceID audio_open(const char *name)
{
    SDL_AudioDeviceID d = SDL_OpenAudioDevice(name, 0, &audio_want, &audio_have, 0);   /* SDL converts if the card wants another rate */
    if (!d && name)
        fprintf(stderr, "audio: %s would not open (%s), the default instead\n", name, SDL_GetError()),
            d = SDL_OpenAudioDevice(0, 0, &audio_want, &audio_have, 0), name = 0;
    static int asked;                            /* (only at the start: a USB card's change does not wait) */
    for (int k = 0; !d && !asked && k < 4; k++) {          /* (1.5.7) the card still held a moment (the Miyoo Mini Plus once: MI_AO_SetPubAttr
                                                    failed, the whole session silent): asked again, four times over 2 s */
        fprintf(stderr, "audio: the card would not open (%s), again in half a second\n", SDL_GetError());
        SDL_Delay(500);
        d = SDL_OpenAudioDevice(name, 0, &audio_want, &audio_have, 0);
    }
    asked = 1;
    if (!d) {
        fprintf(stderr, "no sound: %s\n", SDL_GetError());
        return 0;
    }
    snprintf(audio_name, sizeof audio_name, "%s", name ? name : "");
    {
        SDL_version v;
        SDL_GetVersion(&v);
        dev_rate = audio_have.freq, dev_frames = audio_have.samples;
        audio_rs = 0;
        if (audio_conv)
            SDL_FreeAudioStream(audio_conv), audio_conv = 0;
        if (audio_have.freq == 48000 && !getenv("MD_SDL_RESAMPLE")) {   /* the Brick's codec (and most USB cards): our own resampler */
            static rs_state rs;
            rs_init(&rs);
            audio_rs = &rs;
        } else if (audio_have.freq != BK_RATE) {   /* another rate: SDL converts (see audio_conv) */
            audio_conv = SDL_NewAudioStream(AUDIO_S16SYS, 2, BK_RATE, audio_have.format, audio_have.channels, audio_have.freq);
            if (!audio_conv)
                fprintf(stderr, "audio: no converter: %s\n", SDL_GetError());
        }
        fprintf(stderr, "audio: %s, driver %s, %d Hz (engine 44100%s, SDL %d.%d.%d), %d frames\n", name ? name : "default card",
                SDL_GetCurrentAudioDriver(), audio_have.freq, audio_rs ? ", resampled here (polyphase sinc, 48 taps)" : audio_conv ? ", converted by SDL" : "",
                v.major, v.minor, v.patch, audio_have.samples);
    }
    fflush(stderr);
    return d;
}
/* another card while it plays (a USB card plugged in or pulled out): the ring emptied, the new card opened, the
   garden says where the sound goes now */
static void audio_switch(const char *name)
{
    SDL_AudioDeviceID d;
    SDL_LockMutex(out_lock);
    if (audio_dev)
        SDL_CloseAudioDevice(audio_dev), audio_dev = 0;
    SDL_AtomicSet(&ring_live, 0);
    SDL_AtomicSet(&ring_r, SDL_AtomicGet(&ring_w));
    d = audio_open(name);
    ring_live_from = SDL_GetTicks();
    audio_dev = d;
    SDL_UnlockMutex(out_lock);
    if (d)
        SDL_PauseAudioDevice(d, 0);
    SDL_LockMutex(core_lock);
    bk_say(audio_name[0] ? "USB AUDIO" : "BRICK SPEAKER", audio_name[0] ? audio_name : "THE SOUND IS BACK ON THE BRICK");
    SDL_UnlockMutex(core_lock);
}
/* a frame onto the screen: straight into the window's pixels, or through the renderer */
#include "brick_touch.inc"
#ifdef MD_HAVE_TOUCH
static const char *touch_shot;                   /* MD_TOUCH_SHOT=FILE.bmp: the window as shown, once, after 40 frames (tests) */
#endif

#if defined(__APPLE__)
/* (1.5.10) MD_VPAD_TEST=1: a virtual game controller (SDL 2.0.14+, the Mac's SDL only) pushed through the very event path a
   handheld's controller takes - SDL_CONTROLLERBUTTONDOWN / AXISMOTION, pad_control, the sticks' hand-over to the core -
   to see the Trimui Smart Pro's keys do what they should: the sticks' clicks (L3, R3) open the Sound Lab and the mixer
   (F1, F2), SELECT taps open the menu, the MENU button too, the left stick turns the first dial. Prints "vpad: ..." lines
   and "vpad: PASS" (else "vpad: FAIL n"); start with MD_KEYS=handheld MD_AUTOPLAY=1 MD_NO_SPLASH=1 */
static SDL_Joystick *vpad_js;
static int vpad_failed, vpad_checks, vpad_step;
static void vpad_attach(void)
{
    SDL_VirtualJoystickDesc d;
    int idx;
    SDL_zero(d);
    d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    d.naxes = SDL_CONTROLLER_AXIS_MAX;
    d.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    d.name = "Virtual Smart Pro";
    idx = SDL_JoystickAttachVirtualEx(&d);
    if (idx >= 0)
        vpad_js = SDL_JoystickOpen(idx);
    fprintf(stderr, "vpad: a virtual controller %s\n", vpad_js ? "attached" : "NOT attached");
}
static void vpad_btn(int b, int v) { SDL_JoystickSetVirtualButton(vpad_js, b, (Uint8)v); }
static int vpad_rel = -1;                        /* the button to let go of, once the program has seen it pressed (a press and a release
                                                    inside one event poll would be one change, no event) */
static uint32_t vpad_rel_at;
static void vpad_tap(int b) { vpad_btn(b, 1); vpad_rel = b; vpad_rel_at = SDL_GetTicks() + 150u; }
static void vpad_check(const char *what, int ok)
{
    vpad_checks++;
    if (!ok)
        vpad_failed++;
    fprintf(stderr, "vpad: %-4s %s\n", ok ? "ok" : "FAIL", what);
}
/* one step per call, called by the main loop; the steps wait for the program in between */
static int vpad_tick(SDL_mutex *lock, uint32_t ms)
{
    static uint32_t next = 2500u;
    int gv, mv, m0;
    if (vpad_rel >= 0 && SDL_GetTicks() >= vpad_rel_at)
        vpad_btn(vpad_rel, 0), vpad_rel = -1;
    if (!vpad_js || ms < next || vpad_step < 0)
        return 0;
    SDL_LockMutex(lock);
    gv = bk_garden_visible(), mv = bk_menu_shown(), m0 = bk_macro(0);
    SDL_UnlockMutex(lock);
    switch (vpad_step++) {
    case 0: vpad_check("the garden is there to begin with", gv); vpad_tap(SDL_CONTROLLER_BUTTON_LEFTSTICK); next = ms + 900; break;
    case 1: vpad_check("L3 (the left stick's click) = F1: the Sound Lab opens, the garden goes", !gv); vpad_tap(SDL_CONTROLLER_BUTTON_B); next = ms + 900; break;
    case 2: vpad_check("B goes back: the garden again", gv); vpad_tap(SDL_CONTROLLER_BUTTON_RIGHTSTICK); next = ms + 900; break;
    case 3: vpad_check("R3 (the right stick's click) = F2: the mixer opens, the garden goes", !gv); vpad_tap(SDL_CONTROLLER_BUTTON_B); next = ms + 900; break;
    case 4: vpad_check("B goes back: the garden again", gv); vpad_tap(SDL_CONTROLLER_BUTTON_BACK); next = ms + 900; break;
    case 5: vpad_check("SELECT (back) tapped opens the menu (a handheld without MENU)", mv); vpad_tap(SDL_CONTROLLER_BUTTON_BACK); next = ms + 900; break;
    case 6: vpad_check("SELECT tapped again closes it", !mv); vpad_tap(SDL_CONTROLLER_BUTTON_GUIDE); next = ms + 900; break;
    case 7: vpad_check("the MENU button (guide) opens the menu too", mv); vpad_tap(SDL_CONTROLLER_BUTTON_B); next = ms + 900; break;
    case 8: vpad_check("B closes the menu", !mv);
            vpad_check("the first dial is at its start (500)", m0 == 500);
            SDL_JoystickSetVirtualAxis(vpad_js, SDL_CONTROLLER_AXIS_LEFTX, 3000); next = ms + 1200; break;   /* a stick almost at rest */
    case 9: vpad_check("a small push of the left stick turns nothing", m0 == 500);
            SDL_JoystickSetVirtualAxis(vpad_js, SDL_CONTROLLER_AXIS_LEFTX, 32000); next = ms + 1500; break;
    case 10: vpad_check("the left stick pushed right turns the first dial up", m0 > 620);
             SDL_JoystickSetVirtualAxis(vpad_js, SDL_CONTROLLER_AXIS_LEFTX, 0); next = ms + 400; break;
    default:
        fprintf(stderr, "vpad: %s (%d checks%s)\n", vpad_failed ? "FAIL" : "PASS", vpad_checks, vpad_failed ? ", some failed" : "");
        vpad_step = -1;
        return 1;
    }
    return 0;
}
#endif
static int fit_edge;                             /* (1.6) MD_SCREEN: the bars beside the stage glow with its edge's colours (MD_EDGE=0: black) */
#define FIT_BANDS 24
static SDL_Texture *fit_amb[4];                  /* left, right (1 x 24) and above, below (24 x 1): the edge's colours, averaged in bands, drawn smoothly */
static uint32_t fit_amb_px[4][FIT_BANDS];
static void fit_ambient_update(SDL_Renderer *ren, const uint32_t *px)
{
    static uint32_t n;
    if (n++ % 3u)                                /* (every third frame is plenty: the glow follows the picture slowly) */
        return;
    for (int side = 0; side < 4; side++) {
        int horiz = side < 2, len = horiz ? out_h : out_w, thick = horiz ? out_w : out_h, at = thick / 16;   /* a strip inside the panel: its cards' colours */
        int c0 = (side & 1 ? thick - 1 - at - thick / 40 : at - thick / 40), c1 = c0 + thick / 20;
        if (c0 < 0) c0 = 0;
        if (!fit_amb[side]) {
            fit_amb[side] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, horiz ? 1 : FIT_BANDS, horiz ? FIT_BANDS : 1);
            if (!fit_amb[side])
                continue;
#if SDL_VERSION_ATLEAST(2, 0, 12)
            SDL_SetTextureScaleMode(fit_amb[side], SDL_ScaleModeLinear);
#endif
        }
        for (int b = 0; b < FIT_BANDS; b++) {
            unsigned r = 0, g = 0, bl = 0, cnt = 0;
            int a0 = len * b / FIT_BANDS, a1 = len * (b + 1) / FIT_BANDS;
            for (int a = a0; a < a1; a += 3)
                for (int c = c0; c < c1; c += 3) {
                    uint32_t v = horiz ? px[(size_t)a * (size_t)out_w + (size_t)c] : px[(size_t)c * (size_t)out_w + (size_t)a];
                    r += v >> 16 & 255u, g += v >> 8 & 255u, bl += v & 255u, cnt++;
                }
            if (!cnt)
                cnt = 1;
            fit_amb_px[side][b] = 0xFF000000u | (r / cnt) << 16 | (g / cnt) << 8 | (bl / cnt);
        }
        SDL_UpdateTexture(fit_amb[side], 0, fit_amb_px[side], horiz ? 4 : FIT_BANDS * 4);
    }
}
static void fit_present(SDL_Renderer *ren, SDL_Texture *tex, const uint32_t *px)
{
    int W = 0, H = 0, ow = out_w, oh = out_h;
    SDL_Rect d;
    float sc;
    SDL_GetRendererOutputSize(ren, &W, &H);
    sc = (float)W / (float)ow < (float)H / (float)oh ? (float)W / (float)ow : (float)H / (float)oh;
    d.w = (int)((float)ow * sc), d.h = (int)((float)oh * sc), d.x = (W - d.w) / 2, d.y = (H - d.h) / 2;
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    if (d.x > 0 || d.y > 0) {
        fit_ambient_update(ren, px);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        for (int side = 0; side < 4; side++) {   /* left, right, above, below */
            SDL_Rect dst;
            int horiz = side < 2;
            if (!fit_amb[side] || (horiz && d.x <= 0) || (!horiz && d.y <= 0))
                continue;
            if (horiz)
                dst.x = side ? d.x + d.w : 0, dst.y = d.y, dst.w = side ? W - d.x - d.w : d.x, dst.h = d.h;
            else
                dst.x = d.x, dst.y = side == 3 ? d.y + d.h : 0, dst.w = d.w, dst.h = side == 3 ? H - d.y - d.h : d.y;
            SDL_SetTextureColorMod(fit_amb[side], 235, 235, 235);
            SDL_RenderCopy(ren, fit_amb[side], 0, &dst);
            for (int k = 0; k < 16; k++) {       /* the dark grows from the stage's edge to the screen's: 16 strips */
                SDL_Rect f = dst;
                int span = horiz ? dst.w : dst.h, a0 = span * k / 16, a1 = span * (k + 1) / 16;
                if (horiz)
                    f.x = side ? dst.x + a0 : dst.x + dst.w - a1, f.w = a1 - a0;
                else
                    f.y = side == 3 ? dst.y + a0 : dst.y + dst.h - a1, f.h = a1 - a0;
                SDL_SetRenderDrawColor(ren, 0, 0, 0, (Uint8)(60 + 195 * (k + 1) / 16));
                SDL_RenderFillRect(ren, &f);
            }
        }
    }
    SDL_RenderCopy(ren, tex, 0, &d);
}
static const char *md_shot;                      /* (1.5.10) MD_SHOT=FILE.bmp: the window as shown through the renderer, once, after 40 frames (tests) */
static void window_shot(SDL_Renderer *ren)
{
    int w = 0, h = 0, lw = 0, lh = 0;
    SDL_Surface *sf;
    SDL_GetRendererOutputSize(ren, &w, &h);
    SDL_RenderGetLogicalSize(ren, &lw, &lh);
    SDL_RenderSetLogicalSize(ren, 0, 0);         /* (a read inside the logical size gives only the stage: the whole window wanted) */
    sf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (sf && SDL_RenderReadPixels(ren, 0, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch) == 0)
        SDL_SaveBMP(sf, md_shot), fprintf(stderr, "video: window %d x %d saved to %s\n", w, h, md_shot);
    SDL_FreeSurface(sf);
    SDL_RenderSetLogicalSize(ren, lw, lh);
}
static void present_frame(SDL_Window *win, SDL_Surface *win_surf, SDL_Renderer *ren, SDL_Texture *tex, const uint32_t *px, uint32_t n)
{
    if (win_surf) {
        if (SDL_MUSTLOCK(win_surf))
            SDL_LockSurface(win_surf);
        for (int y = 0; y < out_h; y++)
            memcpy((uint8_t *)win_surf->pixels + (size_t)y * (size_t)win_surf->pitch, px + (size_t)y * (size_t)out_w, (size_t)out_w * 4u);
        if (SDL_MUSTLOCK(win_surf))
            SDL_UnlockSurface(win_surf);
        if (SDL_UpdateWindowSurface(win) != 0 && !n)
            fprintf(stderr, "SDL_UpdateWindowSurface: %s\n", SDL_GetError());
    } else {
        SDL_UpdateTexture(tex, 0, px, out_w * 4);
#ifdef MD_HAVE_TOUCH
        if (touch_on) {                          /* (1.5.8) the stage beside the touch pad */
            touch_present(ren, tex);
            if (touch_shot && n == 40) {
                SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, tw_w, tw_h, 32, SDL_PIXELFORMAT_ARGB8888);
                if (s && SDL_RenderReadPixels(ren, 0, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) == 0)
                    SDL_SaveBMP(s, touch_shot), fprintf(stderr, "touch: window saved to %s\n", touch_shot);
                SDL_FreeSurface(s);
            }
            SDL_RenderPresent(ren);
            return;
        }
#endif
        if (fit_edge) {
            fit_present(ren, tex, px);
        } else {
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, 0, 0);
        }
        if (md_shot && n == 40)
            window_shot(ren);
        SDL_RenderPresent(ren);
    }
}
/* (1.1) the stage's own thread: a frame at the pace asked for (MD_FPS), into the buffer not being shown */
static int draw_loop(void *arg)
{
    uint32_t next = 0, w = 0;
    float avg_ms = 0;
    uint32_t slow_since = 0;
    (void)arg;
    while (SDL_AtomicGet(&draw_run)) {
        uint64_t d0;
        const uint32_t *stage, *px;
        uint32_t dus;
        int busy;
        if ((int32_t)(SDL_GetTicks() - next) < 0) {
            SDL_Delay(1);
            continue;
        }
        SDL_LockMutex(fb_lock);                  /* buffer w: not while it is shown; a frame waiting in it is dropped */
        busy = SDL_AtomicGet(&fb_shown) == (int)w + 1;
        if (!busy && SDL_AtomicGet(&fb_ready) == (int)w + 1)
            SDL_AtomicSet(&fb_ready, 0);
        SDL_UnlockMutex(fb_lock);
        if (busy) {
            SDL_Delay(1);
            continue;
        }
        {   /* (1.5.7: on a steady beat, as on the main thread) */
            uint32_t per = 1000u / (draw_fps ? draw_fps : 30u), now_ms = SDL_GetTicks();
            next += per;
            if ((int32_t)(now_ms - next) >= 0 || (int32_t)(next - now_ms) > (int32_t)per)
                next = now_ms + per;
        }
        d0 = SDL_GetPerformanceCounter();
        SDL_AtomicSet(&stage_waiting, 1);
        SDL_LockMutex(core_lock);
        SDL_AtomicSet(&stage_waiting, 0);
        dp_t0 = d0;
        stage = bk_draw();
        if (dprof)
            dp_add(1, SDL_GetPerformanceCounter());
        if (bk_resting_now())                    /* (1.4: resting after the sleep timer, 4 pictures a second; read
                                                    under the lock that its change is made under) */
            next = SDL_GetTicks() + 250u;
        SDL_UnlockMutex(core_lock);
        px = out_w != BK_W ? scale_frame(stage) : stage;
        memcpy(fbuf[w], px, (size_t)out_w * (size_t)out_h * 4u);
        dus = (uint32_t)((SDL_GetPerformanceCounter() - d0) * 1000000u / SDL_GetPerformanceFrequency());
        SDL_LockMutex(fb_lock);                  /* (the sums too: the main thread reads them under it) */
        draw_us_sum_t += dus;
        if (dus > draw_us_max_t)
            draw_us_max_t = dus;
        frames_made++;
        SDL_AtomicSet(&fb_ready, (int)w + 1);
        SDL_UnlockMutex(fb_lock);
        w ^= 1u;
        if (light_auto && frames_made > 30) {    /* slow for 3 s on end: the light picture, once */
            avg_ms += ((float)dus / 1000.0f - avg_ms) * 0.1f;
            if (avg_ms > 60.0f) {
                if (!slow_since)
                    slow_since = SDL_GetTicks() ? SDL_GetTicks() : 1u;
                else if (SDL_GetTicks() - slow_since > 3000u) {
                    SDL_LockMutex(core_lock);
                    bk_set_light(2);
                    SDL_UnlockMutex(core_lock);
                    light_auto = 0;
                    fprintf(stderr, "picture: light by itself (the stage took %.0f ms a frame)\n", avg_ms);
                }
            } else
                slow_since = 0;
        }
    }
    return 0;
}
static int joy_map[BK_NBUTTONS];                 /* raw joystick button for each control, -1 none */
static int nintendo;                             /* brick-buttons.txt NINTENDO = 1: SDL's A/B and X/Y by position swapped back to the printed labels */
static int swap_ab, swap_xy;                     /* (1.5) SWAP_AB = 1 / SWAP_XY = 1: each pair on its own (the Brick: A and B
                                                    came swapped, X and Y right) */
static int input_logged;                         /* (1.5) the first presses of a start go into log.txt with their codes */
static const char *const NAMES[BK_NBUTTONS] = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y", "L1", "R1",
                                               "L2", "R2", "START", "SELECT", "MENU", "F1", "F2", "FN"};

static int env_is(const char *name, const char *value)   /* (one getenv: the static analyzer's NullArg) */
{
    const char *e = getenv(name);
    return e && !strcmp(e, value);
}

/* (0.80) the keyboard, bindable: keys.txt beside the program's data (mutabledreams/keys.txt on the Brick, the user's
   data folder on a desktop) says which keys are which Brick button, "BUTTON = key, key" with SDL's key names; it is
   written with the defaults when missing. The hints then name the keys bound (bk_set_key_label). */
#define KEYS_MAX 64
static struct { SDL_Keycode k; int b; } keymap[KEYS_MAX];
static int keymap_n = -1;                        /* (-1: keys.txt not read: the built-in table below) */
static const char *const KEY_DEFAULTS[BK_NBUTTONS] = {"Up", "Down", "Left", "Right", "X", "Z", "S", "A", "Q", "W", "1",
                                                      "Left Shift, Right Shift, 2", "Return", "Backspace", "Escape", "F1", "F2", "Tab"};
static void key_label_of(const char *sdl_name, char *out, size_t n)   /* a key's short name for the hint pebbles */
{
    static const char *const short_names[][2] = {{"Left Shift", "SHIFT"}, {"Right Shift", "SHIFT"}, {"Return", "ENTER"},
                                                 {"Backspace", "BKSP"}, {"Escape", "ESC"}, {"Space", "SPACE"},
                                                 {"Left Ctrl", "CTRL"}, {"Right Ctrl", "CTRL"}, {"Left Alt", "ALT"}, {0, 0}};
    int m;
    for (m = 0; short_names[m][0] && strcmp(short_names[m][0], sdl_name); m++)
        ;
    snprintf(out, n, "%s", short_names[m][0] ? short_names[m][1] : sdl_name);
    for (char *p = out; *p; p++)
        *p = (char)toupper((unsigned char)*p);
}
static void keys_load(void)
{
    FILE *f = fopen("keys.txt", "r");
    char line[256], first[BK_NBUTTONS][24];
    memset(first, 0, sizeof first);
    if (!f && (f = fopen("keys.txt", "w"))) {     /* (the defaults, written once, to be changed) */
        fprintf(f, "# Mutable Dreams: the keyboard. Each Brick button and the keys that press it, separated by commas.\n"
                   "# Key names as SDL writes them: A..Z, 0..9, Up, Down, Left, Right, Return, Space, Tab, Escape, Backspace,\n"
                   "# Left Shift, Right Shift, Left Ctrl, F1..F12, Keypad 8 ... Delete this file to get the defaults back.\n");
        for (int b = 0; b < BK_NBUTTONS; b++)
            fprintf(f, "%-7s= %s\n", NAMES[b], KEY_DEFAULTS[b]);
        fclose(f);
        f = fopen("keys.txt", "r");
    }
    if (!f)
        return;
    keymap_n = 0;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '='), *nm = line, *keys;
        int b;
        if (line[0] == '#' || !eq)
            continue;
        *eq = 0, keys = eq + 1;
        while (*nm == ' ' || *nm == '\t')
            nm++;
        for (char *e = nm + strlen(nm); e > nm && (e[-1] == ' ' || e[-1] == '\t'); *--e = 0)
            ;
        for (b = 0; b < BK_NBUTTONS && strcasecmp(NAMES[b], nm); b++)
            ;
        if (b == BK_NBUTTONS) {
            fprintf(stderr, "keys.txt: no button %s\n", nm);
            continue;
        }
        for (char *k = strtok(keys, ",\r\n"); k; k = strtok(0, ",\r\n")) {
            SDL_Keycode code;
            while (*k == ' ' || *k == '\t')
                k++;
            for (char *e = k + strlen(k); e > k && (e[-1] == ' ' || e[-1] == '\t'); *--e = 0)
                ;
            if (!*k)
                continue;
            if ((code = SDL_GetKeyFromName(k)) == SDLK_UNKNOWN) {
                fprintf(stderr, "keys.txt: no key \"%s\" (for %s)\n", k, NAMES[b]);
                continue;
            }
            if (keymap_n < KEYS_MAX)
                keymap[keymap_n].k = code, keymap[keymap_n++].b = b;
            if (!first[b][0])
                key_label_of(k, first[b], sizeof first[b]);
        }
    }
    fclose(f);
    for (int b = 0; b < BK_NBUTTONS; b++)        /* the hints name what is bound */
        if (first[b][0])
            bk_set_key_label(NAMES[b], first[b]);
    if (first[BK_UP][0] && first[BK_DOWN][0] && first[BK_LEFT][0] && first[BK_RIGHT][0]) {
        char dp[48];
        int arrows = !strcmp(first[BK_UP], "UP") && !strcmp(first[BK_DOWN], "DOWN") && !strcmp(first[BK_LEFT], "LEFT") && !strcmp(first[BK_RIGHT], "RIGHT");
        snprintf(dp, sizeof dp, "%s/%s/%s/%s", first[BK_UP], first[BK_LEFT], first[BK_DOWN], first[BK_RIGHT]);
        bk_set_key_label("D-PAD", arrows ? "ARROWS" : dp);
        bk_set_key_label("UP/DOWN", arrows ? "UP/DOWN" : first[BK_UP]);
    }
    fprintf(stderr, "keys: %d keys bound from keys.txt\n", keymap_n);
}
static int key_control(SDL_Keycode k)
{
    if (keymap_n >= 0) {
        for (int i = 0; i < keymap_n; i++)
            if (keymap[i].k == k)
                return keymap[i].b;
        return -1;
    }
    switch (k) {
    case SDLK_UP: return BK_UP;
    case SDLK_DOWN: return BK_DOWN;
    case SDLK_LEFT: return BK_LEFT;
    case SDLK_RIGHT: return BK_RIGHT;
    case SDLK_x: return BK_A;
    case SDLK_z: return BK_B;
    case SDLK_s: return BK_X;
    case SDLK_a: return BK_Y;
    case SDLK_q: return BK_L1;
    case SDLK_w: return BK_R1;
    case SDLK_1: return BK_L2;
    case SDLK_2: case SDLK_LSHIFT: case SDLK_RSHIFT: return BK_R2;
    case SDLK_RETURN: return BK_START;
    case SDLK_BACKSPACE: return BK_SELECT;
    case SDLK_ESCAPE: return BK_MENU;
    case SDLK_F1: return BK_F1;
    case SDLK_F2: return BK_F2;
    case SDLK_TAB: return BK_FN;
    }
    return -1;
}

static int pad_control(int b)
{
    switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return BK_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return BK_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return BK_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return BK_RIGHT;
    case SDL_CONTROLLER_BUTTON_A: return BK_A;
    case SDL_CONTROLLER_BUTTON_B: return BK_B;
    case SDL_CONTROLLER_BUTTON_X: return BK_X;
    case SDL_CONTROLLER_BUTTON_Y: return BK_Y;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return BK_L1;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return BK_R1;
    case SDL_CONTROLLER_BUTTON_START: return BK_START;
    case SDL_CONTROLLER_BUTTON_BACK: return BK_SELECT;
    case SDL_CONTROLLER_BUTTON_GUIDE: return BK_MENU;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return BK_F1;    /* (1.5) the Brick's lit F1 and F2: its controller mapping
                                                               names them the sticks' clicks (it has no sticks) */
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return BK_F2;
    }
    return -1;
}

static void load_joy_map(const char *dir)
{
    char path[1024], line[128];
    FILE *f;
    for (int i = 0; i < BK_NBUTTONS; i++)
        joy_map[i] = -1;
    snprintf(path, sizeof path, "%s/brick-buttons.txt", dir);
    if (!(f = fopen(path, "r")))
        return;
    while (fgets(line, sizeof line, f)) {
        char name[32];
        int n;
        if (line[0] == '#' || sscanf(line, " %31[A-Z0-9_] = %d", name, &n) != 2)
            continue;
        if (!strcmp(name, "NINTENDO"))
            nintendo = n;
        if (!strcmp(name, "SWAP_AB"))
            swap_ab = n;
        if (!strcmp(name, "SWAP_XY"))
            swap_xy = n;
        for (int i = 0; i < BK_NBUTTONS; i++)
            if (!strcmp(name, NAMES[i]))
                joy_map[i] = n;
    }
    fclose(f);
}

/* (1.5) the Brick's F1 and F2 when the controller mapping does not name them: raw buttons 11 and 12 (as the Brick's
   own gamepad reports them), but only a raw button the mapping uses for nothing else; brick-buttons.txt wins.
   The controller's mapping, its raw buttons and what was decided go into log.txt. */
static void f_keys_fallback(SDL_GameController *pad)
{
    int used[64] = {0};
    char *m = pad ? SDL_GameControllerMapping(pad) : 0;
    if (m) {
        fprintf(stderr, "input: controller mapping %.300s\n", m);
        SDL_free(m);
    }
    if (!pad)
        return;
    for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++) {
        SDL_GameControllerButtonBind bd = SDL_GameControllerGetBindForButton(pad, (SDL_GameControllerButton)b);
        if (bd.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON && bd.value.button >= 0 && bd.value.button < 64)
            used[bd.value.button] = 1;
    }
    for (int k = 0; k < 2; k++) {
        int c = k ? BK_F2 : BK_F1, raw = k ? 12 : 11;
        SDL_GameControllerButtonBind bd = SDL_GameControllerGetBindForButton(pad, k ? SDL_CONTROLLER_BUTTON_RIGHTSTICK : SDL_CONTROLLER_BUTTON_LEFTSTICK);
        if (joy_map[c] >= 0)
            fprintf(stderr, "input: %s from brick-buttons.txt: raw %d\n", NAMES[c], joy_map[c]);
        else if (bd.bindType != SDL_CONTROLLER_BINDTYPE_NONE)
            fprintf(stderr, "input: %s is the mapping's %s click\n", NAMES[c], k ? "right" : "left");
        else if (!used[raw]) {
            joy_map[c] = raw;
            fprintf(stderr, "input: %s taken as raw button %d (the mapping does not name it)\n", NAMES[c], raw);
        } else
            fprintf(stderr, "input: %s unknown: raw %d is taken by the mapping; set it in brick-buttons.txt\n", NAMES[c], raw);
    }
}

static int read_battery(void)
{
    static const char *const paths[] = {"/sys/class/power_supply/axp2202-battery/capacity",
                                        "/sys/class/power_supply/battery/capacity",
                                        "/sys/class/power_supply/BAT0/capacity"};
    for (unsigned i = 0; i < sizeof paths / sizeof paths[0]; i++) {
        FILE *f = fopen(paths[i], "r");
        int v;
        if (f) {
            int ok = fscanf(f, "%d", &v) == 1;
            fclose(f);
            if (ok)
                return v;
        }
    }
    return -1;
}

int main(int argc, char **argv)
{
    const char *save = 0, *ahead_env = getenv("MD_AUDIO_AHEAD"), *fps_env = getenv("MD_FPS");
    int windowed = 0, running = 1, desktop = 0, fit_screen = 0, test_w = 0, test_h = 0;
    uint32_t fps = fps_env ? (uint32_t)atoi(fps_env) : 30u;
    SDL_Thread *render_thread = 0;
    char base[1024] = ".";
    SDL_Window *win;
    SDL_Renderer *ren = 0;
    SDL_Surface *win_surf = 0;
    SDL_Texture *tex;
    SDL_AudioDeviceID dev;
    SDL_AudioSpec want;
    SDL_GameController *pad = 0;
    SDL_Joystick *joy = 0;
    uint32_t next_frame = 0, next_batt = 0, frames_drawn = 0, started, run_s = 0, cpu_max = 0, cpu_sum = 0, cpu_n = 0, draw_us_max = 0;
    uint64_t draw_us_sum = 0, present_us_sum = 0;
    const char *run_env = getenv("MD_RUN_SECONDS");

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--window"))
            windowed = 1;
        else if (!strcmp(argv[i], "--keytest"))
            keytest = 1;
        else
            save = argv[i];
    }
    {
        char *b = SDL_GetBasePath();
        if (b) {
            snprintf(base, sizeof base, "%s", b);
            SDL_free(b);
        }
    }
#ifdef __ANDROID__
    {   /* Android: the app's own folder on the device's storage (Android/data/com.kidbunka.mutabledreams/files):
           save/, worlds/, archive/ and samples/ (the app puts the package's files there on its first start), and
           log.txt - Android keeps no stderr of its own */
        const char *ext = SDL_AndroidGetExternalStoragePath();
        char lp[1100];
        if (ext)
            snprintf(base, sizeof base, "%s", ext);
        snprintf(lp, sizeof lp, "%s/log.txt", base);
        if (!freopen(lp, "w", stderr))
            (void)0;
        setenv("MD_KEYS", "handheld", 0);        /* (a handheld's buttons, no MENU / F1 / F2 / FN) */
        setenv("MD_DEVICE", "ANDROID", 0);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");   /* (the stage scaled smoothly by the GPU) */
        if (!getenv("MD_AUDIO_AHEAD"))           /* AAudio fetches in larger bursts: in the emulator 46 ms ran dry ~30 times */
            audio_ahead = 4096u, ahead_max = 6144u;   /* a second, 93 ms never (14.11.2026) */
        fprintf(stderr, "android: files in %s\n", base);
    }
#endif
    load_joy_map(base);
    if (!save) {
        static char def[1100];
        snprintf(def, sizeof def, "%s/save", base);
        save = def;
    }
    mkdir(save, 0755);

    {   /* MD_OUT=640x480: the stage drawn as always, made smaller for the handheld's screen */
        int w = 0, h = 0;
        if (getenv("MD_OUT") && sscanf(getenv("MD_OUT"), "%dx%d", &w, &h) == 2 && scale_init(w, h)) {
            fprintf(stderr, "video: the stage scaled to %d x %d\n", out_w, out_h);
            scale_threads_init();
            if (out_w < 800)
                bk_set_small(1);                 /* (small labels one size up, 0.76) */
        }
    }
    fprintf(stderr, "startup: SDL_Init\n");
    fflush(stderr);
    if (env_is("MD_DESKTOP", "1")) {             /* (0.80.1) a desktop: the compositor stays on for our fullscreen window.
                                                    SDL turns it off by default (_NET_WM_BYPASS_COMPOSITOR); in Plasma's
                                                    XWayland on a VM's virtual screen the picture then went stale in places
                                                    until a mouse click repainted it */
        SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
        if (!ahead_env)                          /* (and a little more sound ahead: a desktop's sound server starts
                                                    with a larger quantum; 58 ms instead of 35, no gap at the start) */
            audio_ahead = 2560u;
    }
#ifdef MD_HAVE_TOUCH
    {   /* (1.5.8) the touch pad: on Android unless MD_TOUCH=0, elsewhere with MD_TOUCH=1 */
#ifdef __ANDROID__
        touch_on = !env_is("MD_TOUCH", "0");
        SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");   /* (the back key comes to us: B) */
        SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");  /* (the phone's tilt is no controller) */
        SDL_SetHint(SDL_HINT_ORIENTATIONS, env_is("MD_PORTRAIT", "0") ? "LandscapeLeft LandscapeRight" : "LandscapeLeft LandscapeRight Portrait");   /* (1.6) upright too: the stage on top) */
#else
        touch_on = env_is("MD_TOUCH", "1");
#endif
        if (touch_on) {
            SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");     /* (fingers as fingers, not also as a mouse) */
            touch_shot = getenv("MD_TOUCH_SHOT");
        }
    }
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    fprintf(stderr, "startup: SDL ready, video %s\n", SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "?");
    keys_load();                                 /* (0.80: keys.txt, the keyboard's bindings) */
    fflush(stderr);
    SDL_ShowCursor(SDL_DISABLE);
    {   /* (0.73) a desktop PC (MD_DESKTOP=1, as Mutable Dreams Linux starts it): fullscreen at the screen's own size,
           the 4:3 stage kept in proportion and smoothly scaled, never the screen switched to 1024 x 768; --window: a
           resizable window */
        const char *dk = getenv("MD_DESKTOP");
        desktop = dk && *dk == '1';
    }
    {   /* (1.5.10) MD_SCREEN=1: a handheld whose screen is not the stage's size (the Trimui Smart Pro, 1280 x 720; the
           H700s): the window is the screen's own size (the driver's desktop mode, no mode switch to the stage's size),
           the renderer draws the stage in proportion in the middle - 1:1, black beside it, when the launcher made the
           stage fit the screen's height (MD_OUT). MD_SCREEN=WxH: the same in a window of that size (tests on a Mac) */
        const char *sc = getenv("MD_SCREEN");
        if (sc && *sc && strcmp(sc, "0")) {
            fit_screen = 1;
            if (sscanf(sc, "%dx%d", &test_w, &test_h) != 2)
                test_w = test_h = 0;
            fprintf(stderr, "video: the screen's own size, the stage in proportion in the middle%s\n", test_w ? " (a test window)" : "");
        }
        md_shot = getenv("MD_SHOT");
        fit_edge = fit_screen && !env_is("MD_EDGE", "0");
    }
    if (desktop || fit_screen)
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    {
        int ww = out_w, wh = out_h;
        if (test_w > 0)
            ww = test_w, wh = test_h;
#ifdef MD_HAVE_TOUCH
        if (touch_on && getenv("MD_TOUCH_WIN"))  /* (tests: a phone's or a tablet's shape, e.g. 2400x1080) */
            sscanf(getenv("MD_TOUCH_WIN"), "%dx%d", &ww, &wh);
#endif
    win = SDL_CreateWindow("Mutable Dreams", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh,
                           desktop ? (windowed ? SDL_WINDOW_RESIZABLE : SDL_WINDOW_FULLSCREEN_DESKTOP) :
                           fit_screen ? (test_w > 0 ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) :
#ifdef __linux__
                           windowed ? 0 : SDL_WINDOW_FULLSCREEN
#else
                           windowed ? 0 : SDL_WINDOW_ALLOW_HIGHDPI
#endif
    );
    }
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }
    if (!desktop && !fit_screen && env_is("MD_PRESENT", "window")) {
        /* MD_PRESENT=window (surface.on): the picture straight into the window's own pixels, no texture and no
           renderer in between - two copies of the frame fewer. Only where the window is exactly the picture's size
           and 32-bit RGB; else the renderer as always */
        SDL_Surface *ws = SDL_GetWindowSurface(win);
        /* The stage is 0xAARRGGBB; only the 32-bit layouts with red in bits 16-23 copy byte for byte. XRGB8888
           (alpha ignored) is what KMSDRM gives on the RGB20S, ARGB8888 on the Brick. RGB888 (24-bit) used to be
           accepted here, but the copy writes 4 bytes a pixel: never again. A surface in another order stays with
           the renderer. */
        if (ws && ws->w == out_w && ws->h == out_h &&
            (ws->format->format == SDL_PIXELFORMAT_ARGB8888 || ws->format->format == SDL_PIXELFORMAT_XRGB8888))
            win_surf = ws;
        fprintf(stderr, "video: %s\n", win_surf ? "straight into the window surface" :
                "the window surface does not fit (size or format): the renderer instead");
    }
    if (!win_surf) {   /* on the Brick the software renderer onto the window surface, as TrackWerk does on the same Knulli (the
           Mali GL path is the risk of a black screen); MD_VIDEO=gpu tries the accelerated one first */
        const char *vm = getenv("MD_VIDEO");
        SDL_RendererInfo ri;
#if defined(__ANDROID__)
        int gpu = !vm || strcmp(vm, "surface");  /* (Android: OpenGL ES, a phone's screen may be far larger) */
#elif defined(__linux__)
        int gpu = vm && !strcmp(vm, "gpu");
#else
        int gpu = !vm || strcmp(vm, "surface");
#endif
        ren = gpu ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : NULL;
        if (!ren)
            ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
        if (!ren) {                              /* (1.0.1) an SDL without the software renderer - the Miyoo's mmiyoo
                                                    has only its own: then the accelerated one, then whatever it offers
                                                    (as TrackWerk does; 1.0.0 gave up here: a black screen) */
            fprintf(stderr, "video: no software renderer (%s), trying the others\n", SDL_GetError());
            ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
            if (!ren)
                ren = SDL_CreateRenderer(win, -1, 0);
        }
        if (!ren) {
            fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
            return 1;
        }
        if (desktop)
            SDL_RenderSetLogicalSize(ren, out_w, out_h);   /* (letterboxed, in proportion) */
        if (SDL_GetRendererInfo(ren, &ri) == 0)
            fprintf(stderr, "video: driver %s, renderer %s\n", SDL_GetCurrentVideoDriver(), ri.name);
        fflush(stderr);
    }
    if (ren && !fit_edge)
        SDL_RenderSetLogicalSize(ren, out_w, out_h);
#ifdef MD_HAVE_TOUCH
    if (ren && touch_on) {                       /* (the touch pad places the stage itself, beside the bars) */
        int W = 0, H = 0;
        SDL_RenderSetLogicalSize(ren, 0, 0);
        SDL_RenderSetViewport(ren, 0);
        SDL_GetRendererOutputSize(ren, &W, &H);
        touch_layout(W, H);
        fprintf(stderr, "touch: the buttons on the screen, %d x %d, stage %d x %d\n", W, H, stage_dst.w, stage_dst.h);
        if (env_is("MD_TOUCH_SELFTEST", "1"))
            touch_selftest();
        if (getenv("MD_TOUCH_LAYOUTS"))
            fprintf(stderr, "touch: layouts, %d wrong\n", touch_layouts(getenv("MD_TOUCH_LAYOUTS"))), touch_layout(W, H);
    } else if (!ren)
        touch_on = 0;
#endif
    tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, out_w, out_h) : 0;
    if (ren && !tex) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        return 1;
    }

    SDL_zero(want);
#ifdef __linux__
    want.freq = 48000;                           /* the Brick's codec at its own rate (as TrackWerk on the same card): SDL
                                                    converts 44.1 kHz well; ALSA's plug converted it with grit */
#else
    want.freq = BK_RATE;
#endif
    if (getenv("MD_AUDIO_RATE"))
        want.freq = atoi(getenv("MD_AUDIO_RATE"));
    want.format = AUDIO_S16SYS;                  /* (the card is 16 bits: dithered here, not truncated by SDL) */
    want.channels = 2;
#ifdef __linux__
    want.samples = 1024;                         /* the Brick's ALSA (audiocodec): 512 crackled under load */
#else
    want.samples = 512;
#endif
    if (getenv("MD_AUDIO_FRAMES"))
        want.samples = (Uint16)atoi(getenv("MD_AUDIO_FRAMES"));
    want.callback = audio_cb;                    /* (our ring: see audio_cb) */
    audio_want = want;
    out_lock = SDL_CreateMutex();
    {
        char usb[128];
        dev = audio_open(audio_pick(usb, sizeof usb));   /* (0.80: a USB-C sound card, when one is in) */
    }
    fflush(stderr);

#if defined(__APPLE__)
    if (env_is("MD_VPAD_TEST", "1"))
        vpad_attach();
#endif
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i) && !pad)
            pad = SDL_GameControllerOpen(i);
        if (!joy)
            joy = SDL_JoystickOpen(i);
    }
    bk_set_keyboard(desktop && !pad && !joy);    /* (a PC with only the keyboard: the hints name its keys, 0.78) */
    if (!desktop && !env_is("MD_KEYS", "handheld"))
        f_keys_fallback(pad);

    if (ahead_env)
        audio_ahead = (uint32_t)atoi(ahead_env), ahead_fixed = 1;
    if (audio_ahead < 256u)
        audio_ahead = 256u;
    if (getenv("MD_DEVICE"))
        bk_set_device(getenv("MD_DEVICE"));
    if (env_is("MD_KEYS", "handheld"))
        bk_set_handheld(1);                      /* (no MENU, F1, F2, FN: hints and help point to the quick bar) */
    if (env_is("MD_SIDE_HALF", "1")) {           /* (1.5.8) the columns every other frame in the light picture (Miyoo) */
        bk_set_side_half(1);
        fprintf(stderr, "video: in the light picture the columns beside the stage every other frame while nothing is pressed\n");
    }
    dprof = getenv("MD_PROFILE_DRAW") != 0;
    {   /* MD_LIGHT: 1 the light picture from the start (the weak handhelds' launchers), 0 never by itself;
           else (auto) it comes on by itself when the stage keeps taking longer than 60 ms a frame */
        const char *lt = getenv("MD_LIGHT");
        light_auto = !lt || strcmp(lt, "0");
        if (lt && !strcmp(lt, "1"))
            bk_set_light(1), light_auto = 0;
        fprintf(stderr, "picture: %s\n", bk_light_on() ? "light" : light_auto ? "full (light by itself if slow)" : "full");
        if (env_is("MD_CALM", "1"))
            bk_set_calm(1), fprintf(stderr, "motion: calm\n");
        if (getenv("MD_LANG"))                   /* (0.80) MD_LANG=de / en: the language from the start (else the saved one) */
            fprintf(stderr, "language: %s (%d texts)\n", bk_set_lang_code(getenv("MD_LANG")) >= 0 ? bk_lang_code() : "en", bk_lang_on());
    }
    if (getenv("MD_SOUND_SLOW_US"))
        sound_slow_us = (uint32_t)atoi(getenv("MD_SOUND_SLOW_US"));
    if (getenv("MD_DRAW_SLOW_MS"))
        draw_slow_ms = (uint32_t)atoi(getenv("MD_DRAW_SLOW_MS"));
    if (audio_ahead > ahead_max)
        ahead_max = audio_ahead;
    {
        const char *bu = getenv("MD_AUDIO_BURST");
        ahead_burst = bu ? (uint32_t)atoi(bu) : SDL_GetCPUCount() <= 2 ? 2048u : 0u;
        if (ahead_burst > 8192u)
            ahead_burst = 8192u;
        if (ahead_burst)
            fprintf(stderr, "audio: up to %u frames more made ahead while the stage rests (%.0f ms)\n", (unsigned)ahead_burst,
                    ahead_burst * 1000.0 / BK_RATE);
    }
    fprintf(stderr, "audio: kept %u frames ahead (%.0f ms)%s, the card's buffer %d frames (%.0f ms)\n", (unsigned)audio_ahead,
            audio_ahead * 1000.0 / BK_RATE, ahead_fixed ? "" : ", more by itself after a gap", dev_frames, dev_frames * 1000.0 / dev_rate);
    bk_set_draw_yield(draw_yield);
    core_lock = SDL_CreateMutex();
    bk_init(save);
    if (!getenv("MD_LANG")) {                    /* (1.5.9) every version starts in English (Kid Bunka, 20.11.2026); German
                                                    only when chosen (quick bar / OPTIONS: DEUTSCH, kept in save/) or with
                                                    MD_LANG=de. Until 1.5.8 a German system started in German by itself.
                                                    The system's language is still told in the log. */
        const char *la = getenv("LC_ALL"), *lm = getenv("LC_MESSAGES"), *ln = getenv("LANG");
        const char *l = la && *la ? la : lm && *lm ? lm : ln;
        int de = l && !strncmp(l, "de", 2);
#if SDL_VERSION_ATLEAST(2, 0, 14)
        if (!l || !*l) {                         /* (1.0: no locale in the environment - Android, Onion: ask the system) */
            SDL_Locale *loc = SDL_GetPreferredLocales();
            de = loc && loc[0].language && !strcmp(loc[0].language, "de");
            SDL_free(loc);
        }
#endif
        fprintf(stderr, "language: the system's is %s; English unless German was chosen (%s now)\n", de ? "German" : "not German",
                bk_lang_on() ? bk_lang_code() : "en");
    }
    bk_battery(read_battery());
    audio_dev = dev;
    if (dev) {
        SDL_PauseAudioDevice(dev, 0);
    }
    started = SDL_GetTicks();
    run_s = run_env ? (uint32_t)atoi(run_env) : 0u;
    if (!getenv("MD_AUTOPLAY") && !getenv("MD_NO_SPLASH"))
    {
        bk_splash(1);                            /* the start screen (not for the self-test) */
        bk_help_first_run(1);                    /* and once, the button map */
    }
    if (getenv("MD_AUTOPLAY")) {                 /* START tapped once, after the intro */
        float warm[2 * 256];
        warm_ms = 600u * 256u * 1000u / BK_RATE + 1000u * 256u / BK_RATE;   /* (1.0.2: not counted as the session's sound) */
        for (int i = 0; i < 600; i++)
            bk_render(warm, 256);
        bk_input(1u << BK_START);
        bk_render(warm, 256);
        bk_input(0);
    }
    if (dev)
        render_thread = SDL_CreateThread(render_loop, "render", 0);
    draw_fps = fps ? fps : 30u;
    /* (1.1) the stage on its own thread - (1.5.1) where there are more than two cores: on the Miyoo's two A7 cores it
       fought the sound for them (1.5.0: the stage waited 59 ms a frame for the sound, 7.5 pictures a second against
       12.6 with 1.0.2); MD_DRAW_THREAD=1 / =0 decides it either way */
    if (env_is("MD_DRAW_THREAD", "1") || (!env_is("MD_DRAW_THREAD", "0") && SDL_GetCPUCount() > 2)) {
        fbuf[0] = (uint32_t *)malloc((size_t)out_w * (size_t)out_h * 4u);
        fbuf[1] = (uint32_t *)malloc((size_t)out_w * (size_t)out_h * 4u);
        fb_lock = SDL_CreateMutex();
        if (fbuf[0] && fbuf[1] && fb_lock) {
            SDL_AtomicSet(&draw_run, 1);
            draw_thread = SDL_CreateThread(draw_loop, "stage", 0);
        }
        fprintf(stderr, "video: %s\n", draw_thread ? "the stage drawn on a thread of its own (the next frame while one is shown)" : "the stage drawn on the main thread");
    } else
        fprintf(stderr, "video: the stage drawn on the main thread (%d cores)\n", SDL_GetCPUCount());

    while (running && !bk_quit_requested()) {
        SDL_Event e;
        uint32_t held;
        static int stick_new;
#if defined(__APPLE__)
        if (vpad_js && vpad_tick(core_lock, SDL_GetTicks() - started))
            running = 0;
#endif
        (void)underruns;
        while (SDL_PollEvent(&e)) {
            int c = -1, down = 0;
            switch (e.type) {
            case SDL_QUIT: running = 0; break;
#ifdef MD_HAVE_TOUCH
            case SDL_APP_WILLENTERBACKGROUND: case SDL_APP_DIDENTERBACKGROUND: case SDL_APP_WILLENTERFOREGROUND:
                if (touch_on)
                    touch_release_all();         /* (a finger can not be told of what happens while away) */
                break;
            case SDL_WINDOWEVENT:
                if (touch_on && (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST || e.window.event == SDL_WINDOWEVENT_MINIMIZED ||
                                 e.window.event == SDL_WINDOWEVENT_HIDDEN || e.window.event == SDL_WINDOWEVENT_LEAVE ||
                                 e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED))
                    touch_release_all();
                break;
#endif
#ifdef MD_HAVE_TOUCH
            case SDL_FINGERDOWN: case SDL_FINGERMOTION: case SDL_FINGERUP:
                if (touch_on)
                    touch_finger(e.tfinger.fingerId, e.tfinger.x, e.tfinger.y,
                                 e.type == SDL_FINGERDOWN ? 0 : e.type == SDL_FINGERMOTION ? 1 : 2);
                break;
            case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: case SDL_MOUSEMOTION:   /* (the Mac: the mouse as a finger) */
                if (touch_on && e.button.which != SDL_TOUCH_MOUSEID) {
                    int ww = 1, wh = 1, mx = e.type == SDL_MOUSEMOTION ? e.motion.x : e.button.x,
                        my = e.type == SDL_MOUSEMOTION ? e.motion.y : e.button.y;
                    SDL_GetWindowSize(win, &ww, &wh);
                    if (e.type == SDL_MOUSEMOTION && !(e.motion.state & SDL_BUTTON_LMASK))
                        break;
                    touch_finger(-1, (float)mx / ww, (float)my / wh,
                                 e.type == SDL_MOUSEBUTTONDOWN ? 0 : e.type == SDL_MOUSEMOTION ? 1 : 2);
                }
                break;
#endif
            case SDL_KEYDOWN: case SDL_KEYUP:
                if (e.key.repeat)
                    break;
#ifdef MD_HAVE_TOUCH
                if (e.key.keysym.sym == SDLK_AC_BACK) {   /* (Android's back key: B) */
                    held_kbd = e.type == SDL_KEYDOWN ? held_kbd | 1u << BK_B : held_kbd & ~(1u << BK_B);
                    break;
                }
                if (e.key.keysym.sym == SDLK_MENU || e.key.keysym.sym == SDLK_APPLICATION) {   /* (an Android device's or
                                                    controller's own menu key: the menu) */
                    held_kbd = e.type == SDL_KEYDOWN ? held_kbd | 1u << BK_MENU : held_kbd & ~(1u << BK_MENU);
                    break;
                }
                if (touch_on && e.type == SDL_KEYDOWN)
                    touch_hide();
#endif
                c = key_control(e.key.keysym.sym);
                down = e.type == SDL_KEYDOWN;
                if (down && input_logged < 60 && !desktop) {   /* (a handheld's extra keys may come as keys) */
                    fprintf(stderr, "input: key %s (%d) -> %s\n", SDL_GetKeyName(e.key.keysym.sym), (int)e.key.keysym.sym, c >= 0 ? NAMES[c] : "(nothing)");
                    input_logged++;
                }
                if (c == BK_FN) {                /* a key stands in for the latching switch */
                    if (down)
                        fn_latched ^= 1u << BK_FN;
                    c = -1;
                }
                if (c >= 0)
                    held_kbd = down ? held_kbd | 1u << c : held_kbd & ~(1u << c);
                break;
            case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP:
#ifdef MD_HAVE_TOUCH
                if (touch_on && e.type == SDL_CONTROLLERBUTTONDOWN)
                    touch_hide();
#endif
                c = pad_control(e.cbutton.button);
                if ((nintendo || swap_ab) && (c == BK_A || c == BK_B))
                    c = c == BK_A ? BK_B : BK_A;
                if ((nintendo || swap_xy) && (c == BK_X || c == BK_Y))
                    c = c == BK_X ? BK_Y : BK_X;
                if (keytest)
                    printf("controller button %d %s\n", e.cbutton.button, e.type == SDL_CONTROLLERBUTTONDOWN ? "down" : "up");
                if (e.type == SDL_CONTROLLERBUTTONDOWN && input_logged < 60) {
                    fprintf(stderr, "input: controller button %d -> %s\n", e.cbutton.button, c >= 0 ? NAMES[c] : "(nothing)");
                    input_logged++;
                }
                if (c >= 0)
                    held_pad = e.type == SDL_CONTROLLERBUTTONDOWN ? held_pad | 1u << c : held_pad & ~(1u << c);
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
                    held_pad = e.caxis.value > 16000 ? held_pad | 1u << BK_L2 : held_pad & ~(1u << BK_L2);
                else if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
                    held_pad = e.caxis.value > 16000 ? held_pad | 1u << BK_R2 : held_pad & ~(1u << BK_R2);
                else if (pad)                            /* (1.5.10) noted here, given to the core once a turn of this loop, under its lock:
                                                            a stick sends a hundred events a second, the core's lock may be held by a frame */
                    stick_new = 1;
                break;
            case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP:
#ifdef MD_HAVE_TOUCH
                if (touch_on && e.type == SDL_JOYBUTTONDOWN)
                    touch_hide();
#endif
                if (keytest)
                    printf("joystick button %d %s\n", e.jbutton.button, e.type == SDL_JOYBUTTONDOWN ? "down" : "up");
                if (e.type == SDL_JOYBUTTONDOWN && input_logged < 60) {
                    fprintf(stderr, "input: joystick button %d\n", e.jbutton.button);
                    input_logged++;
                }
                for (int i = 0; i < BK_NBUTTONS; i++)
                    if (joy_map[i] == e.jbutton.button)
                        held_pad = e.type == SDL_JOYBUTTONDOWN ? held_pad | 1u << i : held_pad & ~(1u << i);
                break;
            case SDL_AUDIODEVICEADDED:          /* (0.80) a USB card came: the sound goes to it */
                if (!e.adevice.iscapture && !audio_name[0] && audio_dev) {
                    char usb[128];
                    if (audio_pick(usb, sizeof usb))
                        audio_switch(usb);
                }
                break;
            case SDL_AUDIODEVICEREMOVED:        /* the card in use is gone: back to the Brick's own */
                if (!e.adevice.iscapture && audio_dev && e.adevice.which == audio_dev) {
                    char usb[128];
                    audio_switch(audio_pick(usb, sizeof usb));
                }
                break;
            case SDL_CONTROLLERDEVICEADDED:
                if (!pad)
                    pad = SDL_GameControllerOpen(e.cdevice.which);
                SDL_LockMutex(core_lock);
                bk_set_keyboard(0);                  /* (a controller came: the hints name its buttons) */
                SDL_UnlockMutex(core_lock);
                break;
            }
        }
        held = held_kbd | held_pad | fn_latched;
#ifdef MD_HAVE_TOUCH
        held |= held_touch | touch_latch;
#endif
        SDL_AtomicSet(&input_waiting, 1);        /* (1.1) the stage steps aside for the buttons */
        SDL_LockMutex(core_lock);
        SDL_AtomicSet(&input_waiting, 0);
        if (stick_new && pad) {
            bk_sticks(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX), SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY),
                      SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX), SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY));
            stick_new = 0;
        }
#ifdef MD_HAVE_TOUCH
        if (touch_on)
            touch_sync();
        touch_flush();
        if (touch_on)
            touch_chips_refresh();
#endif
        bk_input(held);
        SDL_UnlockMutex(core_lock);

        if (run_s && SDL_GetTicks() - started >= run_s * 1000u)
            running = 0;
        if (SDL_GetTicks() - started > 1000u) {
            uint32_t c = bk_cpu_pct();          /* (a racy read of a counter: for the self-test only) */
            cpu_sum += c;
            cpu_n++;
            if (c > cpu_max)
                cpu_max = c;
        }
        if (!dev) {                              /* no sound card: the device still runs */
            static float buf[2 * 256];
            SDL_LockMutex(core_lock);
            bk_render(buf, 256);
            SDL_UnlockMutex(core_lock);
            SDL_Delay(5);
        }

        if (draw_thread) {                       /* (1.1) a frame the stage's thread has made: shown */
            int r;
            SDL_LockMutex(fb_lock);
            r = SDL_AtomicGet(&fb_ready);
            SDL_AtomicSet(&fb_ready, 0);
            SDL_AtomicSet(&fb_shown, r);
            draw_us_sum = draw_us_sum_t, draw_us_max = draw_us_max_t;
            SDL_UnlockMutex(fb_lock);
            if (r) {
                uint64_t d1 = SDL_GetPerformanceCounter();
                present_frame(win, win_surf, ren, tex, fbuf[r - 1], frames_drawn);
                SDL_AtomicSet(&fb_shown, 0);
                present_us_sum += (SDL_GetPerformanceCounter() - d1) * 1000000u / SDL_GetPerformanceFrequency();
                if (!frames_drawn) {              /* (the launcher's start watchdog waits for this line) */
                    fprintf(stderr, "startup: first frame presented\n");
                    fflush(stderr);
                }
                frames_drawn++;
            } else
                SDL_Delay(1);
        } else
        if ((int32_t)(SDL_GetTicks() - next_frame) >= 0) {
            /* (1.5.7) the frames on a steady beat: the next one a period after this one was due, not after it began -
               a frame started a few ms late (the wait's grain, the sound's turn) no longer pushed all later ones back
               (15 fps asked, 13.5 had on a stage that kept up easily); fallen a whole period behind: from now */
            uint32_t per = 1000u / (fps ? fps : 30u), now_ms = SDL_GetTicks();
            next_frame += per;
            if ((int32_t)(now_ms - next_frame) >= 0 || (int32_t)(next_frame - now_ms) > (int32_t)per)
                next_frame = now_ms + per;
            uint64_t d0 = SDL_GetPerformanceCounter();
            uint32_t dus;
            const uint32_t *stage;
            uint64_t d1;
            SDL_AtomicSet(&stage_waiting, 1);
            SDL_LockMutex(core_lock);
            SDL_AtomicSet(&stage_waiting, 0);
            dp_t0 = SDL_GetPerformanceCounter();
            stage = bk_draw();
            if (dprof)
                dp_add(1, SDL_GetPerformanceCounter());   /* (the end of the frame) */                   /* (only this thread draws: read after the lock, no copy) */
            if (bk_resting_now())                 /* (1.4: resting, 4 pictures a second) */
                next_frame = SDL_GetTicks() + 250u;
            SDL_UnlockMutex(core_lock);
            const uint32_t *px = out_w != BK_W ? scale_frame(stage) : stage;
            d1 = SDL_GetPerformanceCounter();
            dus = (uint32_t)((d1 - d0) * 1000000u / SDL_GetPerformanceFrequency());
            draw_us_sum += dus;
            if (light_auto && frames_drawn > 30) {   /* slow for 3 s on end: the light picture, once */
                static float avg_ms;
                static uint32_t slow_since;
                avg_ms += ((float)dus / 1000.0f - avg_ms) * 0.1f;
                if (avg_ms > 60.0f) {
                    if (!slow_since)
                        slow_since = SDL_GetTicks() ? SDL_GetTicks() : 1u;   /* (not "| 1u": one ms ahead at an even tick, 0.76) */
                    else if (SDL_GetTicks() - slow_since > 3000u) {
                        SDL_LockMutex(core_lock);
                        bk_set_light(2);
                        SDL_UnlockMutex(core_lock);
                        light_auto = 0;
                        fprintf(stderr, "picture: light by itself (the stage took %.0f ms a frame)\n", avg_ms);
                    }
                } else
                    slow_since = 0;
            }
            if (dus > draw_us_max)
                draw_us_max = dus;
            present_frame(win, win_surf, ren, tex, px, frames_drawn);
            present_us_sum += (SDL_GetPerformanceCounter() - d1) * 1000000u / SDL_GetPerformanceFrequency();
            if (!frames_drawn) {                  /* (the launcher's start watchdog waits for this line) */
                fprintf(stderr, "startup: first frame presented\n");
                fflush(stderr);
            }
            frames_drawn++;
        } else
            SDL_Delay(2);
        {   /* every 30 s a line into log.txt: how the sound and the picture are doing */
            static uint32_t next_status, last_frames, last_under;
            static uint64_t last_draw_sum, last_present_sum, last_wait_sum;
            if (!next_status)
                next_status = SDL_GetTicks() + 30000u;
            if ((int32_t)(SDL_GetTicks() - next_status) >= 0) {
                uint32_t nf = frames_drawn - last_frames;
                uint64_t ws = (uint64_t)SDL_AtomicGet(&draw_wait_us);
                fprintf(stderr, "status %.0f s: %u underruns (+%u), %.1f fps, draw avg %.1f ms (of it waiting for the sound %.1f), "
                        "present avg %.1f ms, ahead %u, sound load now %u%%, page %s\n",
                        (SDL_GetTicks() - started) / 1000.0, (unsigned)underruns, (unsigned)(underruns - last_under), nf / 30.0,
                        nf ? (double)(draw_us_sum - last_draw_sum) / 1000.0 / nf : 0.0,
                        nf ? (double)(ws - last_wait_sum) / 1000.0 / nf : 0.0,
                        nf ? (double)(present_us_sum - last_present_sum) / 1000.0 / nf : 0.0, (unsigned)audio_ahead,
                        (unsigned)bk_cpu_pct(), bk_page_name());
                last_present_sum = present_us_sum, last_wait_sum = ws;
                if (dprof)
                    dp_report();
                fflush(stderr);
                last_frames = frames_drawn, last_under = underruns, last_draw_sum = draw_us_sum, next_status += 30000u;
            }
        }
        if ((int32_t)(SDL_GetTicks() - next_batt) >= 0) {
            next_batt = SDL_GetTicks() + 10000u;
            int pct = read_battery();
            SDL_LockMutex(core_lock);            /* (1.1: the stage reads it on its own thread) */
            bk_battery(pct);
            SDL_UnlockMutex(core_lock);
        }
    }
    SDL_AtomicSet(&draw_run, 0);
    if (draw_thread)
        SDL_WaitThread(draw_thread, 0);
    render_run = 0;
    if (render_thread)
        SDL_WaitThread(render_thread, 0);
    double engine_s = (bk_ms() - warm_ms) / 1000.0;   /* (the time the engine sounded: equal to the clock when the rate is right;
                                                    the self-test's warm-up, 3.5 s before the clock starts, left out since 1.0.2) */
    bk_shutdown();
    {                                            /* (always: a session's summary into log.txt) */
        SDL_RendererInfo ri;
        ri.name = "window surface";
        if (ren)
            SDL_GetRendererInfo(ren, &ri);
        fflush(stdout);
        printf("%s: %.1f s, sound %s %d Hz %d ch %d frames/buffer, renderer %s, %u stage frames (%.1f/s), "
               "%u underruns, sound load avg %u%% max %u%%, stage draw avg %.2f ms max %.2f ms, page %s, biome %s, playing %d, "
               "engine %.1f s, kept %u frames ahead at the end\n",
               run_s ? "self-test" : "session", (SDL_GetTicks() - started) / 1000.0, audio_dev ? "on" : "OFF", audio_have.freq, audio_have.channels, audio_have.samples,
               ri.name, frames_drawn, frames_drawn * 1000.0 / (SDL_GetTicks() - started), underruns,
               cpu_n ? cpu_sum / cpu_n : 0u, cpu_max, frames_drawn ? draw_us_sum / 1000.0 / frames_drawn : 0.0,
               draw_us_max / 1000.0, bk_page_name(),
               bk_biome_name(), bk_playing(), engine_s, (unsigned)audio_ahead);
    }
    for (uint32_t i = 0; i < underruns && i < 8; i++)
            printf("underrun at %u ms\n", (unsigned)underrun_at[i]);
    if (audio_dev)
        SDL_CloseAudioDevice(audio_dev);
    SDL_Quit();
    return 0;
}
