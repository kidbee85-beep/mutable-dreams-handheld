/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the Sound Lab per Biome.
 *
 * Every Biome uses the shared Sound Lab (save/lab.bin) until MASTER > THIS BIOME is set to OWN SOUND LAB: from then
 * on that Biome keeps its own (save/lab-biomes/biome-NNN.bin, its sample file by name included), and whenever the
 * Biome comes back - turned to by hand, recalled from a bank or reached on an Itinera journey - its Sound Lab comes
 * with it. A Biome without one gets the shared Lab. MASTER (the output: gain, limiter...) and LAB FOLLOWS BIOME are
 * the same everywhere. Edits are saved into whichever Lab is in use.
 *
 * The change is smooth (MASTER > LAB CHANGE TIME, 3 s; 0 = at once): every continuous value glides from the old
 * Lab to the new; values that switch (a mode, a model, a file, freeze...) switch half way, and a module whose
 * switches change fades its MIX / LEVEL out over the first half and in, with the new values, over the second.
 * An edit during the change completes it first. */
#include <sys/stat.h>

static int lb_biome = -1, lb_own;
static float lb_from[P_COUNT], lb_to[P_COUNT];
static uint32_t lb_fade_start, lb_fade_ms;
static int lb_fading;
static float lb_progress = 1.0f;                 /* for the picture: 0..1 */
static int lb_own_in_use(void) { return lb_own; }

static void lb_path(char *out, size_t n, int id)
{
    snprintf(out, n, "%s/lab-biomes/biome-%03d.bin", bk_dir, id);
}
static int lb_exists(int id)
{
    char path[700];
    FILE *f;
    if (!bk_dir[0] || id < 0)
        return 0;
    lb_path(path, sizeof path, id);
    if (!(f = fopen(path, "rb")))
        return 0;
    fclose(f);
    return 1;
}
static const char *lb_sample_name(void) { return sa_loaded >= 0 ? sa_names[sa_loaded] : ""; }
static void lb_apply_sample(const char *name)
{
    if (sa_nfiles < 0)
        sa_scan();
    for (int i = 0; i < sa_nfiles; i++)
        if (!strcmp(sa_names[i], name))
            lab_v[P_SA_FILE] = (float)i;
}

static void lb_save_current(void)
{
    if (lb_own && lb_biome >= 0) {
        char path[700], dir[620];
        snprintf(dir, sizeof dir, "%s/lab-biomes", bk_dir);
        mkdir(dir, 0755);
        lb_path(path, sizeof path, lb_biome);
        lab_write_file(path, lb_sample_name());
    }
    lab_save_global_master();
}

/* the Lab for Biome id: its own file, or the shared one (the global parameters untouched either way) */
static void lb_enter(int id)
{
    char path[700], sample[64];
    float keep[P_COUNT];
    memcpy(keep, lab_v, sizeof keep);
    lb_biome = id;
    if (lab_v[P_LB_FOLLOW] > 0.5f && lb_exists(id)) {
        lb_path(path, sizeof path, id);
        if (lab_read_file(path, 1, sample, sizeof sample)) {
            lb_own = 1;
            lb_apply_sample(sample);
        }
    } else {
        int was_own = lb_own;
        lb_own = 0;
        if (was_own) {                           /* back to the shared Lab */
            lab_path(path, sizeof path, "");
            if (!lab_read_file(path, 1, 0, 0)) {
                lab_defaults();
                for (int p = 0; p < P_COUNT; p++)
                    if (lab_is_global(p))
                        lab_v[p] = keep[p];
            }
            sa_loaded = sa_loaded >= 0 ? -2 - sa_loaded : sa_loaded;
            lab_v[P_SA_FILE] = (float)sa_kept_index();
        }
    }
    for (int p = 0; p < P_COUNT; p++)
        if (lab_is_global(p))
            lab_v[p] = keep[p];
    lab_v[P_LB_OWN] = (float)lb_own;
}

static int lb_continuous(int p)
{
    int u = LAB_P[p].unit;
    return u != U_ENUM && u != U_ONOFF && u != U_NUM;
}
/* the MIX / LEVEL that fades a module whose switches change; -1 none */
static int lb_level_of(int m)
{
    switch (m) {
    case M_SAMPLES: return P_SA_LEVEL;
    case M_POLY: return P_PO_LEVEL;
    case M_VOICES: return P_VO_LEVEL;
    case M_SYNTH: return P_SY_LEVEL;
    case M_CLOUD: return P_CL_MIX;
    case M_PARTICLES: return P_PA_MIX;
    case M_SEEDS: return P_BE_MIX;
    case M_SHIMMER: return P_SH_MIX;
    case M_CROSSMOD: return P_XM_MIX;
    }
    return -1;
}

/* the change's values at time t (0..1) into lab_v */
static void lb_apply(float t)
{
    int p, m, duck[M_COUNT] = {0};
    float s = t * t * (3.0f - 2.0f * t);         /* smooth step */
    for (p = 0; p < P_COUNT; p++)
        if (!lab_is_global(p) && !lb_continuous(p) && lb_from[p] != lb_to[p])
            duck[LAB_P[p].mod] = 1;
    for (p = 0; p < P_COUNT; p++) {
        if (lab_is_global(p))
            continue;
        lab_v[p] = lb_continuous(p) ? lb_from[p] + (lb_to[p] - lb_from[p]) * s : t < 0.5f ? lb_from[p] : lb_to[p];
    }
    for (m = 0; m < M_COUNT; m++) {
        int lv = lb_level_of(m);
        if (!duck[m] || lv < 0)
            continue;
        lab_v[lv] = t < 0.5f ? lb_from[lv] * (1.0f - 2.0f * t) : lb_to[lv] * (2.0f * t - 1.0f);
        if (t < 0.5f && lab_v[lv] < 0.01f)
            lab_v[lv] = 0.01f;                   /* (the module stays alive until it switches: no re-start mid-fade) */
    }
}

static void lb_finish(void)
{
    if (!lb_fading)
        return;
    lb_fading = 0;
    lb_progress = 1.0f;
    for (int p = 0; p < P_COUNT; p++)
        if (!lab_is_global(p))
            lab_v[p] = lb_to[p];
}

static void lb_fade_step(void)
{
    uint32_t el;
    if (!lb_fading)
        return;
    el = bk_ms() - lb_fade_start;
    if (el >= lb_fade_ms) {
        lb_finish();
        return;
    }
    lb_progress = (float)el / (float)lb_fade_ms;
    lb_apply(lb_progress);
}

/* every block: has the Biome changed? Then the new Lab, gliding there */
static void lb_check(void)
{
    int id = md_state()->biome_id;
    lb_fade_step();
    if (id == lb_biome)
        return;
    lb_finish();
    if (lab_dirty)                               /* what was edited goes where it was edited first */
        lab_save();
    memcpy(lb_from, lab_v, sizeof lb_from);
    {
        int first = lb_biome < 0;                /* the start: the Lab is simply there */
        lb_enter(id);
        if (first)
            return;
    }
    memcpy(lb_to, lab_v, sizeof lb_to);
    lb_fade_ms = (uint32_t)lab_v[P_LB_TIME];
    if (lb_fade_ms > 0 && memcmp(lb_from, lb_to, sizeof lb_to)) {
        lb_fading = 1;
        lb_progress = 0.0f;
        lb_fade_start = bk_ms();
        lb_apply(0.0f);
    }
}

static void lb_set_own(int own)
{
    char path[700];
    if (own == lb_own)
        return;
    if (own) {                                   /* this Biome keeps the Lab as it is now */
        lb_own = 1;
        lab_v[P_LB_FOLLOW] = 1;
        lb_save_current();
    } else {                                     /* given up: the file goes, the shared Lab comes back */
        lb_path(path, sizeof path, lb_biome);
        remove(path);
        lb_own = 1;                              /* (so lb_enter switches back) */
        lb_enter(lb_biome);
    }
    lab_v[P_LB_OWN] = (float)lb_own;
}

static void lb_set_follow(int on)
{
    lab_v[P_LB_FOLLOW] = (float)on;
    lab_save_global_master();
    lb_enter(lb_biome);
}
