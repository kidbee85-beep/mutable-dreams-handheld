/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the POLY voice of the Sound Lab.
 *
 * Up to eight Plaits voices (all 24 models, brick_mi_plaits.cpp) with an envelope each (ATTACK, DECAY, SUSTAIN,
 * RELEASE: it opens Plaits' low pass gate; the gate input is held with the note). Notes come from
 *   - play mode (FN): with PLAY MODE > POLY the eight scale tones and R2 chords sound here, not on a track
 *   - RANDOM with TRACK set to POLY
 *   - FOLLOW: every note of a chosen track doubles here (GATE long)
 * A voice for a new note: the same note's, else a free one, else the quietest released one, else the oldest. */

#define PO_VOICES 8
#define PO_CHUNK 32
static struct {
    int note, gate, retrig, on, from_follow, stage, held_by_pedal;  /* stage: 0 attack, 1 decay/sustain, 2 release */
    float env, vel, cents;                       /* cents: LOGIC's HARMONIC tunes a voice off the grid */
    uint32_t age, off_at;
} po_v[PO_VOICES];
static float po_next_cents;                      /* the next note's cents (set around po_note_on_vel) */
static uint32_t po_age, po_frames, po_last_event;
static int po_sustain;                           /* MIDI CC 64: note offs wait for the pedal */
static float po_bend, po_mod;                    /* MIDI pitch bend (semitones, +-2) and mod wheel (0..1, onto MORPH) */
static float po_buf[2 * APP_BLOCK], po_level;
static int po_active;
static const char *const PO_ENGINES[24] = {
    "VA + FILTER", "PHASE DISTORTION", "6-OP FM I", "6-OP FM II", "6-OP FM III", "WAVE TERRAIN", "STRING MACHINE",
    "CHIPTUNE", "VIRTUAL ANALOG", "WAVESHAPER", "2-OP FM", "GRAIN FORMANT", "ADDITIVE", "WAVETABLE", "CHORDS",
    "SPEECH", "SWARM", "FILTERED NOISE", "PARTICLE NOISE", "INHARMONIC STRING", "MODAL RESONATOR", "BASS DRUM",
    "SNARE DRUM", "HI-HAT"};

/* each model's gain so that LEVEL means about the same loudness for all 24 (measured: tests/poly.txt models with
 * RANDOM on POLY, tools/labdiff.py; target -26 dB RMS added; percussive models capped at +12 dB for their peaks) */
static const float PO_GAIN[24] = {0.692f, 0.484f, 7.161f, 10.000f, 1.135f, 0.468f, 1.718f, 0.245f, 0.684f, 0.692f, 0.507f, 0.891f, 0.912f, 2.138f, 0.955f, 1.012f, 1.012f, 0.923f, 3.981f, 3.981f, 3.126f, 3.981f, 3.981f, 3.981f};

static int po_voices(void) { return (int)lab_v[P_PO_VOICES]; }

static void po_note_on_vel(int note, int follow, int velocity);
static void po_note_on(int note, int follow) { po_note_on_vel(note, follow, 100); }
static void po_note_on_vel(int note, int follow, int velocity)
{
    int i, v = -1, n = po_voices();
    note = note < 12 ? 12 : note > 108 ? 108 : note;
    for (i = 0; i < n && v < 0; i++)
        if (po_v[i].on && po_v[i].note == note)
            v = i;
    for (i = 0; i < n && v < 0; i++)
        if (!po_v[i].on)
            v = i;
    if (v < 0) {                                  /* steal: the quietest released, else the oldest held */
        float q = 2.0f;
        uint32_t oldest = 0xFFFFFFFFu;
        for (i = 0; i < n; i++)
            if (!po_v[i].gate && po_v[i].env < q) {
                q = po_v[i].env;
                v = i;
            }
        for (i = 0; i < n && v < 0; i++)
            if (po_v[i].age < oldest) {
                oldest = po_v[i].age;
                v = i;
            }
    }
    if (v < 0)
        v = 0;
    po_v[v].retrig = po_v[v].on && po_v[v].gate;
    po_v[v].note = note;
    po_v[v].gate = 1;
    po_v[v].on = 1;
    po_v[v].stage = 0;
    po_v[v].age = ++po_age;
    po_v[v].from_follow = follow;
    po_v[v].held_by_pedal = 0;
    po_v[v].vel = 0.25f + 0.75f * (float)(velocity > 127 ? 127 : velocity < 1 ? 1 : velocity) / 127.0f;
    po_v[v].cents = po_next_cents;
    po_v[v].off_at = follow ? po_frames + (uint32_t)(lab_v[P_PO_GATE] * 44.1f) : 0;
}

static void po_note_off(int note)
{
    note = note < 12 ? 12 : note > 108 ? 108 : note;
    for (int i = 0; i < PO_VOICES; i++)
        if (po_v[i].on && po_v[i].gate && po_v[i].note == note) {
            if (po_sustain) {                    /* the pedal holds it */
                po_v[i].held_by_pedal = 1;
                continue;
            }
            po_v[i].gate = 0;
            po_v[i].stage = 2;
        }
}

static void po_pedal(int down)
{
    po_sustain = down;
    if (!down)
        for (int i = 0; i < PO_VOICES; i++)
            if (po_v[i].held_by_pedal && po_v[i].gate) {
                po_v[i].held_by_pedal = 0;
                po_v[i].gate = 0;
                po_v[i].stage = 2;
            }
}

static void po_all_off(void)
{
    for (int i = 0; i < PO_VOICES; i++)
        if (po_v[i].gate) {
            po_v[i].gate = 0;
            po_v[i].stage = 2;
        }
}

/* the envelope's rate per sample for a time of 0..100 % (1 ms .. ~8 s) */
static float po_rate(float pct) { return 1.0f / (44.1f * (1.0f + 8000.0f * powf(pct / 100.0f, 2.5f))); }

static int po_render(uint32_t n)
{
    const md_stats *st = md_status();
    const md_patch *pt = md_state();
    static float out[PO_CHUNK], aux[PO_CHUNK];
    float level = lab_v[P_PO_LEVEL] / 100 * 0.32f * PO_GAIN[(int)lab_v[P_PO_ENGINE] % 24], spread = lab_v[P_PO_SPREAD] / 100, lvl = 0;
    float ra = po_rate(lab_v[P_PO_ATTACK]), rd = po_rate(lab_v[P_PO_DECAY]), rr = po_rate(lab_v[P_PO_RELEASE]);
    float sus = lab_v[P_PO_SUSTAIN] / 100;
    int follow = (int)lab_v[P_PO_FOLLOW], i, any = 0;
    float morph = lab_v[P_PO_MORPH] / 100 + po_mod;
    mi_plaits_patch pp = {0, lab_v[P_PO_HARM] / 100, lab_v[P_PO_TIMBRE] / 100, morph > 1 ? 1.0f : morph, 0.5f,
                          lab_v[P_PO_COLOUR] / 100, (int)lab_v[P_PO_ENGINE]};
    uint32_t k, c;
    (void)pt;
    po_frames += n;
    if (follow > 0) {                             /* a track's notes, doubled */
        uint32_t ev = st->events[(follow - 1) % 6];
        if (ev != po_last_event) {
            int note = st->last_note[(follow - 1) % 6];
            po_last_event = ev;
            if (note > 0)
                po_note_on(note, 1);
        }
    } else
        po_last_event = st->events[0];
    for (i = 0; i < PO_VOICES; i++) {
        if (po_v[i].from_follow && po_v[i].gate && (int32_t)(po_frames - po_v[i].off_at) >= 0) {
            po_v[i].gate = 0;
            po_v[i].stage = 2;
        }
        if (i >= po_voices() && po_v[i].gate) {   /* fewer voices now: these let go */
            po_v[i].gate = 0;
            po_v[i].stage = 2;
        }
    }
    memset(po_buf, 0, sizeof(float) * 2 * n);
    for (i = 0; i < PO_VOICES; i++) {
        float pan, gl, gr;
        if (!po_v[i].on)
            continue;
        any = 1;
        pan = (i & 1 ? 1.0f : -1.0f) * spread * (0.35f + 0.65f * (float)(i >> 1) / 3.0f);
        gl = sqrtf(0.5f * (1 - pan)) * 1.4142f;
        gr = sqrtf(0.5f * (1 + pan)) * 1.4142f;
        pp.note = (float)po_v[i].note + po_v[i].cents / 100.0f + 12.0f * lab_v[P_PO_OCTAVE] + spread * 0.08f * (float)((i % 3) - 1) + po_bend;
        for (c = 0; c < n; c += PO_CHUNK) {
            uint32_t m = n - c < PO_CHUNK ? n - c : PO_CHUNK;
            float e = po_v[i].env, fade;
            for (k = 0; k < m; k++) {            /* the envelope, a chunk at a time */
                if (po_v[i].stage == 0) {
                    e += ra;
                    if (e >= 1.0f) {
                        e = 1.0f;
                        po_v[i].stage = 1;
                    }
                } else if (po_v[i].stage == 1)
                    e += (sus - e) * rd * 4.0f;
                else
                    e -= e * rr * 4.0f + 1e-6f;
            }
            if (e < 0)
                e = 0;
            po_v[i].env = e;
            mi_plaits_render(i, &pp, e, po_v[i].gate, po_v[i].retrig, out, aux, m);
            po_v[i].retrig = 0;
            fade = po_v[i].stage == 2 ? (e * 30.0f > 1.0f ? 1.0f : e * 30.0f) : 1.0f;   /* ringing models: no click */
            for (k = 0; k < m; k++) {
                float s = out[k] * level * fade * (po_v[i].vel > 0 ? po_v[i].vel : 1.0f);
                po_buf[2 * (c + k)] += s * gl;
                po_buf[2 * (c + k) + 1] += s * gr;
            }
            if (po_v[i].stage == 2 && e < 0.0008f) {
                po_v[i].on = 0;
                po_v[i].env = 0;
                break;
            }
        }
    }
    for (k = 0; k < n; k++)                       /* without DC (pulse-width models leave some): ~20 Hz high pass */
        for (c = 0; c < 2; c++) {
            static float hx[2], hy[2];
            float x = po_buf[2 * k + c];
            hy[c] = 0.9972f * (hy[c] + x - hx[c]);
            hx[c] = x;
            po_buf[2 * k + c] = hy[c];
            lvl += hy[c] * hy[c];
        }
    po_level = sqrtf(lvl / (float)(2 * n));
    return any;
}
