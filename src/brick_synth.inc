/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: SYNTH, six instruments of the garden for the Sound Lab.
 *
 *   JUST DRONE     additive, in just intonation over the Biome's root: the harmonic series, a stacked 4:5:6
 *                  chord, or septimal ratios (CHARACTER); each partial breathes on its own, a little beating (MOTION)
 *   MODAL GARDEN   eight resonators between the key's notes and inharmonic bell ratios (CHARACTER), struck by
 *                  a rain of small impulses (DENSITY) or by notes
 *   SHEPARD        the Shepard-Risset glissando: octave components under a bell over log frequency, rising
 *                  (CHARACTER below 50) or falling without end; MOTION its speed
 *   KARPLUS RAIN   eight plucked strings (Karplus-Strong) in the key; drops by DENSITY; CHARACTER from harp to nylon
 *   GOLDEN BELLS   two-operator FM with an irrational ratio (CHARACTER: sqrt 2, golden ratio, pi / 2, e / 2):
 *                  never quite in tune, never quite out
 *   FORMANT CHOIR  three voices of the key's chord through vowel formants (CHARACTER: a e i o u), the chord
 *                  moving I IV V vi by DENSITY, MOTION a slow vibrato and vowel drift
 *   RINGS          Mutable Instruments' Rings, strummed by every note (MODEL: modal, sympathetic strings, string, FM
 *                  voice, chords, string + reverb); CHARACTER structure, BRIGHTNESS, DECAY damping, MOTION position
 *   ELEMENTS       Mutable Instruments' Elements: bow, blow and strike into a modal resonator (MODEL: modal, string,
 *                  strings); CHARACTER from bow over blow to strike, MOTION the geometry, SPREAD its space
 *   GLASS HARMONICA (1.2) glass bowls rubbed into sound: four inharmonic modes each, swelling slowly
 *                  (CHARACTER: from a slow swell to a quick touch), MOTION the rubbing's tremolo
 *   AURORA PAD     (1.2) a curtain of chords: three notes of the key, five detuned saws each (band-limited),
 *                  through a breathing low-pass (BRIGHTNESS); MOTION the detune, the chord moves like the choir's
 *   CZ CRYSTAL     (1.2) phase distortion as on Casio's CZ: a resonant window whose resonance sweeps down with
 *                  each note (CHARACTER how high it starts, BRIGHTNESS how much), plucked crystal to soft brass
 *   WIND FLUTE     (1.2) breath through a resonant tube: noise in a narrow band at the note, a little tone
 *                  in it (CHARACTER: from breath to tone), MOTION the vibrato
 * PLAYED BY: ITSELF (rain, drift), LOGIC (its notes, when LOGIC sends a voice to SYNTH) or a track's hits.
 * OCTAVE, DECAY, BRIGHTNESS, SPREAD, LEVEL and the sends into CLOUD, PARTICLES, SEEDS and SHIMMER.
 * Included by brick_lab.inc after LOGIC (it uses LOGIC's table of the key). */

enum { SY_DRONE, SY_MODAL, SY_SHEPARD, SY_KARPLUS, SY_BELLS, SY_CHOIR, SY_RINGS, SY_ELEMENTS, SY_GLASS, SY_PAD, SY_CZ, SY_FLUTE,
       SY_ENGINES };
static const char *const SY_NAMES[SY_ENGINES] = {"JUST DRONE", "MODAL GARDEN", "SHEPARD", "KARPLUS RAIN", "GOLDEN BELLS", "FORMANT CHOIR", "RINGS", "ELEMENTS",
                                                 "GLASS HARMONICA", "AURORA PAD", "CZ CRYSTAL", "WIND FLUTE"};
/* each engine's gain: about -22 dB RMS alone at LEVEL 100, peaks at most -6 dB (measured: each engine alone, 10 s) */
static const float SY_GAIN[SY_ENGINES] = {0.134f, 27.4f, 0.079f, 0.49f, 0.107f, 0.059f, 1.03f, 0.225f, 0.15f, 0.031f, 0.13f, 0.72f};

#define SY_SIN 2048
static float sy_sin[SY_SIN + 1];
static float sy_buf[2 * APP_BLOCK], sy_level, sy_flash;
static int sy_active, sy_engine_was = -1;
static float sy_ri_note = 48, sy_el_note = 48;
static int sy_ri_strike;
static uint32_t sy_el_gate_until;
static uint32_t sy_rng = 0x2545F491u, sy_last_event, sy_last_step, sy_frames;
static float sy_hp[2], sy_hx[2];

static inline uint32_t sy_rand(void)
{
    sy_rng ^= sy_rng << 13;
    sy_rng ^= sy_rng >> 17;
    sy_rng ^= sy_rng << 5;
    return sy_rng;
}
static inline float sy_unit(void) { return (float)(sy_rand() & 0xFFFFFFu) / 16777216.0f; }
static inline float sy_noise(void) { return sy_unit() * 2.0f - 1.0f; }
static inline float sy_sine(float ph)            /* ph in turns */
{
    float x = (ph - floorf(ph)) * SY_SIN;
    int i = (int)x;
    return sy_sin[i] + (sy_sin[i + 1] - sy_sin[i]) * (x - (float)i);
}
static float sy_hz(float midi) { return 440.0f * powf(2.0f, (midi - 69.0f) / 12.0f); }

/* ---- JUST DRONE ---- */
#define SY_PARTIALS 10
static float dr_ph[SY_PARTIALS], dr_lfo[SY_PARTIALS], dr_amp[SY_PARTIALS];
static const float DR_SETS[3][SY_PARTIALS] = {
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10},                          /* the harmonic series */
    {1, 1.25f, 1.5f, 2, 2.5f, 3, 3.75f, 4, 5, 6},             /* 4:5:6, stacked */
    {1, 7.0f / 6.0f, 1.5f, 1.75f, 2, 2.625f, 3, 3.5f, 4, 5.25f}};   /* septimal */

/* ---- MODAL GARDEN ---- */
#define SY_MODES 8
static float mg_f[SY_MODES], mg_y1[SY_MODES], mg_y2[SY_MODES], mg_exc[SY_MODES];
static float mg_hit[SY_MODES];                   /* (the picture) */
static const float MG_BELL[SY_MODES] = {1.0f, 2.756f, 5.404f, 8.933f, 13.34f, 1.502f, 3.92f, 6.6f};

/* ---- SHEPARD ---- */
static float sh_pos, sh_ph[10];

/* ---- KARPLUS RAIN ---- */
#define SY_STRINGS 8
#define KS_LEN 2400
static float ks_buf[SY_STRINGS][KS_LEN], ks_lp[SY_STRINGS], ks_env[SY_STRINGS], ks_pan[SY_STRINGS];
static int ks_len[SY_STRINGS], ks_pos[SY_STRINGS];
static uint32_t ks_age[SY_STRINGS], ks_ages;

/* ---- GOLDEN BELLS ---- */
#define SY_NBELLS 6
static struct { float f, car, mod, env, idx, pan; int on; } be_v[SY_NBELLS];
static int be_next;

/* ---- FORMANT CHOIR ---- */
static float ch_ph[3][2], ch_f[3], ch_target[3], ch_bp[3][3][2], ch_vowel;
static int ch_degree;
static const float CH_FORM[5][3] = {{800, 1150, 2900}, {400, 1600, 2700}, {270, 2100, 3000}, {450, 800, 2830}, {325, 700, 2530}};

/* ---- (1.2) the voices of GLASS HARMONICA, CZ CRYSTAL and WIND FLUTE ---- */
#define NV_N 8
static struct { float f, ph[4], env, peak, cz, pan, lp, bp, vib; int on, rising; } nv[NV_N];
static int nv_next;
static const float GL_RATIO[4] = {1.0f, 2.756f, 5.404f, 8.933f}, GL_AMP[4] = {1.0f, 0.38f, 0.18f, 0.08f};
/* ---- AURORA PAD: three chord notes, five saws each ---- */
static float pad_ph[3][5], pad_f[3], pad_lp[2][2], pad_breath;
static inline float sy_blep(float t, float dt)  /* the band-limiting step of a saw's jump */
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0;
}

static void sy_init(void)
{
    if (sy_sin[1] != 0)
        return;
    for (int i = 0; i <= SY_SIN; i++)
        sy_sin[i] = sinf(6.2831853f * (float)i / SY_SIN);
}

/* a note to SYNTH (from LOGIC, a track, or its own rain) */
static void sy_note_on(int note, int vel)
{
    int eng = (int)lab_v[P_SY_ENGINE] % SY_ENGINES;
    float v = (float)vel / 127.0f, n = (float)note + 12.0f * lab_v[P_SY_OCTAVE];
    sy_flash = 1.0f;
    switch (eng) {
    case SY_MODAL: {                             /* the mode nearest in pitch is struck, retuned to the note */
        int best = (int)(sy_rand() % SY_MODES);
        mg_f[best] = sy_hz(n);
        mg_exc[best] += 0.6f * v + 0.2f;
        mg_hit[best] = 1;
        break;
    }
    case SY_KARPLUS: {
        int s = 0;
        uint32_t oldest = 0xFFFFFFFFu;
        float f = sy_hz(n), b = 0.15f + 0.8f * lab_v[P_SY_BRIGHT] / 100.0f, lp = 0;
        for (int i = 0; i < SY_STRINGS; i++)
            if (ks_age[i] < oldest)
                oldest = ks_age[i], s = i;
        ks_len[s] = (int)(44100.0f / f);
        ks_len[s] = ks_len[s] < 8 ? 8 : ks_len[s] > KS_LEN - 1 ? KS_LEN - 1 : ks_len[s];
        for (int k = 0; k < ks_len[s]; k++) {    /* the pluck: noise, as bright as BRIGHTNESS */
            lp += (sy_noise() - lp) * b;
            ks_buf[s][k] = lp * (0.5f + 0.5f * v);
        }
        ks_pos[s] = 0, ks_env[s] = 1, ks_age[s] = ++ks_ages;
        ks_pan[s] = sy_noise() * lab_v[P_SY_SPREAD] / 100.0f;
        break;
    }
    case SY_BELLS: {
        static const float ratio[4] = {1.41421356f, 1.61803399f, 1.57079633f, 1.35914091f};
        int b = be_next++ % SY_NBELLS;
        (void)ratio;
        be_v[b].f = sy_hz(n), be_v[b].car = be_v[b].mod = 0, be_v[b].env = 0.4f + 0.6f * v, be_v[b].idx = 2.5f + 3.0f * v;
        be_v[b].pan = sy_noise() * lab_v[P_SY_SPREAD] / 100.0f, be_v[b].on = 1;
        break;
    }
    case SY_RINGS:                               /* Rings: strummed by the note */
        sy_ri_note = n;
        sy_ri_strike = 1;
        break;
    case SY_ELEMENTS:                            /* Elements: the gate held for a while */
        sy_el_note = n;
        sy_el_gate_until = sy_frames + (uint32_t)(44100.0f * (0.15f + lab_v[P_SY_DECAY] / 100.0f * 1.2f));
        break;
    case SY_GLASS: case SY_CZ: case SY_FLUTE: {   /* (1.2) a voice: the oldest makes room */
        int s = nv_next++ % NV_N;
        memset(&nv[s], 0, sizeof nv[s]);
        nv[s].f = sy_hz(n), nv[s].peak = 0.35f + 0.65f * v, nv[s].rising = 1, nv[s].on = 1, nv[s].cz = 1.0f;
        nv[s].pan = sy_noise() * lab_v[P_SY_SPREAD] / 100.0f;
        if (eng == SY_CZ)
            nv[s].env = nv[s].peak, nv[s].rising = 0;   /* (a pluck: at once) */
        break;
    }
    case SY_PAD:                                 /* (1.2) the note becomes the chord's root, as the choir's */
    case SY_CHOIR: {                             /* the note becomes the chord's root */
        int d = 0;
        for (int i = 0; i < lg_nscale; i++)
            if (lg_scale[i] % 12 == note % 12) {
                d = i;
                break;
            }
        ch_degree = d;
        break;
    }
    default:
        break;
    }
}

/* n frames into sy_buf; 0 = nothing sounds */
static int sy_render(uint32_t n)
{
    const md_stats *st = md_status();
    const md_patch *p = md_state();
    int eng = (int)lab_v[P_SY_ENGINE] % SY_ENGINES, by = (int)lab_v[P_SY_PLAYED_BY];
    float level = lab_v[P_SY_LEVEL] / 100.0f, dens = lab_v[P_SY_DENSITY] / 100.0f, decay = lab_v[P_SY_DECAY] / 100.0f;
    float bright = lab_v[P_SY_BRIGHT] / 100.0f, chr = lab_v[P_SY_CHARACTER] / 100.0f, mot = lab_v[P_SY_MOTION] / 100.0f;
    float spread = lab_v[P_SY_SPREAD] / 100.0f, root = (float)(36 + (int)(p->root % 12)) + 12.0f * lab_v[P_SY_OCTAVE], lvl = 0;
    uint32_t k;
    sy_init();
    sy_frames += n;
    sy_flash *= 0.9f;
    if (level <= 0) {
        sy_level = 0;
        return 0;
    }
    if (eng != sy_engine_was) {                  /* another engine: start quiet */
        sy_engine_was = eng;
        memset(mg_y1, 0, sizeof mg_y1), memset(mg_y2, 0, sizeof mg_y2), memset(mg_exc, 0, sizeof mg_exc);
        memset(ks_env, 0, sizeof ks_env), memset(be_v, 0, sizeof be_v), memset(nv, 0, sizeof nv);
        lg_build_scale();
        for (int i = 0; i < SY_MODES; i++)
            mg_f[i] = 0;
    }
    if (!lg_nscale)
        lg_build_scale();
    if (by >= 2 && by <= 7) {                    /* a track's hits play it */
        uint32_t ev = st->events[by - 2];
        if (ev != sy_last_event) {
            sy_last_event = ev;
            if (st->last_note[by - 2] > 0)
                sy_note_on(st->last_note[by - 2], 100);
        }
    }
    if (by == 0 && (eng == SY_MODAL || eng == SY_KARPLUS || eng == SY_BELLS || eng == SY_RINGS || eng == SY_ELEMENTS
                    || eng == SY_GLASS || eng == SY_CZ || eng == SY_FLUTE)) {   /* ITSELF: rain in the key */
        float rate = (0.15f + dens * dens * 10.0f) * (eng == SY_GLASS || eng == SY_FLUTE ? 0.3f : 1.0f);   /* drops a second (slower for long notes) */
        if (sy_unit() < rate * (float)n / 44100.0f) {
            int d = (int)(sy_rand() % (uint32_t)(lg_nscale > 2 ? lg_nscale - 2 : 1));
            sy_note_on(lg_scale[d] - 12 + (eng == SY_KARPLUS ? 12 : 0), 40 + (int)(sy_rand() % 80u));
        }
    }
    if ((eng == SY_CHOIR || eng == SY_PAD) && by == 0 && st->step != sy_last_step) {   /* the chord moves, I IV V vi, by DENSITY */
        sy_last_step = st->step;
        if (st->step % 16u == 0 && sy_unit() < 0.15f + dens * 0.6f) {
            static const int prog[4] = {0, 3, 4, 5};
            ch_degree = prog[sy_rand() % 4u];
        }
    }
    memset(sy_buf, 0, sizeof(float) * 2 * n);
    switch (eng) {
    case SY_DRONE: {
        const float *set = DR_SETS[chr < 0.34f ? 0 : chr < 0.67f ? 1 : 2];
        float f0 = sy_hz(root);
        for (int i = 0; i < SY_PARTIALS; i++) {
            float target = powf((float)(i + 1), -(2.0f - 1.6f * bright)), rate = (0.02f + 0.03f * (float)i) * (0.3f + mot);
            float det = 1.0f + mot * 0.0025f * (float)((i % 3) - 1), f = f0 * set[i] * det;
            float pan = spread * ((i & 1) ? 1.0f : -1.0f) * (0.3f + 0.07f * (float)i), gl = 1 - pan, gr = 1 + pan;
            if (f > 16000.0f)
                continue;
            for (k = 0; k < n; k++) {
                float a, s;
                dr_lfo[i] += rate / 44100.0f;
                a = target * (0.55f + 0.45f * sy_sine(dr_lfo[i] + 0.37f * (float)i)) ;
                dr_amp[i] += (a - dr_amp[i]) * 0.001f;   /* (smooth: no zipper) */
                dr_ph[i] += f / 44100.0f;
                if (dr_ph[i] >= 1.0f)
                    dr_ph[i] -= 1.0f;
                s = sy_sine(dr_ph[i]) * dr_amp[i];
                sy_buf[2 * k] += s * gl;
                sy_buf[2 * k + 1] += s * gr;
            }
        }
        break;
    }
    case SY_MODAL: {
        float t60 = 0.3f + decay * decay * 9.0f, exc_lp = 0.1f + 0.85f * bright;
        static float elp;
        for (int i = 0; i < SY_MODES; i++)
            if (mg_f[i] <= 0) {                  /* the modes, tuned: key notes, bending to bell ratios */
                int d = (i * 2) % (lg_nscale > 0 ? lg_nscale : 1);
                float key = lg_nscale ? sy_hz((float)lg_scale[d] - 12.0f + 12.0f * lab_v[P_SY_OCTAVE]) : 220.0f;
                float bell = sy_hz(root + 12.0f) * MG_BELL[i];
                mg_f[i] = key * powf(bell / key, chr);
            }
        for (int i = 0; i < SY_MODES; i++) {
            float f = mg_f[i] > 15000.0f ? 15000.0f : mg_f[i], r = expf(-6.9078f / (t60 * 44100.0f) * (1.0f + f / 4000.0f));
            float a1 = -2.0f * r * cosf(6.2831853f * f / 44100.0f), a2 = r * r, g = (1.0f - r * r) * 0.5f;
            float pan = spread * ((float)i / (SY_MODES - 1) * 2.0f - 1.0f), gl = 1 - pan, gr = 1 + pan;
            for (k = 0; k < n; k++) {
                float x = 0, y;
                if (mg_exc[i] > 0.0001f) {       /* the strike: a short burst of noise */
                    elp += (sy_noise() - elp) * exc_lp;
                    x = elp * mg_exc[i];
                    mg_exc[i] *= 0.985f;
                }
                y = g * x - a1 * mg_y1[i] - a2 * mg_y2[i];
                mg_y2[i] = mg_y1[i];
                mg_y1[i] = y + 1e-20f;
                sy_buf[2 * k] += y * gl;
                sy_buf[2 * k + 1] += y * gr;
            }
            mg_hit[i] *= 0.92f;
        }
        break;
    }
    case SY_SHEPARD: {                           /* (frequencies once a block: the glide is slow) */
        float dir = chr < 0.5f ? 1.0f : -1.0f, speed = (0.004f + mot * 0.06f) * dir;   /* octaves a second */
        float centre = 6.0f + 2.5f * bright, fmin = sy_hz(root - 24.0f), inc[10], w[10];
        sh_pos += speed * (float)n / 44100.0f;
        sh_pos -= floorf(sh_pos);
        for (int c = 0; c < 10; c++) {
            float oct = (float)c + sh_pos;
            inc[c] = fmin * powf(2.0f, oct) / 44100.0f;
            w[c] = expf(-0.5f * (oct - centre) * (oct - centre) / 2.2f);
            if (inc[c] > 0.45f)
                w[c] = 0;
        }
        for (k = 0; k < n; k++) {
            float l = 0, r = 0;
            for (int c = 0; c < 10; c++) {
                float x;
                sh_ph[c] += inc[c];
                if (sh_ph[c] >= 1.0f)
                    sh_ph[c] -= 1.0f;
                x = sy_sine(sh_ph[c]) * w[c];
                l += x * (1.0f - spread * 0.5f * (float)(c & 1));
                r += x * (1.0f - spread * 0.5f * (float)(!(c & 1)));
            }
            sy_buf[2 * k] = l;
            sy_buf[2 * k + 1] = r;
        }
        break;
    }
    case SY_KARPLUS: {
        float damp = 0.990f + 0.0098f * decay, blend = 0.5f - 0.35f * chr;   /* CHARACTER: harp .. nylon */
        for (int s = 0; s < SY_STRINGS; s++) {
            float gl = 1 - ks_pan[s], gr = 1 + ks_pan[s];
            if (ks_env[s] <= 0)
                continue;
            for (k = 0; k < n; k++) {
                int a = ks_pos[s], b = (a + 1) % ks_len[s];
                float y = ks_buf[s][a];
                ks_lp[s] = (y * (1.0f - blend) + ks_buf[s][b] * blend) * damp;
                ks_buf[s][a] = ks_lp[s];
                ks_pos[s] = b;
                sy_buf[2 * k] += y * gl;
                sy_buf[2 * k + 1] += y * gr;
            }
            ks_env[s] *= 0.999f;
            if (fabsf(ks_lp[s]) < 1e-5f && ks_env[s] < 0.5f)
                ks_env[s] = 0;
        }
        break;
    }
    case SY_BELLS: {
        static const float ratio[4] = {1.41421356f, 1.61803399f, 1.57079633f, 1.35914091f};
        float rt = ratio[(int)(chr * 3.99f)], dk = expf(-1.0f / (44100.0f * (0.4f + decay * decay * 7.0f)));
        float ik = expf(-1.0f / (44100.0f * (0.15f + decay * 1.5f)));
        for (int b = 0; b < SY_NBELLS; b++) {
            float gl, gr;
            if (!be_v[b].on)
                continue;
            gl = 1 - be_v[b].pan, gr = 1 + be_v[b].pan;
            for (k = 0; k < n; k++) {
                float m, s;
                be_v[b].mod += be_v[b].f * rt / 44100.0f;
                be_v[b].car += be_v[b].f / 44100.0f;
                if (be_v[b].mod >= 1) be_v[b].mod -= 1;
                if (be_v[b].car >= 1) be_v[b].car -= 1;
                m = sy_sine(be_v[b].mod) * be_v[b].idx * (0.3f + bright);
                s = sy_sine(be_v[b].car + m * 0.159f) * be_v[b].env;
                be_v[b].env *= dk;
                be_v[b].idx *= ik;
                sy_buf[2 * k] += s * gl;
                sy_buf[2 * k + 1] += s * gr;
            }
            if (be_v[b].env < 0.0005f)
                be_v[b].on = 0;
        }
        break;
    }
    case SY_RINGS: {
        static float rl[APP_BLOCK], rr[APP_BLOCK];
        mi_rings_params rp;
        memset(&rp, 0, sizeof rp);
        rp.model = (int)lab_v[P_SY_MODEL], rp.polyphony = 2, rp.chord = (int)(chr * 10.0f);
        rp.note = sy_ri_note;
        rp.structure = chr, rp.brightness = bright, rp.damping = 0.2f + 0.75f * decay, rp.position = mot;
        rp.internal_exciter = 1, rp.strum_by_note = 1, rp.strike = sy_ri_strike;
        sy_ri_strike = 0;
        mi_rings_process(2, &rp, 0, rl, rr, n);
        for (k = 0; k < n; k++) {
            float pan = spread * 0.6f;
            sy_buf[2 * k] = rl[k] * (1 - pan) + rr[k] * pan;
            sy_buf[2 * k + 1] = rr[k] * (1 - pan) + rl[k] * pan;
        }
        break;
    }
    case SY_ELEMENTS: {
        static float el_l[APP_BLOCK], el_r[APP_BLOCK];
        mi_elements_params ep;
        float bow = chr < 0.5f ? 1.0f - chr * 2.0f : 0.0f, strike = chr > 0.5f ? chr * 2.0f - 1.0f : 0.0f;
        float blow = 1.0f - fabsf(chr - 0.5f) * 2.0f;
        memset(&ep, 0, sizeof ep);
        ep.model = (int)lab_v[P_SY_MODEL] % 3, ep.gate = (int32_t)(sy_frames - sy_el_gate_until) < 0;
        ep.note = sy_el_note, ep.strength = 0.8f;
        ep.contour = 0.5f, ep.bow = bow * 0.8f, ep.bow_timbre = bright, ep.blow = blow * 0.8f, ep.flow = 0.5f;
        ep.blow_timbre = bright, ep.strike = strike * 0.9f + 0.1f, ep.mallet = 0.5f, ep.strike_timbre = bright;
        ep.geometry = mot, ep.brightness = 0.3f + 0.6f * bright, ep.damping = 0.2f + 0.7f * decay, ep.position = 0.3f;
        ep.space = spread * 0.8f;
        mi_elements_process(&ep, el_l, el_r, n);
        for (k = 0; k < n; k++)
            sy_buf[2 * k] = el_l[k], sy_buf[2 * k + 1] = el_r[k];
        break;
    }
    case SY_GLASS: {                             /* (1.2) four modes of a glass bowl, swelling and fading */
        float att = 1.0f / (44100.0f * (0.05f + (1.0f - chr) * 0.8f)), dk = expf(-1.0f / (44100.0f * (0.6f + decay * decay * 7.0f)));
        for (int s = 0; s < NV_N; s++) {
            float gl, gr, amp[4];
            if (!nv[s].on)
                continue;
            gl = 1 - nv[s].pan, gr = 1 + nv[s].pan;
            for (int m = 0; m < 4; m++)
                amp[m] = GL_AMP[m] * (m ? 0.4f + bright * 1.2f : 1.0f);
            for (k = 0; k < n; k++) {
                float out = 0, trem;
                if (nv[s].rising) {
                    nv[s].env += att * nv[s].peak;
                    if (nv[s].env >= nv[s].peak)
                        nv[s].env = nv[s].peak, nv[s].rising = 0;
                } else
                    nv[s].env *= dk;
                nv[s].vib += 4.7f / 44100.0f;
                if (nv[s].vib >= 1)
                    nv[s].vib -= 1;
                trem = 1.0f + mot * 0.3f * sy_sine(nv[s].vib + (float)s * 0.13f);
                for (int m = 0; m < 4; m++) {
                    float fm = nv[s].f * GL_RATIO[m];
                    if (fm > 15000.0f)
                        break;
                    nv[s].ph[m] += fm / 44100.0f;
                    if (nv[s].ph[m] >= 1)
                        nv[s].ph[m] -= 1;
                    out += sy_sine(nv[s].ph[m]) * amp[m];
                }
                out *= nv[s].env * trem;
                sy_buf[2 * k] += out * gl;
                sy_buf[2 * k + 1] += out * gr;
            }
            if (!nv[s].rising && nv[s].env < 0.0004f)
                nv[s].on = 0;
        }
        break;
    }
    case SY_CZ: {                                /* (1.2) phase distortion: a resonance inside a falling window */
        float dk = expf(-1.0f / (44100.0f * (0.2f + decay * decay * 3.0f))), ck = expf(-1.0f / (44100.0f * (0.05f + decay * 0.6f)));
        float top = 1.5f + chr * 14.0f * (0.3f + bright);
        for (int s = 0; s < NV_N; s++) {
            float gl, gr;
            if (!nv[s].on)
                continue;
            gl = 1 - nv[s].pan, gr = 1 + nv[s].pan;
            for (k = 0; k < n; k++) {
                float p, r, out;
                nv[s].ph[0] += nv[s].f / 44100.0f;
                if (nv[s].ph[0] >= 1)
                    nv[s].ph[0] -= 1;
                p = nv[s].ph[0];
                r = 1.0f + (top - 1.0f) * nv[s].cz;
                out = sy_sine(p * r) * (1.0f - p) * 2.0f - (1.0f - p);   /* (the window keeps it from clicking) */
                nv[s].lp += (out - nv[s].lp) * 0.6f;
                out = nv[s].lp * nv[s].env;
                nv[s].env *= dk;
                nv[s].cz *= ck;
                sy_buf[2 * k] += out * gl;
                sy_buf[2 * k + 1] += out * gr;
            }
            if (nv[s].env < 0.0004f)
                nv[s].on = 0;
        }
        break;
    }
    case SY_FLUTE: {                             /* (1.2) breath in a narrow band at the note, a little tone in it */
        float att = 1.0f / (44100.0f * (0.04f + (1.0f - bright) * 0.25f)), dk = expf(-1.0f / (44100.0f * (0.25f + decay * decay * 3.0f)));
        float q = 0.03f + (1.0f - chr) * 0.12f;
        for (int s = 0; s < NV_N; s++) {
            float gl, gr;
            if (!nv[s].on)
                continue;
            gl = 1 - nv[s].pan, gr = 1 + nv[s].pan;
            for (k = 0; k < n; k++) {
                float vib, f, w, hp, out;
                if (nv[s].rising) {
                    nv[s].env += att * nv[s].peak;
                    if (nv[s].env >= nv[s].peak)
                        nv[s].env = nv[s].peak, nv[s].rising = 0;
                } else
                    nv[s].env *= dk;
                nv[s].vib += 5.3f / 44100.0f;
                if (nv[s].vib >= 1)
                    nv[s].vib -= 1;
                vib = 1.0f + mot * 0.012f * sy_sine(nv[s].vib);
                f = nv[s].f * vib;
                w = 2.0f * sy_sine(f / 88200.0f);     /* (2 sin(pi f / sr), from the table) */
                hp = sy_noise() - nv[s].lp - q * nv[s].bp;
                nv[s].bp += w * hp;
                nv[s].lp += w * nv[s].bp;
                nv[s].ph[0] += f / 44100.0f;
                if (nv[s].ph[0] >= 1)
                    nv[s].ph[0] -= 1;
                out = (nv[s].bp * q * 6.0f * (1.0f - 0.6f * chr) + sy_sine(nv[s].ph[0]) * 0.5f * chr) * nv[s].env;
                sy_buf[2 * k] += out * gl;
                sy_buf[2 * k + 1] += out * gr;
            }
            if (!nv[s].rising && nv[s].env < 0.0004f)
                nv[s].on = 0;
        }
        break;
    }
    case SY_PAD: {                               /* (1.2) three chord notes, five band-limited saws each, a breathing low-pass */
        int per = lg_per_octave(), third = per >= 7 ? 2 : 1, fifth = per >= 7 ? 4 : 2;
        int deg[3] = {ch_degree, ch_degree + third, ch_degree + fifth};
        static const float det[5] = {-1.0f, -0.45f, 0.0f, 0.5f, 1.0f};
        float cents = 4.0f + mot * 18.0f, cut, w;
        for (int v = 0; v < 3; v++) {
            int d = deg[v] % (lg_nscale > 0 ? lg_nscale : 1);
            float tgt = sy_hz((float)(lg_nscale ? lg_scale[d] : 60) + 12.0f * lab_v[P_SY_OCTAVE] - 12.0f);
            if (pad_f[v] <= 0)
                pad_f[v] = tgt;
            pad_f[v] += (tgt - pad_f[v]) * 0.02f;   /* (glides once a block) */
        }
        pad_breath += (float)n / 44100.0f * 0.07f;
        if (pad_breath >= 1)
            pad_breath -= 1;
        cut = 180.0f + bright * bright * 5200.0f * (0.75f + 0.25f * sy_sine(pad_breath));
        w = 2.0f * sy_sine(cut / 88200.0f);
        for (k = 0; k < n; k++) {
            float l = 0, r = 0;
            for (int v = 0; v < 3; v++)
                for (int o = 0; o < 5; o++) {
                    float f = pad_f[v] * (1.0f + det[o] * cents * 0.000578f), dt = f / 44100.0f, saw, pan;
                    pad_ph[v][o] += dt;
                    if (pad_ph[v][o] >= 1)
                        pad_ph[v][o] -= 1;
                    saw = 2.0f * pad_ph[v][o] - 1.0f - sy_blep(pad_ph[v][o], dt);
                    pan = spread * det[o] * 0.8f;
                    l += saw * (1 - pan), r += saw * (1 + pan);
                }
            for (int c = 0; c < 2; c++) {        /* (a 2-pole low-pass each side) */
                float in = c ? r : l;
                pad_lp[c][0] += w * (in - pad_lp[c][0] - 0.7f * pad_lp[c][1]);
                pad_lp[c][1] += w * pad_lp[c][0];
                sy_buf[2 * k + c] = pad_lp[c][1];
            }
        }
        break;
    }
    case SY_CHOIR: {
        int per = lg_per_octave(), third = per >= 7 ? 2 : 1, fifth = per >= 7 ? 4 : 2;
        int deg[3] = {ch_degree, ch_degree + third, ch_degree + fifth}, vi;
        float vow = chr * 4.0f + mot * 0.6f * sinf((float)sy_frames / 44100.0f * 0.21f), fv, fw[3], fq[3], fa[3];
        vi = (int)vow;
        vi = vi < 0 ? 0 : vi > 3 ? 3 : vi;
        fv = vow - (float)vi;
        fv = fv < 0 ? 0 : fv > 1 ? 1 : fv;
        ch_vowel = vow;
        for (int v = 0; v < 3; v++) {
            int d = deg[v] % (lg_nscale > 0 ? lg_nscale : 1);
            ch_target[v] = sy_hz((float)(lg_nscale ? lg_scale[d] : 60) + 12.0f * lab_v[P_SY_OCTAVE] - 12.0f);
            if (ch_f[v] <= 0)
                ch_f[v] = ch_target[v];
        }
        for (int f = 0; f < 3; f++) {            /* the formants of the vowel between two */
            float fc = CH_FORM[vi][f] + (CH_FORM[vi + 1][f] - CH_FORM[vi][f]) * fv;
            fw[f] = 2.0f * sinf(3.1415927f * fc / 44100.0f);
            fq[f] = 0.08f + 0.04f * (float)f;
            fa[f] = f == 0 ? 1.0f : f == 1 ? 0.6f * (0.5f + bright) : 0.25f * (0.3f + bright);
        }
        for (k = 0; k < n; k++)
            for (int v = 0; v < 3; v++) {
                float pan = spread * (float)(v - 1) * 0.8f, saw, vib, out = 0;
                ch_f[v] += (ch_target[v] - ch_f[v]) * 0.0004f;   /* the chord glides to its next */
                vib = 1.0f + (0.002f + mot * 0.004f) * sy_sine((float)(sy_frames + k) / 44100.0f * 5.1f + (float)v * 0.3f);
                ch_ph[v][0] += ch_f[v] * vib / 44100.0f;
                if (ch_ph[v][0] >= 1)
                    ch_ph[v][0] -= 1;
                saw = 2.0f * ch_ph[v][0] - 1.0f;
                for (int f = 0; f < 3; f++) {    /* a state-variable band pass for each formant */
                    float hp = saw - ch_bp[v][f][0] - fq[f] * ch_bp[v][f][1];
                    ch_bp[v][f][1] += fw[f] * hp;
                    ch_bp[v][f][0] += fw[f] * ch_bp[v][f][1];
                    out += ch_bp[v][f][1] * fa[f];
                }
                sy_buf[2 * k] += out * (1 - pan);
                sy_buf[2 * k + 1] += out * (1 + pan);
            }
        break;
    }
    }
    for (k = 0; k < n; k++)                       /* gain, without DC, and its level */
        for (int c = 0; c < 2; c++) {
            float x = sy_buf[2 * k + c] * SY_GAIN[eng] * level;
            sy_hp[c] = 0.9972f * (sy_hp[c] + x - sy_hx[c]);
            sy_hx[c] = x;
            x = sy_hp[c];
            x = x > 1.5f ? 1.5f : x < -1.5f ? -1.5f : x;
            sy_buf[2 * k + c] = x;
            lvl += x * x;
        }
    sy_level = sqrtf(lvl / (float)(2 * n));
    return 1;
}
