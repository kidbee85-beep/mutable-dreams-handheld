/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick: the headless runner. Plays a script of Brick button presses against the core and
 * writes stage pictures (PNG) and the sound (WAV): the automatic checks run without a window or a sound card.
 *
 *   brick-headless SCRIPT [SAVE_DIR]
 *
 *   wait MS                 run the device for MS milliseconds (the stage is drawn every 33 ms of it)
 *   press BTN | release BTN | tap BTN [MS]     BTN: UP DOWN LEFT RIGHT A B X Y L1 R1 L2 R2 START SELECT MENU F1 F2 FN
 *   shot FILE.png           the stage now
 *   fm1shot FILE.png        the FM-1 screen alone (240 x 240)
 *   wav FILE.wav | wavstop  record the output (44.1 kHz, 16-bit stereo)
 *   print                   page, Biome, transport
 *   splash | expect_splash 0|1   the start screen (off unless asked)
 *   expect_page NAME        exit 1 unless the page is NAME (spaces as _)
 *   expect_playing 0|1
 *   expect_sound            exit 1 unless the recording so far has a peak above -60 dBFS
 *   bench MS                render MS milliseconds as fast as possible and print the time it took per second of
 *                           sound (the stage drawn every 33 ms of it, too) */
#include "brick_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *const NAMES[BK_NBUTTONS] = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y", "L1", "R1",
                                               "L2", "R2", "START", "SELECT", "MENU", "F1", "F2", "FN"};
static uint32_t held;
static FILE *wav;
static uint32_t wav_frames;
static float peak;
static uint32_t seq_marked;                     /* (seq_mark, expect_seq_same) */
static uint32_t next_draw;
/* film PATH ... filmstop (1.5, the promo): every frame at exactly 30 a second of the engine's time, each as 4 bytes of
   the held buttons and the stage's 1024 x 768 pixels (BGRA), into PATH (a pipe to tools/film.swift) */
static FILE *film;
static uint32_t film_t0, film_n;

/* MD_PROFILE=MS: every stretch the core is held longer than MS (this machine's time) is printed with its page and
   script line: the sound's input, a frame between two of its yields, a block of sound. On the device the sound
   thread waits through each such stretch, so these are the places that make it stutter. */
static double prof_th, prof_seg, prof_max[4];
static int prof_ln;
static const char *const PROF_KIND[4] = {"input", "draw-segment", "render", "draw-frame"};
static double prof_now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
static void prof_note(int k, double ms)
{
    if (ms > prof_max[k])
        prof_max[k] = ms;
    if (ms > prof_th)
        printf("PROFILE %-12s %7.2f ms  line %d  page %s\n", PROF_KIND[k], ms, prof_ln, bk_page_name());
}
static void *prof_from;
/* MD_PROFILE_SUM: the frame's time summed by the place each stretch ended (its yield's caller), printed at the end */
#define PROF_SLOTS 512
static void *prof_at[PROF_SLOTS];
static double prof_sum[PROF_SLOTS];
static void prof_add(void *at, double ms)
{
    unsigned h = (unsigned)(((uintptr_t)at >> 2) % PROF_SLOTS);
    while (prof_at[h] && prof_at[h] != at)
        h = (h + 1) % PROF_SLOTS;
    prof_at[h] = at, prof_sum[h] += ms;
}
static void prof_yield(void)
{
    double t = prof_now();
    void *at = __builtin_return_address(0);
    if (getenv("MD_PROFILE_SUM"))
        prof_add(at, t - prof_seg);
    if (t - prof_seg > prof_th && getenv("MD_PROFILE_AT"))
        printf("PROFILE-AT %.2f ms from %p to %p\n", t - prof_seg, prof_from, at);
    prof_note(1, t - prof_seg);
    prof_seg = t;
    prof_from = at;
}
static void p_input(uint32_t h)
{
    double t = prof_th > 0 ? prof_now() : 0;
    bk_input(h);
    if (prof_th > 0)
        prof_note(0, prof_now() - t);
}
static const uint32_t *p_draw(void)
{
    const uint32_t *r;
    double t = prof_th > 0 ? prof_now() : 0;
    prof_seg = t;
    prof_from = 0;
    r = bk_draw();
    if (prof_th > 0) {
        if (prof_now() - prof_seg > prof_th && getenv("MD_PROFILE_AT"))
            printf("PROFILE-AT %.2f ms from %p to end\n", prof_now() - prof_seg, prof_from);
        if (getenv("MD_PROFILE_SUM"))
            prof_add((void *)1, prof_now() - prof_seg);
        prof_note(1, prof_now() - prof_seg);
        prof_note(3, prof_now() - t);
    }
    return r;
}
static void p_render(float *lr, int n)
{
    double t = prof_th > 0 ? prof_now() : 0;
    bk_render(lr, n);
    if (prof_th > 0)
        prof_note(2, prof_now() - t);
}
static char marked_biome[64];
static int marked_value;

/* ------------------------------------------------------------ files --- */
static uint32_t crc_table[256];
static uint32_t crc(uint32_t c, const uint8_t *p, size_t n)
{
    if (!crc_table[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t v = i;
            for (int k = 0; k < 8; k++)
                v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            crc_table[i] = v;
        }
    c = ~c;
    while (n--)
        c = crc_table[(c ^ *p++) & 255u] ^ (c >> 8);
    return ~c;
}
static void be32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void chunk(FILE *f, const char *type, const uint8_t *d, uint32_t n)
{
    uint8_t h[8];
    uint32_t c;
    be32(h, n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n)
        fwrite(d, 1, n, f);
    c = crc(crc(0, (const uint8_t *)type, 4), d, n);
    be32(h, c);
    fwrite(h, 1, 4, f);
}
/* a PNG with stored (uncompressed) deflate blocks: no zlib needed */
static int png(const char *path, const uint32_t *argb, int w, int h)
{
    FILE *f = fopen(path, "wb");
    size_t raw_n = (size_t)h * (1 + 3 * (size_t)w), z_n, i, at = 0;
    uint8_t *raw, *z, ihdr[13];
    uint32_t a = 1, b = 0;
    if (!f)
        return -1;
    raw = malloc(raw_n);
    for (int y = 0; y < h; y++) {
        uint8_t *r = raw + (size_t)y * (1 + 3 * (size_t)w);
        r[0] = 0;
        for (int x = 0; x < w; x++) {
            uint32_t c = argb[y * w + x];
            r[1 + 3 * x] = c >> 16;
            r[2 + 3 * x] = c >> 8;
            r[3 + 3 * x] = c;
        }
    }
    z_n = 2 + raw_n + 5 * (raw_n / 65535 + 1) + 4;
    z = malloc(z_n);
    z[at++] = 0x78;
    z[at++] = 0x01;
    for (i = 0; i < raw_n; i += 65535) {
        size_t n = raw_n - i > 65535 ? 65535 : raw_n - i;
        z[at++] = i + n >= raw_n;
        z[at++] = n & 255u;
        z[at++] = n >> 8;
        z[at++] = ~n & 255u;
        z[at++] = (~n >> 8) & 255u;
        memcpy(z + at, raw + i, n);
        at += n;
    }
    for (i = 0; i < raw_n; i++) {
        a = (a + raw[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    be32(z + at, (b << 16) | a);
    at += 4;
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    be32(ihdr, (uint32_t)w);
    be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, (uint32_t)at);
    chunk(f, "IEND", 0, 0);
    fclose(f);
    free(raw);
    free(z);
    return 0;
}
static void le32(FILE *f, uint32_t v) { uint8_t b[4] = {v, v >> 8, v >> 16, v >> 24}; fwrite(b, 1, 4, f); }
static void le16(FILE *f, uint32_t v) { uint8_t b[2] = {v, v >> 8}; fwrite(b, 1, 2, f); }
static void wav_header(FILE *f, uint32_t frames)
{
    fwrite("RIFF", 1, 4, f); le32(f, 36 + frames * 4); fwrite("WAVEfmt ", 1, 8, f);
    le32(f, 16); le16(f, 1); le16(f, 2); le32(f, BK_RATE); le32(f, BK_RATE * 4); le16(f, 4); le16(f, 16);
    fwrite("data", 1, 4, f); le32(f, frames * 4);
}

/* the MIDI ledger (0.73, "midi_ledger 1"): after every block the engine's MIDI out is read and each note followed,
   per channel - which notes are held now, how many began, offs without an on; to find hanging notes after mute,
   stop, a Biome change, the kit */
static int ml_on;
static uint8_t ml_held[16][128];
static uint32_t ml_ons[16], ml_stray;
static void ml_drain(void)
{
    uint8_t o[8192];
    int n = bk_midi_out(o, sizeof o), i = 0;
    while (i < n) {
        uint8_t st = o[i];
        if (st >= 0xF8) { i++; continue; }       /* clock and other real time: one byte */
        if (st >= 0xF0) { i++; continue; }
        {
            int len = (st & 0xE0) == 0xC0 ? 2 : 3, ch = st & 15, ty = st & 0xF0;
            uint8_t d1 = i + 1 < n ? o[i + 1] : 0, d2 = i + 2 < n ? o[i + 2] : 0;
            if (ty == 0x90 && d2) {
                if (ml_held[ch][d1 & 127] < 255) ml_held[ch][d1 & 127]++;
                ml_ons[ch]++;
            } else if (ty == 0x80 || (ty == 0x90 && !d2)) {
                if (ml_held[ch][d1 & 127]) ml_held[ch][d1 & 127]--; else ml_stray++;
            } else if (ty == 0xB0 && (d1 == 123 || d1 == 120))
                memset(ml_held[ch], 0, sizeof ml_held[ch]);
            i += len;
        }
    }
}
static int ml_count(int ch)                      /* notes held now: on one channel (0..15), or all (-1) */
{
    int c = 0;
    for (int k = 0; k < 16; k++)
        if (ch < 0 || ch == k)
            for (int nn = 0; nn < 128; nn++)
                c += ml_held[k][nn];
    return c;
}

/* ------------------------------------------------------------- run --- */
static void run(uint32_t ms, int draw)
{
    static float lr[2 * 256];
    uint32_t end = bk_ms() + ms;
    while ((int32_t)(bk_ms() - end) < 0) {
        p_render(lr, 256);
        if (ml_on)
            ml_drain();
        for (int i = 0; i < 512; i++) {
            float v = fabsf(lr[i]);
            if (v > peak && wav)
                peak = v;
        }
        if (wav) {
            for (int i = 0; i < 512; i++) {
                float v = lr[i] > 1 ? 1 : lr[i] < -1 ? -1 : lr[i];
                le16(wav, (uint16_t)(int16_t)lrintf(v * 32767.0f));
            }
            wav_frames += 256;
        }
        if (film) {
            if ((int32_t)(bk_ms() - (film_t0 + (uint32_t)((uint64_t)film_n * 1000u / 30u))) >= 0) {
                const uint32_t *px = p_draw();
                fwrite(&held, 4, 1, film);
                fwrite(px, 4, (size_t)BK_W * BK_H, film);
                film_n++;
            }
        } else if (draw && (int32_t)(bk_ms() - next_draw) >= 0) {
            next_draw = bk_ms() + 33u;
            p_draw();
        }
    }
}

static int button(const char *n)
{
    for (int i = 0; i < BK_NBUTTONS; i++)
        if (!strcmp(n, NAMES[i]))
            return i;
    fprintf(stderr, "unknown button %s\n", n);
    exit(2);
}

int main(int argc, char **argv)
{
    FILE *sc;
    char line[512];
    int fails = 0, ln = 0;
    if (argc < 2) {
        fprintf(stderr, "usage: %s SCRIPT [SAVE_DIR]\n", argv[0]);
        return 2;
    }
    if (!(sc = fopen(argv[1], "r"))) {
        perror(argv[1]);
        return 2;
    }
    if (argc > 2)
        mkdir(argv[2], 0755);
    if (getenv("MD_PROFILE") && (prof_th = atof(getenv("MD_PROFILE"))) > 0)
        bk_set_draw_yield(prof_yield);
    if (getenv("MD_PROFILE_AT"))
        printf("PROFILE-BASE %p\n", (void *)&main);
    bk_sync_loading(1);                          /* (VOICES: loaded at once, the tests stay the same each run) */
    bk_init(argc > 2 ? argv[2] : 0);
    {
        const char *kb = getenv("MD_KEYBOARD"), *sm = getenv("MD_SMALL");
        if (kb && kb[0] == '1')
            bk_set_keyboard(1);                  /* (a PC without a controller: tools/textscan.sh MD_KEYBOARD=1) */
        if (sm && sm[0] == '1')
            bk_set_small(1);                     /* (as a 640 x 480 handheld: tools/textscan.sh MD_SMALL=1) */
        if (getenv("MD_LANG"))                   /* (MD_LANG=de: the stage in German, tools/textscan.sh too) */
            bk_set_lang_code(getenv("MD_LANG"));
    }
    while (fgets(line, sizeof line, sc)) {
        char cmd[64] = "", a[400] = "", b[64] = "";
        ln++;
        prof_ln = ln;
        if (line[0] == '#' || sscanf(line, "%63s %399s %63s", cmd, a, b) < 1)
            continue;
        if (!strcmp(cmd, "wait"))
            run((uint32_t)atoi(a), 1);
        else if (!strcmp(cmd, "press")) {
            held |= 1u << button(a);
            p_input(held);
        } else if (!strcmp(cmd, "release")) {
            held &= ~(1u << button(a));
            p_input(held);
        } else if (!strcmp(cmd, "tap")) {
            held |= 1u << button(a);
            p_input(held);
            run(b[0] ? (uint32_t)atoi(b) : 80u, 1);
            held &= ~(1u << button(a));
            p_input(held);
            run(60, 1);
        } else if (!strcmp(cmd, "lab")) {      /* lab MODULE.PARAMETER VALUE (spaces in the name as _) */
            char name[400], *q;
            const char *val = strrchr(line, ' ');
            snprintf(name, sizeof name, "%s", a);
            for (q = name; *q; q++)
                if (*q == '_')
                    *q = ' ';
            if (bk_lab_set(name, (float)atof(b))) {
                printf("FAIL line %d: no lab parameter %s\n", ln, name);
                fails++;
            }
            (void)val;
        } else if (!strcmp(cmd, "expect_labmod")) {   /* expect_labmod NAME: the Lab shows that module (1.3; _ for spaces) */
            extern const char *bk_lab_module_name(void);
            char name[64], *q;
            snprintf(name, sizeof name, "%s", a);
            for (q = name; *q; q++)
                if (*q == '_')
                    *q = ' ';
            if (strcmp(bk_lab_module_name(), name)) {
                printf("FAIL line %d: the Lab shows %s, expected %s\n", ln, bk_lab_module_name(), name);
                fails++;
            }
        } else if (!strcmp(cmd, "expect_lab")) {   /* expect_lab MODULE.PARAMETER VALUE */
            char name[400], *q;
            float got;
            snprintf(name, sizeof name, "%s", a);
            for (q = name; *q; q++)
                if (*q == '_')
                    *q = ' ';
            got = bk_lab_get(name);
            if (strchr(b, ':')) {                 /* a range LO:HI (a change under way) */
                float lo = (float)atof(b), hi = (float)atof(strchr(b, ':') + 1);
                if (got != got || got < lo || got > hi) {
                    printf("FAIL line %d: %s is %g, expected %s\n", ln, name, got, b);
                    fails++;
                } else
                    printf("  ok %s %g in %s\n", name, got, b);
            } else if (got != got || fabsf(got - (float)atof(b)) > 0.01f) {
                printf("FAIL line %d: %s is %g, expected %s\n", ln, name, got, b);
                fails++;
            } else
                printf("  ok %s %s\n", name, b);
        } else if (!strcmp(cmd, "midi")) {      /* midi B0 [B1 [B2]]  (hex) */
            uint8_t m[3];
            int k = 0;
            char c3[64] = "";
            sscanf(line, "%*s %*s %*s %63s", c3);
            m[k++] = (uint8_t)strtoul(a, 0, 16);
            if (b[0])
                m[k++] = (uint8_t)strtoul(b, 0, 16);
            if (c3[0])
                m[k++] = (uint8_t)strtoul(c3, 0, 16);
            bk_midi_in(m, k);
        } else if (!strcmp(cmd, "midiout")) {   /* print what the engine sent (and forget it); midiout N: expect >= N bytes */
            uint8_t o[4096];
            int n = bk_midi_out(o, sizeof o), clocks = 0, notes = 0;
            for (int i = 0; i < n; i++) {
                clocks += o[i] == 0xF8;
                notes += (o[i] & 0xF0) == 0x90;
            }
            printf("midi out: %d bytes, %d clocks, %d note ons\n", n, clocks, notes);
            if (a[0] && n < atoi(a)) {
                printf("FAIL line %d: fewer than %s bytes out\n", ln, a);
                fails++;
            }
        } else if (!strcmp(cmd, "expect_poly")) {   /* expect_poly SOUNDING HELD */
            int v = bk_poly_voices(0), h = bk_poly_voices(1);
            if (v != atoi(a) || (b[0] && h != atoi(b))) {
                printf("FAIL line %d: poly %d sounding, %d held; expected %s %s\n", ln, v, h, a, b);
                fails++;
            } else
                printf("  ok poly %d sounding, %d held\n", v, h);
        } else if (!strcmp(cmd, "hits")) {     /* hits TRACK: remember its count; hits TRACK more: expect it grew */
            static uint32_t mark[6];
            int t = atoi(a) % 6;
            if (!strcmp(b, "more")) {
                if (bk_track_hits(t) == mark[t]) {
                    printf("FAIL line %d: track %d had no hit\n", ln, t + 1);
                    fails++;
                } else
                    printf("  ok track %d played (%u hits)\n", t + 1, bk_track_hits(t) - mark[t]);
            } else if (!strcmp(b, "same")) {
                if (bk_track_hits(t) != mark[t]) {
                    printf("FAIL line %d: track %d played\n", ln, t + 1);
                    fails++;
                } else
                    printf("  ok track %d silent\n", t + 1);
            }
            mark[t] = bk_track_hits(t);
        } else if (!strcmp(cmd, "expect_track")) {   /* expect_track TRACK(1..6) WHAT VALUE */
            char c3[64] = "";
            int got;
            sscanf(line, "%*s %*s %*s %63s", c3);
            got = bk_track_value(atoi(a) - 1, b);
            if (got != atoi(c3)) {
                printf("FAIL line %d: track %s %s is %d, expected %s\n", ln, a, b, got, c3);
                fails++;
            } else
                printf("  ok track %s %s %d\n", a, b, got);
        } else if (!strcmp(cmd, "mark_track")) {     /* mark_track TRACK WHAT; expect_track_delta TRACK WHAT D */
            marked_value = bk_track_value(atoi(a) - 1, b);
        } else if (!strcmp(cmd, "expect_track_delta")) {
            char c3[64] = "";
            int got;
            sscanf(line, "%*s %*s %*s %63s", c3);
            got = bk_track_value(atoi(a) - 1, b) - marked_value;
            if (c3[0] == '>' || c3[0] == '<' ? (c3[0] == '>' ? got <= atoi(c3 + 1) : got >= atoi(c3 + 1)) : got != atoi(c3)) {
                printf("FAIL line %d: track %s %s moved %d, expected %s\n", ln, a, b, got, c3);
                fails++;
            } else
                printf("  ok track %s %s moved %d\n", a, b, got);
        } else if (!strcmp(cmd, "mark_biome")) {
            snprintf(marked_biome, sizeof marked_biome, "%s", bk_biome_name());
        } else if (!strcmp(cmd, "expect_biome_changed") || !strcmp(cmd, "expect_biome_same")) {
            int changed = strcmp(marked_biome, bk_biome_name()) != 0;
            if (changed != !strcmp(cmd, "expect_biome_changed")) {
                printf("FAIL line %d: biome %s (was %s)\n", ln, bk_biome_name(), marked_biome);
                fails++;
            } else
                printf("  ok biome %s (was %s)\n", bk_biome_name(), marked_biome);
        } else if (!strcmp(cmd, "biome")) {     /* biome +1 | -1: the next / previous Biome, at once (stopped) */
            held |= 1u << BK_R2;
            p_input(held);
            held |= 1u << (atoi(a) > 0 ? BK_RIGHT : BK_LEFT);
            p_input(held);
            run(40, 1);
            held &= ~((1u << BK_R2) | (1u << BK_RIGHT) | (1u << BK_LEFT));
            p_input(held);
        } else if (!strcmp(cmd, "expect_help")) {   /* expect_help N: the button map on page N (0: closed) */
            if (bk_help_shown() != atoi(a)) {
                printf("FAIL line %d: button map %d, expected %s\n", ln, bk_help_shown(), a);
                fails++;
            } else
                printf("  ok button map %s\n", a);
        } else if (!strcmp(cmd, "handheld"))     /* handheld 1: as on a handheld without MENU, F1, F2, FN */
            bk_set_handheld(atoi(a));
        else if (!strcmp(cmd, "expect_play") || !strcmp(cmd, "expect_menu")) {   /* play mode / the jump menu: 0 or 1 */
            int is = !strcmp(cmd, "expect_play") ? bk_play_mode() : bk_menu_shown();
            if (is != atoi(a)) {
                printf("FAIL line %d: %s %d, expected %s\n", ln, cmd + 7, is, a);
                fails++;
            } else
                printf("  ok %s %s\n", cmd + 7, a);
        } else if (!strcmp(cmd, "worlds")) {      /* worlds: how many, and a mistake in worlds.txt */
            printf("  worlds: %d%s%s\n", bk_world_count(), bk_world_error()[0] ? ", " : "", bk_world_error());
            if (bk_world_error()[0] || bk_world_count() <= 0) {
                printf("FAIL line %d: worlds.txt\n", ln);
                fails++;
            }
        } else if (!strcmp(cmd, "world")) {       /* world N: enter it; expect_world N: it has arrived */
            bk_world_enter(atoi(a));
        } else if (!strcmp(cmd, "expect_world")) {
            if (bk_world_current() != atoi(a)) {
                printf("FAIL line %d: world %d (%s), expected %s (%s)\n", ln, bk_world_current(), bk_world_name(bk_world_current()), a, bk_world_name(atoi(a)));
                fails++;
            } else
                printf("  ok world %s: %s\n", a, bk_world_name(atoi(a)));
        } else if (!strcmp(cmd, "list_biomes")) {
            bk_list_biomes();
        } else if (!strcmp(cmd, "print_logic")) {   /* print_logic: LOGIC's last notes (scale steps) */
            printf("  logic: %s\n", bk_logic_summary());
        } else if (!strcmp(cmd, "wait_fetch")) {   /* wait_fetch: until the ARCHIVE fetch ends (curl runs in real time) */
            int k;
            for (k = 0; k < 3000 && bk_archive_busy(); k++) {
                usleep(10000);
                run(10, 0);
                p_input(held);
            }
            printf("  fetch over: %s\n", bk_archive_status());
        } else if (!strcmp(cmd, "expect_archive")) {   /* expect_archive I 0|1: entry I fetched or not */
            char c3[16] = "";
            sscanf(line, "%*s %*s %15s", c3);
            if (bk_archive_have(atoi(a)) != atoi(b)) {
                printf("FAIL line %d: archive entry %s is %d (%s)\n", ln, a, bk_archive_have(atoi(a)), bk_archive_status());
                fails++;
            } else
                printf("  ok archive entry %s: %d\n", a, atoi(b));
            (void)c3;
        } else if (!strcmp(cmd, "expect_voices")) {   /* expect_voices >N: the loaded file has more than N sentences */
            int got = bk_voices_ready(), want = atoi(a[0] == '>' ? a + 1 : a);
            if (a[0] == '>' ? got <= want : got != want) {
                printf("FAIL line %d: voices %d sentences, expected %s\n", ln, got, a);
                fails++;
            } else
                printf("  ok voices: %d sentences\n", got);
        } else if (!strcmp(cmd, "expect_garden")) {   /* expect_garden 0|1 */
            if (bk_garden_visible() != atoi(a)) {
                printf("FAIL line %d: garden %s\n", ln, bk_garden_visible() ? "shown" : "not shown");
                fails++;
            } else
                printf("  ok garden %s\n", bk_garden_visible() ? "shown" : "not shown");
        } else if (!strcmp(cmd, "splash")) {   /* splash: show the start screen now (the runner starts without) */
            bk_splash(1);
        } else if (!strcmp(cmd, "expect_splash")) {   /* expect_splash 0|1 */
            if (bk_splash_active() != atoi(a)) {
                printf("FAIL line %d: start screen %s\n", ln, bk_splash_active() ? "still shown" : "not shown");
                fails++;
            } else
                printf("  ok start screen %s\n", bk_splash_active() ? "shown" : "gone");
        } else if (!strcmp(cmd, "shot")) {
            p_draw();
            if (png(a, p_draw(), BK_W, BK_H))
                fails++;
        } else if (!strcmp(cmd, "fm1shot")) {
            static uint32_t px[240 * 240];
            const uint16_t *fb = bk_fm1_screen();
            for (int i = 0; i < 240 * 240; i++) {
                uint32_t c = (uint32_t)((fb[i] >> 8) | ((fb[i] & 255u) << 8));
                px[i] = 0xFF000000u | (((c >> 11) & 31u) * 255u / 31u) << 16 | (((c >> 5) & 63u) * 255u / 63u) << 8 |
                        (c & 31u) * 255u / 31u;
            }
            png(a, px, 240, 240);
        } else if (!strcmp(cmd, "wav")) {
            wav = fopen(a, "wb");
            wav_frames = 0;
            peak = 0;
            if (wav)
                wav_header(wav, 0);
        } else if (!strcmp(cmd, "film")) {      /* film PATH: every frame from now on into PATH (filmstop ends it) */
            film = fopen(a, "wb");
            film_t0 = bk_ms(), film_n = 0;
            if (!film) {
                printf("FAIL line %d: film %s\n", ln, a);
                fails++;
            }
        } else if (!strcmp(cmd, "filmstop")) {
            if (film)
                fclose(film), film = 0;
            printf("film: %u frames (%.1f s)\n", film_n, film_n / 30.0);
        } else if (!strcmp(cmd, "wavstop")) {
            if (wav) {
                fseek(wav, 0, SEEK_SET);
                wav_header(wav, wav_frames);
                fclose(wav);
                wav = 0;
            }
            printf("wav %u frames, peak %.1f dBFS\n", wav_frames, 20.0 * log10(peak > 1e-9f ? peak : 1e-9f));
        } else if (!strcmp(cmd, "print"))
            printf("t=%u ms page=%s biome=%s playing=%d\n", bk_ms(), bk_page_name(), bk_biome_name(), bk_playing());
        else if (!strcmp(cmd, "expect_where")) {   /* expect_where GARDEN / SOUND_LAB / BIOME_ATLAS ... */
            char want[400];
            snprintf(want, sizeof want, "%s", a);
            for (char *p = want; *p; p++)
                if (*p == '_')
                    *p = ' ';
            if (strcmp(want, bk_where())) {
                printf("FAIL line %d: at %s, expected %s\n", ln, bk_where(), want);
                fails++;
            } else
                printf("  ok at %s\n", want);
        } else if (!strcmp(cmd, "expect_page")) {
            char want[400];
            snprintf(want, sizeof want, "%s", a);
            for (char *p = want; *p; p++)
                if (*p == '_')
                    *p = ' ';
            if (strcmp(want, bk_page_name())) {
                printf("FAIL line %d: page %s, expected %s\n", ln, bk_page_name(), want);
                fails++;
            } else
                printf("  ok page %s\n", want);
        } else if (!strcmp(cmd, "sleeptimer")) {   /* sleeptimer MS: the sleep timer runs out in MS (1.4) */
            bk_sleep_in((uint32_t)atoi(a));
        } else if (!strcmp(cmd, "expect_sleep")) {   /* expect_sleep MIN: the sleep timer's minutes left (0: off) */
            if (bk_sleep_left_min() != atoi(a)) {
                printf("FAIL line %d: sleep timer %d min, expected %s\n", ln, bk_sleep_left_min(), a);
                fails++;
            }
        } else if (!strcmp(cmd, "expect_resting")) {   /* expect_resting 0/1: the screen rests after the timer */
            if (bk_resting_now() != atoi(a)) {
                printf("FAIL line %d: resting %d\n", ln, bk_resting_now());
                fails++;
            }
        } else if (!strcmp(cmd, "expect_playing")) {
            if (bk_playing() != atoi(a)) {
                printf("FAIL line %d: playing %d\n", ln, bk_playing());
                fails++;
            } else
                printf("  ok playing %d\n", bk_playing());
        } else if (!strcmp(cmd, "light")) {        /* light 1/0: the light picture */
            bk_set_light(atoi(a));
        } else if (!strcmp(cmd, "sticks")) {      /* sticks LX LY RX RY: the analogue sticks held at these values (-32768..32767), 0 0 0 0 lets go (1.5.10) */
            int v[4] = {0, 0, 0, 0};
            sscanf(line, "%*s %d %d %d %d", &v[0], &v[1], &v[2], &v[3]);
            bk_sticks(v[0], v[1], v[2], v[3]);
        } else if (!strcmp(cmd, "chips")) {       /* chips: the touch buttons the place names, listed (1.6) */
            bk_chip c[12];
            int n = bk_touch_chips(c, 12);
            printf("  chips (%d):", n);
            for (int q = 0; q < n; q++)
                printf(" [%d %s]", c[q].btn, c[q].label);
            printf("\n");
        } else if (!strcmp(cmd, "expect_chip")) { /* expect_chip BUTTON_NUMBER LABEL_WORD: such a chip is there (_ for spaces) */
            bk_chip c[12];
            int n = bk_touch_chips(c, 12), found = 0;
            char want[40];
            snprintf(want, sizeof want, "%s", b);
            for (char *q = want; *q; q++)
                if (*q == '_')
                    *q = ' ';
            for (int q = 0; q < n; q++)
                if (c[q].btn == atoi(a) && (!want[0] || strstr(c[q].label, want)))
                    found = 1;
            if (!found) {
                printf("FAIL line %d: no chip %s %s\n", ln, a, b);
                fails++;
            } else
                printf("  ok chip %s %s\n", a, b);
        } else if (!strcmp(cmd, "tcmd")) {        /* tcmd N: the dock's own thing: 0 home, 1 options, 2 help (1.6) */
            bk_touch_cmd(atoi(a));
        } else if (!strcmp(cmd, "touch")) {       /* touch X Y: a tap on the stage at that point (1.6) */
            bk_touch_event(0, 0, (float)atoi(a), (float)atoi(b));
            bk_touch_event(2, 0, (float)atoi(a), (float)atoi(b));
        } else if (!strcmp(cmd, "tdown") || !strcmp(cmd, "tmove") || !strcmp(cmd, "tup")) {   /* tdown S X Y, tmove S X Y, tup S X Y: a finger (slot S) */
            int sl = atoi(a), px = 0, py = 0;
            sscanf(line, "%*s %*d %d %d", &px, &py);
            bk_touch_event(cmd[1] == 'd' ? 0 : cmd[1] == 'm' ? 1 : 2, sl, (float)px, (float)py);
        } else if (!strcmp(cmd, "swipe")) {       /* swipe FINGERS AXIS D [N]: a drag of N (default 1) steps from the middle of the stage (1.6) */
            int fg = 1, ax = 0, dd = 1, nn = 1;
            sscanf(line, "%*s %d %d %d %d", &fg, &ax, &dd, &nn);
            for (int q = 0; q < fg; q++)
                bk_touch_event(0, q, 500.0f + 80.0f * q, 400.0f);
            for (int q = 0; q < fg; q++)
                bk_touch_event(1, q, 500.0f + 80.0f * q + (ax ? 0 : 50.0f * nn * dd), 400.0f + (ax ? 50.0f * nn * dd : 0));
            for (int q = 0; q < fg; q++)
                bk_touch_event(2, q, 500.0f + 80.0f * q + (ax ? 0 : 50.0f * nn * dd), 400.0f + (ax ? 50.0f * nn * dd : 0));
        } else if (!strcmp(cmd, "sidehalf")) {    /* sidehalf 1/0: the columns every other frame in the light picture (1.5.8) */
            bk_set_side_half(atoi(a));
        } else if (!strcmp(cmd, "sidecheck")) {   /* sidecheck: four frames at one moment, the pixels that differ told
                                                    (MD_SIDECHECK_DUMP=FILE: the four frames raw; 1.5.8) */
            static uint32_t f[4][BK_W * BK_H];
            int diff = 0, y0 = BK_H, y1 = -1, x0 = BK_W, x1 = -1;
            for (int q = 0; q < 4; q++)
                memcpy(f[q], p_draw(), sizeof f[q]);
            if (getenv("MD_SIDECHECK_DUMP")) {
                FILE *df = fopen(getenv("MD_SIDECHECK_DUMP"), "wb");
                if (df)
                    fwrite(f, sizeof f, 1, df), fclose(df);
            }
            for (int q = 1; q < 4; q++)
                for (int i = 0; i < BK_W * BK_H; i++)
                    if (f[q][i] != f[0][i]) {
                        int y = i / BK_W, x = i % BK_W;
                        diff++;
                        y0 = y < y0 ? y : y0, y1 = y > y1 ? y : y1, x0 = x < x0 ? x : x0, x1 = x > x1 ? x : x1;
                    }
            /* (a report, no verdict: a whole frame changes a few hundred pixels by itself - the sky's turn, the
               sparks - so the copy is judged by tools/flicker_scan.py on a film instead) */
            printf("  sidecheck: %d pixels differ over four frames at one moment, rows %d..%d, columns %d..%d\n", diff, y0, y1, x0, x1);
        } else if (!strcmp(cmd, "keyboard")) {    /* keyboard 1/0: hints with the PC keyboard's key names */
            bk_set_keyboard(atoi(a));
        } else if (!strcmp(cmd, "small")) {       /* small 1/0: as on a 640 x 480 handheld (the small labels one size up) */
            bk_set_small(atoi(a));
        } else if (!strcmp(cmd, "options")) {     /* options 1/0: OPTIONS opened / closed (1.1) */
            bk_options_show(atoi(a));
        } else if (!strcmp(cmd, "expect_options")) {
            if (bk_options_shown() != atoi(a)) {
                printf("FAIL line %d: options %d, expected %s\n", ln, bk_options_shown(), a);
                fails++;
            } else
                printf("  ok options %s\n", a);
        } else if (!strcmp(cmd, "expect_part")) {   /* expect_part P L: physical button P (number) plays L */
            if (bk_button_part(atoi(a)) != atoi(b)) {
                printf("FAIL line %d: button %s plays %d, expected %s\n", ln, a, bk_button_part(atoi(a)), b);
                fails++;
            } else
                printf("  ok button %s plays %s\n", a, b);
        } else if (!strcmp(cmd, "lang")) {         /* lang de/en: the stage's language (0.80) */
            int n = bk_set_lang_code(a);
            printf("  language %s (%d texts)\n", bk_lang_code(), n);
        } else if (!strcmp(cmd, "expect_lang")) {
            if (strncmp(bk_lang_code(), a, 2)) {
                printf("FAIL line %d: language %s, expected %s\n", ln, bk_lang_code(), a);
                fails++;
            } else
                printf("  ok language %s\n", a);
        } else if (!strcmp(cmd, "opt")) {          /* opt NAME VALUE: pscale, clock, listen, bigtext, contrast, hour (tenths), listenms (1.6) */
            if (bk_option_set(a, atoi(b))) {
                printf("FAIL line %d: no option %s\n", ln, a);
                fails++;
            }
        } else if (!strcmp(cmd, "expect_opt")) {
            if (bk_option_get(a) != atoi(b)) {
                printf("FAIL line %d: option %s is %d, expected %s\n", ln, a, bk_option_get(a), b);
                fails++;
            } else
                printf("  ok option %s %s\n", a, b);
        } else if (!strcmp(cmd, "tale")) {         /* tale N: begin chapter N (1.6) */
            bk_tale_set(atoi(a));
        } else if (!strcmp(cmd, "coach")) {        /* coach N: the first five minutes at step N (5: done) (1.6) */
            bk_coach_set(atoi(a));
        } else if (!strcmp(cmd, "expect_coach")) {
            if (bk_coach_step() != atoi(a)) {
                printf("FAIL line %d: coach at step %d, expected %s\n", ln, bk_coach_step(), a);
                fails++;
            } else
                printf("  ok coach %s\n", a);
        } else if (!strcmp(cmd, "expect_note")) {  /* expect_note DEGREE NOTE: what play mode gives that degree (1.6) */
            if (bk_note_for_degree(atoi(a)) != atoi(b)) {
                printf("FAIL line %d: degree %s gives note %d, expected %s\n", ln, a, bk_note_for_degree(atoi(a)), b);
                fails++;
            } else
                printf("  ok degree %s note %s\n", a, b);
        } else if (!strcmp(cmd, "expect_journal")) {   /* expect_journal BIOMES WORLDS (-1: any) */
            if ((atoi(a) >= 0 && bk_journal_count(0) != atoi(a)) || (atoi(b) >= 0 && bk_journal_count(1) != atoi(b))) {
                printf("FAIL line %d: journal %d Biomes %d worlds, expected %s %s\n", ln, bk_journal_count(0), bk_journal_count(1), a, b);
                fails++;
            } else
                printf("  ok journal %s %s\n", a, b);
        } else if (!strcmp(cmd, "calm")) {         /* calm 1/0: CALM MOTION */
            bk_set_calm(atoi(a));
        } else if (!strcmp(cmd, "expect_calm")) {
            if (bk_calm_on() != atoi(a)) {
                printf("FAIL line %d: calm motion %d, expected %s\n", ln, bk_calm_on(), a);
                fails++;
            } else
                printf("  ok calm motion %s\n", a);
        } else if (!strcmp(cmd, "expect_light")) {
            if (bk_light_on() != atoi(a)) {
                printf("FAIL line %d: light picture %d, expected %s\n", ln, bk_light_on(), a);
                fails++;
            } else
                printf("  ok light picture %s\n", a);
        } else if (!strcmp(cmd, "expect_seq")) {   /* expect_seq TRACK(1..6) N: its sequence plays, with at least N notes */
            int t = atoi(a) - 1, want = b[0] ? atoi(b) : 1, got = bk_seq_notes(t);
            if (got < want) {
                printf("FAIL line %d: track %d's sequence has %d notes, expected at least %d\n", ln, t + 1, got, want);
                fails++;
            } else
                printf("  ok track %d sequence %d notes\n", t + 1, got);
        } else if (!strcmp(cmd, "seq_mark")) {     /* seq_mark TRACK, then expect_seq_same TRACK: unchanged since */
            seq_marked = bk_seq_print(atoi(a) - 1);
        } else if (!strcmp(cmd, "expect_seq_same") || !strcmp(cmd, "expect_seq_changed")) {
            int same = bk_seq_print(atoi(a) - 1) == seq_marked, want = !strcmp(cmd, "expect_seq_same");
            if (same != want) {
                printf("FAIL line %d: track %s's sequence %s\n", ln, a, same ? "unchanged" : "changed");
                fails++;
            } else
                printf("  ok track %s sequence %s\n", a, same ? "as marked" : "changed");
        } else if (!strcmp(cmd, "expect_macro_above") || !strcmp(cmd, "expect_macro_below")) {   /* expect_macro_above K V */
            int k = atoi(a), v = atoi(b), got = bk_macro(k), above = !strcmp(cmd, "expect_macro_above");
            if (above ? got <= v : got >= v) {
                printf("FAIL line %d: dial %d at %d, expected %s %d\n", ln, k, got, above ? "above" : "below", v);
                fails++;
            } else
                printf("  ok dial %d at %d\n", k, got);
        } else if (!strcmp(cmd, "guide")) {      /* guide 1/0: THE GUIDE opened (as on the first start) / closed */
            bk_guide_show(atoi(a));
        } else if (!strcmp(cmd, "expect_guide")) {   /* expect_guide N: on page N (0: closed) */
            if (bk_guide_shown() != atoi(a)) {
                printf("FAIL line %d: guide at %d, expected %s\n", ln, bk_guide_shown(), a);
                fails++;
            } else
                printf("  ok guide %s\n", a);
        } else if (!strcmp(cmd, "garden_fx")) {   /* garden_fx aurora|rain|off */
            bk_garden_fx(!strcmp(a, "aurora") ? 1 : !strcmp(a, "rain") ? 2 : 0);
        } else if (!strcmp(cmd, "expect_gfx_notes")) {   /* expect_gfx_notes N: the aurora / rain answered >= N notes */
            if (bk_gfx_notes() < (uint32_t)atoi(a)) {
                printf("FAIL line %d: the garden answered %u notes, expected at least %s\n", ln, bk_gfx_notes(), a);
                fails++;
            } else
                printf("  ok garden answered %u notes\n", bk_gfx_notes());
        } else if (!strcmp(cmd, "midi_unplug")) {   /* the MIDI device pulled out (as brick_sdl.c's midi_poll sees it) */
            bk_midi_lost();
        } else if (!strcmp(cmd, "midi_ledger")) {   /* midi_ledger 1: follow every note out (0: stop) */
            ml_on = atoi(a);
            memset(ml_held, 0, sizeof ml_held), memset(ml_ons, 0, sizeof ml_ons), ml_stray = 0;
        } else if (!strcmp(cmd, "midi_report")) {   /* what began per channel, what is held now */
            printf("midi ledger: held %d, offs without on %u, notes begun per channel:", ml_count(-1), ml_stray);
            for (int k = 0; k < 16; k++)
                if (ml_ons[k])
                    printf(" ch%d=%u(held %d)", k + 1, ml_ons[k], ml_count(k));
            printf("\n");
        } else if (!strcmp(cmd, "expect_midi_held")) {   /* expect_midi_held N [CH]: at most N notes held (CH 1..16: on it) */
            int got = ml_count(b[0] ? atoi(b) - 1 : -1);
            if (got > atoi(a)) {
                printf("FAIL line %d: %d MIDI notes held%s%s, expected at most %s\n", ln, got, b[0] ? " on channel " : "", b, a);
                fails++;
            } else
                printf("  ok %d MIDI notes held%s%s\n", got, b[0] ? " on channel " : "", b);
        } else if (!strcmp(cmd, "expect_midi_channels")) {   /* expect_midi_channels N: notes began on at least N channels */
            int c = 0;
            for (int k = 0; k < 16; k++)
                c += ml_ons[k] > 0;
            if (c < atoi(a)) {
                printf("FAIL line %d: notes on %d channels, expected %s\n", ln, c, a);
                fails++;
            } else
                printf("  ok notes on %d channels\n", c);
        } else if (!strcmp(cmd, "prefs_reload")) {
            bk_prefs_reload();
        } else if (!strcmp(cmd, "expect_toy")) {   /* expect_toy N: the playground's toy (0 BOUNCE ..) */
            if (bk_toy_index() != atoi(a)) {
                printf("FAIL line %d: toy %d, expected %s\n", ln, bk_toy_index(), a);
                fails++;
            } else
                printf("  ok toy %s\n", a);
        } else if (!strcmp(cmd, "expect_tale_card")) {   /* expect_tale_card N: chapter N's card is up (-1: none) */
            if (bk_tale_card() != atoi(a)) {
                printf("FAIL line %d: tale card %d, expected %s\n", ln, bk_tale_card(), a);
                fails++;
            } else
                printf("  ok tale card %s\n", a);
        } else if (!strcmp(cmd, "expect_tale")) {   /* expect_tale N: the tale at chapter N (0-based) or beyond */
            if (bk_tale_chapter() < atoi(a)) {
                printf("FAIL line %d: the tale at chapter %d, expected %s\n", ln, bk_tale_chapter(), a);
                fails++;
            } else
                printf("  ok tale at chapter %d\n", bk_tale_chapter());
        } else if (!strcmp(cmd, "expect_toy_notes")) {   /* expect_toy_notes N: the PLAYGROUND played at least N notes */
            if (bk_toy_notes() < (uint32_t)atoi(a)) {
                printf("FAIL line %d: the playground played %u notes, expected at least %s\n", ln, bk_toy_notes(), a);
                fails++;
            } else
                printf("  ok playground %u notes\n", bk_toy_notes());
        } else if (!strcmp(cmd, "expect_silence")) {   /* expect_silence: since the last wav, below -60 dBFS, or nothing (1.4) */
            if (!(peak < 0.001f)) {
                printf("FAIL line %d: peak %.2f dBFS, expected silence\n", ln, 20.0 * log10(peak));
                fails++;
            }
        } else if (!strcmp(cmd, "expect_peak_below")) {   /* expect_peak_below DBFS: since the last wav (NaN fails) */
            if (!(peak > 0.001f && 20.0 * log10(peak) < atof(a))) {
                printf("FAIL line %d: peak %.2f dBFS, expected sound below %s\n", ln, 20.0 * log10(peak), a);
                fails++;
            } else
                printf("  ok peak %.2f dBFS below %s\n", 20.0 * log10(peak), a);
        } else if (!strcmp(cmd, "expect_sound")) {
            if (peak < 0.001f) {
                printf("FAIL line %d: silent (peak %g)\n", ln, peak);
                fails++;
            } else
                printf("  ok sound, peak %.1f dBFS\n", 20.0 * log10(peak));
        } else if (!strcmp(cmd, "bench") || !strcmp(cmd, "benchsound") || !strcmp(cmd, "benchdraw")) {
            struct timespec t0, t1;
            uint32_t ms = (uint32_t)atoi(a);
            clock_gettime(CLOCK_MONOTONIC, &t0);
            if (!strcmp(cmd, "benchdraw"))
                for (uint32_t k = 0; k < ms / 33u; k++)
                    p_draw();
            else
                run(ms, strcmp(cmd, "benchsound") != 0);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            double s = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
            printf("%s: %u ms of sound + stage in %.3f s (%.1f %% of real time)\n", cmd, ms, s, 100.0 * s * 1000.0 / ms);
        } else {
            printf("FAIL line %d: unknown command %s\n", ln, cmd);
            fails++;
        }
    }
    fclose(sc);
    bk_shutdown();
    if (getenv("MD_PROFILE_SUM"))
        for (int i = 0; i < PROF_SLOTS; i++)
            if (prof_at[i])
                printf("PROFILE-SUM %p %.1f\n", prof_at[i], prof_sum[i]);
    if (prof_th > 0)
        printf("PROFILE max: input %.2f ms, draw segment %.2f ms, render block %.2f ms, frame %.2f ms\n",
               prof_max[0], prof_max[1], prof_max[2], prof_max[3]);
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
