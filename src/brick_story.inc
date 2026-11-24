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
    /* (1.6) the second book: the dials, the falling stars, the worlds, the journal, the scales, the clock */
    {"THE FIRST WIND", "Wind rises from nowhere, and the dials", "are its hands: turn them, and feel the weather.",
     "TURN THE DIALS 12 TIMES"},
    {"STARS THAT FALL", "Some nights the sky lets go of its lights;", "whoever is quick enough may catch them.",
     "CATCH 5 STARS (STARFALL)"},
    {"A SECOND SKY", "Beyond this garden lie worlds, each with", "its own weather and its own song.",
     "ENTER A WORLD (L2: ALL PLACES)"},
    {"THE LONG ECHO", "A listener's patience is the pond's favourite song:", "wait longer, and it answers longer.",
     "REACH ROUND 8 IN ECHO"},
    {"THREE KEPT SONGS", "A garden becomes a home by what it keeps.", "Keep three melodies, and they will remember you.",
     "KEEP 3 MORE MELODIES: R2 + X"},
    {"THE MAP OF DREAMS", "Every place you rest in is written down,", "in a book that only the garden can read.",
     "STAY IN 4 NEW BIOMES (JOURNAL: OPTIONS)"},
    {"ANOTHER SCALE", "Music is what we agree to call a note.", "Other peoples chose other steps. Try theirs.",
     "CHOOSE A SCALE FOR PLAYING (OPTIONS)"},
    {"THE GARDEN KEEPS TIME", "Morning is not the same as midnight.", "Let the garden know what hour it is.",
     "TURN ON CLOCK GARDEN (OPTIONS)"},
};
#define ST_N ((int)(sizeof ST_CHAPTERS / sizeof ST_CHAPTERS[0]))

static int st_chapter = -1;                      /* the chapter under way (ST_N: the tale is told); -1 not read yet */
static int st_dirty, st_card = -1, st_card_intro;   /* st_card_intro: the card opens a chapter (not: told) */               /* st_card: whose card is up (-1 none), since st_card_at */
static uint32_t st_card_at, st_base_notes, st_base_keeps, st_base_lab;
/* (1.6) THE FIRST FIVE MINUTES: on a first start (no settings or tale kept yet) five small steps lead through the garden, shown
   where the tale's line is; each is done by doing it. bk_coach 0..4 the step, 5 done (also for everyone who already has settings) */
static const char *const BK_COACH[5] = {"PRESS START: THE GARDEN BEGINS TO PLAY", "TURN A DIAL: THE WEATHER OF THE GARDEN",
                                         "CHOOSE ANOTHER TRACK: A NEW PLANT", "OPEN THE MENU: ALL PLACES AND THE GUIDE",
                                         "VISIT THE PLAYGROUND: A TOY TO PLAY"};
static int bk_quick_open;                        /* (defined in brick_core.c) */
static int bk_coach = 5, bk_coach_track0 = -1;
static uint32_t bk_coach_turns0, bk_coach_done_at;
static void bk_coach_to(int s)
{
    bk_coach = s, bk_coach_turns0 = bk_turns, bk_coach_track0 = dream_ui.track, st_dirty = 1;
    if (s >= 5)
        bk_coach_done_at = bk_stamp(bk_ms());
}
static void bk_coach_tick(void)
{
    if (bk_coach >= 5)
        return;
    switch (bk_coach) {
    case 0: if (md_playing()) bk_coach_to(1); break;
    case 1: if (bk_turns - bk_coach_turns0 >= 3u) bk_coach_to(2); break;
    case 2: if (dream_ui.track != bk_coach_track0) bk_coach_to(3); break;
    case 3: if (bk_menu_open || bk_quick_open) bk_coach_to(4); break;
    case 4: if (bk_view == V_TOYS) bk_coach_to(5); break;
    }
}
int bk_coach_step(void) { return bk_coach; }
void bk_coach_set(int s) { bk_coach_to(s < 0 ? 0 : s > 5 ? 5 : s); }   /* (the tests) */
static int st_base_biome = -1, st_base_world = -1, st_base_jr, st_base_scale, st_base_clock;
static uint32_t st_base_turns;

static void st_begin(int c)                      /* a chapter starts: what it counts, from here */
{
    st_chapter = c;
    st_base_notes = ty_notes, st_base_keeps = (uint32_t)ty_keeps, st_base_lab = lab_turned_at;
    st_base_biome = md_state()->biome_id;
    st_base_turns = bk_turns, st_base_world = bk_world_current(), st_base_jr = bk_journal_count(0);
    st_base_scale = bk_play_scale, st_base_clock = bk_clock_on;
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
    if (bk_dir[0]) {                             /* (1.6) the journal */
        snprintf(path, sizeof path, "%s/journal.txt", bk_dir);
        if ((f = fopen(path, "r"))) {
            while (fgets(line, sizeof line, f)) {
                int v = atoi(line + 2);
                if (line[0] == 'b' && v >= 0 && v < MD_BIOMES)
                    bk_jr_set(bk_jr_b, v);
                else if (line[0] == 'w' && v >= 0 && v < 128)
                    bk_jr_set(bk_jr_w, v);
            }
            fclose(f);
            bk_jr_dirty = 0;
        }
    }
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
                    bk_lang_choose_idx(bk_lang_by_code(line + 5));
                else if (!strncmp(line, "coach ", 6))
                    bk_coach = clamp(atoi(line + 6), 0, 5);
                else if (!strncmp(line, "pscale ", 7))
                    bk_play_scale = clamp(atoi(line + 7), 0, BK_NPSCALES - 1);
                else if (!strncmp(line, "clock ", 6))
                    bk_clock_on = atoi(line + 6) != 0;
                else if (!strncmp(line, "listen ", 7))
                    bk_listen_sel = clamp(atoi(line + 7), 0, 3);
                else if (!strncmp(line, "bigtext ", 8))
                    bk_big_text = atoi(line + 8) != 0;
                else if (!strncmp(line, "contrast ", 9))
                    bk_contrast = atoi(line + 9) != 0;
                else if (!strncmp(line, "tsize ", 6))
                    bk_tsize = clamp(atoi(line + 6), 0, 2);
                else if (!strncmp(line, "tside ", 6))
                    bk_tside = atoi(line + 6) != 0;
                else if (!strncmp(line, "tglow ", 6))
                    bk_tglow = clamp(atoi(line + 6), 0, 2);
                else if (!strncmp(line, "tbuzz ", 6))
                    bk_tbuzz = atoi(line + 6) != 0;
                else if (!strncmp(line, "tlift ", 6))
                    bk_tlift = atoi(line + 6) != 0;
                else if (!strncmp(line, "perf ", 5))
                    bk_perf = atoi(line + 5) != 0;
                else if (!strncmp(line, "tpad ", 5))
                    bk_tpad = atoi(line + 5) != 0;
                else if (!strncmp(line, "tmode ", 6))
                    bk_tmode = clamp(atoi(line + 6), 0, 2);
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
    bk_coach = 5;
    if (bk_dir[0]) {                             /* (1.6) the coach: only for a start with nothing kept yet */
        struct stat sb;
        snprintf(path, sizeof path, "%s/brick-prefs.txt", bk_dir);
        if (stat(path, &sb) != 0 && c == 0)
            bk_coach = 0, bk_coach_turns0 = bk_turns, bk_coach_track0 = dream_ui.track;
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
    if (bk_jr_dirty) {                           /* (1.6) the garden journal: the Biomes and worlds visited */
        snprintf(path, sizeof path, "%s/journal.txt", bk_dir);
        if ((f = fopen(path, "w"))) {
            for (int i = 0; i < MD_BIOMES; i++)
                if (bk_jr_get(bk_jr_b, i))
                    fprintf(f, "b %d\n", i);
            for (int i = 0; i < 128; i++)
                if (bk_jr_get(bk_jr_w, i))
                    fprintf(f, "w %d\n", i);
            fprintf(f, "(Mutable Dreams: the Biomes and worlds you have stayed in; delete to begin the journal again)\n");
            fclose(f);
        }
        bk_jr_dirty = 0;
    }
    snprintf(path, sizeof path, "%s/brick-prefs.txt", bk_dir);
    if ((f = fopen(path, "w"))) {
        fprintf(f, "calm %d\nlight %d\ntoy %d\nlang %s\nrepeat %d\ncoach %d\npscale %d\nclock %d\nlisten %d\nbigtext %d\ncontrast %d\ntsize %d\ntside %d\ntglow %d\ntbuzz %d\ntlift %d\nperf %d\ntpad %d\ntmode %d\nbuttons", bk_calm,
                bk_light && !bk_light_auto, ty_toy, BK_LANG_CODE[bk_lang_idx], bk_rep_speed, bk_coach, bk_play_scale, bk_clock_on, bk_listen_sel,
                bk_big_text, bk_contrast, bk_tsize, bk_tside, bk_tglow, bk_tbuzz, bk_tlift, bk_perf, bk_tpad, bk_tmode);
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
    case 8: return bk_turns - st_base_turns >= 12u;
    case 9: return ty_fl_best >= 5;
    case 10: return bk_world_current() >= 0 && bk_world_current() != st_base_world;
    case 11: return ty_ec_best >= 8;
    case 12: return (uint32_t)ty_keeps >= st_base_keeps + 3u;
    case 13: return bk_journal_count(0) >= st_base_jr + 4;
    case 14: return bk_play_scale != st_base_scale;
    case 15: return bk_clock_on != st_base_clock;
    }
    return 0;
}

/* in bk_tick: a deed done shows its card and the next chapter begins */
static void st_tick(void)
{
    static int last_ec_best, last_fl_best;
    if (st_chapter < 0)
        return;
    bk_coach_tick();
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
void bk_tale_set(int c) { st_card = -1; st_begin(c < 0 ? 0 : c > ST_N ? ST_N : c); }   /* (the tests) */
/* (the tests) as at a new start: the settings forgotten, then read again from save/ */
void bk_prefs_reload(void)
{
    bk_calm = 0, bk_light = 0, ty_toy = 0, bk_lang = bk_lang_idx = 0, bk_rep_speed = 1;
    bk_play_scale = bk_clock_on = bk_listen_sel = bk_big_text = bk_contrast = 0, bk_coach = 5;
    bk_tsize = 1, bk_tside = 0, bk_tglow = 1, bk_tbuzz = 1, bk_tlift = 0, bk_perf = 0, bk_tpad = 0, bk_tmode = 0;
    bk_map_reset();
    st_load();
}
int bk_toy_index(void) { return ty_toy; }       /* (the tests: whose card is up, -1 none, -2 the first still to come) */
int bk_macro(int k) { return md_state()->macro[k & 3]; }
