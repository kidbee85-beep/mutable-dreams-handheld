/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the engine's 44.1 kHz to the card's 48 kHz, in float, before the
 * sound becomes 16 bits (0.63). A polyphase windowed-sinc resampler, 160 up / 147 down (48000 / 44100 = 160 / 147),
 * 48 taps per phase, Kaiser window (beta 9.5): flat to 18 kHz, -0.3 dB at 19 kHz, -2.6 dB at 20 kHz; what it adds
 * (images, ripple, rounding) stays below -97 dB up to 20 kHz (a sine test). About 4.6 million multiply-adds a
 * second for stereo, some 1.5 % of one Brick core.
 * Until 0.62 SDL converted the already dithered 16-bit stream with its own short filter. */
#ifndef BRICK_RESAMPLE_H
#define BRICK_RESAMPLE_H
#include <math.h>
#include <string.h>

#define RS_L 160                                  /* phases (up) */
#define RS_M 147                                  /* step (down) */
#define RS_T 48                                   /* taps per phase */

typedef struct {
    float h[RS_L][RS_T];                          /* phase p, tap k: the prototype's h[p + RS_L * k] */
    float hist[2][2 * RS_T];                      /* each channel's last RS_T inputs, twice (no wrap when read) */
    int at, p;
} rs_state;

static double rs_bessel_i0(double x)
{
    double s = 1, t = 1;
    for (int k = 1; k < 40; k++) {
        t *= (x / (2 * k)) * (x / (2 * k));
        s += t;
    }
    return s;
}

static void rs_init(rs_state *r)
{
    const int n = RS_L * RS_T;
    const double fs_up = 44100.0 * RS_L, fc = 20600.0, beta = 9.5, mid = (n - 1) / 2.0;
    memset(r, 0, sizeof *r);
    for (int i = 0; i < n; i++) {
        double x = i - mid, w = 2.0 * fc / fs_up, s = x == 0 ? w : sin(M_PI * w * x) / (M_PI * x);
        double k = 2.0 * i / (n - 1) - 1.0, win = rs_bessel_i0(beta * sqrt(1 - k * k)) / rs_bessel_i0(beta);
        r->h[i % RS_L][i / RS_L] = (float)(s * win);
    }
    for (int p = 0; p < RS_L; p++) {              /* each phase summing to exactly 1: no ripple at DC */
        double sum = 0;
        for (int k = 0; k < RS_T; k++)
            sum += r->h[p][k];
        for (int k = 0; k < RS_T; k++)
            r->h[p][k] = (float)(r->h[p][k] / sum);
    }
}

/* n stereo frames in (interleaved), the 48 kHz frames out into out (room for n * 160 / 147 + 2); returns their count */
static int rs_run(rs_state *r, const float *in, int n, float *out)
{
    int o = 0;
    for (int j = 0; j < n; j++) {
        r->at = (r->at + RS_T - 1) % RS_T;        /* the newest input first: hist[c][at + k] is x[j - k] */
        r->hist[0][r->at] = r->hist[0][r->at + RS_T] = in[2 * j];
        r->hist[1][r->at] = r->hist[1][r->at + RS_T] = in[2 * j + 1];
        while (r->p < RS_L) {
            const float *h = r->h[r->p], *a = &r->hist[0][r->at], *b = &r->hist[1][r->at];
            float sl = 0, sr = 0;
            for (int k = 0; k < RS_T; k++)
                sl += h[k] * a[k], sr += h[k] * b[k];
            out[2 * o] = sl, out[2 * o + 1] = sr;
            o++;
            r->p += RS_M;
        }
        r->p -= RS_L;
    }
    return o;
}
#endif
