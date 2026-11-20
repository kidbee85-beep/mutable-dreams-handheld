/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the studio renderer. The same engine as on the Brick, run in
 * real time with no window and no sound card, streaming the stereo mix out of stdout and taking short control
 * lines in on stdin. For the radio studio: the Mac's CPU takes the whole Sound Lab, and the plan (Biome, World,
 * mastering) can be steered from outside.
 *
 *   brick-studio [SAVE_DIR] [--seconds N] [--wav FILE.wav]
 *
 * stdout: raw little-endian float32, interleaved stereo, 44100 Hz (BK_RATE). With --wav, a 16-bit WAV is written
 *         instead (and stdout stays free for status); both can be used at once.
 * stderr: status lines (Biome, playing, engine time, drift).
 * stdin  (one line at a time): quit · biome N · world N · play · stop · master NAME · light 0|1 · lab MOD.PARAM V
 *   NAME: OWN, CLEAN, GENTLE, WARM, LOUD, AIRY (or a number 0..5). The change is pending and settles over the
 *   next blocks, like a press on the device. lab reaches the Sound Lab by name ("MASTER.SOUND", "POLY.LEVEL",
 *   spaces as _), so the full Mutable Instruments set (Rings, Elements, Clouds, ...) can be turned on remotely. */
#include "brick_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>

static double now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

static const char *const MASTERS[] = {"OWN", "CLEAN", "GENTLE", "WARM", "LOUD", "AIRY"};

/* one line of control, applied right away (the engine settles it over the next blocks) */
static void control(const char *line)
{
    char a[64] = "", b[64] = "";
    if (sscanf(line, "%63s %63s", a, b) < 1)
        return;
    if (!strcmp(a, "quit"))
        exit(0);
    if (!strcmp(a, "biome") && b[0]) {
        int n = atoi(b);
        if (n >= 0 && n < bk_biome_count())
            bk_scene(n);
        else
            fprintf(stderr, "studio: biome %d out of range (0..%d)\n", n, bk_biome_count() - 1);
    } else if (!strcmp(a, "world") && b[0]) {
        int n = atoi(b);
        if (n >= 0 && n < bk_world_count())
            bk_world_enter(n);
        else
            fprintf(stderr, "studio: world %d out of range (0..%d)\n", n, bk_world_count() - 1);
    } else if (!strcmp(a, "play"))
        bk_play(1);
    else if (!strcmp(a, "stop"))
        bk_play(0);
    else if (!strcmp(a, "master") && b[0]) {
        int v = -1, i;
        for (i = 0; i < 6; i++)
            if (!strcasecmp(b, MASTERS[i]))
                v = i;
        if (v < 0)
            v = atoi(b);
        if (v >= 0 && v <= 5 && bk_lab_set("MASTER.SOUND", (float)v) == 0)
            fprintf(stderr, "studio: master %s\n", MASTERS[v]);
        else
            fprintf(stderr, "studio: no master %s\n", b);
    } else if (!strcmp(a, "lab")) {           /* lab MODULE.PARAMETER VALUE (spaces in the name as _) */
        char name[128];
        const char *val = strrchr(line, ' ');
        size_t i;
        for (i = 0; i < sizeof name - 1 && b[i]; i++)
            name[i] = b[i] == '_' ? ' ' : b[i];
        name[i] = 0;
        if (val && bk_lab_set(name, (float)atof(val + 1)) == 0)
            fprintf(stderr, "studio: lab %s = %s\n", name, val + 1);
        else
            fprintf(stderr, "studio: no lab parameter %s\n", name);
    } else
        fprintf(stderr, "studio: unknown command %s\n", a);
}

static int stdin_line(char *line, size_t n)     /* a pending control line, or 0 (none); -1 the pipe is closed */
{
    fd_set fds;
    struct timeval tv = {0, 0};
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    if (select(1, &fds, 0, 0, &tv) <= 0)
        return 0;
    if (!fgets(line, (int)n, stdin))
        return -1;
    return 1;
}

static void stdin_ready(void)              /* read every pending control line, never block the stream */
{
    char line[256];
    int r;
    while ((r = stdin_line(line, sizeof line)) > 0) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = 0;
        control(line);
    }
}

int main(int argc, char **argv)
{
    const char *save = 0, *wav_path = 0;
    double seconds = 0;
    FILE *wav = 0;
    uint32_t wav_frames = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--seconds") && i + 1 < argc)
            seconds = atof(argv[++i]);
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc)
            wav_path = argv[++i];
        else if (!save)
            save = argv[i];
    }
    setvbuf(stdout, 0, _IONBF, 0);         /* the stream is live, never wait for a full buffer */
    bk_init(save);
    if (wav_path) {
        wav = fopen(wav_path, "wb");
        if (!wav) {
            fprintf(stderr, "studio: cannot write %s\n", wav_path);
            return 2;
        }
        /* a 16-bit stereo header, patched at the end (like the test runner's wav) */
        {
            uint8_t h[44] = {0};
            uint32_t v;
            memcpy(h, "RIFF", 4); v = 36; memcpy(h + 4, &v, 4);
            memcpy(h + 8, "WAVEfmt ", 8);
            v = 16; memcpy(h + 16, &v, 4);
            h[20] = 1; h[22] = 2;
            v = BK_RATE; memcpy(h + 24, &v, 4);
            v = BK_RATE * 4; memcpy(h + 28, &v, 4);
            h[32] = 4; h[34] = 16;
            memcpy(h + 36, "data", 4);
            fwrite(h, 1, 44, wav);
        }
    }
    bk_play(1);
    fprintf(stderr, "studio: streaming, Biome %s\n", bk_biome_name());
    {
        double next = now_ms();
        double t0 = next, eng0 = (double)bk_ms();
        uint32_t last = 0;
        for (;;) {
            static float lr[2 * 256];
            int16_t pcm[2 * 256];
            bk_render(lr, 256);
            if (wav) {
                for (int i = 0; i < 512; i++) {
                    float v = lr[i] > 1 ? 1 : lr[i] < -1 ? -1 : lr[i];
                    pcm[i] = (int16_t)(v * 32767.0f);
                }
                fwrite(pcm, 2, 512, wav);
                wav_frames += 256;
            } else {
                fwrite(lr, 4, 512, stdout);
            }
            /* the engine's clock: sleep off the difference to the wall clock */
            next += 256.0 * 1000.0 / (double)BK_RATE;
            {
                double now = now_ms();
                if (now < next) {
                    useconds_t u = (useconds_t)((next - now) * 1000.0);
                    if (u)
                        usleep(u);
                } else if (now - next > 100.0)
                    next = now;            /* a stall: carry on from here, no burst */
            }
            stdin_ready();
            {   /* a status line every 5 s of engine time */
                uint32_t s = bk_ms() / 1000u;
                if (s / 5u != last / 5u) {
                    last = s;
                    fprintf(stderr, "studio: %3us  drift %+.0f ms  cpu %u%%  %s  %s\n", s,
                            (double)bk_ms() - eng0 - (now_ms() - t0), bk_cpu_pct(),
                            bk_playing() ? "playing" : "stopped", bk_biome_name());
                }
            }
            if (seconds > 0 && now_ms() - t0 >= seconds * 1000.0)
                break;
        }
    }
    if (wav) {
        uint32_t v = 36 + wav_frames * 4;
        fseek(wav, 4, SEEK_SET); fwrite(&v, 4, 1, wav);
        v = wav_frames * 4;
        fseek(wav, 40, SEEK_SET); fwrite(&v, 4, 1, wav);
        fclose(wav);
        fprintf(stderr, "studio: wrote %s (%u frames, %.1f s)\n", wav_path, wav_frames, wav_frames / 44100.0);
    }
    bk_shutdown();
    return 0;
}
