/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the full views of the stage's centre, in place of the FM-1 screen.
 *
 *   MIXER     six channel strips: fader (LEVEL), PAN, FX SEND, DELAY, REVERB as dials, mute, level meters
 *             LEFT/RIGHT track · UP/DOWN the value · L1/R1 the row · A mute · B (or F2) back
 *   ATLAS     every Biome as a tile, by family: L1/R1 family · D-pad the tile · A load (while playing: A again to
 *             change now, B to keep) · B (or R2+F2) back
 *   SEQUENCE  the six tracks' steps (sequences, generated rhythms, the kit) with their playheads:
 *             UP/DOWN track · A mute · X edit the track · B back. Editing (the track a piano roll):
 *             LEFT/RIGHT step · UP/DOWN the field's value (NOTE moves by the Biome's scale) · L1/R1 field (note,
 *             velocity, chance, gate, length) · A step on / off · Y generate (the next of the eight generators) ·
 *             X or B done. The kit track: UP/DOWN pad, A its hit on the step. With R2 held, the tools (the engine's
 *             own): L1 copy · R1 paste · UP/DOWN transpose a semitone · LEFT/RIGHT an octave · A mirror the phrase ·
 *             Y step or phrase · X queue a fill. The engine's answer (copied, pasted, at the note limit...) is shown
 *   DREAM MAP an Itinera journey as a map: its stations (the Biomes it will pass, in the engine's own order) in four
 *             rows, one per chapter with its line of the story; the traveller between them while it dreams.
 *             L1/R1 journey · UP/DOWN setting (length, pace, room) · LEFT/RIGHT its value · A dream / wake ·
 *             B back (the journey's table: build/gen/itinera_map.h, read from the 2.1 sources)
 *   MODULATION each track's LFO A and LFO B as curves (their own shapes, at their own period; the phase is drawn,
 *             not read from the engine), the four living links with their live values, the four macros and the
 *             Biome drift. UP/DOWN track · L1/R1 field (A/B target, shape, depth, rate) · LEFT/RIGHT its value ·
 *             X the links: UP/DOWN link · L1/R1 field (source, signal, destination, target, depth, release) ·
 *             LEFT/RIGHT its value · A on (its last depth, else 50 %) / off · X the drift: L1 capture scene A,
 *             R1 scene B (with the drift off), UP/DOWN field (mode off / manual / auto, position, cycle, depth),
 *             LEFT/RIGHT its value · X back to the LFOs · B back
 * The engine's own commands (dream_cmd) change the sound, exactly as the FM-1's pages do; nothing new is stored. */

#include "itinera_map.h"
enum { V_NONE, V_MIXER, V_ATLAS, V_SEQ, V_DREAM, V_MOD, V_VOICE, V_SPACE, V_ARCHIVE, V_WORLDS, V_TOYS, V_COUNT };
static void ty_open(void);                       /* the PLAYGROUND (brick_toys.inc, at the end) */
static void ty_key(int b, int times);
static void ty_tick(void);
static void st_tick(void);                      /* DREAM TALES (brick_story.inc, at the end) */
#include "brick_archive.inc"
#include "brick_worlds.inc"
enum { MF_A_TARGET, MF_A_SHAPE, MF_A_DEPTH, MF_A_RATE, MF_B_TARGET, MF_B_SHAPE, MF_B_DEPTH, MF_B_RATE, MF_COUNT };
static int mf_field = MF_A_DEPTH;
static int mf_pond, mf_ab;                      /* the firefly pond (Y): the LFOs as fireflies; mf_ab: A or B chosen */
enum { SF_NOTE, SF_VELOCITY, SF_CHANCE, SF_GATE, SF_RULE, SF_LENGTH, SF_COUNT };
static int sq_edit, sq_field, sq_last_note[6], sq_phrase;     /* sq_phrase: the tools act on the whole phrase */
static const char *const SQ_RULES[4] = {"ALWAYS", "1ST", "2ND", "FILL"};
#define SQ_SHIFT 32                              /* a key with R2 held, in sq_key */
enum { LF_SOURCE, LF_SIGNAL, LF_DEST, LF_TARGET, LF_DEPTH, LF_RELEASE, LF_COUNT };
static int lk_mode, lk_slot, lk_field = LF_DEPTH, lk_last_depth[4];   /* lk_mode: 0 LFOs, 1 links, 2 drift, 3 macros */
static int mc_sel;                               /* the macro being edited */
static const int LK_NEXT[4] = {1, 3, 0, 2};       /* X: LFOs > links > macros > drift > LFOs */
enum { DF_MODE, DF_POSITION, DF_CYCLE, DF_DEPTH, DF_COUNT };
static int df_field = DF_MODE;
static const int DF_CMD[DF_COUNT] = {MD_DRIFT_MODE, MD_DRIFT_POSITION, MD_DRIFT_RATE, MD_DRIFT_DEPTH};
static const char *const DF_NAMES[DF_COUNT] = {"MODE", "POSITION", "CYCLE", "DEPTH"};
static const char *const DF_MODES[3] = {"OFF", "MANUAL", "AUTO"};
static const int LF_CMD[LF_COUNT] = {MD_LINK_SOURCE, MD_LINK_SIGNAL, MD_LINK_DEST, MD_LINK_TARGET, MD_LINK_DEPTH, MD_LINK_RELEASE};
static const int LF_MIN[LF_COUNT] = {0, 0, 0, 0, -1000, 0}, LF_MAX[LF_COUNT] = {5, 3, 5, 5, 1000, 1000};
static const char *const LF_NAMES[LF_COUNT] = {"SOURCE", "SIGNAL", "DESTINATION", "TARGET", "DEPTH", "RELEASE"};
static const char *const SF_NAMES[SF_COUNT] = {"NOTE", "VELOCITY", "CHANCE", "GATE", "RULE", "LENGTH"};
static const int MF_CMD[MF_COUNT] = {MD_LFO_TARGET, MD_LFO_SHAPE, MD_TIDES, MD_TIDES_RATE, MD_LFO2_TARGET, MD_LFO2_SHAPE,
                                     MD_LFO2_DEPTH, MD_LFO2_RATE};
static const int MF_MAX[MF_COUNT] = {6, 3, 1000, 1000, 6, 3, 1000, 1000};
static int dm_fly;                               /* free flight (Y): the wisp is yours */
static void dm_fly_key(int b, int times);
static void dm_tick(void);
static int dm_row = 1;                           /* the focused setting: 1 length, 2 pace, 3 room (radio_adjust's knobs) */
static int bk_view, mx_row;
enum { MX_LEVEL, MX_PAN, MX_SEND, MX_DELAY, MX_REVERB, MX_ROWS };
static const char *const MX_NAMES[MX_ROWS] = {"LEVEL", "PAN", "FX SEND", "DELAY", "REVERB"};
static const int MX_CMD[MX_ROWS] = {MD_LEVEL, MD_PAN, MD_SEND, MD_DELAY_SEND, MD_REVERB_SEND};

/* the atlas: each family's Biomes in the engine's own browse order */
#define AT_MAX_FAM 24
static int at_n[AT_MAX_FAM], at_ids[AT_MAX_FAM][MD_BIOMES], at_nfam = -1, at_fam, at_pos, at_top;
static int at_col = 1;                          /* the D-pad's side: 0 = families, 1 = Biome tiles */
static uint32_t at_rng = 0x9e3779b9u;

static void at_build(void)
{
    int pos, f;
    at_nfam = (int)md_biome_family_count();
    if (at_nfam > AT_MAX_FAM)
        at_nfam = AT_MAX_FAM;
    memset(at_n, 0, sizeof at_n);
    for (pos = 0; pos < MD_BIOMES; pos++) {
        int id = md_biome_at((unsigned)pos);
        if (id < 0)
            continue;
        f = md_biome_family_index(id);
        if (f >= 0 && f < at_nfam)
            at_ids[f][at_n[f]++] = id;
    }
}
static void at_focus_current(void)
{
    int id = md_state()->biome_id, f = md_biome_family_index(id);
    if (at_nfam < 0)
        at_build();
    if (f < 0 || f >= at_nfam)
        return;
    at_fam = f;
    for (int i = 0; i < at_n[f]; i++)
        if (at_ids[f][i] == id)
            at_pos = i;
}

static int mx_value(int t, int row)
{
    const md_patch *p = md_state();
    switch (row) {
    case MX_LEVEL: return p->track[t].level;
    case MX_PAN: return p->track[t].pan;
    case MX_SEND: return p->track[t].send;
    case MX_DELAY: return p->ext[t].delay_send;
    default: return p->ext[t].reverb_send;
    }
}

static int mf_value(int t, int f)
{
    const md_patch *p = md_state();
    switch (f) {
    case MF_A_TARGET: return p->motion.target[t];
    case MF_A_SHAPE: return p->motion.wave[t];
    case MF_A_DEPTH: return p->ext[t].tides;
    case MF_A_RATE: return p->ext[t].tides_rate;
    case MF_B_TARGET: return p->motion.target2[t];
    case MF_B_SHAPE: return p->motion.wave2[t];
    case MF_B_DEPTH: return p->motion.depth2[t];
    default: return p->motion.rate2[t];
    }
}

static int lk_value(int j, int f)
{
    const md_link *l = &md_state()->links[j & 3];
    switch (f) {
    case LF_SOURCE: return l->source;
    case LF_SIGNAL: return l->signal;
    case LF_DEST: return l->destination;
    case LF_TARGET: return l->target;
    case LF_DEPTH: return l->depth;
    default: return l->release;
    }
}

/* VOICE: one track's whole sound in five panels (SOUND, FILTER, ENVELOPE, RHYTHM, PLAY), four fields each;
 * L1/R1 track, UP/DOWN field, LEFT/RIGHT value, X next panel, A held: listen (the track's note) */
enum { VF_PRESET, VF_MODEL, VF_TONE, VF_HARM, VF_CUTOFF, VF_RES, VF_CHAR, VF_BREATHE, VF_ATTACK, VF_DECAY, VF_SHAPE,
       VF_LEVEL, VF_HITS, VF_LENGTH, VF_ROTATE, VF_CHANCE, VF_NOTE, VF_ACCENT, VF_REPEATS, VF_DIV, VF_COUNT };
static const int VF_CMD[VF_COUNT] = {MD_TONE_PRESET, MD_MODEL, MD_TONE, MD_HARM, MD_CUTOFF, MD_RESONANCE, MD_FILTER,
                                     MD_BREATHE, MD_ATTACK, MD_DECAY, MD_SHAPE, MD_LEVEL, MD_HITS, MD_LENGTH, MD_ROTATE,
                                     MD_PROB, MD_NOTE, MD_ACCENT, MD_RATCHET, MD_SPEED};
static const char *const VF_NAMES[VF_COUNT] = {"PRESET", "MODEL", "TONE", "HARMONICS", "CUTOFF", "RESONANCE",
                                               "CHARACTER", "BREATHE", "ATTACK", "DECAY", "SHAPE", "LEVEL", "HITS",
                                               "LENGTH", "ROTATION", "CHANCE", "NOTE", "ACCENT", "REPEATS", "CLOCK DIV"};
static const char *const VF_KEYS[VF_COUNT] = {"preset", "model", "tone", "harm", "cutoff", "res", "char", "breathe",
                                              "attack", "decay", "shape", "level", "hits", "length", "rotate", "chance",
                                              "note", "accent", "repeats", "div"};
static int vf_field, vo_listen_track = -1, vo_listen_note;

static int vf_value(int t, int f)
{
    const md_patch *p = md_state();
    const md_track *k = &p->track[t];
    switch (f) {
    case VF_PRESET: return p->tone_preset[t];
    case VF_MODEL: return k->model;
    case VF_TONE: return k->tone;
    case VF_HARM: return k->harm;
    case VF_CUTOFF: return k->cutoff;
    case VF_RES: return k->resonance;
    case VF_CHAR: return k->filter;
    case VF_BREATHE: return k->breathe;
    case VF_ATTACK: return k->attack;
    case VF_DECAY: return k->decay;
    case VF_SHAPE: return k->shape;
    case VF_LEVEL: return k->level;
    case VF_HITS: return k->hits;
    case VF_LENGTH: return k->length;
    case VF_ROTATE: return k->rotate;
    case VF_CHANCE: return k->prob;
    case VF_NOTE: return k->note;
    case VF_ACCENT: return k->accent;
    case VF_REPEATS: return k->ratchet;
    default: return k->speed;
    }
}

static void vf_range(int t, int f, int *lo, int *hi, int *step)
{
    const md_track *k = &md_state()->track[t];
    *lo = 0, *hi = 1000, *step = 20;
    switch (f) {
    case VF_PRESET: *hi = MD_TONE_PRESETS - 1, *step = 1; break;
    case VF_MODEL: *hi = MD_MODELS - 1, *step = 1; break;
    case VF_CHAR: *hi = 3, *step = 1; break;
    case VF_HITS: *hi = k->length, *step = 1; break;
    case VF_LENGTH: *lo = 1, *hi = 32, *step = 1; break;
    case VF_ROTATE: *hi = k->length - 1, *step = 1; break;
    case VF_NOTE: *lo = 24, *hi = 96, *step = 1; break;
    case VF_REPEATS: *lo = 1, *hi = 4, *step = 1; break;
    case VF_DIV: *lo = 1, *hi = 8, *step = 1; break;
    }
}

static void vo_turn(int t, int d, int times)
{
    int lo, hi, step, v = vf_value(t, vf_field);
    vf_range(t, vf_field, &lo, &hi, &step);
    if (vf_field == VF_PRESET && v < 0)          /* an edited sound: the list starts at the first preset */
        v = d > 0 ? -1 : 1;
    v = clamp(v + d * step * (step > 1 ? times : 1), lo, hi);
    dream_cmd(VF_CMD[vf_field], t, v);
    if (vf_field == VF_PRESET)
        bk_notice(md_tone_name(v), "");
}

static void vo_listen(int t, int on)
{
    if (on) {
        vo_listen_track = t;
        vo_listen_note = md_state()->track[t].note;
        dream_cmd(MD_TRIGGER, t, vo_listen_note);
    } else if (vo_listen_track >= 0) {
        dream_cmd(MD_NOTE_OFF, vo_listen_track, vo_listen_note);
        vo_listen_track = -1;
    }
}

/* SPACE: the Biome's shared effects in four panels (ECHO, GRAIN, TAPE, DUCK), four fields each;
 * UP/DOWN field, LEFT/RIGHT value, X or R1 next panel, L1 the one before, A freeze on/off */
enum { SF2_ECHO_TIME, SF2_ECHO_FB, SF2_ROOM, SF2_RETURN, SF2_SPACE, SF2_SIZE, SF2_POSITION, SF2_GRAIN_FB, SF2_PITCH,
       SF2_REVERSE, SF2_FREEZE, SF2_NOISE, SF2_DUCK_FROM, SF2_DUCK, SF2_DUCK_RELEASE, SF2_SWING, SF2_COUNT };
static const struct { int cmd, lo, hi, step; const char *name, *key; } SP_F[SF2_COUNT] = {
    {MD_DELAY_TIME, 40, 350, 5, "ECHO TIME", "echo_time"},  {MD_DELAY_FB, 0, 800, 20, "FEEDBACK", "echo_fb"},
    {MD_REVERB_TIME, 0, 850, 20, "ROOM TAIL", "room"},      {MD_FX_LEVEL, 0, 1000, 20, "RETURN", "return"},
    {MD_MACRO, 0, 1000, 20, "SPACE (MACRO)", "space"},      {MD_GRAIN_SIZE, 0, 1000, 20, "GRAIN SIZE", "size"},
    {MD_GRAIN_POS, 0, 1000, 20, "POSITION", "position"},    {MD_FEEDBACK, 0, 850, 20, "FEEDBACK", "grain_fb"},
    {MD_GRAIN_PITCH, -24, 24, 1, "PITCH", "pitch"},         {MD_REVERSE, 0, 1, 1, "DIRECTION", "reverse"},
    {MD_FREEZE, 0, 1, 1, "FREEZE", "freeze"},               {MD_GRAIN_NOISE, 0, 1000, 20, "NOISE", "noise"},
    {MD_DUCK_SOURCE, -1, 5, 1, "DUCK FROM", "duck_from"},   {MD_DUCK, 0, 1000, 20, "DUCK AMOUNT", "duck"},
    {MD_DUCK_RELEASE, 0, 1000, 20, "RELEASE", "duck_release"}, {MD_SWING, 0, 600, 20, "SWING", "swing"}};
static int sp_field;

static int sp_value(int f)
{
    const md_patch *p = md_state();
    switch (f) {
    case SF2_ECHO_TIME: return p->delay_ms;
    case SF2_ECHO_FB: return p->delay_fb;
    case SF2_ROOM: return p->reverb_time;
    case SF2_RETURN: return p->fx_level;
    case SF2_SPACE: return p->macro[3];
    case SF2_SIZE: return p->grain_size;
    case SF2_POSITION: return p->grain_pos;
    case SF2_GRAIN_FB: return p->feedback;
    case SF2_PITCH: return p->grain_pitch;
    case SF2_REVERSE: return p->reverse;
    case SF2_FREEZE: return p->freeze;
    case SF2_NOISE: return p->grain_noise;
    case SF2_DUCK_FROM: return p->duck_source;
    case SF2_DUCK: return p->duck_amount;
    case SF2_DUCK_RELEASE: return p->duck_release;
    default: return p->swing;
    }
}

static void sp_turn(int d, int times)
{
    int f = sp_field, v = clamp(sp_value(f) + d * SP_F[f].step * (SP_F[f].step > 1 ? times : 1), SP_F[f].lo, SP_F[f].hi);
    dream_cmd(SP_F[f].cmd, f == SF2_SPACE ? 3 : 0, v);
}

static int df_value(int f)
{
    const md_drift *d = &md_state()->drift;
    return f == DF_MODE ? d->mode : f == DF_POSITION ? d->position : f == DF_CYCLE ? d->rate : d->depth;
}

static void bk_view_open(int v)
{
    bk_view = bk_view == v ? V_NONE : v;
    sq_edit = 0;
    lk_mode = 0;
    if (bk_view != V_NONE) {
        lab_open = 0;
        bk_menu_open = 0;
        if (bk_view == V_ATLAS) {
            at_focus_current();
            at_col = 1;
        }
        if (bk_view == V_TOYS)
            ty_open();
    }
}

/* ------------------------------------------------- editing a sequence --- */
static int sq_kit(int t) { return md_state()->kit.enabled && t == 3; }

static void sq_begin_edit(int t)
{
    const md_patch *p = md_state();
    const md_sequence *q = &p->sequence[t];
    int any = 0;
    sq_edit = 1;
    if (sq_kit(t))
        return;
    for (int i = 0; i < q->length; i++)
        any |= q->steps[i].note != 0;
    if (!q->enabled) {                           /* a generated rhythm becomes an editable sequence */
        if (any)
            dream_cmd(MD_SEQ_ENABLE, t, 1);
        else
            dream_cmd(MD_SEQ_GENERATE, t, q->generator);
        bk_notice("SEQUENCE ON", "");
    }
}

/* the next tone of the Biome's scale above (dir 1) or below (-1) note */
static int sq_scale_step(int note, int dir)
{
    const md_patch *p = md_state();
    for (int k = 1; k <= 12; k++) {
        int q = note + dir * k;
        if (q < 24 || q > 96)
            return note;
        if (md_quantize(q, p->root, p->scale) == q)
            return q;
    }
    return note;
}

/* the tools, with R2 held */
static void sq_tool(int b)
{
    int t = dream_ui.track, scope = sq_phrase ? 1 : 0;
    if (md_record_mode() && b != BK_L1) {
        bk_notice("TURN RECORDING OFF", "");
        return;
    }
    switch (b) {
    case BK_L1: dream_cmd(MD_SEQ_COPY, t, scope); break;
    case BK_R1: dream_cmd(MD_SEQ_PASTE, t, scope); break;
    case BK_UP: case BK_DOWN: case BK_LEFT: case BK_RIGHT: {
        int d = b == BK_UP ? 1 : b == BK_DOWN ? -1 : b == BK_RIGHT ? 12 : -12;
        dream_cmd(MD_SEQ_TRANSPOSE, t, d + 24 + 64 * scope);
        break;
    }
    case BK_A: dream_cmd(MD_SEQ_MIRROR, t, 1); break;
    case BK_Y: sq_phrase = !sq_phrase; break;
    case BK_X:
        if (md_playing())
            dream_cmd(MD_FILL, 0, -1);
        else
            bk_notice("PLAY TO QUEUE FILL", "");
        break;
    }
}

static void sq_key(int b, int times)
{
    const md_patch *p = md_state();
    int t = dream_ui.track;
    const md_sequence *q = &p->sequence[t];
    if (b >= SQ_SHIFT) {
        sq_tool(b - SQ_SHIFT);
        return;
    }
    if (b == BK_X) {
        sq_edit = 0;
        return;
    }
    if (sq_kit(t)) {                             /* the kit: pads and their hits */
        const md_kit *k = &p->kit;
        if (b == BK_LEFT || b == BK_RIGHT)
            dream_cmd(MD_KIT_STEP, 3, (k->step + (b == BK_RIGHT ? 1 : -1) + k->length) % k->length);
        else if (b == BK_UP || b == BK_DOWN)
            dream_cmd(MD_KIT_PAD, 3, clamp(k->pad + (b == BK_DOWN ? 1 : -1), 0, 7));
        else if (b == BK_A)
            dream_cmd(MD_KIT_GATE, 3, !((k->gates[k->pad] >> k->step) & 1u));
        return;
    }
    if (b == BK_LEFT || b == BK_RIGHT)
        dream_cmd(MD_SEQ_STEP, t, (q->edit + (b == BK_RIGHT ? 1 : -1) + q->length) % q->length);
    else if (b == BK_L1 || b == BK_R1)
        sq_field = clamp(sq_field + (b == BK_R1 ? 1 : -1), 0, SF_COUNT - 1);
    else if (b == BK_A) {                        /* a step on or off */
        const md_step *st = &q->steps[q->edit];
        if (st->note) {
            sq_last_note[t] = st->note;
            dream_cmd(MD_SEQ_NOTE, t, 0);
        } else {
            int nt = sq_last_note[t] ? sq_last_note[t] : md_quantize(p->track[t].note, p->root, p->scale);
            dream_cmd(MD_SEQ_NOTE, t, clamp(nt, 24, 96));
            if (!st->velocity)
                dream_cmd(MD_SEQ_VELOCITY, t, 80);
        }
    } else if (b == BK_Y)
        dream_cmd(MD_SEQ_GENERATE, t, (q->generator + 1) % 8);
    else if (b == BK_UP || b == BK_DOWN) {
        const md_step *st = &q->steps[q->edit];
        int d = b == BK_UP ? 1 : -1;
        switch (sq_field) {
        case SF_NOTE: {
            int nt = st->note ? st->note : (sq_last_note[t] ? sq_last_note[t] : p->track[t].note);
            for (int k = 0; k < times; k++)
                nt = sq_scale_step(nt, d);
            sq_last_note[t] = nt;
            dream_cmd(MD_SEQ_NOTE, t, nt);
            break;
        }
        case SF_VELOCITY: dream_cmd(MD_SEQ_VELOCITY, t, clamp(st->velocity + 5 * d * times, 1, 100)); break;
        case SF_CHANCE: dream_cmd(MD_SEQ_CHANCE, t, clamp((int)md_step_chance(st) + 5 * d * times, 0, 100)); break;
        case SF_GATE: dream_cmd(MD_SEQ_GATE, t, clamp((int)md_step_gate(st) + 5 * d * times, 1, 100)); break;
        case SF_RULE: dream_cmd(MD_SEQ_RULE, t, ((int)md_step_rule(st) + d + 4) % 4); break;
        default: dream_cmd(MD_SEQ_LENGTH, t, clamp(q->length + d, 1, 32)); break;
        }
    }
}

/* a key inside a view; dir for the repeating D-pad keys (times: the repeat's step) */
static void bk_back(void);
static void bk_view_key(int b, int times)
{
    const md_patch *p = md_state();
    int n = md_active_tracks(), t = dream_ui.track;
    if (b == BK_B && bk_view == V_SEQ && sq_edit) {
        sq_edit = 0;
        return;
    }
    if (b == BK_B && bk_view == V_ARCHIVE && ar_smp) {   /* B: from the sampler back to the poems */
        ar_smp = 0;
        return;
    }
    if (b == BK_B && bk_view == V_MOD && mf_pond) {      /* B: out of the pond, back to the grid */
        mf_pond = 0;
        return;
    }
    if (b == BK_B && bk_view == V_DREAM && dm_fly) {     /* B: the flight ends, the journey again */
        dm_fly = 0;
        bk_notice("THE JOURNEY", "");
        return;
    }
    if (b == BK_B) {
        if (bk_view == V_ATLAS && md_pending_biome() >= 0)
            dream_cmd(MD_SCENE_CONFIRM, 0, 0);   /* keep the Biome that plays */
        else
            bk_back();                           /* the way we came (brick_core.c) */
        return;
    }
    switch (bk_view) {
    case V_MIXER:
        if (b == BK_LEFT || b == BK_RIGHT)
            dream_ui.track = clamp(t + (b == BK_RIGHT ? 1 : -1), 0, n - 1);
        else if (b == BK_L1 || b == BK_R1)
            mx_row = clamp(mx_row + (b == BK_R1 ? 1 : -1), 0, MX_ROWS - 1);
        else if (b == BK_UP || b == BK_DOWN)
            dream_cmd(MX_CMD[mx_row], t, clamp(mx_value(t, mx_row) + (b == BK_UP ? 20 : -20) * times, 0, 1000));
        else if (b == BK_A)
            dream_cmd(MD_MUTE, t, !p->track[t].mute);
        break;
    case V_ATLAS:
        if (at_nfam < 0)
            at_build();
        if (b == BK_L1 || b == BK_R1 || (at_col == 0 && (b == BK_UP || b == BK_DOWN))) {   /* another family */
            int d = (b == BK_R1 || b == BK_DOWN ? 1 : -1) * (b == BK_UP || b == BK_DOWN ? times : 1);
            int f = at_col == 0 ? clamp(at_fam + d, 0, at_nfam - 1) : (at_fam + at_nfam + d) % at_nfam;
            if (f != at_fam) {
                at_fam = f;
                at_pos = 0;
                at_top = 0;
                for (int i = 0; i < at_n[f]; i++)       /* in the playing Biome's family: back onto it */
                    if (at_ids[f][i] == p->biome_id)
                        at_pos = i;
            }
        } else if (at_col == 0) {
            if ((b == BK_RIGHT || b == BK_A) && at_n[at_fam] > 0)
                at_col = 1;                          /* into the family's Biomes */
        } else if (b == BK_LEFT) {
            if (at_pos % 2 == 0)
                at_col = 0;                          /* from the left column out to the families */
            else
                at_pos--;
        } else if (b == BK_RIGHT) {                  /* in reading order: from the right column on to the next row */
            if (at_pos + 1 < at_n[at_fam])
                at_pos++;
        } else if (b == BK_UP || b == BK_DOWN)
            at_pos = clamp(at_pos + (b == BK_DOWN ? 2 : -2) * times, 0, at_n[at_fam] - 1);
        else if (b == BK_Y) {                        /* a surprise: any Biome of any family, loaded at once */
            int tot = 0, k;
            for (int f = 0; f < at_nfam; f++)
                tot += at_n[f];
            for (int tries = 0; tries < 4 && tot > 1; tries++) {
                at_rng = at_rng * 1664525u + 1013904223u + (uint32_t)bk_ms();
                k = (int)((at_rng >> 8) % (uint32_t)tot);
                for (int f = 0; f < at_nfam; f++) {
                    if (k < at_n[f]) {
                        at_fam = f;
                        at_pos = k;
                        break;
                    }
                    k -= at_n[f];
                }
                if (at_ids[at_fam][at_pos] != p->biome_id)
                    break;
            }
            at_col = 1;
            if (at_n[at_fam] > 0 && at_ids[at_fam][at_pos] != p->biome_id && dream_cmd(MD_SCENE, 0, at_ids[at_fam][at_pos]))
                bk_notice(md_biome_name(at_ids[at_fam][at_pos]), "SURPRISE");
        } else if (b == BK_A) {
            if (md_pending_biome() >= 0)
                dream_cmd(MD_SCENE_CONFIRM, 0, 1);   /* change now */
            else if (at_n[at_fam] > 0) {
                int id = at_ids[at_fam][at_pos];
                if (id != p->biome_id && dream_cmd(MD_SCENE, 0, id))
                    bk_notice(md_biome_name(id), "");
            }
        }
        break;
    case V_DREAM: {
        md_itinera_view iv;
        md_itinera_read(&iv);
        if (b == BK_Y) {
            if (iv.active)
                bk_notice("WAKE FIRST TO FLY", "");
            else {
                dm_fly = !dm_fly;
                bk_notice(dm_fly ? "FREE FLIGHT" : "THE JOURNEY", dm_fly ? "CATCH THE FALLING STARS" : "");
            }
            break;
        }
        if (dm_fly && !iv.active && b != BK_L1 && b != BK_R1) {
            dm_fly_key(b, times);
            break;
        }
        if (b == BK_L1 || b == BK_R1) {
            if (!iv.active)
                radio_adjust(0, b == BK_R1 ? 1 : -1);
        } else if (b == BK_UP || b == BK_DOWN)
            dm_row = clamp(dm_row + (b == BK_DOWN ? 1 : -1), 1, 3);
        else if (b == BK_LEFT || b == BK_RIGHT) {
            if (!iv.active)
                radio_adjust((unsigned)dm_row, b == BK_RIGHT ? 1 : -1);
        } else if (b == BK_A) {
            if (iv.active)
                dream_cmd(MD_PLAY, 0, 0);        /* wake: the dream ends */
            else if (md_playing())
                bk_notice("STOP TO START A DREAM", "");
            else
                dream_cmd(MD_ITINERA_START, radio_preset, (int)radio_settings());
        }
        break;
    }
    case V_MOD:
        if (b == BK_Y) {                         /* the firefly pond, in and out (from links, macros, drift too) */
            mf_pond = lk_mode ? 1 : !mf_pond;
            lk_mode = 0;
            mf_ab = mf_field >= MF_B_TARGET;
            break;
        }
        if (mf_pond && lk_mode == 0) {           /* the pond: each LFO a firefly, played directly */
            int base = mf_ab ? MF_B_TARGET : MF_A_TARGET;
            if (b == BK_L1 || b == BK_R1) {      /* the next firefly: 1A 1B 2A 2B ... */
                int k = clamp(t * 2 + mf_ab + (b == BK_R1 ? 1 : -1), 0, n * 2 - 1);
                dream_ui.track = k / 2;
                mf_ab = k & 1;
            } else if (b == BK_UP || b == BK_DOWN) {   /* how high it flies: its depth */
                mf_field = base + 2;
                dream_cmd(MF_CMD[mf_field], t, clamp(mf_value(t, mf_field) + (b == BK_UP ? 20 : -20) * times, 0, 1000));
            } else if (b == BK_LEFT || b == BK_RIGHT) {   /* how fast its wings: a shorter period to the right */
                mf_field = base + 3;
                dream_cmd(MF_CMD[mf_field], t, clamp(mf_value(t, mf_field) + (b == BK_RIGHT ? -20 : 20) * times, 0, 1000));
            } else if (b == BK_A) {              /* its dance: the next shape */
                mf_field = base + 1;
                dream_cmd(MF_CMD[mf_field], t, (mf_value(t, mf_field) + 1) % (MF_MAX[mf_field] + 1));
            } else if (b == BK_X) {              /* what it touches: the next target */
                mf_field = base;
                dream_cmd(MF_CMD[mf_field], t, (mf_value(t, mf_field) + 1) % (MF_MAX[mf_field] + 1));
            }
            break;
        }
        if (b == BK_X) {
            lk_mode = LK_NEXT[lk_mode & 3];
            break;
        }
        if (lk_mode == 3) {                      /* the four macros */
            if (b == BK_UP || b == BK_DOWN || b == BK_L1 || b == BK_R1)
                mc_sel = clamp(mc_sel + (b == BK_DOWN || b == BK_R1 ? 1 : -1), 0, 3);
            else if (b == BK_LEFT || b == BK_RIGHT)
                dream_cmd(MD_MACRO, mc_sel, clamp(p->macro[mc_sel] + (b == BK_RIGHT ? 20 : -20) * times, 0, 1000));
            break;
        }
        if (lk_mode == 2) {                      /* the Biome drift */
            const md_drift *dr = &p->drift;
            if (b == BK_L1 || b == BK_R1) {
                if (dr->mode)
                    bk_notice("TURN DRIFT OFF TO CAPTURE", "");
                else {
                    dream_cmd(MD_DRIFT_CAPTURE, 0, b == BK_R1);
                    bk_notice(b == BK_R1 ? "SCENE B CAPTURED" : "SCENE A CAPTURED", "");
                }
            } else if (b == BK_UP || b == BK_DOWN)
                df_field = clamp(df_field + (b == BK_DOWN ? 1 : -1), 0, DF_COUNT - 1);
            else if (b == BK_LEFT || b == BK_RIGHT) {
                int d = b == BK_RIGHT ? 1 : -1;
                if (df_field == DF_MODE) {
                    int m = clamp(dr->mode + d, 0, 2);
                    if (m && !md_drift_ready())
                        bk_notice("CAPTURE A AND B TO BEGIN", "");
                    else
                        dream_cmd(MD_DRIFT_MODE, 0, m);
                } else
                    dream_cmd(DF_CMD[df_field], 0, clamp(df_value(df_field) + d * 20 * times, 0, 1000));
            }
            break;
        }
        if (lk_mode == 1) {                      /* the living links */
            if (b == BK_UP || b == BK_DOWN)
                lk_slot = clamp(lk_slot + (b == BK_DOWN ? 1 : -1), 0, 3);
            else if (b == BK_L1 || b == BK_R1)
                lk_field = clamp(lk_field + (b == BK_R1 ? 1 : -1), 0, LF_COUNT - 1);
            else if (b == BK_LEFT || b == BK_RIGHT) {
                int big = LF_MAX[lk_field] > 10, d = (b == BK_RIGHT ? 1 : -1) * (big ? 20 * times : 1);
                int v = lk_value(lk_slot, lk_field) + d;
                if (!big)                        /* (tracks, signals, targets: round and round) */
                    v = v > LF_MAX[lk_field] ? 0 : v < 0 ? LF_MAX[lk_field] : v;
                dream_cmd(LF_CMD[lk_field], lk_slot, clamp(v, LF_MIN[lk_field], LF_MAX[lk_field]));
            } else if (b == BK_A) {
                int d = lk_value(lk_slot, LF_DEPTH);
                if (d) {
                    lk_last_depth[lk_slot] = d;
                    dream_cmd(MD_LINK_DEPTH, lk_slot, 0);
                } else
                    dream_cmd(MD_LINK_DEPTH, lk_slot, lk_last_depth[lk_slot] ? lk_last_depth[lk_slot] : 500);
            }
            break;
        }
        if (b == BK_UP || b == BK_DOWN)
            dream_ui.track = clamp(t + (b == BK_DOWN ? 1 : -1), 0, n - 1);
        else if (b == BK_L1 || b == BK_R1)
            mf_field = clamp(mf_field + (b == BK_R1 ? 1 : -1), 0, MF_COUNT - 1);
        else if (b == BK_LEFT || b == BK_RIGHT) {
            int big = MF_MAX[mf_field] > 10, d = (b == BK_RIGHT ? 1 : -1) * (big ? 20 * times : 1);
            dream_cmd(MF_CMD[mf_field], t, clamp(mf_value(t, mf_field) + d, 0, MF_MAX[mf_field]));
        }
        break;
    case V_ARCHIVE:
        if (ar_smp && (b == BK_LEFT || b == BK_RIGHT)) {   /* (held: faster) */
            ar_smp_turn(b == BK_RIGHT ? 1 : -1, times);
            break;
        }
        ar_key(b);
        break;
    case V_WORLDS:
        wd_key(b);
        break;
    case V_TOYS:
        ty_key(b, times);
        break;
    case V_SPACE:
        if (b == BK_UP || b == BK_DOWN)
            sp_field = clamp(sp_field + (b == BK_DOWN ? 1 : -1), 0, SF2_COUNT - 1);
        else if (b == BK_X || b == BK_R1)
            sp_field = (sp_field / 4 + 1) % 4 * 4;
        else if (b == BK_L1)
            sp_field = (sp_field / 4 + 3) % 4 * 4;
        else if (b == BK_LEFT || b == BK_RIGHT)
            sp_turn(b == BK_RIGHT ? 1 : -1, times);
        else if (b == BK_A) {
            dream_cmd(MD_FREEZE, 0, !p->freeze);
            bk_notice(p->freeze ? "FROZEN" : "MEMORY FLOWS", "");
        }
        break;
    case V_VOICE:
        if (b == BK_L1 || b == BK_R1)
            dream_ui.track = clamp(t + (b == BK_R1 ? 1 : -1), 0, n - 1);
        else if (b == BK_UP || b == BK_DOWN)
            vf_field = clamp(vf_field + (b == BK_DOWN ? 1 : -1), 0, VF_COUNT - 1);
        else if (b == BK_X)
            vf_field = (vf_field / 4 + 1) % 5 * 4;
        else if (b == BK_LEFT || b == BK_RIGHT)
            vo_turn(t, b == BK_RIGHT ? 1 : -1, times);
        else if (b == BK_A)
            vo_listen(t, 1);
        break;
    case V_SEQ:
        if (!sq_edit) {
            if (b == BK_UP || b == BK_DOWN)
                dream_ui.track = clamp(t + (b == BK_DOWN ? 1 : -1), 0, n - 1);
            else if (b == BK_A)
                dream_cmd(MD_MUTE, t, !p->track[t].mute);
            else if (b == BK_X)
                sq_begin_edit(t);
        } else
            sq_key(b, times);
        break;
    }
    dream_ui.dirty = 1;
}

#include "brick_toys.inc"
#include "brick_story.inc"
