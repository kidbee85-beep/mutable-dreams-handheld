/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: THE GUIDE (0.73), how it all works, for anyone, in fourteen
 * pictured pages - the garden, the tracks, the dials, playing, getting around, the places, play mode, the
 * playground, the tale, the Sound Lab, keeping your music, worlds and sleep, who made it (1.4). Each page a living picture and a few plain lines.
 *
 * It opens by itself on the very first start (the SDL program); later from the button map (X) - SELECT held
 * anywhere, or MENU > BUTTONS. LEFT/RIGHT (or L1/R1, or A) turn the pages, Y the button map, B closes.
 * Included by brick_stage.inc after the button map. */

typedef struct { const char *title, *l1, *l2, *l3, *keys; } bk_guide_page_t;
static const bk_guide_page_t BK_GUIDE[] = {
    {"Welcome to the dream garden", "Mutable Dreams makes music by itself, like a garden grows.",
     "You do not need to know music: you tend it, and it answers.", "This guide shows you how. It takes two minutes.",
     "RIGHT OR A: NEXT PAGE   B: CLOSE"},
    {"Six voices, six plants", "On the left grow six plants: the six voices of the music.",
     "They grow with their sound and bloom on every note.", "L1 and R1 choose a voice; A mutes or wakes it.",
     "L1 / R1: CHOOSE A VOICE   A: MUTE"},
    {"Four dials shape the dream", "FIREFLIES makes it denser, WIND moves it, GLOW colours it,",
     "MIST gives it space. They are on the right, in the garden.", "UP and DOWN choose a dial, LEFT and RIGHT turn it.",
     "UP/DOWN: A DIAL   LEFT/RIGHT: TURN IT"},
    {"Play, and travel between worlds", "START plays and stops. Every sound world is a Biome:",
     "there are 272 of them, in families, from bells to storms.", "R2 with LEFT/RIGHT changes the Biome; R2 with F2 shows them all.",
     "START: PLAY   R2 + LEFT/RIGHT: BIOME"},
    {"Never lost", "L2 tapped opens the quick bar: every place in a row.",
     "B always goes one step back, the way you came. B held: home.", "SELECT tapped tells you where you are; held, it shows the buttons.",
     "L2: ALL PLACES   B: BACK   SELECT: WHERE AM I"},
    {"The places", "The Sound Lab (F1), the Mixer (F2), the Atlas, the Sequence,", "the Dream Map, Modulation, Voice, Space, the Archive, Worlds",
     "and the Playground. Each one has its keys written at the bottom.", "MENU OR L2: GO SOMEWHERE"},
    {"Play it yourself", "In the quick bar, PLAY MODE turns the buttons into notes:",
     "the D-pad and A B X Y play eight tones that always fit.", "R2 adds a chord. SELECT ends it: the buttons steer again.",
     "L2, PLAY MODE   R2: CHORD"},
    {"The playground", "Ten toys that play with you: bouncing balls, planets, rain,",
     "pendulums, a firefly, rhythm wheels - and four little games.", "R2 + X keeps what a toy played as part of the song.",
     "L1 / R1: ANOTHER TOY   R2 + X: KEEP IT"},
    {"A dream tale", "A story in eight chapters is told while you play.",
     "Each chapter asks a small deed and leads you somewhere new.", "When it is done, its card rises. The garden shows where you are.",
     "IT STARTS IN THE PLAYGROUND"},
    {"The Sound Lab", "Here the sound itself can be bent: synths, Mutable Instruments",
     "modules, effects, and at the end MASTER - EQ, warmth, a limiter.", "L1 and R1 together (or F2) show every module at once: choose, A.",
     "F1: SOUND LAB   L1 + R1: ALL MODULES"},
    {"Keep your music", "R2 with SELECT, held a second, saves everything.",
     "Your own samples go into the samples folder on the card.", "Nothing you made is ever lost when Mutable Dreams is updated.",
     "R2 + SELECT HELD: SAVE"},
    {"Worlds, and a night's sleep", "WORLDS are whole scenes: a Biome, its tempo, its Sound Lab, a story.",
     "There are 96, in families; the LULLABIES are for falling asleep.", "OPTIONS has a SLEEP TIMER: the music fades and stops, the screen rests.",
     "L2: WORLDS, OPTIONS"},
    {"Made with", "Mutable Dreams is by Kid Bunka, free under the GPL 3.",
     "Its voices are Mutable Instruments' open modules by Emilie Gillet (MIT).", "Poems from LibriVox, free for all. SOURCE.TXT on the card says it all.",
     "THANK YOU FOR LISTENING"},
    {"Now dream", "Press START, turn a dial, and listen.", "Whenever you need it: SELECT held, then X, brings this guide back.",
     "Have a good dream.", "B: CLOSE THE GUIDE"},
};
#define BK_GUIDE_N ((int)(sizeof BK_GUIDE / sizeof BK_GUIDE[0]))
/* the same pages on a handheld without the Brick's MENU, F1, F2 and FN (RGB20S, Miyoo: MD_KEYS=handheld): there the
   quick bar (L2) leads everywhere, and these pages say so instead of naming keys that are not there */
static const struct { int page; bk_guide_page_t g; } BK_GUIDE_HH[] = {
    {3, {"Play, and travel between worlds", "START plays and stops. Every sound world is a Biome:",
         "there are 272 of them, in families, from bells to storms.", "R2 with LEFT/RIGHT changes the Biome; L2, ATLAS shows them all.",
         "START: PLAY   R2 + LEFT/RIGHT: BIOME"}},
    {5, {"The places", "The Sound Lab, the Mixer, the Atlas, the Sequence, the Dream Map,",
         "Modulation, Voice, Space, the Archive, Worlds and the Playground.", "L2 opens the quick bar: every place is a stone in it.",
         "L2: GO SOMEWHERE"}},
    {6, {"Play it yourself", "In the quick bar, PLAY MODE turns the buttons into notes:",
         "the D-pad and A B X Y play eight tones that always fit.", "R2 adds a chord. SELECT ends it: the buttons steer again.",
         "L2, PLAY MODE   R2: CHORD"}},
    {9, {"The Sound Lab", "Here the sound itself can be bent: synths, Mutable Instruments",
         "modules, effects, and at the end MASTER - EQ, warmth, a limiter.", "L1/R1 choose a module, UP/DOWN a setting, LEFT/RIGHT change it.",
         "L2, SOUND LAB   L1 + R1: ALL MODULES"}},
};

static float bk_guide_glide;                     /* (the page gliding in) */

/* a page's living picture, in the box (x, y, w, h) */
static void bk_guide_picture(int page, float t, int x, int y, int w, int h, uint32_t col)
{
    float cx = (float)(x + w / 2), cy = (float)(y + h / 2);
    switch (page) {
    case 0: {                                    /* a flower opening under the moon, fireflies round it */
        float o = 0.55f + 0.45f * sinf(t * 0.8f);
        bk_disc(cx + 170.0f, cy - 60.0f, 26.0f, ARGB(255, 240, 236, 210));
        bk_disc(cx + 170.0f, cy - 60.0f, 44.0f, ARGB(40, 240, 236, 210));
        bk_line(cx, cy + 90.0f, cx, cy + 10.0f, 3.0f, ARGB(255, 90, 150, 100));
        gd_flower(cx, cy, 46.0f, o, col, BK_INK);
        for (int k = 0; k < 9; k++) {
            float a = t * 0.5f + (float)k * 0.7f, r = 90.0f + 30.0f * sinf(t + (float)k);
            bk_disc(cx + cosf(a) * r, cy + sinf(a) * r * 0.6f, 2.5f, ARGB((uint32_t)(150 + 100 * sinf(t * 3.0f + (float)k)), 255, 236, 140));
        }
        break;
    }
    case 1:                                      /* six plants swaying, the chosen one framed */
        for (int i = 0; i < 6; i++) {
            float px = (float)x + 60.0f + (float)i * (float)(w - 120) / 5.0f, sway = 8.0f * sinf(t * 1.1f + (float)i);
            int sel = (int)(t / 1.5f) % 6 == i;
            uint32_t c = bk_track_col(i);
            if (sel)
                ty_blob(px, cy + 20.0f, 52.0f, BK_ALPHA(c, 60), t, i, 255);
            bk_line(px, cy + 90.0f, px + sway, cy - 10.0f, 3.0f, ARGB(255, 90, 150, 100));
            gd_flower(px + sway, cy - 14.0f, 20.0f + 6.0f * sinf(t * 2.0f + (float)i), 0.7f + 0.3f * sinf(t + (float)i), c, BK_INK);
            bk_text_c((int)px, (int)cy + 98, &FONT_S, 1, (const char *[]){"1", "2", "3", "4", "5", "6"}[i], sel ? BK_INK : BK_DIM);
        }
        break;
    case 2: {                                    /* four dials, the chosen one turning */
        static const char *const nm[4] = {"FIREFLIES", "WIND", "GLOW", "MIST"};
        int sel = (int)(t / 1.6f) % 4;
        for (int i = 0; i < 4; i++) {
            float dx = (float)x + 90.0f + (float)i * (float)(w - 180) / 3.0f, a = (i == sel ? t * 1.5f : (float)i) - 1.5708f;
            ty_orb(dx, cy, 40.0f, i == sel ? col : ARGB(255, 70, 80, 96), i == sel ? 0.6f : 0.0f, 255);
            bk_line(dx, cy, dx + cosf(a) * 30.0f, cy + sinf(a) * 30.0f, 3.0f, ARGB(255, 20, 24, 34));
            bk_text_c((int)dx, (int)cy + 54, &FONT_S, 1, nm[i], i == sel ? BK_INK : BK_DIM);
        }
        break;
    }
    case 3: {                                    /* the globe of Biomes turning, the play triangle breathing */
        float br = 0.5f + 0.5f * sinf(t * 2.0f);
        bk_icon(2, cx - 110.0f, cy, 70.0f, col, t);
        ty_orb(cx + 120.0f, cy, 50.0f, col, br, 255);
        for (int k = 0; k < 3; k++)
            bk_line(cx + 104.0f + (float)k * 4.0f, cy - 22.0f + (float)k * 3.0f, cx + 104.0f + (float)k * 4.0f, cy + 22.0f - (float)k * 3.0f, 4.0f, ARGB(255, 20, 18, 14));
        bk_line(cx + 104.0f, cy - 22.0f, cx + 140.0f, cy, 4.0f, ARGB(255, 20, 18, 14));
        bk_line(cx + 104.0f, cy + 22.0f, cx + 140.0f, cy, 4.0f, ARGB(255, 20, 18, 14));
        break;
    }
    case 4:                                      /* the quick bar's stones, one lifted, an arrow back */
        for (int i = 0; i < 7; i++) {
            int sel = (int)(t / 0.9f) % 7 == i;
            float sx = (float)x + 70.0f + (float)i * (float)(w - 140) / 6.0f;
            ty_blob(sx, cy - (sel ? 14.0f : 0.0f), sel ? 32.0f : 25.0f, sel ? col : ARGB(255, 40, 56, 60), t, i, 255);
            bk_icon((int[]){8, 1, 0, 2, 3, 4, 15}[i], sx, cy - (sel ? 14.0f : 0.0f), sel ? 24.0f : 19.0f, sel ? ARGB(255, 22, 28, 42) : bk_track_col(i % 6), t);
        }
        bk_text_c((int)cx, (int)cy + 62, &FONT_S, 1, "L2: THE QUICK BAR", BK_DIM);
        break;
    case 5:                                      /* the places' signs in two rows */
        for (int i = 0; i < 12; i++) {
            static const int ic[12] = {0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 15, 8};
            float px = (float)x + 70.0f + (float)(i % 6) * (float)(w - 140) / 5.0f, py = cy - 46.0f + (float)(i / 6) * 96.0f;
            float lift = 4.0f * sinf(t * 2.0f + (float)i);
            ty_blob(px, py + lift, 30.0f, ARGB(255, 36, 48, 56), t, i, 255);
            bk_icon(ic[i], px, py + lift, 24.0f, bk_track_col(i % 6), t);
        }
        break;
    case 6:                                      /* eight lily pads, lit in turn: the scale under the buttons */
        for (int i = 0; i < 8; i++) {
            static const char *const key[8] = {"<", "v", ">", "^", "Y", "X", "B", "A"};
            float px = (float)x + 50.0f + (float)i * (float)(w - 100) / 7.0f, f = (int)(t * 3.0f) % 8 == i ? 1.0f : 0.0f;
            ty_blob(px, cy + 10.0f - 8.0f * f, 30.0f, gd_mix(ARGB(255, 40, 92, 70), col, 0.2f + 0.7f * f), t, i, 255);
            bk_text_c((int)px, (int)cy - 2 - (int)(8.0f * f), &FONT_B, 1, key[i], f > 0.5f ? ARGB(255, 12, 16, 24) : BK_INK);
        }
        break;
    case 7: {                                    /* a ball bouncing on a pad, a planet circling */
        float hgt = fabsf(sinf(t * 2.6f)), pa = t * 1.2f;
        ty_blob(cx - 120.0f, cy + 70.0f, 40.0f, ARGB(255, 60, 120, 90), t, 2, 255);
        ty_orb(cx - 120.0f, cy + 30.0f - hgt * 110.0f, 16.0f, bk_track_col(2), 1.0f - hgt, 255);
        ty_orb(cx + 120.0f, cy, 22.0f, ARGB(255, 255, 206, 120), 0.5f, 255);
        bk_arc(cx + 120.0f, cy, 70.0f, 1.2f, 0, 6.2832f, ARGB(60, 255, 255, 255));
        ty_orb(cx + 120.0f + cosf(pa) * 70.0f, cy + sinf(pa) * 70.0f, 10.0f, bk_track_col(4), 0.3f, 255);
        break;
    }
    case 8: {                                    /* a little chapter card, rising */
        float r = 6.0f * sinf(t * 1.2f);
        bk_rrect((int)cx - 150, (int)(cy - 70.0f + r), 300, 140, 18, ARGB(240, 10, 12, 26));
        bk_rframe((int)cx - 150, (int)(cy - 70.0f + r), 300, 140, 18, 2, col);
        gd_sprig(cx - 144.0f, cy + 64.0f + r, 1, -1, col);
        gd_sprig(cx + 144.0f, cy + 64.0f + r, -1, -1, col);
        bk_text_c((int)cx, (int)(cy - 54.0f + r), BK_TINY, 1, "CHAPTER I", col);
        bk_text_c((int)cx, (int)(cy - 30.0f + r), &FONT_B, 1, "The Sleeping Garden", BK_INK);
        break;
    }
    case 9:                                      /* the flask bubbling, the master chain as stones */
        bk_icon(0, cx - 160.0f, cy, 70.0f, col, t);
        for (int i = 0; i < 4; i++) {
            static const char *const nm[4] = {"EQ", "WARMTH", "GLUE", "LIMIT"};
            float sx = cx - 40.0f + (float)i * 76.0f;
            ty_blob(sx, cy, 30.0f, bk_track_col(i + 1), t, i, 255);
            bk_text_c((int)sx, (int)cy + 38, BK_TINY, 1, nm[i], BK_DIM);
            if (i)
                bk_line(sx - 46.0f, cy, sx - 30.0f, cy, 2.0f, ARGB(120, 255, 255, 255));
        }
        break;
    case 10:                                     /* a seedling growing out of a seed: kept */
        gd_seedling(cx, cy + 20.0f, col);
        ty_orb(cx, cy + 50.0f, 24.0f, ARGB(255, 150, 110, 70), 0.2f, 255);
        bk_text_c((int)cx, (int)cy + 80, &FONT_S, 1, "R2 + SELECT", BK_DIM);
        break;
    case 11: {                                   /* (1.4) a sleeping moon over a world's island, its z's rising */
        for (int k = 0; k < 5; k++)              /* the island: a mound in layers, its water rings */
            bk_disc(cx, cy + 70.0f + (float)k * 4.0f, 80.0f - (float)k * 12.0f, ARGB((uint32_t)(30 + 18 * k), 60, 90, 80));
        for (int k = 0; k < 3; k++) {
            float ph = fmodf(t * 0.2f + (float)k / 3.0f, 1.0f);
            bk_arc(cx, cy + 40.0f, 90.0f + ph * 60.0f, 1.4f, 2.0f, 4.28f, ARGB((uint32_t)(90 * (1.0f - ph)), 140, 190, 200));
        }
        gd_seedling(cx - 30.0f, cy + 30.0f, col);
        gd_seedling(cx + 34.0f, cy + 36.0f, bk_track_col(3));
        bk_disc(cx + 120.0f, cy - 40.0f, 30.0f, ARGB(230, 240, 232, 200));
        bk_disc(cx + 132.0f, cy - 50.0f, 27.0f, ARGB(255, 10, 14, 22));
        for (int k = 0; k < 3; k++) {
            float ph = fmodf(t * 0.35f + (float)k / 3.0f, 1.0f);
            bk_text((int)(cx + 150.0f + ph * 40.0f), (int)(cy - 70.0f - ph * 60.0f), &FONT_S, 1, "z", ARGB((uint32_t)(220 * (1.0f - ph)), 230, 226, 205));
        }
        break;
    }
    case 12: {                                   /* (1.4) who made it: six little lights, one per voice, round a heart of light */
        ty_orb(cx, cy, 30.0f, ARGB(255, 255, 214, 160), 0.6f + 0.4f * sinf(t * 1.3f), 255);
        for (int k = 0; k < 6; k++) {
            float a = t * 0.3f + (float)k * 1.0472f;
            ty_blob(cx + cosf(a) * 110.0f, cy + sinf(a) * 60.0f, 14.0f, bk_track_col(k), t, k, 255);
        }
        break;
    }
    default: {                                   /* the moon and a ring of fireflies */
        ty_orb(cx, cy, 44.0f, ARGB(255, 240, 236, 210), 0.5f + 0.5f * sinf(t), 255);
        for (int k = 0; k < 14; k++) {
            float a = t * 0.4f + (float)k * 0.449f;
            bk_disc(cx + cosf(a) * 100.0f, cy + sinf(a) * 70.0f, 3.0f, ARGB((uint32_t)(140 + 110 * sinf(t * 3.0f + (float)k)), 255, 236, 140));
        }
        break;
    }
    }
}

static void bk_draw_guide(float t)
{
    uint32_t col = bk_track_col(dream_ui.track);
    const bk_guide_page_t *g = &BK_GUIDE[bk_guide_page];
    if (bk_handheld)                             /* (a handheld: its own words where the Brick's keys are missing) */
        for (unsigned k = 0; k < sizeof BK_GUIDE_HH / sizeof BK_GUIDE_HH[0]; k++)
            if (BK_GUIDE_HH[k].page == bk_guide_page)
                g = &BK_GUIDE_HH[k].g;
    int x = 90, y = 70, w = BK_W - 180;
    float slide;
    char buf[32];
    if (fabsf((float)bk_guide_page - bk_guide_glide) > 1.0f)   /* (opened again, or a jump: one page's glide, never from afar) */
        bk_guide_glide = (float)bk_guide_page + ((float)bk_guide_page > bk_guide_glide ? -1.0f : 1.0f);
    bk_guide_glide += ((float)bk_guide_page - bk_guide_glide) * bk_glide(10.0f);
    slide = bk_calm ? 0.0f : ((float)bk_guide_page - bk_guide_glide) * 60.0f;   /* (the new page glides in from the side) */
    bk_rect(0, 0, BK_W, BK_H, ARGB(255, 4, 7, 14));   /* (opaque: nothing of the stage shows through, and cheaper) */
    bk_vines_at(t, col, 40.0f, 30.0f, (float)(BK_W - 40), (float)(BK_H - 60));
    gd_seedling(70, 52, col);
    bk_text(90, 36, BK_TINY, 1, "THE GUIDE", BK_ALPHA(col, 220));
    snprintf(buf, sizeof buf, "%d / %d", bk_guide_page + 1, BK_GUIDE_N);
    bk_text(BK_W - 140, 36, BK_TINY, 1, buf, BK_DIM);
    bk_guide_picture(bk_guide_page, t, x + (int)slide, y + 10, w, 300, col);
    bk_text_c(BK_W / 2 + (int)slide, y + 330, &FONT_M, 1, g->title, BK_INK);
    bk_text_c(BK_W / 2 + (int)slide, y + 392, &FONT_S, 1, g->l1, BK_DIM);
    bk_text_c(BK_W / 2 + (int)slide, y + 422, &FONT_S, 1, g->l2, BK_DIM);
    bk_text_c(BK_W / 2 + (int)slide, y + 452, &FONT_S, 1, g->l3, BK_DIM);
    {   /* what to press here, in a pebble */
        int tw = bk_text_w(BK_TINY, g->keys, 1);
        ty_blob((float)(BK_W / 2 - tw / 2 - 34), (float)(y + 511), 12.0f, col, t, 3, 220);
        bk_rrect(BK_W / 2 - tw / 2 - 18, y + 496, tw + 36, 30, 15, BK_ALPHA(col, 50));
        bk_text_c(BK_W / 2, y + 503, BK_TINY, 1, g->keys, col);
    }
    for (int i = 0; i < BK_GUIDE_N; i++)         /* the pages, as seeds */
        bk_disc((float)(BK_W / 2) - (float)(BK_GUIDE_N - 1) * 9.0f + (float)i * 18.0f, (float)(BK_H - 82), i == bk_guide_page ? 5.0f : 3.0f,
                i == bk_guide_page ? col : ARGB(90, 200, 255, 210));
    if (bk_keyboard)                             /* (0.78) at a PC keyboard: which key is which button, on every page */
    {
        char l1[160], l2[160], k[8][20];          /* (0.80.1: the keys keys.txt binds, not always the defaults) */
        static const char *const b1[8] = {"A", "B", "X", "Y", "L1", "R1", "L2", "R2"};
        for (int q = 0; q < 8; q++)
            snprintf(k[q], sizeof k[q], "%s", bk_key_label(b1[q]));
        snprintf(l1, sizeof l1, "%s  A = %s   B = %s   X = %s   Y = %s   L1/R1 = %s/%s   L2 = %s", T("ON THE KEYBOARD:"), k[0], k[1], k[2], k[3], k[4], k[5], k[6]);
        snprintf(k[0], sizeof k[0], "%s", bk_key_label("START"));
        snprintf(k[1], sizeof k[1], "%s", bk_key_label("SELECT"));
        snprintf(k[2], sizeof k[2], "%s", bk_key_label("MENU"));
        snprintf(k[3], sizeof k[3], "%s", bk_key_label("FN"));
        snprintf(l2, sizeof l2, "R2 = %s   START = %s   SELECT = %s   MENU = %s   FN = %s", k[7], k[0], k[1], k[2], k[3]);
        bk_tr_keys = 1;                          /* (names of keys: as they are) */
        bk_text_c(BK_W / 2, BK_H - 144, BK_TINY, 1, l1, ARGB(255, 200, 220, 255));
        bk_text_c(BK_W / 2, BK_H - 124, BK_TINY, 1, l2, ARGB(255, 200, 220, 255));
        bk_tr_keys = 0;
    }
    if (bk_keyboard) {                           /* (0.80.1: with the keys bound) */
        char l[160], ka[20], ky[20];
        snprintf(ka, sizeof ka, "%s", bk_key_label("A"));
        snprintf(ky, sizeof ky, "%s", bk_key_label("Y"));
        snprintf(l, sizeof l, T("LEFT/RIGHT OR %s: TURN THE PAGE   %s: THE BUTTON MAP   %s: CLOSE"), ka, ky, bk_key_label("B"));
        bk_text_c(BK_W / 2, BK_H - 56, BK_TINY, 1, l, BK_FAINT);
    } else
        bk_text_c(BK_W / 2, BK_H - 56, BK_TINY, 1, "LEFT/RIGHT OR A: TURN THE PAGE   Y: THE BUTTON MAP   B: CLOSE", BK_FAINT);
}

/* the guide's keys (brick_core.c sends them here while it is open) */
static void bk_guide_press(int b)
{
    if (b == BK_RIGHT || b == BK_R1 || b == BK_A) {
        if (bk_guide_page + 1 < BK_GUIDE_N)
            bk_guide_page++;
        else if (b == BK_A)
            bk_guide_open = 0;                   /* (A on the last page: done) */
    } else if (b == BK_LEFT || b == BK_L1) {
        if (bk_guide_page > 0)
            bk_guide_page--;
    } else if (b == BK_Y) {                      /* the button map instead */
        bk_guide_open = 0;
        bk_help_open = 1;
        bk_help_page = 0;
    } else if (b == BK_B || b == BK_MENU || b == BK_SELECT)
        bk_guide_open = 0;
}
