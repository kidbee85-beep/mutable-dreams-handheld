/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the SAMPLES module of the Sound Lab.
 *
 * WAV files from mutabledreams/samples/ (beside save/): 16, 24 or 32-bit PCM or 32-bit float, mono or stereo, any
 * rate (converted to 44.1 kHz when loaded), at most 90 s each. Two ways to play one:
 *   LOOP          a field recording or a bed: the window START..LENGTH loops with a crossfade, at any PITCH,
 *                 forwards or reversed
 *   FOLLOW TRACK  every hit of a track plays the sample (four voices): with FOLLOW PITCH the track's note sets the
 *                 pitch (C4 = as recorded), else PITCH alone; DECAY shapes each hit
 * A low pass (CUTOFF), PAN and the sends into CLOUD, PARTICLES, SEEDS and SHIMMER (SENDS page). The chosen file is
 * kept by name in save/lab-sample.txt. Included by brick_lab.inc. */
#include <dirent.h>
#include <ctype.h>

#define SA_MAX_FILES 64
#define SA_MAX_SECONDS 90
#define SA_VOICES 4
#define SA_PEAKS 330
static char sa_names[SA_MAX_FILES][64];
static int sa_nfiles = -1, sa_loaded = -1;
static float *sa_data;                           /* interleaved stereo at 44.1 kHz */
static uint32_t sa_frames, sa_src_rate, sa_src_channels, sa_src_bits;
static float sa_norm = 1.0f, sa_peak_min[SA_PEAKS], sa_peak_max[SA_PEAKS];
static char sa_error[64];
static struct { double pos, step; float env, decay; int on, dir; } sa_voice[SA_VOICES];
static double sa_loop_pos;
static uint32_t sa_last_event;
static float sa_lp[2], sa_buf[2 * APP_BLOCK], sa_level;
static int sa_active;

static void sa_dir(char *out, size_t n)
{
    snprintf(out, n, "%s/../samples", bk_dir);
}

static int sa_cmp(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

static void sa_scan(void)
{
    char dir[600];
    DIR *d;
    struct dirent *e;
    sa_nfiles = 0;
    if (!bk_dir[0])
        return;
    sa_dir(dir, sizeof dir);
    if (!(d = opendir(dir)))
        return;
    while ((e = readdir(d)) && sa_nfiles < SA_MAX_FILES) {
        size_t l = strlen(e->d_name);
        if (l > 4 && l < 64 && e->d_name[0] != '.' && tolower((unsigned char)e->d_name[l - 1]) == 'v' &&
            tolower((unsigned char)e->d_name[l - 2]) == 'a' && tolower((unsigned char)e->d_name[l - 3]) == 'w' &&
            e->d_name[l - 4] == '.')
            snprintf(sa_names[sa_nfiles++], 64, "%s", e->d_name);
    }
    closedir(d);
    qsort(sa_names, (size_t)sa_nfiles, 64, sa_cmp);
}

static uint32_t sa_rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t sa_rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }

/* the file into sa_data: 0 = loaded, else sa_error says why */
static int sa_load(int index)
{
    char path[700];
    FILE *f;
    uint8_t hdr[12], ch[8];
    uint32_t fmt = 0, chans = 0, rate = 0, bits = 0, data_len = 0;
    long data_at = -1;
    float *raw = 0, *out = 0, peak = 0;
    uint32_t frames, oframes, i;
    sa_error[0] = 0;
    sa_loaded = -1;
    bk_defer_free(sa_data);                      /* (the stage may still be drawing its waveform) */
    sa_data = 0;
    sa_frames = 0;
    memset(sa_voice, 0, sizeof sa_voice);
    sa_loop_pos = 0;
    if (index < 0 || index >= sa_nfiles)
        return -1;
    sa_dir(path, sizeof path);
    snprintf(path + strlen(path), sizeof path - strlen(path), "/%s", sa_names[index]);
    if (!(f = fopen(path, "rb")) || fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
        snprintf(sa_error, sizeof sa_error, "NOT A WAV FILE");
        if (f)
            fclose(f);
        return -1;
    }
    while (fread(ch, 1, 8, f) == 8) {             /* the chunks: fmt and data, anything else skipped */
        uint32_t len = sa_rd32(ch + 4);
        if (!memcmp(ch, "fmt ", 4)) {
            uint8_t b[40] = {0};
            if (fread(b, 1, len < 40 ? len : 40, f) < 16)
                break;
            if (len > 40)
                fseek(f, (long)(len - 40), SEEK_CUR);
            fmt = sa_rd16(b);
            chans = sa_rd16(b + 2);
            rate = sa_rd32(b + 4);
            bits = sa_rd16(b + 14);
            if (fmt == 0xFFFEu && len >= 26)
                fmt = sa_rd16(b + 24);           /* WAVE_FORMAT_EXTENSIBLE: the sub-format's first word */
        } else if (!memcmp(ch, "data", 4)) {
            data_at = ftell(f);
            data_len = len;
            break;
        } else
            fseek(f, (long)(len + (len & 1u)), SEEK_CUR);
    }
    if (data_at < 0 || (fmt != 1 && fmt != 3) || chans < 1 || chans > 2 || rate < 8000 || rate > 192000 ||
        !(bits == 16 || bits == 24 || bits == 32) || (fmt == 3 && bits != 32)) {
        snprintf(sa_error, sizeof sa_error, "UNSUPPORTED WAV (%u BIT, %u CH)", (unsigned)bits, (unsigned)chans);
        fclose(f);
        return -1;
    }
    frames = data_len / (chans * bits / 8);
    if (frames > rate * SA_MAX_SECONDS)
        frames = rate * SA_MAX_SECONDS;
    raw = (float *)malloc(sizeof(float) * 2 * (frames + 1));
    {
        uint32_t bpf = chans * bits / 8, chunk = 4096;
        uint8_t *buf = (uint8_t *)malloc((size_t)bpf * chunk);
        uint32_t done = 0;
        while (done < frames) {
            uint32_t m = frames - done < chunk ? frames - done : chunk, got = (uint32_t)fread(buf, bpf, m, f), k;
            for (k = 0; k < got; k++)
                for (uint32_t c = 0; c < 2; c++) {
                    const uint8_t *s = buf + k * bpf + (c < chans ? c : 0) * (bits / 8);
                    float v;
                    if (fmt == 3) {
                        uint32_t u = sa_rd32(s);
                        memcpy(&v, &u, 4);
                    } else if (bits == 16)
                        v = (float)(int16_t)sa_rd16(s) / 32768.0f;
                    else if (bits == 24)
                        v = (float)((int32_t)((uint32_t)s[0] << 8 | (uint32_t)s[1] << 16 | (uint32_t)s[2] << 24) >> 8) / 8388608.0f;
                    else
                        v = (float)(int32_t)sa_rd32(s) / 2147483648.0f;
                    raw[2 * (done + k) + c] = v != v ? 0.0f : v;   /* (no NaN from a broken float file) */
                }
            done += got;
            if (got < m)
                break;
        }
        frames = done;
        free(buf);
    }
    fclose(f);
    if (frames < 64) {
        free(raw);
        snprintf(sa_error, sizeof sa_error, "FILE TOO SHORT");
        return -1;
    }
    raw[2 * frames] = raw[2 * frames - 2];
    raw[2 * frames + 1] = raw[2 * frames - 1];
    /* to 44.1 kHz, linear: plain and enough for beds and hits (the sends smooth the rest) */
    oframes = (uint32_t)((double)frames * 44100.0 / (double)rate);
    out = (float *)malloc(sizeof(float) * 2 * (oframes + 2));
    for (i = 0; i < oframes; i++) {
        double sp = (double)i * (double)rate / 44100.0;
        uint32_t a = (uint32_t)sp;
        float fr = (float)(sp - a);
        if (a >= frames)
            a = frames - 1;
        for (uint32_t c = 0; c < 2; c++) {
            float v = raw[2 * a + c] + (raw[2 * (a + 1) + c] - raw[2 * a + c]) * fr;
            out[2 * i + c] = v;
            if (fabsf(v) > peak)
                peak = fabsf(v);
        }
    }
    out[2 * oframes] = out[2 * oframes + 2] = out[2 * oframes - 2];
    out[2 * oframes + 1] = out[2 * oframes + 3] = out[2 * oframes - 1];
    free(raw);
    for (i = 0; i < SA_PEAKS; i++) {             /* the picture: min and max per column */
        uint32_t a = (uint32_t)((uint64_t)oframes * i / SA_PEAKS), b = (uint32_t)((uint64_t)oframes * (i + 1) / SA_PEAKS), k;
        float lo = 0, hi = 0;
        for (k = a; k < b; k++) {
            float v = (out[2 * k] + out[2 * k + 1]) * 0.5f;
            lo = v < lo ? v : lo;
            hi = v > hi ? v : hi;
        }
        sa_peak_min[i] = lo;
        sa_peak_max[i] = hi;
    }
    sa_norm = peak > 1e-5f ? 0.2f / peak : 1.0f;  /* every file meets the mix at the same height (-14 dB peaks) */
    for (i = 0; i < SA_PEAKS; i++) {
        sa_peak_min[i] *= peak > 1e-5f ? 1.0f / peak : 1.0f;
        sa_peak_max[i] *= peak > 1e-5f ? 1.0f / peak : 1.0f;
    }
    sa_data = out;
    sa_frames = oframes;
    sa_src_rate = rate;
    sa_src_channels = chans;
    sa_src_bits = bits;
    sa_loaded = index;
    return 0;
}

/* the chosen file, by name (save/lab-sample.txt): its index now, or 0. A Biome with its own Sound Lab keeps its
 * sample in its own file (brick_labbiome.inc), so the shared choice is left alone then */
static int lb_own_in_use(void);
static void sa_keep_name(void)
{
    char path[600];
    FILE *f;
    if (!bk_dir[0] || sa_loaded < 0 || lb_own_in_use())
        return;
    snprintf(path, sizeof path, "%s/lab-sample.txt", bk_dir);
    if ((f = fopen(path, "w"))) {
        fprintf(f, "%s\n", sa_names[sa_loaded]);
        fclose(f);
    }
}
static int sa_kept_index(void)
{
    char path[600], name[80] = "";
    FILE *f;
    snprintf(path, sizeof path, "%s/lab-sample.txt", bk_dir);
    if (!(f = fopen(path, "r")))
        return 0;
    if (fgets(name, sizeof name, f)) {
        size_t l = strlen(name);
        while (l && (name[l - 1] == '\n' || name[l - 1] == '\r'))
            name[--l] = 0;
    }
    fclose(f);
    for (int i = 0; i < sa_nfiles; i++)
        if (!strcmp(sa_names[i], name))
            return i;
    return 0;
}

/* one hit: a free voice (or the quietest) from START */
static void sa_hit(float semis)
{
    int i, v = 0;
    float quiet = 2.0f;
    for (i = 0; i < SA_VOICES; i++) {
        if (!sa_voice[i].on) {
            v = i;
            break;
        }
        if (sa_voice[i].env < quiet) {
            quiet = sa_voice[i].env;
            v = i;
        }
    }
    {
        double w0 = (double)sa_frames * lab_v[P_SA_START] / 100.0, len = (double)sa_frames * lab_v[P_SA_LENGTH] / 100.0;
        int rev = lab_v[P_SA_REVERSE] > 0.5f;
        if (w0 + len > sa_frames)
            len = sa_frames - w0;
        sa_voice[v].dir = rev ? -1 : 1;
        sa_voice[v].pos = rev ? w0 + len - 1 : w0;
        sa_voice[v].step = pow(2.0, semis / 12.0);
        sa_voice[v].env = 1.0f;
        sa_voice[v].decay = expf(-1.0f / (44100.0f * (0.05f + 8.0f * powf(lab_v[P_SA_DECAY] / 100, 2.0f))));
        sa_voice[v].on = 1;
    }
}

static inline float sa_read(double pos, int c)
{
    uint32_t a = (uint32_t)pos;
    float f = (float)(pos - a);
    if (a >= sa_frames)
        return 0.0f;
    return sa_data[2 * a + c] + (sa_data[2 * (a + 1) + c] - sa_data[2 * a + c]) * f;
}

/* n frames into sa_buf (and sa_level); 0 = nothing sounds */
static int sa_render(uint32_t n)
{
    const md_stats *st = md_status();
    float level = lab_v[P_SA_LEVEL] / 100, pan = lab_v[P_SA_PAN] / 100;
    float gl = sqrtf(0.5f * (1 - pan)) * 1.4142f, gr = sqrtf(0.5f * (1 + pan)) * 1.4142f, lvl = 0;
    float cut = 1.0f - expf(-6.2831853f * (200.0f * powf(100.0f, lab_v[P_SA_CUTOFF] / 100)) / 44100.0f);
    int file = (int)lab_v[P_SA_FILE], mode = (int)lab_v[P_SA_MODE], track = (int)lab_v[P_SA_TRACK];
    uint32_t k;
    if (sa_nfiles < 0)
        sa_scan();
    if (file != sa_loaded && file < sa_nfiles && sa_nfiles > 0) {
        sa_load(file);
        sa_keep_name();
        sa_last_event = st->events[track % 6];
    }
    if (!sa_data || level <= 0) {
        sa_level = 0;
        return 0;
    }
    memset(sa_buf, 0, sizeof(float) * 2 * n);
    if (mode == 0) {                              /* LOOP: the window, crossfaded over 50 ms at its seam */
        double w0 = (double)sa_frames * lab_v[P_SA_START] / 100.0, len = (double)sa_frames * lab_v[P_SA_LENGTH] / 100.0;
        double step = pow(2.0, lab_v[P_SA_PITCH] / 12.0), xf = 2205.0;
        int rev = lab_v[P_SA_REVERSE] > 0.5f;
        if (w0 + len > sa_frames - 2)
            len = sa_frames - 2 - w0;
        if (len < 4410)
            len = 4410 < sa_frames - 2 - w0 ? 4410 : sa_frames - 2 - w0;
        if (xf > len / 4)
            xf = len / 4;
        for (k = 0; k < n; k++) {
            double ph = sa_loop_pos, p = rev ? w0 + len - 1 - ph : w0 + ph;
            float a = 1.0f, l, r;
            l = sa_read(p, 0);
            r = sa_read(p, 1);
            if (ph > len - xf) {                  /* the end fades into the start */
                float t = (float)((ph - (len - xf)) / xf);
                double q = rev ? w0 + len - 1 - (ph - (len - xf)) : w0 + (ph - (len - xf));
                a = 1.0f - t;
                l = l * a + sa_read(q, 0) * t;
                r = r * a + sa_read(q, 1) * t;
            }
            sa_buf[2 * k] = l;
            sa_buf[2 * k + 1] = r;
            sa_loop_pos += step;
            if (sa_loop_pos >= len)
                sa_loop_pos = sa_loop_pos - len + xf;
        }
    } else {                                      /* FOLLOW TRACK: a hit for each of the track's notes */
        uint32_t ev = st->events[track % 6];
        if (ev != sa_last_event) {
            int note = st->last_note[track % 6];
            float semis = lab_v[P_SA_PITCH];
            if (lab_v[P_SA_FOLLOW] > 0.5f && note > 0)
                semis += (float)(note - 60);
            sa_last_event = ev;
            sa_hit(semis);
        }
        for (int v = 0; v < SA_VOICES; v++) {
            double w0 = (double)sa_frames * lab_v[P_SA_START] / 100.0, end = w0 + (double)sa_frames * lab_v[P_SA_LENGTH] / 100.0;
            if (!sa_voice[v].on)
                continue;
            if (end > sa_frames - 1)
                end = sa_frames - 1;
            for (k = 0; k < n; k++) {
                double p = sa_voice[v].pos;
                float e = sa_voice[v].env, fade = 1.0f;
                if (p < w0 || p >= end || e < 0.0005f) {
                    sa_voice[v].on = 0;
                    break;
                }
                if (sa_voice[v].dir > 0 && end - p < 441.0)
                    fade = (float)((end - p) / 441.0);   /* (a 10 ms fade at the window's end: no click) */
                else if (sa_voice[v].dir < 0 && p - w0 < 441.0)
                    fade = (float)((p - w0) / 441.0);
                sa_buf[2 * k] += sa_read(p, 0) * e * fade;
                sa_buf[2 * k + 1] += sa_read(p, 1) * e * fade;
                sa_voice[v].pos += sa_voice[v].step * sa_voice[v].dir;
                sa_voice[v].env = e * sa_voice[v].decay;
            }
        }
    }
    for (k = 0; k < n; k++) {
        for (int c = 0; c < 2; c++) {
            sa_lp[c] += (sa_buf[2 * k + c] - sa_lp[c]) * cut;
            sa_buf[2 * k + c] = sa_lp[c] * sa_norm * level * (c ? gr : gl);
        }
        lvl += sa_buf[2 * k] * sa_buf[2 * k] + sa_buf[2 * k + 1] * sa_buf[2 * k + 1];
    }
    sa_level = sqrtf(lvl / (float)(2 * n));
    return 1;
}
