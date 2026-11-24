/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: WORLDS, whole scenes built on the engine's Biomes.
 *
 * mutabledreams/worlds/worlds.txt (plain text, to read and to extend) holds the worlds, each a block:
 *   [FAMILY] Name of the world
 *   biome  = 89                      the engine's Biome it grows from (its number in the atlas)
 *   story  = One line about it.
 *   tempo  = 52                      (optional) BPM
 *   macros = 35 30 45 60             (optional) DENSITY MOTION COLOUR SPACE in %
 *   engine = MUTE 4 1; CUTOFF 2 40   (optional) the engine: NAME [TRACK] VALUE, ';' between
 *   lab    = LOGIC.PLAY 1; LOGIC.RULE 0; SYNTH.LEVEL 60   the Sound Lab (names as on its pages, _ for spaces)
 *   sleep  = 30                      (optional, 1.4) the SLEEP TIMER in minutes (15, 30, 60, 90), set on entering
 *                                    when none runs yet
 * Entering a world: its Biome is loaded (asked first, as in the atlas, when something is unsaved), then the
 * Sound Lab goes back to neutral (files kept) and the world's settings follow; the result counts as the Biome's
 * starting point, so only your own changes afterwards are "unsaved". WANDER moves to the family's next world
 * every few minutes by itself. Included by brick_views.inc. */

#define WD_MAX 128
#define WD_SETS 40
typedef struct {
    char family[24], name[40], story[120];
    int biome, tempo, macros[4], nengine, nlab, sleep;
    struct { int cmd, track, value; } engine[12];
    struct { int p; float v; } lab[WD_SETS];
} wd_world;
static wd_world wd_w[WD_MAX];
#define WD_FAMS 24                               /* (1.6) up to 24 families: 20 with the 128 worlds */
static int wd_n = -1, wd_fam, wd_pos[WD_FAMS], wd_top[WD_FAMS], wd_nfam;
static char wd_fams[WD_FAMS][24];
static int wd_pending = -1, wd_current = -1, wd_wander, wd_wander_min = 6;
static uint32_t wd_entered_at, wd_mark_at;
static char wd_error[96];

/* the engine's commands a world may give, and how their values are written */
static const struct { const char *name; int cmd, track, scale; } WD_CMDS[] = {
    {"MUTE", MD_MUTE, 1, 1}, {"LEVEL", MD_LEVEL, 1, 10}, {"SEND", MD_SEND, 1, 10}, {"CUTOFF", MD_CUTOFF, 1, 10},
    {"RESONANCE", MD_RESONANCE, 1, 10}, {"DECAY", MD_DECAY, 1, 10}, {"TONE", MD_TONE, 1, 10}, {"HARM", MD_HARM, 1, 10},
    {"PAN", MD_PAN, 1, 10}, {"ATTACK", MD_ATTACK, 1, 10}, {"BREATHE", MD_BREATHE, 1, 10}, {"PROB", MD_PROB, 1, 10},
    {"SWING", MD_SWING, 0, 10}, {"DELAY_TIME", MD_DELAY_TIME, 0, 1}, {"DELAY_FB", MD_DELAY_FB, 0, 10},
    {"REVERB_TIME", MD_REVERB_TIME, 0, 10}, {"FX_LEVEL", MD_FX_LEVEL, 0, 10}, {"GRAIN_SIZE", MD_GRAIN_SIZE, 0, 10},
    {"GRAIN_POS", MD_GRAIN_POS, 0, 10}, {"FEEDBACK", MD_FEEDBACK, 0, 10}, {"GRAIN_PITCH", MD_GRAIN_PITCH, 0, 1},
    {"REVERSE", MD_REVERSE, 0, 1}, {"FREEZE", MD_FREEZE, 0, 1}, {"ROOT", MD_ROOT, 0, 1}, {"TRANSPOSE", MD_TRANSPOSE, 0, 1},
    {0, 0, 0, 0}};

static int bk_lab_find(const char *what);         /* brick_core.c */

static char *wd_trim(char *s)
{
    char *e;
    while (*s == ' ' || *s == '\t')
        s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = 0;
    return s;
}

static void wd_load(void)
{
    char path[700], line[600];
    FILE *f;
    wd_world *w = 0;
    int ln = 0;
    wd_n = 0, wd_nfam = 0, wd_error[0] = 0;
    if (!bk_dir[0])
        return;
    snprintf(path, sizeof path, "%s/../worlds/worlds.txt", bk_dir);
    if (getenv("MD_WORLDS"))
        snprintf(path, sizeof path, "%s", getenv("MD_WORLDS"));
    if (!(f = fopen(path, "r")))
        return;
    while (fgets(line, sizeof line, f)) {
        char *s = wd_trim(line), *eq;
        ln++;
        if (!*s || *s == '#')
            continue;
        if (*s == '[') {                         /* a new world: [FAMILY] Name */
            char *close = strchr(s, ']');
            if (!close || wd_n >= WD_MAX)
                continue;
            *close = 0;
            w = &wd_w[wd_n++];
            memset(w, 0, sizeof *w);
            w->biome = -1, w->tempo = 0, w->macros[0] = -1, w->sleep = 0;
            snprintf(w->family, sizeof w->family, "%s", wd_trim(s + 1));
            snprintf(w->name, sizeof w->name, "%s", wd_trim(close + 1));
            {
                int b;
                for (b = 0; b < wd_nfam && strcmp(wd_fams[b], w->family); b++)
                    ;
                if (b == wd_nfam && wd_nfam < WD_FAMS)
                    snprintf(wd_fams[wd_nfam++], 24, "%s", w->family);
            }
            continue;
        }
        if (!w || !(eq = strchr(s, '=')))
            continue;
        *eq = 0;
        {
            char *key = wd_trim(s), *val = wd_trim(eq + 1);
            if (!strcmp(key, "biome"))
                w->biome = atoi(val);
            else if (!strcmp(key, "story"))
                snprintf(w->story, sizeof w->story, "%s", val);
            else if (!strcmp(key, "tempo"))
                w->tempo = atoi(val);
            else if (!strcmp(key, "sleep"))
                w->sleep = atoi(val);
            else if (!strcmp(key, "macros"))
                sscanf(val, "%d %d %d %d", &w->macros[0], &w->macros[1], &w->macros[2], &w->macros[3]);
            else if (!strcmp(key, "engine") || !strcmp(key, "lab")) {
                char *tok = strtok(val, ";");
                while (tok) {
                    char name[48] = "";
                    float a = 0, b = 0;
                    int got = sscanf(wd_trim(tok), "%47s %f %f", name, &a, &b);
                    if (!strcmp(key, "lab"))
                        for (char *q = name; *q; q++)   /* (the Lab's names are written with _ for their spaces) */
                            if (*q == '_')
                                *q = ' ';
                    if (!strcmp(key, "lab") && got >= 2 && w->nlab < WD_SETS) {
                        int p = bk_lab_find(name);
                        if (p >= 0) {
                            w->lab[w->nlab].p = p, w->lab[w->nlab].v = a;
                            w->nlab++;
                        } else if (!wd_error[0])
                            snprintf(wd_error, sizeof wd_error, "LINE %d: NO LAB SETTING %.40s", ln, name);
                    } else if (!strcmp(key, "engine") && got >= 2 && w->nengine < 12) {
                        int c;
                        for (c = 0; WD_CMDS[c].name && strcmp(WD_CMDS[c].name, name); c++)
                            ;
                        if (WD_CMDS[c].name) {
                            int tr = WD_CMDS[c].track ? (int)a - 1 : 0, v = WD_CMDS[c].track ? (int)b : (int)a;
                            w->engine[w->nengine].cmd = c, w->engine[w->nengine].track = tr < 0 ? 0 : tr > 5 ? 5 : tr;
                            w->engine[w->nengine].value = v * WD_CMDS[c].scale;
                            w->nengine++;
                        } else if (!wd_error[0])
                            snprintf(wd_error, sizeof wd_error, "LINE %d: NO ENGINE COMMAND %.30s", ln, name);
                    }
                    tok = strtok(0, ";");
                }
            }
        }
    }
    fclose(f);
}

static int wd_list(int fam, int *idx)
{
    int n = 0;
    for (int i = 0; i < wd_n; i++)
        if (fam < wd_nfam && !strcmp(wd_w[i].family, wd_fams[fam]))
            idx[n++] = i;
    return n;
}

/* the world's settings, once its Biome plays */
static void wd_apply(int i)
{
    const wd_world *w = &wd_w[i];
    for (int p = 0; p < P_COUNT; p++)            /* the Sound Lab to neutral (MASTER and the chosen files stay) */
        if (!lab_is_global(p) && p != P_SA_FILE && p != P_VO_FILE)
            lab_set(p, LAB_P[p].def);
    if (w->tempo > 0)
        dream_cmd(MD_TEMPO, 0, w->tempo);
    if (w->macros[0] >= 0)
        for (int m = 0; m < 4; m++)
            dream_cmd(MD_MACRO, m, w->macros[m] * 10);
    for (int k = 0; k < w->nengine; k++)
        dream_cmd(WD_CMDS[w->engine[k].cmd].cmd, w->engine[k].track, w->engine[k].value);
    for (int k = 0; k < w->nlab; k++)
        lab_set(w->lab[k].p, w->lab[k].v);
    wd_mark_at = bk_ms();                        /* the engine takes the commands in its next blocks: then marked */
    wd_current = i;
    wd_entered_at = bk_ms();
    if (w->sleep > 0 && !bk_sleep_end) {         /* (1.4) a world for falling asleep brings its timer (none running) */
        int sel = 1;
        while (sel < 4 && BK_SLEEP_MIN[sel] < w->sleep)
            sel++;
        bk_sleep_set(sel);
        char line[48];
        snprintf(line, sizeof line, "SLEEP TIMER %d MIN", BK_SLEEP_MIN[sel]);
        bk_notice(w->name, line);
    } else
        bk_notice(w->name, "");
}

static void wd_enter(int i, int confirm)
{
    const md_patch *p = md_state();
    if (i < 0 || i >= wd_n || wd_w[i].biome < 0 || wd_w[i].biome >= MD_BIOMES)
        return;
    if (md_pending_biome() >= 0 && confirm) {
        dream_cmd(MD_SCENE_CONFIRM, 0, 1);       /* change now */
        return;
    }
    wd_pending = i;
    if (p->biome_id != wd_w[i].biome)
        dream_cmd(MD_SCENE, 0, wd_w[i].biome);
}

/* followed from bk_tick: the world's Biome arrived; wandering on */
static void wd_poll(void)
{
    const md_patch *p = md_state();
    if (wd_mark_at && bk_ms() - wd_mark_at > 60u) {   /* this is now where the Biome starts: changes from here are unsaved */
        uint32_t f = plat_lock();
        md_mark_saved();
        plat_unlock(f);
        wd_mark_at = 0;
    }
    if (wd_pending >= 0 && p->biome_id == wd_w[wd_pending].biome && md_pending_biome() < 0 && !lb_fading) {
        int i = wd_pending;
        wd_pending = -1;
        wd_apply(i);
    }
    if (wd_wander && wd_current >= 0 && wd_pending < 0 && bk_ms() - wd_entered_at > (uint32_t)wd_wander_min * 60000u) {
        int idx[WD_MAX], fam = 0, n, k;
        for (int b = 0; b < wd_nfam; b++)
            if (!strcmp(wd_fams[b], wd_w[wd_current].family))
                fam = b;
        n = wd_list(fam, idx);
        for (k = 0; k < n && idx[k] != wd_current; k++)
            ;
        if (n > 0) {                             /* (0.77, the static analyzer: an empty family left idx unset) */
            wd_enter(idx[(k + 1) % n], 0);
            if (md_pending_biome() >= 0)
                dream_cmd(MD_SCENE_CONFIRM, 0, 1);   /* (wandering: the change is what was asked for) */
        }
        wd_entered_at = bk_ms();
    }
}

static void wd_key(int b)
{
    int idx[WD_MAX], n;
    if (wd_n < 0)
        wd_load();
    if (!wd_nfam)
        return;
    n = wd_list(wd_fam, idx);
    if (b == BK_L1 || b == BK_R1)
        wd_fam = (wd_fam + wd_nfam + (b == BK_R1 ? 1 : -1)) % wd_nfam;
    else if ((b == BK_UP || b == BK_DOWN) && n)
        wd_pos[wd_fam] = clamp(wd_pos[wd_fam] + (b == BK_DOWN ? 1 : -1), 0, n - 1);
    else if (b == BK_LEFT || b == BK_RIGHT) {
        wd_wander_min = clamp(wd_wander_min + (b == BK_RIGHT ? 1 : -1), 2, 30);
        bk_notice(wd_wander ? "WANDERING" : "WANDER TIME SET", "");
    } else if (b == BK_A && n)
        wd_enter(idx[wd_pos[wd_fam]], 1);
    else if (b == BK_Y) {
        wd_wander = !wd_wander;
        if (wd_wander && wd_current < 0 && n)
            wd_enter(idx[wd_pos[wd_fam]], 1);
        wd_entered_at = bk_ms();
        bk_notice(wd_wander ? "WANDERING THROUGH THE WORLDS" : "STAYING HERE", "");
    }
}

int bk_world_count(void) { if (wd_n < 0) wd_load(); return wd_n; }
int bk_world_current(void) { return wd_current; }
const char *bk_world_error(void) { return wd_error; }
void bk_world_enter(int i) { if (wd_n < 0) wd_load(); wd_enter(i, 1); }
const char *bk_world_name(int i) { return i >= 0 && i < wd_n ? wd_w[i].name : ""; }
int bk_world_biome(int i) { return i >= 0 && i < wd_n ? wd_w[i].biome : -1; }
