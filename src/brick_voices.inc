/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the VOICES module of the Sound Lab.
 *
 * Spoken and sung voices (poems, stories, chants, mantras) from Ogg Vorbis or WAV files in
 * mutabledreams/samples/voices/ and mutabledreams/samples/archive/<item>/, cut at the breaths into sentences and
 * word fragments and played into the music:
 *   SENTENCE   whole sentences, one after the other
 *   FRAGMENTS  short pieces of words, rhythmic, up to three at once, some reversed
 *   VOWEL      a held vowel: two grains crossing over a small window, a pad from a voice
 *   MANTRA     one sentence (PHRASE) repeated in time with the bars
 * TRIGGER: FREE (by DENSITY), the hits of a track, or every bar. PITCH, REVERSE chance, RADIO (telephone band,
 * crackle, a little drive), SPREAD, and sends into CLOUD, PARTICLES, SEEDS and SHIMMER (SENDS page).
 *
 * A file is decoded and analysed in a thread of its own (stb_vorbis, third_party/stb, public domain/MIT); the sound
 * takes the finished file between two blocks, so a long file never stops the music. A file may have a .txt beside
 * it: its first line the title, its second the credit (author, reader, licence), shown in the Lab and the garden.
 * At most 6 minutes of each file are used. Included by brick_lab.inc. */
#include <pthread.h>
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_INTEGER_CONVERSION
#include "third_party/stb/stb_vorbis.c"
#undef L
#undef C
#undef R

#define VO_MAX_FILES 96
#define VO_MAX_SECONDS 360
#define VO_MAX_PH 1024
#define VO_MAX_FR 4096
#define VO_VOICES 3
#define VO_PEAKS 330
#define VO_WIN 441                               /* the analysis window: 10 ms */

typedef struct {
    int index, ok;
    char error[64], title[96], credit[128];
    int16_t *pcm;                                /* mono, 44.1 kHz */
    uint32_t frames, src_rate, src_channels;
    float gain;                                  /* to the common speech level */
    int nph, nfr;
    uint32_t ph_a[VO_MAX_PH], ph_b[VO_MAX_PH], fr_a[VO_MAX_FR], fr_b[VO_MAX_FR];
    float peak_min[VO_PEAKS], peak_max[VO_PEAKS];
} vo_file;

static char vo_names[VO_MAX_FILES][128];         /* relative to samples/: voices/x.ogg, archive/item/x.ogg */
static int vo_nfiles = -1;
static vo_file *vo_cur, *volatile vo_ready;
static volatile int vo_busy, vo_sync;
static const char *const VC_MODES[5] = {"SENTENCE", "FRAGMENTS", "VOWEL", "MANTRA", "GRAINS"};
static const char *const VO_TIDE_TO[6] = {"POSITION", "PITCH", "GRAIN SIZE", "GRAINS", "SPRAY", "LEVEL"};

static struct { double pos, step; uint32_t a, b; int dir, on, stop; float env, pan; } vo_v[VO_VOICES];
static float vo_buf[2 * APP_BLOCK], vo_level, vo_env;   /* (vo_env: how much a voice speaks now, for the garden) */
static int vo_active, vo_now_phrase = -1;
static uint32_t vo_frames, vo_wait_until, vo_last_event, vo_last_step, vo_mantra_steps, vo_phrase_started;
static int vo_next_phrase;
static float vo_hp[2][2], vo_lp[2][2], vo_crackle;
static uint32_t vo_rng = 0x9E3779B9u;
static uint32_t vo_rand(void)
{
    vo_rng ^= vo_rng << 13;
    vo_rng ^= vo_rng >> 17;
    vo_rng ^= vo_rng << 5;
    return vo_rng;
}

/* ---- GRAINS: the voice as a granular sampler ----
 * Grains of the file itself (not of a live buffer): POSITION where in the voice, SCAN moving through it (100 % =
 * the speed it was spoken, negative backwards, 0 frozen), GRAIN SIZE, GRAINS per second, SPRAY (scatter in time,
 * up to a second either side), SCATTER (pitch in steps of a pentatonic scale), REVERSE CHANCE, PITCH, SPREAD.
 * TRIGGER FREE: a steady cloud; a track's hits or the bar: bursts; MARBLES: on each sixteenth a second Marbles
 * (brick_mi_marbles.cpp) decides whether a burst comes (DENSITY) and where in the voice (DEJA VU loops of 8 bring
 * the same words back).
 * TIDE: Mutable Instruments' Tides 2 (its PolySlopeGenerator, looping, control range; brick_mi_tides.cpp): RATE
 * (60 s to a quarter second), SLOPE (rise against fall), SHAPE (its wavetable of curves), SMOOTH (below the
 * middle: its low-pass; above: its wavefolder), DEPTH onto TIDE TO (position, pitch, grain size, grains, spray,
 * level). */
#define VO_GRAINS 24
static struct { double pos, step; uint32_t age, len; float gl, gr; int dir; } vo_g[VO_GRAINS];
static double vo_scan_off;
static float vo_centre = 0.25f, vo_tide, vo_tide_ph, vo_marble = -1;
static float vo_hann[512];
static int32_t vo_grain_wait;

/* the tide: Mutable Instruments' Tides 2 itself (brick_mi_tides.cpp), looping at control range */
static float vo_tide_step(uint32_t n)
{
    float rate = lab_v[P_VO_TIDE_RATE] / 100, period = 60.0f * powf(0.25f / 60.0f, rate);
    vo_tide_ph += (float)n / 44100.0f / period;   /* (a phase of our own, for the picture) */
    vo_tide_ph -= floorf(vo_tide_ph);
    return mi_tides_render(1.0f / period, lab_v[P_VO_TIDE_SLOPE] / 100, lab_v[P_VO_TIDE_SHAPE] / 100, lab_v[P_VO_TIDE_SMOOTH] / 100, n);
}

/* one cycle of the tide as it is set, for drawing (made again only when a setting changes) */
static const float *vo_tide_points(void)
{
    static float pts[64], key[3] = {-1, -1, -1};
    float k[3] = {lab_v[P_VO_TIDE_SLOPE], lab_v[P_VO_TIDE_SHAPE], lab_v[P_VO_TIDE_SMOOTH]};
    if (k[0] != key[0] || k[1] != key[1] || k[2] != key[2]) {
        mi_tides_preview(k[0] / 100, k[1] / 100, k[2] / 100, pts, 64);
        memcpy(key, k, sizeof key);
    }
    return pts;
}

static void vo_grain(const vo_file *vf, float centre, float size_s, double step, float spray)
{
    static const int PENTA[6] = {0, 2, 4, 7, 9, 12};
    int i, g = -1;
    uint32_t len = (uint32_t)(size_s * 44100.0f), oldest = 0;
    double span, pos;
    float pan;
    if (len < 256)
        len = 256;
    for (i = 0; i < VO_GRAINS && g < 0; i++)
        if (!vo_g[i].len)
            g = i;
    (void)oldest;
    if (g < 0)                                   /* none free: this one is not born (cutting one off would click) */
        return;
    {
        int sc = (int)lab_v[P_VO_SCATTER];
        if (sc > 0) {                            /* a step of the pentatonic scale, up or down */
            int k = PENTA[vo_rand() % 6u];
            while (k > sc)
                k = PENTA[vo_rand() % 6u];
            step *= pow(2.0, (double)((vo_rand() & 1) ? k : -k) / 12.0);
        }
    }
    span = (double)len * step + 4;
    if ((double)vf->frames <= span + 8)
        return;
    pos = (double)centre * (double)vf->frames + ((double)(vo_rand() % 20001u) / 10000.0 - 1.0) * (double)spray * 44100.0;
    pos = fmod(pos, (double)vf->frames - span);
    if (pos < 0)
        pos += (double)vf->frames - span;
    vo_g[g].dir = (float)(vo_rand() % 100u) < lab_v[P_VO_REVERSE] ? -1 : 1;
    vo_g[g].pos = vo_g[g].dir < 0 ? pos + span - 4 : pos;
    vo_g[g].step = step, vo_g[g].age = 0, vo_g[g].len = len;
    pan = ((float)(vo_rand() % 2001u) / 1000.0f - 1.0f) * lab_v[P_VO_SPREAD] / 100;
    vo_g[g].gl = sqrtf(0.5f * (1 - pan)) * 1.4142f, vo_g[g].gr = sqrtf(0.5f * (1 + pan)) * 1.4142f;
}

/* ---- the files ---- */
static void vo_dir(char *out, size_t n) { snprintf(out, n, "%s/../samples", bk_dir); }

static int vo_is_sound(const char *nm)
{
    size_t l = strlen(nm);
    if (l < 5 || nm[0] == '.')
        return 0;
    return (!strcasecmp(nm + l - 4, ".ogg") || !strcasecmp(nm + l - 4, ".wav"));
}

static void vo_scan_dir(const char *rel)
{
    char dir[700];
    DIR *d;
    struct dirent *e;
    vo_dir(dir, sizeof dir);
    snprintf(dir + strlen(dir), sizeof dir - strlen(dir), "/%s", rel);
    if (!(d = opendir(dir)))
        return;
    while ((e = readdir(d)) && vo_nfiles < VO_MAX_FILES)
        if (vo_is_sound(e->d_name) && strlen(rel) + strlen(e->d_name) + 2 < 128)
            snprintf(vo_names[vo_nfiles++], 128, "%s/%s", rel, e->d_name);
    closedir(d);
}

static int vo_cmp(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
static int vo_fcmp(const void *a, const void *b) { float x = *(const float *)a, y = *(const float *)b; return x < y ? -1 : x > y; }

static void vo_scan(void)
{
    char dir[700];
    DIR *d;
    struct dirent *e;
    vo_nfiles = 0;
    if (!bk_dir[0])
        return;
    vo_scan_dir("voices");
    vo_dir(dir, sizeof dir);
    strncat(dir, "/archive", sizeof dir - strlen(dir) - 1);
    if ((d = opendir(dir))) {                     /* each archive item a folder */
        while ((e = readdir(d)) && vo_nfiles < VO_MAX_FILES)
            if (e->d_name[0] != '.' && strlen(e->d_name) < 80) {
                char rel[100];
                snprintf(rel, sizeof rel, "archive/%s", e->d_name);
                vo_scan_dir(rel);
            }
        closedir(d);
    }
    qsort(vo_names, (size_t)vo_nfiles, 128, vo_cmp);
}

/* the file's name for the screen: without folder and ending, in capitals */
static void vo_display_name(int i, char *buf, size_t n)
{
    const char *s = strrchr(vo_names[i], '/');
    size_t l;
    snprintf(buf, n, "%s", s ? s + 1 : vo_names[i]);
    l = strlen(buf);
    if (l > 4)
        buf[l - 4] = 0;
    for (char *q = buf; *q; q++)
        *q = *q == '_' || *q == '-' ? ' ' : (char)toupper((unsigned char)*q);
}

/* the source as mono float at its own rate: 0 = read */
static int vo_read_ogg(const char *path, float **out, uint32_t *frames, uint32_t *rate, uint32_t *chans, char *err)
{
    int e = 0;
    stb_vorbis *v = stb_vorbis_open_filename(path, &e, 0);
    stb_vorbis_info in;
    uint32_t cap, n = 0;
    float *buf, chunk[4096 * 2];
    if (!v) {
        snprintf(err, 64, "BROKEN OR UNREADABLE OGG (%d)", e);
        return -1;
    }
    in = stb_vorbis_get_info(v);
    *rate = in.sample_rate;
    *chans = (uint32_t)in.channels;
    cap = in.sample_rate * VO_MAX_SECONDS;
    if (!(buf = (float *)malloc(sizeof(float) * cap))) {
        stb_vorbis_close(v);
        snprintf(err, 64, "NOT ENOUGH MEMORY");
        return -1;
    }
    for (;;) {
        int got = stb_vorbis_get_samples_float_interleaved(v, 2, chunk, 4096 * 2);
        if (got <= 0)
            break;
        for (int k = 0; k < got && n < cap; k++)
            buf[n++] = (in.channels > 1 ? (chunk[2 * k] + chunk[2 * k + 1]) * 0.5f : chunk[2 * k]);
        if (n >= cap)
            break;
    }
    stb_vorbis_close(v);
    *out = buf;
    *frames = n;
    return 0;
}

static int vo_read_wav(const char *path, float **out, uint32_t *frames, uint32_t *rate, uint32_t *chans, char *err)
{
    FILE *f = fopen(path, "rb");
    uint8_t hdr[12], ch[8];
    uint32_t fmt = 0, nch = 0, sr = 0, bits = 0, len = 0, n, bpf, k;
    long at = -1;
    float *buf;
    uint8_t *raw;
    if (!f || fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
        if (f)
            fclose(f);
        snprintf(err, 64, "NOT A WAV FILE");
        return -1;
    }
    while (fread(ch, 1, 8, f) == 8) {
        uint32_t l = sa_rd32(ch + 4);
        if (!memcmp(ch, "fmt ", 4)) {
            uint8_t b[40] = {0};
            if (fread(b, 1, l < 40 ? l : 40, f) < 16)
                break;
            if (l > 40)
                fseek(f, (long)(l - 40), SEEK_CUR);
            fmt = sa_rd16(b), nch = sa_rd16(b + 2), sr = sa_rd32(b + 4), bits = sa_rd16(b + 14);
            if (fmt == 0xFFFEu && l >= 26)
                fmt = sa_rd16(b + 24);
        } else if (!memcmp(ch, "data", 4)) {
            at = ftell(f);
            len = l;
            break;
        } else
            fseek(f, (long)(l + (l & 1u)), SEEK_CUR);
    }
    if (at < 0 || (fmt != 1 && fmt != 3) || nch < 1 || nch > 2 || sr < 8000 || sr > 192000 ||
        !(bits == 16 || bits == 24 || bits == 32) || (fmt == 3 && bits != 32)) {
        fclose(f);
        snprintf(err, 64, "UNSUPPORTED WAV");
        return -1;
    }
    bpf = nch * bits / 8;
    n = len / bpf;
    if (n > sr * VO_MAX_SECONDS)
        n = sr * VO_MAX_SECONDS;
    buf = (float *)malloc(sizeof(float) * (n + 1));
    raw = (uint8_t *)malloc((size_t)bpf * 4096);
    for (k = 0; k < n;) {
        uint32_t m = n - k < 4096 ? n - k : 4096, got = (uint32_t)fread(raw, bpf, m, f);
        for (uint32_t j = 0; j < got; j++) {
            float s = 0;
            for (uint32_t c = 0; c < nch; c++) {
                const uint8_t *p = raw + j * bpf + c * (bits / 8);
                float v;
                if (fmt == 3) {
                    uint32_t u = sa_rd32(p);
                    memcpy(&v, &u, 4);
                } else if (bits == 16)
                    v = (float)(int16_t)sa_rd16(p) / 32768.0f;
                else if (bits == 24)
                    v = (float)((int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8) / 8388608.0f;
                else
                    v = (float)(int32_t)sa_rd32(p) / 2147483648.0f;
                s += v != v ? 0 : v;
            }
            buf[k + j] = s / (float)nch;
        }
        k += got;
        if (got < m)
            break;
    }
    free(raw);
    fclose(f);
    *out = buf, *frames = k, *rate = sr, *chans = nch;
    return 0;
}

/* where the breaths are. A pause is where the voice falls well below its own surroundings (the mean over +-1.5 s),
 * not below a fixed level: so a reading over room noise or soft music is cut as well as a clean one. Sentences
 * between pauses of 250 ms, fragments between dips of 50 ms inside them. */
static void vo_analyse(vo_file *vf)
{
    uint32_t nw = vf->frames / VO_WIN, w, k;
    float *e = (float *)malloc(sizeof(float) * (nw + 1)), *es, *lm, peak = 0, voiced2 = 0, quiet_abs;
    double *cs;
    uint8_t *quiet, *dipq;
    uint32_t nvoiced = 0;
    if (!e || nw < 10) {
        free(e);
        return;
    }
    {   /* RMS per 10 ms, in the speech band (about 300 Hz .. 3.4 kHz): hum and rumble under a reading don't count */
        float h1 = 0, h2 = 0, l1 = 0, l2 = 0;
        for (w = 0; w < nw; w++) {
            double s = 0;
            for (k = 0; k < VO_WIN; k++) {
                float x = (float)vf->pcm[w * VO_WIN + k] / 32768.0f, y;
                h1 += (x - h1) * 0.042f;
                y = x - h1;
                h2 += (y - h2) * 0.042f;
                y -= h2;
                l1 += (y - l1) * 0.38f;
                l2 += (l1 - l2) * 0.38f;
                s += l2 * l2;
            }
            e[w] = (float)sqrt(s / VO_WIN);
            peak = e[w] > peak ? e[w] : peak;
        }
    }
    for (w = 0; w < nw && 0; w++) {
        double s = 0;
        for (k = 0; k < VO_WIN; k++) {
            float v = (float)vf->pcm[w * VO_WIN + k] / 32768.0f;
            s += v * v;
        }
        e[w] = (float)sqrt(s / VO_WIN);
        peak = e[w] > peak ? e[w] : peak;
    }
    es = (float *)malloc(sizeof(float) * nw), lm = (float *)malloc(sizeof(float) * nw);
    cs = (double *)malloc(sizeof(double) * (nw + 1));
    quiet = (uint8_t *)malloc(nw), dipq = (uint8_t *)malloc(nw);
    for (w = 0; w < nw; w++)                      /* smoothed over 30 ms */
        es[w] = (e[w > 0 ? w - 1 : 0] + e[w] + e[w + 1 < nw ? w + 1 : w]) / 3.0f;
    cs[0] = 0;
    for (w = 0; w < nw; w++)
        cs[w + 1] = cs[w] + es[w];
    quiet_abs = peak * 0.02f;
    for (w = 0; w < nw; w++) {                    /* the surroundings: +-1.5 s */
        uint32_t a = w > 150 ? w - 150 : 0, b = w + 150 < nw ? w + 150 : nw;
        lm[w] = (float)((cs[b] - cs[a]) / (double)(b - a));
        quiet[w] = es[w] < fmaxf(lm[w] * 0.45f, quiet_abs);
        dipq[w] = es[w] < fmaxf(lm[w] * 0.62f, quiet_abs);
        if (!quiet[w]) {
            voiced2 += e[w] * e[w];
            nvoiced++;
        }
    }
    if (getenv("VO_DEBUG"))
        fprintf(stderr, "voices: windows %u, peak %.4f, voiced %u\n", nw, peak, nvoiced);
    {   /* sentences */
        uint32_t start = 0, silent = 0, in = 0;
        for (w = 0; w <= nw && vf->nph < VO_MAX_PH; w++) {
            int on = w < nw && !quiet[w];
            if (!in && on)
                in = 1, start = w, silent = 0;
            else if (in && !on) {
                if (++silent >= 25 || w == nw) {   /* 250 ms of quiet: the sentence ends */
                    uint32_t end = w - silent + 1;
                    if (end - start >= 25) {
                        uint32_t a = start > 3 ? start - 3 : 0, b = end + 4 < nw ? end + 4 : nw;
                        while (b - a > 1200 && vf->nph < VO_MAX_PH - 1) {   /* (very long: in pieces of 12 s) */
                            vf->ph_a[vf->nph] = a * VO_WIN, vf->ph_b[vf->nph++] = (a + 1200) * VO_WIN;
                            a += 1200;
                        }
                        vf->ph_a[vf->nph] = a * VO_WIN, vf->ph_b[vf->nph++] = b * VO_WIN;
                    }
                    in = 0;
                }
            } else if (in)
                silent = 0;
        }
    }
    for (int p = 0; p < vf->nph && vf->nfr < VO_MAX_FR; p++) {   /* fragments inside each sentence */
        uint32_t a = vf->ph_a[p] / VO_WIN, b = vf->ph_b[p] / VO_WIN, start = a, dip = 0;
        for (w = a; w < b && vf->nfr < VO_MAX_FR; w++) {
            dip = dipq[w] ? dip + 1 : 0;
            if ((dip >= 5 && w - start >= 15) || w - start >= 90 || w == b - 1) {
                if (w - start >= 15)
                    vf->fr_a[vf->nfr] = start * VO_WIN, vf->fr_b[vf->nfr++] = (w + 1) * VO_WIN;
                start = w + 1;
                dip = 0;
            }
        }
    }
    voiced2 = 0;                                  /* the loudness: the whole signal where the voice speaks */
    for (w = 0; w < nw; w++)
        if (!quiet[w]) {
            double sq = 0;
            for (k = 0; k < VO_WIN; k += 3) {
                float v = (float)vf->pcm[w * VO_WIN + k] / 32768.0f;
                sq += v * v;
            }
            voiced2 += (float)(sq / (VO_WIN / 3));
        }
    vf->gain = nvoiced ? 0.08f / sqrtf(voiced2 / (float)nvoiced) : 1.0f;   /* every voice at about -22 dBFS RMS */
    vf->gain = vf->gain > 20.0f ? 20.0f : vf->gain;
    for (int i = 0; i < VO_PEAKS; i++) {         /* the picture */
        uint32_t a = (uint32_t)((uint64_t)vf->frames * i / VO_PEAKS), b = (uint32_t)((uint64_t)vf->frames * (i + 1) / VO_PEAKS);
        float lo = 0, hi = 0;
        for (k = a; k < b; k += 7) {
            float v = (float)vf->pcm[k] / 32768.0f * vf->gain * 1.3f;
            lo = v < lo ? v : lo, hi = v > hi ? v : hi;
        }
        vf->peak_min[i] = lo < -1 ? -1 : lo, vf->peak_max[i] = hi > 1 ? 1 : hi;
    }
    free(e), free(es), free(lm), free(cs), free(quiet), free(dipq);
}

static vo_file *vo_load(int index)
{
    vo_file *vf = (vo_file *)calloc(1, sizeof(vo_file));
    char path[800], side[800];
    float *src = 0;
    uint32_t frames = 0, rate = 0, chans = 0, i;
    FILE *f;
    if (!vf)
        return 0;
    vf->index = index;
    if (index < 0 || index >= vo_nfiles) {
        snprintf(vf->error, sizeof vf->error, "NO FILE");
        return vf;
    }
    vo_dir(path, sizeof path);
    snprintf(path + strlen(path), sizeof path - strlen(path), "/%s", vo_names[index]);
    if ((strlen(path) > 4 && !strcasecmp(path + strlen(path) - 4, ".ogg") ? vo_read_ogg : vo_read_wav)(path, &src, &frames, &rate, &chans, vf->error))
        return vf;
    if (frames < rate / 2) {
        free(src);
        snprintf(vf->error, sizeof vf->error, "FILE TOO SHORT");
        return vf;
    }
    vf->frames = (uint32_t)((double)frames * 44100.0 / (double)rate);
    vf->pcm = (int16_t *)malloc(sizeof(int16_t) * (vf->frames + 2));
    for (i = 0; i < vf->frames; i++) {           /* to 44.1 kHz, linear */
        double sp = (double)i * (double)rate / 44100.0;
        uint32_t a = (uint32_t)sp;
        float fr = (float)(sp - a), v = a + 1 < frames ? src[a] + (src[a + 1] - src[a]) * fr : src[frames - 1];
        v = v > 1 ? 1 : v < -1 ? -1 : v;
        vf->pcm[i] = (int16_t)lrintf(v * 32767.0f);
    }
    vf->pcm[vf->frames] = vf->pcm[vf->frames + 1] = 0;
    free(src);
    vf->src_rate = rate, vf->src_channels = chans;
    vo_analyse(vf);
    snprintf(side, sizeof side, "%.*s.txt", (int)(strlen(path) - 4), path);   /* the sidecar: title, credit */
    if ((f = fopen(side, "r"))) {
        if (fgets(vf->title, sizeof vf->title, f))
            vf->title[strcspn(vf->title, "\r\n")] = 0;
        if (fgets(vf->credit, sizeof vf->credit, f))
            vf->credit[strcspn(vf->credit, "\r\n")] = 0;
        fclose(f);
    }
    if (!vf->title[0])
        vo_display_name(index, vf->title, sizeof vf->title);
    if (getenv("VO_DEBUG"))
        fprintf(stderr, "voices: %s: %u frames (%u Hz, %u ch), %d sentences, %d fragments, gain %.2f, %s\n", vo_names[index],
                vf->frames, rate, chans, vf->nph, vf->nfr, vf->gain, vf->error);
    vf->ok = vf->nph > 0;
    if (!vf->ok)
        snprintf(vf->error, sizeof vf->error, "NO VOICE FOUND IN IT");
    return vf;
}

static void *vo_thread(void *arg)
{
    vo_file *vf = vo_load((int)(intptr_t)arg);
    if (vf && getenv("VO_DEBUG") && vf->error[0])
        fprintf(stderr, "voices: file %d: %s\n", vf->index, vf->error);
    __atomic_store_n(&vo_ready, vf, __ATOMIC_RELEASE);
    return 0;
}

/* load a file: in a thread of its own (or at once, for the test runner) */
static void vo_request(int index)
{
    pthread_t th;
    if (vo_busy)
        return;
    vo_busy = 1;
    if (vo_sync || pthread_create(&th, 0, vo_thread, (void *)(intptr_t)index))
        vo_thread((void *)(intptr_t)index);
    else
        pthread_detach(th);
}

/* ---- playing ---- */
static void vo_start(uint32_t a, uint32_t b, int rev)
{
    int i, v = -1;
    float quiet = 2;
    for (i = 0; i < VO_VOICES && v < 0; i++)
        if (!vo_v[i].on)
            v = i;
    for (i = 0; i < VO_VOICES && v < 0; i++)     /* none free: the quietest stops, this one takes its place */
        if (vo_v[i].env < quiet)
            quiet = vo_v[i].env, v = i;
    if (v < 0)
        v = 0;
    vo_v[v].a = a, vo_v[v].b = b, vo_v[v].dir = rev ? -1 : 1;
    vo_v[v].pos = rev ? (double)b - 1 : (double)a;
    vo_v[v].step = pow(2.0, lab_v[P_VO_PITCH] / 12.0);
    vo_v[v].on = 1, vo_v[v].stop = 0, vo_v[v].env = 1;
    vo_v[v].pan = ((float)(vo_rand() % 2001u) / 1000.0f - 1.0f) * lab_v[P_VO_SPREAD] / 100;
}

static void vo_stop_all(void)
{
    for (int i = 0; i < VO_VOICES; i++)
        if (vo_v[i].on)
            vo_v[i].stop = 1;
}

static int vo_any_on(void)
{
    for (int i = 0; i < VO_VOICES; i++)
        if (vo_v[i].on && !vo_v[i].stop)
            return 1;
    return 0;
}

static void vo_trigger(const vo_file *vf, int mode)
{
    int rev = (float)(vo_rand() % 100u) < lab_v[P_VO_REVERSE];
    switch (mode) {
    case 0:                                      /* a sentence: the next one */
        if (vo_any_on())
            return;
        vo_next_phrase %= vf->nph;
        vo_now_phrase = vo_next_phrase;
        vo_start(vf->ph_a[vo_next_phrase], vf->ph_b[vo_next_phrase], rev);
        vo_next_phrase++;
        vo_phrase_started = vo_frames;
        break;
    case 1:                                      /* a fragment, anywhere */
        if (vf->nfr) {
            int f = (int)(vo_rand() % (uint32_t)vf->nfr);
            vo_start(vf->fr_a[f], vf->fr_b[f], rev);
        }
        break;
    case 2: {                                    /* the vowel moves to another fragment's middle */
        int f = vf->nfr ? (int)(vo_rand() % (uint32_t)vf->nfr) : 0;
        uint32_t mid = vf->nfr ? (vf->fr_a[f] + vf->fr_b[f]) / 2 : vf->ph_a[0];
        vo_v[0].a = mid > 4000 ? mid - 4000 : 0, vo_v[0].b = mid + 4000 < vf->frames ? mid + 4000 : vf->frames;
        break;
    }
    case 4: {                                    /* GRAINS: a burst where the voice stands now (or where Marbles says) */
        int k, nb = 2 + (int)lab_v[P_VO_GRAINS] / 8;
        for (k = 0; k < nb; k++)
            vo_grain(vf, vo_marble >= 0 && (int)lab_v[P_VO_TRIGGER] == 8 ? vo_marble : vo_centre, lab_v[P_VO_SIZE] / 1000, pow(2.0, lab_v[P_VO_PITCH] / 12.0), lab_v[P_VO_SPRAY] / 100);
        break;
    }
    case 3: {                                    /* the mantra: the chosen sentence again */
        int p = ((int)lab_v[P_VO_PHRASE] - 1) % vf->nph;
        vo_stop_all();
        vo_now_phrase = p;
        vo_start(vf->ph_a[p], vf->ph_b[p], 0);
        vo_phrase_started = vo_frames;
        break;
    }
    }
}

static inline float vo_read(const vo_file *vf, double pos)
{
    uint32_t a = (uint32_t)pos;
    float f = (float)(pos - a);
    if (a + 1 >= vf->frames)
        return 0;
    return ((float)vf->pcm[a] + ((float)vf->pcm[a + 1] - (float)vf->pcm[a]) * f) / 32768.0f;
}

/* n frames into vo_buf; 0 = nothing sounds */
static int vo_render(uint32_t n)
{
    const md_stats *st = md_status();
    vo_file *ready = __atomic_load_n(&vo_ready, __ATOMIC_ACQUIRE), *vf;
    float level = lab_v[P_VO_LEVEL] / 100, radio = lab_v[P_VO_RADIO] / 100, lvl = 0;
    int file = (int)lab_v[P_VO_FILE], mode = clamp((int)lab_v[P_VO_MODE], 0, 4), trig = (int)lab_v[P_VO_TRIGGER];
    uint32_t k;
    if (ready) {                                  /* a file finished loading: take it */
        vo_file *old = vo_cur;
        vo_cur = ready;
        __atomic_store_n(&vo_ready, (vo_file *)0, __ATOMIC_RELEASE);
        vo_busy = 0;
        memset(vo_v, 0, sizeof vo_v);
        vo_next_phrase = ((int)lab_v[P_VO_PHRASE] - 1) % (vo_cur->nph > 0 ? vo_cur->nph : 1);
        vo_wait_until = vo_frames;
        if (old) {                               /* (the stage may still be drawing it) */
            bk_defer_free(old->pcm);
            bk_defer_free(old);
        }
    }
    if (vo_nfiles < 0)
        vo_scan();
    if (vo_nfiles > 0 && file < vo_nfiles && (!vo_cur || vo_cur->index != file) && !vo_busy)
        vo_request(file);
    vf = vo_cur;
    vo_frames += n;
    if (!vf || !vf->ok || level <= 0) {
        vo_level = vo_env = 0;
        return 0;
    }
    /* the triggers: a hit of the track, a bar, or free time */
    if (trig >= 1 && trig <= 6) {
        uint32_t ev = st->events[trig - 1];
        if (ev != vo_last_event) {
            vo_last_event = ev;
            if (mode != 3)
                vo_trigger(vf, mode);
        }
    }
    if (st->step != vo_last_step) {               /* the sixteenths, while playing */
        uint32_t steps = st->step - vo_last_step;
        vo_last_step = st->step;
        if (md_playing() && steps < 64) {
            vo_mantra_steps += steps;
            if (trig == 7 && st->step % 16u == 0 && mode != 3)
                vo_trigger(vf, mode);
            if (trig == 8 && mode != 3) {        /* MARBLES: whether, and (for GRAINS) where */
                mi_random_params mp = {lab_v[P_VO_DEJAVU] / 100, 0.5f, 0.5f, lab_v[P_VO_DENSITY] / 100, 8};
                float where;
                if (mi_random2_step(&mp, &where)) {
                    vo_marble = where;
                    if (mode != 0 || !vo_any_on())
                        vo_trigger(vf, mode);
                }
            }
            if (trig == 0 && mode == 1 && (float)(vo_rand() % 100u) < lab_v[P_VO_DENSITY] * 0.5f)
                vo_trigger(vf, 1);               /* free fragments: a chance on every sixteenth */
            if (mode == 3) {                     /* the mantra: again after its length, rounded up to whole beats */
                int p = ((int)lab_v[P_VO_PHRASE] - 1) % vf->nph;
                double beat = 44100.0 * 60.0 / (double)(md_tempo_now() > 20 ? md_tempo_now() : 20);
                uint32_t beats = (uint32_t)ceil((double)(vf->ph_b[p] - vf->ph_a[p]) / beat) + 1;
                if (vo_mantra_steps >= beats * 4 || !vo_any_on())
                    if (st->step % 4u == 0 || !vo_any_on()) {
                        vo_trigger(vf, 3);
                        vo_mantra_steps = 0;
                    }
            }
        }
    }
    if (trig == 0 && !vo_any_on()) {              /* free: the next after a pause set by DENSITY */
        if (mode == 0 && (int32_t)(vo_frames - vo_wait_until) >= 0) {
            vo_trigger(vf, 0);
            vo_wait_until = vo_frames + (uint32_t)(44100.0f * (0.3f + (1.0f - lab_v[P_VO_DENSITY] / 100) * 6.0f));
        } else if (mode == 1 && !md_playing() && (vo_rand() % 1000u) < (uint32_t)(lab_v[P_VO_DENSITY] * 0.3f))
            vo_trigger(vf, 1);
        else if (mode == 3 && !md_playing())
            vo_trigger(vf, 3);
    }
    if (mode == 0 && trig == 0 && vo_any_on())
        vo_wait_until = vo_frames + (uint32_t)(44100.0f * (0.3f + (1.0f - lab_v[P_VO_DENSITY] / 100) * 6.0f));
    memset(vo_buf, 0, sizeof(float) * 2 * n);
    {   /* the tide, and what it moves */
        float d = lab_v[P_VO_TIDE_DEPTH] / 100;
        vo_tide = vo_tide_step(n);
        if (d > 0 && (int)lab_v[P_VO_TIDE_TO] == 5)
            level *= 1 - d + d * vo_tide;
        if (d > 0 && (int)lab_v[P_VO_TIDE_TO] == 1 && mode != 4 && mode != 2)   /* pitch on the speaking voices too */
            for (int v = 0; v < VO_VOICES; v++)
                vo_v[v].step = pow(2.0, (double)(lab_v[P_VO_PITCH] + (vo_tide * 2 - 1) * 12.0f * d) / 12.0);
    }
    if (mode == 4) {                              /* GRAINS */
        int to = (int)lab_v[P_VO_TIDE_TO];
        float d = lab_v[P_VO_TIDE_DEPTH] / 100, bip = vo_tide * 2 - 1;
        float size = lab_v[P_VO_SIZE] / 1000, rate = lab_v[P_VO_GRAINS], spray = lab_v[P_VO_SPRAY] / 100;
        double step = pow(2.0, lab_v[P_VO_PITCH] / 12.0);
        float ov, g;
        if (!vo_hann[256])
            for (k = 0; k < 512; k++)
                vo_hann[k] = 0.5f - 0.5f * cosf(6.2831853f * (float)k / 511.0f);
        vo_scan_off += (double)lab_v[P_VO_SCAN] / 100.0 * (double)n;
        vo_scan_off = fmod(vo_scan_off, (double)vf->frames);
        vo_centre = lab_v[P_VO_POSITION] / 100 + (float)(vo_scan_off / (double)vf->frames);
        if (d > 0) {
            if (to == 0) vo_centre += bip * 0.5f * d;
            else if (to == 1) step *= pow(2.0, (double)(bip * 12.0f * d) / 12.0);
            else if (to == 2) size *= powf(4.0f, bip * d);
            else if (to == 3) rate *= powf(4.0f, bip * d);
            else if (to == 4) spray += vo_tide * d;
        }
        vo_centre -= floorf(vo_centre);
        if (trig == 0) {                          /* FREE: a steady cloud of grains */
            vo_marble = -1;
            vo_grain_wait -= (int32_t)n;
            while (vo_grain_wait <= 0) {
                vo_grain(vf, vo_centre, size, step, spray);
                vo_grain_wait += (int32_t)(44100.0f / (rate > 0.5f ? rate : 0.5f) * (0.7f + 0.6f * (float)(vo_rand() % 1000u) / 1000.0f));
            }
        }
        ov = rate * size;
        g = 1.3f / sqrtf(ov > 1 ? ov : 1) * vf->gain;
        for (int i = 0; i < VO_GRAINS; i++) {
            if (!vo_g[i].len)
                continue;
            for (k = 0; k < n; k++) {
                float w = vo_hann[vo_g[i].age * 511u / vo_g[i].len], x = vo_read(vf, vo_g[i].pos) * w * g;
                vo_buf[2 * k] += x * vo_g[i].gl;
                vo_buf[2 * k + 1] += x * vo_g[i].gr;
                vo_g[i].pos += vo_g[i].step * vo_g[i].dir;
                if (vo_g[i].pos < 0)
                    vo_g[i].pos = 0;
                if (++vo_g[i].age >= vo_g[i].len) {
                    vo_g[i].len = 0;
                    break;
                }
            }
        }
    } else if (mode == 2) {                              /* VOWEL: two grains of 120 ms crossing over the window */
        static double gpos[2];
        static uint32_t gage[2] = {0, 2646};
        const uint32_t G = 5292;
        double step = pow(2.0, lab_v[P_VO_PITCH] / 12.0);
        if (!vo_v[0].b || vo_v[0].b > vf->frames)
            vo_trigger(vf, 2);
        for (k = 0; k < n; k++) {
            float s = 0;
            for (int g = 0; g < 2; g++) {
                float w;
                if (gage[g] >= G) {               /* a new grain somewhere in the window */
                    gage[g] = 0;
                    gpos[g] = (double)vo_v[0].a + (double)(vo_rand() % (vo_v[0].b - vo_v[0].a > G ? vo_v[0].b - vo_v[0].a - G : 1u));
                }
                w = 0.5f - 0.5f * cosf(6.2831853f * (float)gage[g] / (float)G);
                s += vo_read(vf, gpos[g]) * w;
                gpos[g] += step;
                gage[g]++;
            }
            vo_buf[2 * k] = vo_buf[2 * k + 1] = s * vf->gain * 1.4f;
        }
        vo_env += (fabsf(vo_buf[0]) * 6.0f - vo_env) * 0.2f;
    } else
        for (int v = 0; v < VO_VOICES; v++) {
            float gl, gr;
            if (!vo_v[v].on)
                continue;
            gl = sqrtf(0.5f * (1 - vo_v[v].pan)) * 1.4142f, gr = sqrtf(0.5f * (1 + vo_v[v].pan)) * 1.4142f;
            for (k = 0; k < n; k++) {
                double p = vo_v[v].pos, to_end = vo_v[v].dir > 0 ? (double)vo_v[v].b - p : p - (double)vo_v[v].a;
                double from = vo_v[v].dir > 0 ? p - (double)vo_v[v].a : (double)vo_v[v].b - p;
                float edge = (float)fmin(fmin(to_end, from) / 220.0, 1.0), s;
                if (to_end <= 1 || vo_v[v].env <= 0.001f) {
                    vo_v[v].on = 0;
                    break;
                }
                if (vo_v[v].stop)
                    vo_v[v].env -= 1.0f / 882.0f;   /* (stopped: out in 20 ms) */
                s = vo_read(vf, p) * edge * vo_v[v].env * vf->gain;
                vo_buf[2 * k] += s * gl;
                vo_buf[2 * k + 1] += s * gr;
                vo_v[v].pos += vo_v[v].step * vo_v[v].dir;
            }
        }
    for (k = 0; k < n; k++) {                     /* RADIO: the telephone band, crackle, a little drive */
        for (int c = 0; c < 2; c++) {
            float x = vo_buf[2 * k + c], y;
            if (radio > 0) {
                vo_hp[c][0] += (x - vo_hp[c][0]) * 0.042f;       /* ~300 Hz */
                y = x - vo_hp[c][0];
                vo_hp[c][1] += (y - vo_hp[c][1]) * 0.042f;
                y -= vo_hp[c][1];
                vo_lp[c][0] += (y - vo_lp[c][0]) * 0.35f;       /* ~3 kHz */
                vo_lp[c][1] += (vo_lp[c][0] - vo_lp[c][1]) * 0.35f;
                y = vo_lp[c][1] * 2.2f + vo_crackle * (c ? -1.0f : 1.0f) + ((float)(vo_rand() % 2001u) / 1000.0f - 1.0f) * 0.004f * radio;
                y = tanhf(y * (1.0f + 2.0f * radio)) / (1.0f + radio);
                x = x * (1 - radio) + y * radio;
            }
            {   /* without DC (old recordings bring some along): ~20 Hz high pass */
                static float dx[2], dy[2];
                dy[c] = 0.9972f * (dy[c] + x - dx[c]);
                dx[c] = x;
                x = dy[c];
            }
            vo_buf[2 * k + c] = x * level;
        }
        if (radio > 0) {
            vo_crackle *= 0.86f;
            if (vo_rand() % 100000u < (uint32_t)(40.0f * radio))
                vo_crackle = ((float)(vo_rand() % 2001u) / 1000.0f - 1.0f) * 0.25f * radio;
        }
        lvl += vo_buf[2 * k] * vo_buf[2 * k] + vo_buf[2 * k + 1] * vo_buf[2 * k + 1];
    }
    vo_level = sqrtf(lvl / (float)(2 * n));
    if (mode != 2)
        vo_env += ((vo_level / (level > 0.01f ? level : 0.01f)) * 10.0f - vo_env) * 0.3f;
    vo_env = vo_env > 1 ? 1 : vo_env;
    return 1;
}
