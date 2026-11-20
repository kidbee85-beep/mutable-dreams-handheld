/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: DREAM TALES (0.69), a story told while one plays.
 *
 * Eight chapters, each a few lines of the tale and a small deed that leads through the Brick: the playground, the
 * firefly, KEEP, the ECHO game, the planets, another Biome, the Sound Lab, and all the balls at once. When a deed is
 * done its chapter's card rises over the stage (its title and lines, a few seconds, any time), the next chapter
 * begins; the playground shows where the tale stands. Watched in bk_tick (cheap: a few compares); the chapter
 * reached is written to save/tale.txt by the drawing side (never a file from the sound's thread). Also there: the
 * ECHO game's best round. Nothing else of the user's is touched. */

typedef struct { const char *title, *line1, *line2, *deed; } st_chapter_t;
static const st_chapter_t ST_CHAPTERS[] = {
    {"THE SLEEPING GARDEN", "Under the night the garden dreams of sound.", "Touch it, and it will answer.",
     "PLAY 16 NOTES IN THE PLAYGROUND"},
    {"THE FIREFLY'S SONG", "A small light drifts through the dark,", "gathering tones the way others gather seeds.",
     "CATCH A MELODY OF 4 NOTES (FIREFLY)"},
    {"WHAT THE NIGHT KEEPS", "Not every song has to fade at dawn.", "Some are kept, and go on growing.",
     "KEEP A MELODY: R2 + X IN THE PLAYGROUND"},
    {"THE ECHOING POND", "The pond sings back what it hears,", "but only to those who listen first.",
     "REACH ROUND 4 IN ECHO"},
    {"PLANETS IN TUNE", "Far above, the planets keep their own time,", "and still they meet, now and then, in one chord.",
     "LET 4 PLANETS SING AT ONCE (ORBITS)"},
    {"THE WANDERING MIND", "Every garden is a door", "to another garden, dreaming another dream.",
     "TRAVEL TO ANOTHER BIOME"},
    {"THE ALCHEMIST'S BENCH", "In the Sound Lab the sound itself", "can be bent, folded and set alight.",
     "OPEN THE SOUND LAB, TURN A DIAL"},
    {"MUTABLE DREAMS", "Seven lights in the air, falling, rising, singing.", "When you wake, the dream goes on without you.",
     "THROW 7 BALLS AT ONCE (BOUNCE)"},
};
#define ST_N ((int)(sizeof ST_CHAPTERS / sizeof ST_CHAPTERS[0]))

static int st_chapter = -1;                      /* the chapter under way (ST_N: the tale is told); -1 not read yet */
static int st_dirty, st_card = -1, st_card_intro;   /* st_card_intro: the card opens a chapter (not: told) */               /* st_card: whose card is up (-1 none), since st_card_at */
static uint32_t st_card_at, st_base_notes, st_base_keeps, st_base_lab;
static int st_base_biome = -1;

static void st_begin(int c)                      /* a chapter starts: what it counts, from here */
{
    st_chapter = c;
    st_base_notes = ty_notes, st_base_keeps = (uint32_t)ty_keeps, st_base_lab = lab_turned_at;
    st_base_biome = md_state()->biome_id;
}
static void st_load(void)                        /* (the drawing side, once) */
{
    char path[600], line[64];
    FILE *f;
    int c = 0, best = 0;
    if (bk_dir[0]) {
        snprintf(path, sizeof path, "%s/tale.txt", bk_dir);
        if ((f = fopen(path, "r"))) {
            while (fgets(line, sizeof line, f)) {
                if (!strncmp(line, "chapter ", 8))
                    c = atoi(line + 8);
                else if (!strncmp(line, "echo best ", 10))
                    best = atoi(line + 10);
                else if (!strncmp(line, "stars best ", 11) && atoi(line + 11) > ty_fl_best)
                    ty_fl_best = atoi(line + 11);
            }
            fclose(f);
        }
    }
    ty_ec_best = best > ty_ec_best ? best : ty_ec_best;
    if (bk_dir[0]) {                             /* (0.77) the player's own settings: calm, light, the last toy */
        snprintf(path, sizeof path, "%s/brick-prefs.txt", bk_dir);
        if ((f = fopen(path, "r"))) {
            while (fgets(line, sizeof line, f)) {
                if (!strncmp(line, "calm ", 5))
                    bk_calm = atoi(line + 5) != 0;
                else if (!strncmp(line, "light ", 6))
                    bk_light = atoi(line + 6) != 0;
                else if (!strncmp(line, "toy ", 4) && atoi(line + 4) >= 0 && atoi(line + 4) < TY_COUNT)
                    ty_toy = atoi(line + 4);
                else if (!strncmp(line, "lang ", 5) && !bk_lang_forced)
                    bk_lang_choose(!strncmp(line + 5, "de", 2));
                else if (!strncmp(line, "repeat ", 7))
                    bk_rep_speed = clamp(atoi(line + 7), 0, 2);
                else if (!strncmp(line, "buttons ", 8)) {   /* (1.1) each physical button's part, in order */
                    int v[BK_NBUTTONS], k = 0, seen = 0;
                    char *p = line + 8;
                    while (k < BK_NBUTTONS && *p) {
                        v[k] = (int)strtol(p, &p, 10);
                        if (v[k] < 0 || v[k] >= BK_NBUTTONS)
                            break;
                        seen |= 1 << v[k];
                        k++;
                    }
                    if (k == BK_NBUTTONS && seen == (1 << BK_NBUTTONS) - 1) {   /* (only a whole, sound mapping) */
                        for (k = 0; k < BK_NBUTTONS; k++)
                            bk_map[k] = v[k];
                        bk_map_set = 1;
                    }
                }
            }
            fclose(f);
        }
    }
    st_begin(c < 0 ? 0 : c > ST_N ? ST_N : c);
    if (st_chapter == 0)                         /* the very first: the tale's first card on the first visit */
        st_card = -2;
}
static void st_save(void)
{
    char path[600];
    FILE *f;
    if (!bk_dir[0])
        return;
    snprintf(path, sizeof path, "%s/tale.txt", bk_dir);
    if ((f = fopen(path, "w"))) {
        fprintf(f, "chapter %d\necho best %d\nstars best %d\n(Mutable Dreams: how far the tale has come; delete to begin it again)\n", st_chapter, ty_ec_best, ty_fl_best);
        fclose(f);
    }
    snprintf(path, sizeof path, "%s/brick-prefs.txt", bk_dir);
    if ((f = fopen(path, "w"))) {
        fprintf(f, "calm %d\nlight %d\ntoy %d\nlang %s\nrepeat %d\nbuttons", bk_calm, bk_light && !bk_light_auto, ty_toy, bk_lang ? "de" : "en", bk_rep_speed);
        if (!bk_map_set)
            bk_map_reset();
        for (int k = 0; k < BK_NBUTTONS; k++)
            fprintf(f, " %d", bk_map[k]);
        fprintf(f, "\n(Mutable Dreams: CALM MOTION, LIGHT PICTURE, the playground's last toy, the language (de or en), how held buttons"
                   " repeat (0-2) and each button's part (OPTIONS), kept between starts; delete this file for the defaults)\n");
        fclose(f);
    }
}

static int st_deed_done(int c)
{
    int on = 0;
    switch (c) {
    case 0: return ty_notes - st_base_notes >= 16u;
    case 1: return bk_view == V_TOYS && ty_toy == TY_FIREFLY && ty_nloop >= 4;
    case 2: return (uint32_t)ty_keeps > st_base_keeps;
    case 3: return ty_ec_best >= 4;
    case 4:
        for (int i = 0; i < TY_PLANETS; i++)
            on += ty_pl[i].on;
        return bk_view == V_TOYS && ty_toy == TY_ORBITS && on >= 4;
    case 5: return st_base_biome >= 0 && md_state()->biome_id != st_base_biome && md_pending_biome() < 0;
    case 6: return lab_open && lab_turned_at && lab_turned_at != st_base_lab;
    case 7:
        for (int i = 0; i < TY_BALLS; i++)
            on += ty_ball[i].on;
        return bk_view == V_TOYS && ty_toy == TY_BOUNCE && on >= TY_BALLS;
    }
    return 0;
}

/* in bk_tick: a deed done shows its card and the next chapter begins */
static void st_tick(void)
{
    static int last_ec_best, last_fl_best;
    if (st_chapter < 0)
        return;
    if (ty_ec_best != last_ec_best)              /* (a new best in ECHO: kept on the card too) */
        last_ec_best = ty_ec_best, st_dirty = 1;
    if (ty_fl_best != last_fl_best && ty_fl_round == 0)   /* (STARFALL's best: written when the round is over) */
        last_fl_best = ty_fl_best, st_dirty = 1;
    if (st_chapter >= ST_N || st_card >= 0)
        return;
    if (st_deed_done(st_chapter)) {
        st_card = st_chapter, st_card_at = bk_stamp(bk_ms());
        st_begin(st_chapter + 1);
        st_dirty = 1;
    }
}
int bk_tale_chapter(void) { return st_chapter; }  /* (the tests) */
int bk_tale_card(void) { return st_card; }
/* (the tests) as at a new start: the settings forgotten, then read again from save/ */
void bk_prefs_reload(void)
{
    bk_calm = 0, bk_light = 0, ty_toy = 0, bk_lang = 0, bk_rep_speed = 1;
    bk_map_reset();
    st_load();
}
int bk_toy_index(void) { return ty_toy; }       /* (the tests: whose card is up, -1 none, -2 the first still to come) */
int bk_macro(int k) { return md_state()->macro[k & 3]; }
