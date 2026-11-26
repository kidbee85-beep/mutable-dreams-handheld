/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka.
 * The core every front end (SDL2 on the Brick and the Mac, the headless test runner) drives: the complete
 * Mutable Dreams 2.1 firmware (app, engine, all pages) through fm1-base's browser emulator layer, the Brick's
 * buttons mapped onto the FM-1 panel, and the 1024 x 768 stage around the FM-1 screen. Single-threaded: a front end
 * calls everything from one thread and pushes the rendered audio into its own queue. */
#pragma once
#include <stdint.h>

#define BK_W 1024
#define BK_H 768
#define BK_RATE 44100

/* the Brick's controls (bit numbers of bk_input's mask) */
enum {
    BK_UP, BK_DOWN, BK_LEFT, BK_RIGHT, BK_A, BK_B, BK_X, BK_Y,
    BK_L1, BK_R1, BK_L2, BK_R2, BK_START, BK_SELECT, BK_MENU, BK_F1, BK_F2, BK_FN, BK_NBUTTONS
};

/* save_dir: where the saved objects live (created by the front end); 0 = nothing kept */
void bk_init(const char *save_dir);
/* the held Brick controls (bit BK_*); FN is the latching switch's position. Call on every change and at least once
 * per frame (repeats and timed holds run from bk_render's clock) */
void bk_input(uint32_t held);
/* Brick Pro: the sticks, -32768..32767 each (0 at rest) */
void bk_sticks(int lx, int ly, int rx, int ry);
int bk_touch_overlay(void);                       /* (1.6) a menu, options, quick bar, help, guide or journal is open */
void bk_touch_event(int phase, int id, float x, float y);   /* a finger on the stage: 0 down, 1 move, 2 up; id a slot 0..9 */
typedef struct { int8_t btn; char label[26]; } bk_chip;   /* (1.6) an action of the place you are in, named, for a touch button */
int bk_touch_chips(bk_chip *out, int max);        /* what the hint line names, as touch buttons (the stage's thread makes them) */
void bk_touch_cmd(int cmd);                       /* 0 home, 1 options, 2 help (the touch dock's own things) */
const char *bk_touch_text(const char *en);        /* a label in the stage's language */
int bk_touch_state(int what);                     /* 0 playing, 1 FN (play mode) latched in the core, 2 R2 held */
void bk_touch_tick(uint32_t now);                 /* (held fingers: the help) */
void bk_touch_tap(int x, int y);                  /* a tap at a point of the stage (0..BK_W, 0..BK_H) */
/* n frames of stereo float into lr (interleaved), running the device as long as that takes */
void bk_render(float *lr, uint32_t n);
/* the stage as 0xAARRGGBB pixels, BK_W x BK_H, drawn now */
const uint32_t *bk_draw(void);
/* a front end with a sound thread: fn is called between the stage's parts while it draws (it lets go of the lock the
 * sound thread needs and takes it again), so a long frame never holds up the sound */
void bk_set_draw_yield(void (*fn)(void));
/* the animated start screen: on shows it now (until a button, or 10 s; then a 0.7 s fade); active while shown */
void bk_splash(int on);
int bk_splash_active(void);
/* the garden: the Brick's own home screen in place of the FM-1's on the LIVE page; 1 while it is shown */
int bk_garden_visible(void);
/* VOICES: load files at once instead of in a thread (the test runner); the loaded file's sentences (0 not yet, -1 failed) */
void bk_sync_loading(int on);
/* the button map: shown once on the very first start (the SDL program turns this on); open: its page + 1, else 0 */
void bk_help_first_run(int on);
int bk_help_shown(void);
int bk_play_mode(void);
void bk_set_handheld(int on);
void bk_set_side_half(int on);                   /* (1.5.8) light picture: the columns around the stage every other frame */
void bk_set_light(int on);               /* the light picture: less decoration, for slow handhelds (quick bar: LIGHT);
                                           2 = switched on by itself (it says so) */
int bk_light_on(void);
void bk_set_calm(int on);
void bk_set_small(int on);
int bk_set_lang(int on);
int bk_option_set(const char *name, int value);          /* (1.6) the tests: "pscale", "clock", "listen", "bigtext", "contrast"; "hour" fixes the clock (x10 = tenths), "listenms" the wait */
int bk_option_get(const char *name);
int bk_coach_step(void);                                 /* (1.6, the tests) the first five minutes: step 0..4, 5 done */
void bk_coach_set(int step);
void bk_tale_set(int chapter);                           /* (1.6, the tests) the tale begins that chapter (0-based) */
int bk_note_for_degree(int degree);                      /* (1.6) the note play mode gives a button's degree 0..7 */
int bk_journal_count(int worlds);                        /* (1.6) Biomes (0) or worlds (1) in the garden journal */
int bk_set_lang_code(const char *code);          /* (1.6) "en", "de", "fr", "es": the table's count, 0 English, -1 none found */
const char *bk_lang_code(void);                         /* (0.80) 1 Deutsch, 0 English: its table's size, -1 none found */
int bk_lang_on(void);
void bk_lang_default(int on);
int bk_options_shown(void);                      /* (1.1) OPTIONS open (tests) */
void bk_options_show(int on);
int bk_button_part(int phys);                    /* (1.1) the part a physical button plays (tests) */                    /* (0.80) the system language, before the saved choice is read */
void bk_say(const char *a, const char *b);
void bk_set_key_label(const char *brick, const char *key);   /* (0.80) the keyboard key bound to a Brick button, for the hints */       /* (0.80) a message over the stage (USB AUDIO, ...) */
void bk_set_keyboard(int on);         /* a PC without a controller: hints name keyboard keys (0.78) */            /* the stage is shown at 640 x 480: small labels one size up (0.76) */             /* CALM MOTION (0.75) */
int bk_calm_on(void);
uint32_t bk_toy_notes(void);
int bk_tale_chapter(void);
int bk_tale_card(void);
void bk_prefs_reload(void);           /* as at a new start: calm, light, the toy read again from save/ (tests) */
int bk_toy_index(void);               /* the chapter card up (-1 none) (tests) */
int bk_guide_shown(void);             /* THE GUIDE: its page 1.., 0 closed (tests) */
void bk_guide_show(int on);
int bk_macro(int k);
uint32_t bk_gfx_notes(void);
void bk_garden_fx(int mode);          /* force the musical aurora (1) or rain (2), 0 as the Biome says (tests) */          /* notes the garden's musical aurora / rain answered (tests) */                   /* the garden's four dials 0..1000: FIREFLIES, WIND, GLOW, MIST (tests) */            /* DREAM TALES: the chapter under way (tests) */
int bk_seq_notes(int track);           /* the notes of a track's sequence, -1 when it is off (tests) */
uint32_t bk_seq_print(int track);     /* a fingerprint of a track's whole sequence (tests) */          /* how many notes the PLAYGROUND has played (tests) */
void bk_set_no_touch(int on);                    /* a device without touch: OPTIONS without the touch rows */
void bk_set_device(const char *name);            /* the handheld's name for title and start screen (default TRIMUI BRICK) */                    /* no MENU, F1, F2, FN keys: hints and help point to the quick bar */                          /* play mode on (the FN switch, or latched from the quick bar) */
int bk_menu_shown(void);                         /* the jump menu open */
void bk_list_biomes(void);
int bk_voices_ready(void);
/* ARCHIVE: catalogue entries, whether entry i is fetched (1/0, -1 none), a fetch running, the last message */
int bk_archive_count(void);
int bk_archive_have(int i);
int bk_archive_busy(void);
const char *bk_archive_status(void);
/* LOGIC: its rule and the last steps it played, as text (the test runner) */
const char *bk_logic_summary(void);
/* WORLDS: how many, the one entered (-1 none), a mistake found in worlds.txt */
int bk_world_count(void);
int bk_world_current(void);
const char *bk_world_error(void);
void bk_world_enter(int i);
const char *bk_world_name(int i);
int bk_world_biome(int i);
/* the FM-1 screen alone (240 x 240 RGB565 as the panel takes it, byte-swapped) */
const uint16_t *bk_fm1_screen(void);
/* 1: the player chose QUIT in the menu */
int bk_quit_requested(void);
int bk_resting_now(void);                        /* (1.4) the sleep timer ran out: the screen rests, 4 pictures a second */
int bk_sleep_left_min(void);
void bk_sleep_in(uint32_t ms);                   /* (the tests: the timer runs out in ms) */
/* keep what is unsaved in the store (front end: before exit) */
void bk_shutdown(void);
/* status for tests and the front end's title */
const char *bk_page_name(void);
const char *bk_where(void);           /* the place: GARDEN, SOUND LAB, a view or an FM-1 page (SELECT tapped says it) */
const char *bk_biome_name(void);
int bk_playing(void);
uint32_t bk_ms(void);
/* (studio) load a Biome by its number (0..bk_biome_count()-1), as the atlas does */
void bk_scene(int biome_id);
int bk_biome_count(void);
/* (studio) the transport: 1 play, 0 stop */
void bk_play(int on);
/* the sound's load: % of the real-time budget the last block took */
uint32_t bk_cpu_pct(void);
/* the Sound Lab by name, "MODULE.PARAMETER" ("PARTICLES.MIX", "SENDS.TRACK 1 > CLOUD"): 0 = set, -1 = no such */
int bk_lab_set(const char *module_dot_name, float value);
/* its value now; NAN = no such parameter */
float bk_lab_get(const char *module_dot_name);
/* MIDI from a device: one whole message (1..3 bytes; system exclusive not passed). Notes on POLY's MIDI channel
 * play POLY (while POLY sounds); everything else goes to the Mutable Dreams engine as if from the FM-1's USB */
void bk_midi_in(const uint8_t *msg, int len);
/* MIDI the engine sends (to USB on the FM-1): up to max bytes into out, whole messages; the count */
int bk_midi_out(uint8_t *out, int max);
void bk_midi_lost(void);              /* a MIDI device unplugged: release all it held (engine, POLY, pedal) */
/* the front end's devices, for the stage: how many, the first's name */
void bk_midi_devices(int count, const char *first_name);
/* for tests: POLY's sounding voices (held: gate open), a track's hits so far */
int bk_poly_voices(int held_only);
uint32_t bk_track_hits(int track);
/* for tests: a track's mixer value ("level", "pan", "send", "delay", "reverb", "mute"); -1 none */
int bk_track_value(int track, const char *what);
/* battery 0..100 or -1 (front end supplies it: /sys on the Brick) */
void bk_battery(int percent);
