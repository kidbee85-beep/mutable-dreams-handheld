/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: LOGIC, a generator of music from musical mathematics.
 *
 * Seven rules write two voices in the Biome's key (voice 2 may go elsewhere), in time with the engine's clock:
 *   TINTINNABULI  after Arvo Part: a melody (M) moves by step in growing phrases from a centre; its T voice always
 *                 takes the nearest note of the tonic triad (above, alternating, below: SHAPE)
 *   PHASING       after Steve Reich: one short pattern in two voices, the second loop one step longer: they drift
 *                 apart and meet again after L x (L + 1) steps (SHAPE: the pattern's length)
 *   FIBONACCI     the golden word as rhythm (never quite repeating), the golden angle as melody, a canon at five
 *                 steps a third lower (SHAPE: the melody's span)
 *   AUTOMATON     a 16-cell cellular automaton, a generation each bar (SHAPE: rule 30, 90, 110, 150, 184, 54, 22,
 *                 126); living cells play, the columns stacked in thirds; newborn cells sound in the bass
 *   MARKOV        a chain over scale steps: steps likely, leaps rare, a pull to tonic and fifth; SHAPE the
 *                 temperature (calm .. wild); the bass holds the region's root
 *   L-SYSTEM      a grammar grows a melody (SHAPE: Fibonacci A>AB B>A, Thue-Morse A>AB B>BA, Koch A>A+A-A),
 *                 a generation deeper each cycle until it starts again
 *   HARMONIC      the natural overtones of the Biome's root (SHAPE: up to 4 .. 16 partials), overtone singing over
 *                 a held fundamental; on POLY in just intonation (to the cent), on a track the nearest note
 * SEED makes every rule's choices again the same; VARIATION lets it change a little each cycle; DENSITY thins.
 * Included by brick_lab.inc (after RANDOM). */

enum { LG_TINTINNABULI, LG_PHASING, LG_FIBONACCI, LG_AUTOMATON, LG_MARKOV, LG_LSYSTEM, LG_HARMONIC, LG_RULES };
static const char *const LG_RULE_NAMES[LG_RULES] = {"TINTINNABULI", "PHASING", "FIBONACCI", "AUTOMATON", "MARKOV", "L-SYSTEM", "HARMONIC"};
static const int LG_CA_RULES[8] = {30, 90, 110, 150, 184, 54, 22, 126};
#define LG_HIST 48
#define LG_NOTES 12
#define LG_LSYS 512
#define LG_REST (-1000)                          /* a step without a note */

static double lg_clock;
static uint32_t lg_frames, lg_step, lg_rng = 1;
static int lg_was_playing, lg_last_rule = -1, lg_last_seed = -1;
static struct { int note, target, on; uint32_t off_at; } lg_on[LG_NOTES];
static int lg_scale[64], lg_nscale;              /* the key's notes from the root in octave 3 up */
static int lg_hist1[LG_HIST], lg_hist2[LG_HIST], lg_hist_w;   /* what played: scale steps, LG_REST (the picture) */
/* each rule's state */
static int lg_tin_phrase, lg_tin_pos, lg_tin_dir = 1, lg_tin_alt;
static int lg_ph_pat[16], lg_ph_len;
static uint16_t lg_ca[24];                       /* the automaton's last generations, [0] now */
static int lg_mk_deg;
static char lg_ls[LG_LSYS];
static int lg_ls_len, lg_ls_pos, lg_ls_deg, lg_ls_depth;
static int lg_hm_partial = 1;
static float lg_flash;                           /* a note just played (the picture) */

static uint32_t lg_rand(void)
{
    lg_rng ^= lg_rng << 13;
    lg_rng ^= lg_rng >> 17;
    lg_rng ^= lg_rng << 5;
    return lg_rng;
}
static float lg_unit(void) { return (float)(lg_rand() % 10000u) / 10000.0f; }

static void lg_build_scale(void)
{
    const md_patch *p = md_state();
    int base = 48 + (int)(p->root % 12);
    lg_nscale = 0;
    for (int n = base; n < base + 60 && lg_nscale < 64; n++)
        if (md_quantize(n, p->root, p->scale) == n)
            lg_scale[lg_nscale++] = n;
}

/* scale step d (0 = the root in octave 3) as a note; steps below wrap down an octave */
static int lg_note(int d)
{
    int per = 0, oct = 0;
    const md_patch *p = md_state();
    for (int k = 0; k < lg_nscale && lg_scale[k] < lg_scale[0] + 12; k++)
        per++;
    if (per < 1)
        return 60;
    while (d < 0)
        d += per, oct--;
    while (d >= lg_nscale)
        d -= per, oct++;
    (void)p;
    return lg_scale[d] + 12 * oct + 12 * dream_ui.octave;
}
static int lg_per_octave(void)
{
    int per = 0;
    for (int k = 0; k < lg_nscale && lg_scale[k] < lg_scale[0] + 12; k++)
        per++;
    return per > 0 ? per : 7;
}

/* a note to a target (0..5 a track, 6 POLY, 7 SYNTH) for gate frames; cents only on POLY */
static void lg_play(int target, int note, int vel, uint32_t gate, float cents)
{
    int slot = -1;
    note = note < 24 ? 24 : note > 108 ? 108 : note;
    for (int i = 0; i < LG_NOTES && slot < 0; i++)
        if (!lg_on[i].on)
            slot = i;
    if (slot < 0)
        return;
    if (target == 7) {                           /* SYNTH: its instrument plays it (no note off needed) */
        if (lab_v[P_SY_LEVEL] <= 0 || (int)lab_v[P_SY_PLAYED_BY] != 1)
            return;
        sy_note_on(note, vel);
        lg_flash = 1.0f;
        return;
    }
    if (target == 6) {
        if (lab_v[P_PO_LEVEL] <= 0)
            return;
        po_next_cents = cents;
        po_note_on_vel(note, 0, vel);
        po_next_cents = 0;
    } else if (md_state()->kit.enabled && target == 3)
        dream_cmd(MD_KIT_TRIGGER, 3, note % 8);
    else
        dream_cmd(MD_TRIGGER, target, note);
    lg_on[slot].note = note, lg_on[slot].target = target, lg_on[slot].on = 1, lg_on[slot].off_at = lg_frames + gate;
    lg_flash = 1.0f;
}

static void lg_release(int all)
{
    for (int i = 0; i < LG_NOTES; i++)
        if (lg_on[i].on && (all || (int32_t)(lg_frames - lg_on[i].off_at) >= 0)) {
            if (lg_on[i].target == 6)
                po_note_off(lg_on[i].note);
            else if (!(md_state()->kit.enabled && lg_on[i].target == 3))
                dream_cmd(MD_NOTE_OFF, lg_on[i].target, lg_on[i].note);
            lg_on[i].on = 0;
        }
}

static void lg_reset(void)
{
    int seed = (int)lab_v[P_LG_SEED];
    lg_rng = 0x9E3779B9u ^ (uint32_t)(seed * 2654435761u);
    if (!lg_rng)
        lg_rng = 1;
    lg_step = 0;
    lg_tin_phrase = 1, lg_tin_pos = 0, lg_tin_dir = 1, lg_tin_alt = 0;
    lg_ph_len = 0;
    memset(lg_ca, 0, sizeof lg_ca);
    lg_mk_deg = -1;
    lg_ls_len = 0, lg_ls_depth = 2;
    lg_hm_partial = 1;
    for (int i = 0; i < LG_HIST; i++)
        lg_hist1[i] = lg_hist2[i] = LG_REST;
}

/* the L-system's string at its depth */
static void lg_ls_grow(void)
{
    int g = (int)(lab_v[P_LG_SHAPE] / 34.0f);    /* 0 Fibonacci, 1 Thue-Morse, 2 Koch */
    static char tmp[LG_LSYS];
    lg_ls[0] = 'A', lg_ls_len = 1;
    for (int it = 0; it < lg_ls_depth; it++) {
        int n = 0;
        for (int i = 0; i < lg_ls_len && n < LG_LSYS - 6; i++) {
            char c = lg_ls[i];
            if (g == 0)
                n += c == 'A' ? (tmp[n] = 'A', tmp[n + 1] = 'B', 2) : (tmp[n] = 'A', 1);
            else if (g == 1)
                n += c == 'A' ? (tmp[n] = 'A', tmp[n + 1] = 'B', 2) : (tmp[n] = 'B', tmp[n + 1] = 'A', 2);
            else if (c == 'A')
                memcpy(tmp + n, "A+A-A", 5), n += 5;
            else
                tmp[n++] = c;
        }
        memcpy(lg_ls, tmp, (size_t)n);
        lg_ls_len = n;
    }
    lg_ls_pos = 0, lg_ls_deg = 0;
}

static int lg_triad_near(int d, int up)          /* the nearest tonic-triad step above (or below) d */
{
    int per = lg_per_octave(), third = per >= 7 ? 2 : 1, fifth = per >= 7 ? 4 : 2;
    for (int k = 1; k <= per; k++) {
        int c = up ? d + k : d - k, m = ((c % per) + per) % per;
        if (m == 0 || m == third || m == fifth)
            return c;
    }
    return d;
}

/* one step of the rule: voice 1 and voice 2 as scale steps (LG_REST: none); HARMONIC gives notes and cents (n1, n2) */
static void lg_rule_step(int rule, int *v1, int *v2, float *c1, float *c2, int *n1, int *n2)
{
    int per = lg_per_octave(), range = per * (int)lab_v[P_LG_RANGE], centre = per;
    float shape = lab_v[P_LG_SHAPE] / 100.0f, dens = lab_v[P_LG_DENSITY] / 100.0f, var = lab_v[P_LG_VARIATION] / 100.0f;
    *v1 = *v2 = LG_REST, *c1 = *c2 = 0, *n1 = *n2 = -1;
    switch (rule) {
    case LG_TINTINNABULI: {                      /* growing phrases from the centre, the triad answering */
        if (lg_tin_pos >= lg_tin_phrase) {      /* a phrase ends: a breath, the next one longer, the other way */
            lg_tin_pos = 0;
            lg_tin_phrase = lg_tin_phrase >= 8 ? 1 : lg_tin_phrase + 1;
            lg_tin_dir = -lg_tin_dir;
            return;
        }
        *v1 = centre + lg_tin_dir * lg_tin_pos;
        lg_tin_pos++;
        if (lg_unit() > dens + 0.15f)
            return;
        *v2 = shape < 0.34f ? lg_triad_near(*v1, 1) : shape < 0.67f ? lg_triad_near(*v1, (lg_tin_alt++ & 1) == 0) : lg_triad_near(*v1, 0);
        *v2 -= per;                              /* the T voice an octave lower: the bell under the line */
        break;
    }
    case LG_PHASING: {                           /* one pattern, two loops of L and L + 1 */
        int L = 5 + (int)(shape * 9.0f), i;
        if (lg_ph_len != L) {
            lg_ph_len = L;
            for (i = 0; i < L; i++)
                lg_ph_pat[i] = lg_unit() < dens ? centre + (int)(lg_rand() % (uint32_t)(per + 1)) : LG_REST;
        }
        if (lg_step % (uint32_t)L == 0 && lg_unit() < var * 0.5f) {   /* a cycle ends: maybe one note changes */
            i = (int)(lg_rand() % (uint32_t)L);
            lg_ph_pat[i] = lg_unit() < dens ? centre + (int)(lg_rand() % (uint32_t)(per + 1)) : LG_REST;
        }
        *v1 = lg_ph_pat[lg_step % (uint32_t)L];
        *v2 = lg_ph_pat[lg_step % (uint32_t)(L + 1)];
        if (*v2 != LG_REST)
            *v2 += per;                          /* the second loop an octave up */
        break;
    }
    case LG_FIBONACCI: {                         /* the golden word and the golden angle; a canon five steps on */
        const double phi = 1.6180339887498949;
        uint32_t i = lg_step;
        int hit = (int)(floor((double)(i + 2) / phi) - floor((double)(i + 1) / phi)) == 1;
        double fr = fmod((double)i * phi, 1.0);
        int span = 3 + (int)(shape * (float)range);
        if (hit && lg_unit() < dens + 0.2f)
            *v1 = centre - span / 2 + (int)(fr * (double)span);
        {
            int back = (lg_hist_w + LG_HIST - 5) % LG_HIST;   /* the canon: five steps ago, a third lower */
            if (lg_hist1[back] != LG_REST && lg_unit() < 0.7f)
                *v2 = lg_hist1[back] - 2 - per;
        }
        break;
    }
    case LG_AUTOMATON: {                         /* a generation each bar; living cells stacked in thirds */
        int col = (int)(lg_step % 16u), rule = LG_CA_RULES[(int)(shape * 7.99f)];
        if (col == 0) {
            uint16_t now = lg_ca[0], next = 0;
            if (!now || (lg_step && lg_unit() < var * 0.15f))
                now ^= (uint16_t)(1u << (lg_rand() % 16u));   /* a dead world, or a variation: a seed */
            if (lg_step == 0)
                now = (uint16_t)(lg_rand() & 0xFFFFu) | 0x0100u;
            for (int c = 0; c < 16; c++) {
                int l = (now >> ((c + 15) % 16)) & 1, m = (now >> c) & 1, r = (now >> ((c + 1) % 16)) & 1;
                next |= (uint16_t)(((rule >> (l * 4 + m * 2 + r)) & 1) << c);
            }
            memmove(lg_ca + 1, lg_ca, sizeof lg_ca - sizeof lg_ca[0]);
            lg_ca[0] = lg_step == 0 ? now : next;
        }
        if ((lg_ca[0] >> col) & 1 && lg_unit() < dens + 0.3f)
            *v1 = (col * 2) % (range + 1);       /* the columns in thirds */
        if ((lg_ca[0] >> col) & 1 && !((lg_ca[1] >> col) & 1))
            *v2 = (col * 2) % per - per;         /* born just now: the bass */
        break;
    }
    case LG_MARKOV: {                            /* steps likely, leaps rare, tonic and fifth pulling */
        static const float w[9] = {0.45f, 0.35f, 0.3f, 1.0f, 0.35f, 1.0f, 0.3f, 0.35f, 0.45f};   /* -4 .. +4 */
        float t = 0.3f + shape * 2.2f, sum = 0, r, ws[9];
        if (lg_mk_deg < 0)
            lg_mk_deg = centre;
        for (int k = 0; k < 9; k++) {
            int d = lg_mk_deg + k - 4, m = ((d % per) + per) % per;
            float x = w[k] * (m == 0 ? 1.6f : m == (per >= 7 ? 4 : 2) ? 1.3f : 1.0f);
            if (d < 0 || d > range)
                x = 0.001f;
            ws[k] = powf(x, 1.0f / t);
            sum += ws[k];
        }
        r = lg_unit() * sum;
        for (int k = 0; k < 9; k++)
            if ((r -= ws[k]) <= 0) {
                lg_mk_deg += k - 4;
                break;
            }
        if (lg_unit() < dens)
            *v1 = lg_mk_deg;
        if (lg_step % 8u == 0)                   /* the region's root below */
            *v2 = lg_mk_deg - (((lg_mk_deg % per) + per) % per) - per;
        break;
    }
    case LG_LSYSTEM: {                           /* the grammar's string, read as steps */
        if (!lg_ls_len || lg_ls_pos >= lg_ls_len) {
            lg_ls_depth = lg_ls_len && lg_ls_depth < 7 ? lg_ls_depth + 1 : 2;
            if (lg_unit() < var)
                lg_ls_depth = 2 + (int)(lg_rand() % 5u);
            lg_ls_grow();
        }
        {
            char c = lg_ls[lg_ls_pos++];
            if (c == 'A') {
                *v1 = centre + lg_ls_deg;
                lg_ls_deg = (lg_ls_deg + 1) % (range / 2 + 2);
            } else if (c == 'B') {
                if (lg_unit() < dens)
                    *v1 = centre + lg_ls_deg - 2;
                *v2 = centre - per + ((lg_ls_deg + 4) % per);
            } else if (c == '+')
                lg_ls_deg += 2;
            else if (c == '-')
                lg_ls_deg -= 3;
            lg_ls_deg = lg_ls_deg < -per ? -per : lg_ls_deg;
        }
        break;
    }
    case LG_HARMONIC: {                          /* overtones of the root: singing over the held fundamental */
        int top = 4 + (int)(shape * 12.0f);
        double f0 = (double)(36 + (int)(md_state()->root % 12) + 12 * dream_ui.octave);
        if (lg_unit() < dens) {
            int s = (int)(lg_rand() % 5u) - 2;   /* a walk, leaning up */
            lg_hm_partial += s >= 0 ? s : s / 2;
            if (lg_hm_partial < 3)
                lg_hm_partial = 3 + (int)(lg_rand() % 3u);
            if (lg_hm_partial > top)
                lg_hm_partial = top - (int)(lg_rand() % 3u);
            {
                double m = f0 + 12.0 * log2((double)lg_hm_partial);
                *n1 = (int)floor(m + 0.5);
                *c1 = (float)((m - (double)*n1) * 100.0);
                *v1 = lg_hm_partial;             /* (the picture shows the partial) */
            }
        }
        if (lg_step % 16u == 0) {                /* the fundamental and its octave, held */
            *n2 = (int)f0;
            *v2 = 1;
        }
        break;
    }
    }
}

/* LOGIC's clock: a step every RATE while the transport runs */
static void lg_run(uint32_t n)
{
    int playing = md_playing() && lab_v[P_LG_ON] > 0.5f, rule = (int)lab_v[P_LG_RULE] % LG_RULES;
    double step = 44100.0 * 60.0 / (double)(md_tempo_now() > 20 ? md_tempo_now() : 20) * LAB_RATE_BEATS[(int)lab_v[P_LG_RATE]];
    lg_frames += n;
    lg_flash *= 0.85f;
    lg_release(0);
    if (rule != lg_last_rule || (int)lab_v[P_LG_SEED] != lg_last_seed) {
        lg_last_rule = rule, lg_last_seed = (int)lab_v[P_LG_SEED];
        lg_release(1);
        lg_reset();
    }
    if (!playing) {
        if (lg_was_playing)
            lg_release(1);
        lg_was_playing = 0;
        lg_clock = 0;
        return;
    }
    if (!lg_was_playing) {
        lg_build_scale();
        lg_reset();
    }
    lg_was_playing = 1;
    lg_clock -= (double)n;
    while (lg_clock <= 0) {
        int v1, v2, n1, n2, t1 = (int)lab_v[P_LG_TARGET], t2 = (int)lab_v[P_LG_TARGET2];
        float c1, c2;
        uint32_t gate = (uint32_t)(step * lab_v[P_LG_GATE] / 100.0);
        lg_clock += step;
        if (lg_step % 16u == 0)
            lg_build_scale();                    /* (the key may have changed) */
        lg_rule_step(rule, &v1, &v2, &c1, &c2, &n1, &n2);
        t2 = t2 == 0 ? t1 : t2 - 1;              /* voice 2: SAME, a track, or POLY */
        if (rule == LG_HARMONIC) {
            if (n1 >= 0)
                lg_play(t1, n1, 96, gate, c1);
            if (n2 >= 0)
                lg_play(t2, n2, 70, (uint32_t)(step * 15.0), c2);
        } else {
            if (v1 != LG_REST)
                lg_play(t1, lg_note(v1), 96, gate, 0);
            if (v2 != LG_REST)
                lg_play(t2, lg_note(v2), 72, rule == LG_MARKOV ? (uint32_t)(step * 7.0) : gate, 0);
        }
        lg_hist1[lg_hist_w % LG_HIST] = v1;
        lg_hist2[lg_hist_w % LG_HIST] = v2;
        lg_hist_w++;
        lg_step++;
    }
}

const char *bk_logic_summary(void)
{
    static char buf[400];
    int n = snprintf(buf, sizeof buf, "%s step %u:", LG_RULE_NAMES[(int)lab_v[P_LG_RULE] % LG_RULES], (unsigned)lg_step);
    for (int i = 16; i > 0 && n < (int)sizeof buf - 12; i--) {
        int a = lg_hist1[(lg_hist_w + LG_HIST - i) % LG_HIST], b = lg_hist2[(lg_hist_w + LG_HIST - i) % LG_HIST];
        n += a == LG_REST ? snprintf(buf + n, sizeof buf - (size_t)n, " .") : snprintf(buf + n, sizeof buf - (size_t)n, " %d", a);
        if (b != LG_REST)
            n += snprintf(buf + n, sizeof buf - (size_t)n, "/%d", b);
    }
    return buf;
}
