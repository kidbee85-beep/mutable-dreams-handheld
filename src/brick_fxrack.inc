/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: two FX RACKs for the Lab's sources (VOICES, SAMPLES, SYNTH, POLY).
 *
 * A source sent INTO a rack goes through its chain before it meets the mix (and its sends):
 *   FILTER     low, band or high pass (a zero-delay state-variable filter) with RESONANCE; FOLLOW opens or closes it with
 *              the signal's own loudness (an envelope follower), SWEEP moves it with a slow LFO
 *   DRIVE      warm saturation;  CRUSH  fewer bits and a lower sample rate
 *   RESONATOR  Mutable Instruments' Rings: the signal rings a modal body, sympathetic strings or a string, tuned
 *              to the Biome's root (RES NOTE semitones from it); STRUCTURE, BRIGHTNESS, DAMPING, POSITION
 *   ECHO       a tape echo in time (1/16 .. 1/2), FEEDBACK, WOW (the tape wavering), TONE (darker repeats),
 *              the repeats crossing left and right
 * Each stage costs nothing while it is at 0. Several sources in one rack are summed; the result takes the first
 * one's place (and its sends). Included by brick_lab.inc after SYNTH. */

typedef struct {
    float lp[2], bp[2], env, lfo, crush_hold[2], crush_count;
    float *echo;                                  /* 2 x 2.2 s, interleaved */
    uint32_t echo_w;
    float echo_lp[2], wow;
    float level_in, level_out, cutoff_now, res_level, echo_level;   /* (the pictures) */
} fx_rack;
#define FX_ECHO_FRAMES 97020u                    /* 2.2 s */
static fx_rack fx_r[2];
static const char *const FX_FILTERS[4] = {"OFF", "LOW PASS", "BAND PASS", "HIGH PASS"};
static const char *const FX_ECHO_TIMES[6] = {"1/16", "1/8", "1/8 DOTTED", "1/4", "1/4 DOTTED", "1/2"};
static const float FX_ECHO_BEATS[6] = {0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f};
static const char *const FX_RES_MODELS[6] = {"MODAL", "SYMPATHETIC STRINGS", "STRING", "FM VOICE", "SYMPATHETIC CHORDS", "STRING + REVERB"};

#define FXP(r, name) ((r) ? P_RB_##name : P_RA_##name)

static float fx_cutoff_hz(float pct) { return 40.0f * powf(400.0f, pct / 100.0f); }   /* 40 Hz .. 16 kHz */

static int fx_routed(int r, int src)             /* src: 0 VOICES, 1 SAMPLES, 2 SYNTH, 3 POLY; the rack it goes into */
{
    int pa = P_RA_IN_VOICES + src, pb = P_RB_IN_VOICES + src;
    if (lab_v[pa] > 0.5f)
        return r == 0;
    return r == 1 && lab_v[pb] > 0.5f;
}

static void fx_rack_process(int r, float *buf, uint32_t n)
{
    fx_rack *x = &fx_r[r];
    int ftype = (int)lab_v[FXP(r, FILTER)];
    float res = lab_v[FXP(r, RESONANCE)] / 100.0f, follow = lab_v[FXP(r, FOLLOW)] / 100.0f;
    float sweep = lab_v[FXP(r, SWEEP)] / 100.0f, srate = 0.02f * powf(300.0f, lab_v[FXP(r, SWEEP_RATE)] / 100.0f);
    float drive = lab_v[FXP(r, DRIVE)] / 100.0f, crush = lab_v[FXP(r, CRUSH)] / 100.0f;
    float rmix = lab_v[FXP(r, RES_MIX)] / 100.0f, emix = lab_v[FXP(r, ECHO_MIX)] / 100.0f;
    float lin = 0, lout = 0;
    uint32_t k;
    for (k = 0; k < 2 * n; k++)
        lin += buf[k] * buf[k];
    x->level_in = sqrtf(lin / (float)(2 * n));
    if (ftype > 0) {                              /* FILTER: a zero-delay state-variable filter (after Simper), stable at any
                                                     cutoff; its cutoff following the signal and sweeping */
        float kd = 2.0f - 1.9f * res, base = lab_v[FXP(r, CUTOFF)], a1 = 0, a2 = 0, a3 = 0;
        for (k = 0; k < n; k++) {
            float a = fabsf(buf[2 * k]) + fabsf(buf[2 * k + 1]);
            x->env += (a - x->env) * (a > x->env ? 0.01f : 0.0005f);
            x->lfo += srate / 44100.0f;
            if (x->lfo >= 1.0f)
                x->lfo -= 1.0f;
            if ((k & 15u) == 0) {                 /* (the coefficients every 16 samples) */
                float pct = base + follow * 60.0f * fminf(x->env * 6.0f, 1.0f) + sweep * 40.0f * sinf(6.2831853f * x->lfo), g;
                pct = pct < 0 ? 0 : pct > 100 ? 100 : pct;
                x->cutoff_now = pct;
                g = tanf(3.1415927f * fminf(fx_cutoff_hz(pct), 18000.0f) / 44100.0f);
                a1 = 1.0f / (1.0f + g * (g + kd)), a2 = g * a1, a3 = g * a2;
            }
            for (int c = 0; c < 2; c++) {
                float in = buf[2 * k + c], v3 = in - x->lp[c], v1 = a1 * x->bp[c] + a2 * v3, v2 = x->lp[c] + a2 * x->bp[c] + a3 * v3;
                x->bp[c] = 2.0f * v1 - x->bp[c];  /* (bp, lp hold the two integrators' states) */
                x->lp[c] = 2.0f * v2 - x->lp[c];
                buf[2 * k + c] = ftype == 1 ? v2 : ftype == 2 ? v1 * (0.6f + res) : in - kd * v1 - v2;
            }
        }
    }
    if (drive > 0) {                              /* DRIVE: warmth, made up for in level */
        float g = 1.0f + drive * 12.0f, comp = 1.0f / tanhf(g * 0.25f) * 0.25f;
        for (k = 0; k < 2 * n; k++)
            buf[k] = tanhf(buf[k] * g) * comp;
    }
    if (crush > 0) {                              /* CRUSH: fewer bits, held samples */
        float levels = powf(2.0f, 16.0f - 12.0f * crush), hold = 1.0f + crush * 30.0f;
        for (k = 0; k < n; k++) {
            x->crush_count += 1.0f;
            if (x->crush_count >= hold) {
                x->crush_count -= hold;
                for (int c = 0; c < 2; c++)
                    x->crush_hold[c] = floorf(buf[2 * k + c] * levels + 0.5f) / levels;
            }
            buf[2 * k] = x->crush_hold[0];
            buf[2 * k + 1] = x->crush_hold[1];
        }
    }
    if (rmix > 0) {                               /* RESONATOR: Rings, rung by the signal */
        static float mono[APP_BLOCK], rl[APP_BLOCK], rr[APP_BLOCK];
        mi_rings_params rp;
        float lv = 0;
        memset(&rp, 0, sizeof rp);
        rp.model = (int)lab_v[FXP(r, RES_MODEL)], rp.polyphony = 2, rp.chord = 0;
        rp.note = (float)(48 + (int)(md_state()->root % 12)) + lab_v[FXP(r, RES_NOTE)] + 12.0f * (float)dream_ui.octave;
        rp.structure = lab_v[FXP(r, STRUCTURE)] / 100.0f, rp.brightness = lab_v[FXP(r, BRIGHTNESS)] / 100.0f;
        rp.damping = lab_v[FXP(r, DAMPING)] / 100.0f, rp.position = lab_v[FXP(r, POSITION)] / 100.0f;
        for (k = 0; k < n; k++)
            mono[k] = (buf[2 * k] + buf[2 * k + 1]) * 1.2f;
        mi_rings_process(r, &rp, mono, rl, rr, n);
        for (k = 0; k < n; k++) {
            buf[2 * k] = buf[2 * k] * (1.0f - rmix) + rl[k] * rmix * 0.9f;
            buf[2 * k + 1] = buf[2 * k + 1] * (1.0f - rmix) + rr[k] * rmix * 0.9f;
            lv += rl[k] * rl[k];
        }
        x->res_level = sqrtf(lv / (float)n);
    } else
        x->res_level = 0;
    if (emix > 0) {                               /* ECHO: a tape in time, crossing sides */
        double beat = 60.0 / (double)(md_tempo_now() > 20 ? md_tempo_now() : 20);
        float fb = lab_v[FXP(r, ECHO_FEEDBACK)] / 100.0f * 0.92f, wow = lab_v[FXP(r, WOW)] / 100.0f;
        float tone = 0.08f + 0.9f * lab_v[FXP(r, ECHO_TONE)] / 100.0f, lv = 0;
        float d = (float)(beat * FX_ECHO_BEATS[(int)lab_v[FXP(r, ECHO_TIME)] % 6] * 44100.0);
        if (!x->echo && !(x->echo = (float *)calloc(2 * FX_ECHO_FRAMES, sizeof(float))))
            emix = 0;
        if (d > (float)(FX_ECHO_FRAMES - 4))
            d = (float)(FX_ECHO_FRAMES - 4);
        for (k = 0; k < n && emix > 0; k++) {
            float dd, fr, out[2];
            uint32_t a;
            x->wow += 0.6f / 44100.0f;
            dd = d * (1.0f + wow * 0.004f * sinf(6.2831853f * x->wow) + wow * 0.0015f * sinf(6.2831853f * x->wow * 4.7f));
            a = (uint32_t)dd;
            fr = dd - (float)a;
            for (int c = 0; c < 2; c++) {
                uint32_t i0 = (x->echo_w + FX_ECHO_FRAMES - a) % FX_ECHO_FRAMES, i1 = (i0 + FX_ECHO_FRAMES - 1) % FX_ECHO_FRAMES;
                out[c] = x->echo[2 * i0 + c] * (1.0f - fr) + x->echo[2 * i1 + c] * fr;
            }
            for (int c = 0; c < 2; c++) {         /* the repeat, darker, into the other side */
                float in = buf[2 * k + c] + out[1 - c] * fb;
                x->echo_lp[c] += (in - x->echo_lp[c]) * tone;
                x->echo[2 * x->echo_w + c] = tanhf(x->echo_lp[c]);
            }
            x->echo_w = (x->echo_w + 1) % FX_ECHO_FRAMES;
            buf[2 * k] += out[0] * emix;
            buf[2 * k + 1] += out[1] * emix;
            lv += out[0] * out[0];
        }
        x->echo_level = sqrtf(lv / (float)n);
    } else
        x->echo_level = 0;
    for (k = 0; k < 2 * n; k++)
        lout += buf[k] * buf[k];
    x->level_out = sqrtf(lout / (float)(2 * n));
}

/* the racks over the sources' blocks (before they meet the mix): bufs[src] and whether it sounds */
static void fx_run(float *const bufs[4], const int active[4], uint32_t n)
{
    static float sum[2 * APP_BLOCK];
    for (int r = 0; r < 2; r++) {
        int first = -1;
        uint32_t k;
        for (int s = 0; s < 4; s++)
            if (active[s] && fx_routed(r, s)) {
                if (first < 0) {
                    first = s;
                    memcpy(sum, bufs[s], sizeof(float) * 2 * n);
                } else {
                    for (k = 0; k < 2 * n; k++)
                        sum[k] += bufs[s][k];
                    memset(bufs[s], 0, sizeof(float) * 2 * n);
                }
            }
        if (first < 0) {
            fx_r[r].level_in = fx_r[r].level_out = 0;
            continue;
        }
        fx_rack_process(r, sum, n);
        memcpy(bufs[first], sum, sizeof(float) * 2 * n);
    }
}
