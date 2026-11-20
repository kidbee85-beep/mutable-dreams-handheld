/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: PLAYGROUND (0.65; SWING, FIREFLY 0.66; WHEEL, KEEP 0.67; ECHO 0.69; SEED 0.70; JELLY 0.72), nine toys that play the chosen track in the
 * Biome's own scale - music by touching, without knowing anything about music.
 *
 *   BOUNCE   balls fall and bounce in a box; every bounce rings: on the floor the lily pad it lands on (eight scale
 *            tones, low left to high right), on the ceiling the same an octave up, on the walls root and fifth.
 *            A throws a ball (up to seven) · LEFT/RIGHT tilt the box · UP/DOWN gravity (none: they float) · Y empty
 *   ORBITS   up to five planets circle a sun; whenever one passes the gate at the top it sings its note. Each turns at
 *            its own ratio of the bar (1/4 .. 4): polyrhythms to watch. UP/DOWN planet · LEFT/RIGHT its speed ·
 *            A on / off · Y its note
 *   RAIN     a cloud rains on the eight lily pads; each drop rings the pad it lands on. LEFT/RIGHT move the cloud ·
 *            UP/DOWN how much rain · A thunder (a burst) · Y IN TIME: drops fall on the beat's sixteenths
 *   SWING    a pendulum wave: eight pendulums drift apart and come together again, each rings its tone in the
 *            middle. A releases them again · LEFT/RIGHT the cycle (20 .. 90 s) · UP/DOWN slower / faster · Y one way
 *   FIREFLY  steer a firefly (D-pad) to the glowing notes; each one caught joins a melody that loops in time, and the
 *            trail behind it is that melody. A forgets the melody · Y quarters / eighths / sixteenths
 *   WHEEL    four rhythm circles, each its hits spread evenly round its steps, each ring a track: a drum circle.
 *            UP/DOWN ring · LEFT/RIGHT hits · Y steps (3 .. 16) · A turn the ring a step
 *   ECHO     a game (0.69): the pond sings a phrase on four lily pads, you sing it back with the D-pad; each round
 *            one note longer and a little faster. A starts · a wrong pad ends it; the best round is kept
 *   SEED     a game that plays the sound itself (0.70): a dandelion seed on the night wind. Its height is the
 *            garden's GLOW, how far right it flies its WIND - turned live as you fly; light spores caught ring
 *            and thicken the FIREFLIES, burrs touched thicken the MIST and break the streak. D-pad fly · A a gust
 *            (a chord) · Y the four dials back as they were when you came
 *   JELLY    the other two dials (0.72): a jellyfish in a deep pond; A pulses it up, it sinks slowly, LEFT/RIGHT
 *            drift. Its depth is the MIST (deep: wide and far), how far right it swims the FIREFLIES; glowing
 *            plankton rings as it is caught, each pulse a soft low note. Y the dials back as they were
 *   STARFALL a catching game (0.79): stars fall in the tempo, each in one of eight lanes (the scale's tones, low left
 *            to high right), in little melodies; LEFT/RIGHT move a lotus cup under them and every star caught rings its
 *            tone. A begins a round (three moons: three missed and it ends; it quickens as you catch) · Y gentle /
 *            steady / quick. The best round is kept with the tale (save/tale.txt)
 *   KEEP     R2 + X: the chosen track's notes of the last bar become its sequence (16 steps); R2 + A undoes it
 *   All:     L1/R1 the toy · X the track it plays (the next one) · B back
 *
 * The physics runs in bk_tick on the sound's clock (every 256 samples, 5.8 ms), so a bounce sounds when it happens,
 * not when the picture comes round. Notes are the engine's own (MD_TRIGGER / MD_NOTE_OFF; the kit's pads on a kit
 * track; the Sound Lab's POLY voice when it is set to be played), as in play mode. Nothing here is stored. */

enum { TY_BOUNCE, TY_ORBITS, TY_RAIN, TY_SWING, TY_FIREFLY, TY_WHEEL, TY_ECHO, TY_SEED, TY_JELLY, TY_STARS, TY_COUNT };
static const char *const TY_NAMES[TY_COUNT] = {"BOUNCE", "ORBITS", "RAIN", "SWING", "FIREFLY", "WHEEL", "ECHO", "SEED", "JELLY", "STARFALL"};
static int ty_keeps;                             /* (how often KEEP worked: the tale asks for it) */
static int st_dirty;                             /* (brick_story.inc: the tale and the settings to be written) */
static void ty_record(int note, int vel, int track);   /* (KEEP: what was played, for R2 + X) */
static int ty_toy, ty_ready;
static uint32_t ty_rng = 0x2545F491u, ty_last_ms, ty_notes;   /* ty_notes: how many it played (the tests) */
static float ty_pad_flash[8], ty_pad_vel[8];      /* the lily pads lit by what rang them */

/* the box (stage pixels): the toys inside, the lily pads below */
#define TY_SX 152                                /* = BK_SX, BK_SY (brick_stage.inc, checked in brick_toys_draw.inc) */
#define TY_SY 30
#define TY_L ((float)(TY_SX + 30))
#define TY_R ((float)(TY_SX + 690))
#define TY_T ((float)(TY_SY + 70))
#define TY_B ((float)(TY_SY + 600))
#define TY_PAD_H 52

static float ty_rand(void)
{
    ty_rng ^= ty_rng << 13;
    ty_rng ^= ty_rng >> 17;
    ty_rng ^= ty_rng << 5;
    return (float)(ty_rng >> 8) / 16777216.0f;
}
static int ty_pad_at(float x)
{
    int d = (int)((x - TY_L) * 8.0f / (TY_R - TY_L));
    return d < 0 ? 0 : d > 7 ? 7 : d;
}

/* ------------------------------------------------------------ sparks --- */
/* (0.68) every hit shows: a ring spreading and a few sparks flying, falling, fading. Moved on the sound's clock
   with the toys, drawn by brick_toys_draw.inc. ci: a track's colour 0..5, 6 gold, 7 rain blue, 8 white */
#define TY_SPARKS 96
static struct { float x, y, vx, vy, life, decay, size; int ci, ring; } ty_sp[TY_SPARKS];
static int ty_sp_w;
static uint32_t ty_switch_at;                    /* when the toy last changed (its fade in) */

static void ty_spark(float x, float y, float vx, float vy, float size, float decay, int ci, int ring)
{
    int k = ty_sp_w++ % TY_SPARKS;
    ty_sp[k].x = x, ty_sp[k].y = y, ty_sp[k].vx = vx, ty_sp[k].vy = vy, ty_sp[k].size = size;
    ty_sp[k].decay = decay, ty_sp[k].ci = ci, ty_sp[k].ring = ring, ty_sp[k].life = 1.0f;
}
static void ty_burst(float x, float y, int ci, int n, float speed)
{
    ty_spark(x, y, 0, 0, 10.0f + speed * 0.02f, 2.4f, ci, 1);
    for (int i = 0; i < n; i++) {
        float a = ty_rand() * 6.2831853f, v = speed * (0.4f + 0.6f * ty_rand());
        ty_spark(x, y, cosf(a) * v, sinf(a) * v - speed * 0.3f, 2.0f + 2.5f * ty_rand(), 1.5f + ty_rand(), ci, 0);
    }
}
static void ty_sparks_step(float dt)
{
    for (int k = 0; k < TY_SPARKS; k++) {
        if (ty_sp[k].life <= 0)
            continue;
        ty_sp[k].life -= ty_sp[k].decay * dt;
        if (ty_sp[k].ring)
            continue;
        ty_sp[k].x += ty_sp[k].vx * dt, ty_sp[k].y += ty_sp[k].vy * dt;
        ty_sp[k].vy += 380.0f * dt;
        ty_sp[k].vx *= 1.0f - 1.8f * dt, ty_sp[k].vy *= 1.0f - 1.2f * dt;
    }
}

/* ------------------------------------------------------------- notes --- */
#define TY_ON 10
static struct { int note, track, kit, poly, on; uint32_t off_at, at; } ty_on[TY_ON];

static void ty_off(int i)
{
    if (!ty_on[i].on)
        return;
    if (ty_on[i].poly)
        po_note_off(ty_on[i].note);
    else if (!ty_on[i].kit)
        dream_cmd(MD_NOTE_OFF, ty_on[i].track, ty_on[i].note);
    ty_on[i].on = 0;
}
static void ty_release(int all)
{
    uint32_t now = bk_ms();
    for (int i = 0; i < TY_ON; i++)
        if (ty_on[i].on && (all || (int32_t)(now - ty_on[i].off_at) >= 0))
            ty_off(i);
}
/* the degree-th tone of the scale (0 = the root; 7 an octave up) on the chosen track, for gate ms */
static void ty_play(int degree, int vel, uint32_t gate)
{
    const md_patch *p = md_state();
    int track = dream_ui.track, slot = -1, note, i;
    uint32_t now = bk_ms(), oldest = 0;
    if (track >= md_active_tracks())
        track = 0;
    note = bk_scale_note(degree);
    vel = vel < 30 ? 30 : vel > 127 ? 127 : vel;
    for (i = 0; i < TY_ON; i++)                  /* the same note still ringing: let it go first */
        if (ty_on[i].on && ty_on[i].note == note && ty_on[i].track == track)
            ty_off(i);
    for (i = 0; i < TY_ON && slot < 0; i++)
        if (!ty_on[i].on)
            slot = i;
    if (slot < 0) {                              /* all ringing: the oldest makes room */
        for (i = 0; i < TY_ON; i++)
            if (now - ty_on[i].at >= oldest)
                oldest = now - ty_on[i].at, slot = i;
        ty_off(slot);
    }
    ty_on[slot].kit = p->kit.enabled && track == 3;
    ty_on[slot].poly = !ty_on[slot].kit && lab_v[P_PO_LEVEL] > 0 && lab_v[P_PO_PLAYMODE] > 0.5f;
    if (ty_on[slot].poly)
        po_note_on_vel(note, 0, vel);
    else if (ty_on[slot].kit)
        dream_cmd(MD_KIT_TRIGGER, 3, degree & 7);
    else
        dream_cmd(MD_TRIGGER, track, note);
    ty_on[slot].note = note, ty_on[slot].track = track, ty_on[slot].on = 1;
    ty_on[slot].at = now, ty_on[slot].off_at = now + gate;
    ty_notes++;
    if (!ty_on[slot].kit)
        ty_record(note, vel, track);
    ty_pad_flash[degree & 7] = 1.0f;
    ty_pad_vel[degree & 7] = (float)vel / 127.0f;
    dream_dismiss_intro();
}

/* ------------------------------------------------------------ BOUNCE --- */
#define TY_BALLS 7
#define TY_BALL_R 15.0f
#define TY_BTRAIL 7
static struct { float x, y, vx, vy, rest, hit, tacc, tx[TY_BTRAIL], tyy[TY_BTRAIL]; int on; } ty_ball[TY_BALLS];   /* (tx, tyy: where it was, for its trail) */
static int ty_tilt, ty_grav = 4;                 /* tilt -3..3, gravity 0..8 (x 220 px/s^2) */
static uint32_t ty_bounce_at;                    /* (a bounce at most every 30 ms: a rattle, not a machine gun) */

static void ty_throw(void)
{
    int i, k = -1;
    for (i = 0; i < TY_BALLS && k < 0; i++)
        if (!ty_ball[i].on)
            k = i;
    if (k < 0) {                                 /* all in play: the one lying stillest goes again */
        float most = -1;
        for (i = 0; i < TY_BALLS; i++)
            if (ty_ball[i].rest > most)
                most = ty_ball[i].rest, k = i;
    }
    ty_ball[k].on = 1;
    ty_ball[k].x = TY_L + 80.0f + ty_rand() * (TY_R - TY_L - 160.0f);
    ty_ball[k].y = TY_T + 30.0f;
    ty_ball[k].vx = (ty_rand() - 0.5f) * 520.0f;
    ty_ball[k].vy = -120.0f - ty_rand() * 200.0f;
    ty_ball[k].rest = 0, ty_ball[k].hit = 1.0f;
    for (int q = 0; q < TY_BTRAIL; q++)          /* (no trail from where it was before) */
        ty_ball[k].tx[q] = ty_ball[k].x, ty_ball[k].tyy[q] = ty_ball[k].y;
}
static void ty_ring(int degree, float speed)
{
    uint32_t now = bk_ms();
    if (speed < 110.0f || now - ty_bounce_at < 30u)
        return;
    ty_bounce_at = now;
    ty_play(degree, (int)(36.0f + speed / 9.0f), 160u + (uint32_t)(speed * 0.2f));
}
static void ty_bounce_step(float dt)
{
    float gx = (float)ty_tilt * 160.0f, gy = (float)ty_grav * 220.0f;
    const float e = 0.84f, lo = TY_T + TY_BALL_R, hi = TY_B - TY_BALL_R, l = TY_L + TY_BALL_R, r = TY_R - TY_BALL_R;
    for (int i = 0; i < TY_BALLS; i++) {
        float s;
        if (!ty_ball[i].on)
            continue;
        ty_ball[i].vx += gx * dt, ty_ball[i].vy += gy * dt;
        ty_ball[i].vx *= 1.0f - 0.08f * dt, ty_ball[i].vy *= 1.0f - 0.08f * dt;   /* (a little air) */
        ty_ball[i].x += ty_ball[i].vx * dt, ty_ball[i].y += ty_ball[i].vy * dt;
        ty_ball[i].hit *= 1.0f - 6.0f * dt;
        if (ty_ball[i].y > hi) {                 /* the floor: the lily pad under it */
            s = ty_ball[i].vy;
            ty_ball[i].y = hi, ty_ball[i].vy = -s * e;
            ty_ball[i].vx *= 0.97f;
            if (s > 110.0f)
                ty_ball[i].hit = 1.0f;
            ty_ring(ty_pad_at(ty_ball[i].x), s);
            if (s > 110.0f)
                ty_burst(ty_ball[i].x, hi + TY_BALL_R, i % 6, 3 + (int)(s / 160.0f), s * 0.35f);
        } else if (ty_ball[i].y < lo) {          /* the ceiling: an octave up */
            s = -ty_ball[i].vy;
            ty_ball[i].y = lo, ty_ball[i].vy = s * e;
            if (s > 110.0f)
                ty_ball[i].hit = 1.0f;
            ty_ring(ty_pad_at(ty_ball[i].x) + 7, s);
            if (s > 110.0f)
                ty_burst(ty_ball[i].x, lo - TY_BALL_R, i % 6, 3, s * 0.3f);
        }
        if (ty_ball[i].x < l || ty_ball[i].x > r) {   /* the walls: root on the left, fifth on the right */
            int right = ty_ball[i].x > r;
            s = fabsf(ty_ball[i].vx);
            ty_ball[i].x = right ? r : l, ty_ball[i].vx = (right ? -s : s) * e;
            if (s > 110.0f)
                ty_ball[i].hit = 1.0f;
            ty_ring(right ? 4 : 0, s);
            if (s > 110.0f)
                ty_burst(right ? r + TY_BALL_R : l - TY_BALL_R, ty_ball[i].y, i % 6, 3, s * 0.3f);
        }
        if ((ty_ball[i].tacc += dt) >= 0.018f) {   /* (its trail: a place every 18 ms) */
            ty_ball[i].tacc = 0;
            memmove(ty_ball[i].tx + 1, ty_ball[i].tx, (TY_BTRAIL - 1) * sizeof(float));
            memmove(ty_ball[i].tyy + 1, ty_ball[i].tyy, (TY_BTRAIL - 1) * sizeof(float));
            ty_ball[i].tx[0] = ty_ball[i].x, ty_ball[i].tyy[0] = ty_ball[i].y;
        }
        for (int j = i + 1; j < TY_BALLS; j++) {   /* two balls meeting: they push apart (no note) */
            float dx, dy, d2, d, nx, ny, rel;
            if (!ty_ball[j].on)
                continue;
            dx = ty_ball[j].x - ty_ball[i].x, dy = ty_ball[j].y - ty_ball[i].y, d2 = dx * dx + dy * dy;
            if (d2 >= 4.0f * TY_BALL_R * TY_BALL_R || d2 < 1e-4f)
                continue;
            d = sqrtf(d2), nx = dx / d, ny = dy / d;
            rel = (ty_ball[j].vx - ty_ball[i].vx) * nx + (ty_ball[j].vy - ty_ball[i].vy) * ny;
            if (rel < 0) {
                ty_ball[i].vx += rel * nx * e, ty_ball[i].vy += rel * ny * e;
                ty_ball[j].vx -= rel * nx * e, ty_ball[j].vy -= rel * ny * e;
            }
            ty_ball[i].x -= nx * (2.0f * TY_BALL_R - d) * 0.5f, ty_ball[i].y -= ny * (2.0f * TY_BALL_R - d) * 0.5f;
            ty_ball[j].x += nx * (2.0f * TY_BALL_R - d) * 0.5f, ty_ball[j].y += ny * (2.0f * TY_BALL_R - d) * 0.5f;
        }
        if (fabsf(ty_ball[i].vx) + fabsf(ty_ball[i].vy) < 40.0f)
            ty_ball[i].rest += dt;
        else
            ty_ball[i].rest = 0;
        if (ty_ball[i].rest > 4.0f)              /* lying still a while: it melts away */
            ty_ball[i].on = 0;
    }
}

/* ------------------------------------------------------------ ORBITS --- */
#define TY_PLANETS 5
static const float TY_RATIOS[] = {0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f, 4.0f};
static const char *const TY_RATIO_NAMES[] = {"1/4", "1/2", "3/4", "1", "5/4", "3/2", "2", "3", "4"};
#define TY_NRATIOS ((int)(sizeof TY_RATIOS / sizeof TY_RATIOS[0]))
static struct { int on, ratio, degree; float ph, flash; } ty_pl[TY_PLANETS] = {
    {1, 3, 0, 0, 0}, {1, 5, 2, 0, 0}, {1, 6, 4, 0, 0}, {0, 7, 7, 0, 0}, {0, 2, 9, 0, 0}};
static int ty_sel;

static void ty_orbit_step(float dt)
{
    float bar_hz = (float)(md_tempo_now() > 20 ? md_tempo_now() : 20) / 240.0f;   /* a turn a bar at ratio 1 */
    for (int i = 0; i < TY_PLANETS; i++) {
        ty_pl[i].flash *= 1.0f - 4.0f * dt;
        if (!ty_pl[i].on)
            continue;
        ty_pl[i].ph += dt * bar_hz * TY_RATIOS[ty_pl[i].ratio];
        if (ty_pl[i].ph >= 1.0f) {               /* through the gate at the top */
            ty_pl[i].ph -= floorf(ty_pl[i].ph);
            ty_pl[i].flash = 1.0f;
            ty_burst((float)(TY_SX + 360), (float)(TY_SY + 330) - (80.0f + (float)i * 46.0f), (i + 1) % 6, 6, 160.0f);
            ty_play(ty_pl[i].degree, 70 + 10 * (TY_PLANETS - i), (uint32_t)(220.0f / sqrtf(TY_RATIOS[ty_pl[i].ratio])));
        }
    }
}

/* -------------------------------------------------------------- RAIN --- */
#define TY_DROPS 28
#define TY_RIPPLES 12
static struct { float x, y, vy; int on; } ty_drop[TY_DROPS];
static struct { float x, age; int on; } ty_rip[TY_RIPPLES];
static float ty_cloud = 0.5f, ty_cloud_shown = 0.5f, ty_rain_acc, ty_thunder;
#define TY_BOLT 7
static float ty_bolt[TY_BOLT];
static int ty_rain = 3, ty_in_time;              /* rain 0..8; IN TIME: drops on the sixteenths */
static uint32_t ty_sixteenth;

static void ty_drop_new(float spread)
{
    for (int i = 0; i < TY_DROPS; i++)
        if (!ty_drop[i].on) {
            float cx = TY_L + ty_cloud * (TY_R - TY_L);
            ty_drop[i].on = 1;
            ty_drop[i].x = fminf(fmaxf(cx + (ty_rand() - 0.5f) * spread, TY_L + 4.0f), TY_R - 4.0f);
            ty_drop[i].y = TY_T + 70.0f;
            ty_drop[i].vy = 60.0f + ty_rand() * 80.0f;
            return;
        }
}
static void ty_rain_step(float dt)
{
    uint32_t now = bk_ms();
    ty_thunder *= 1.0f - 3.0f * dt;
    if (ty_in_time) {                            /* a drop on each sixteenth, as many as the rain says */
        uint32_t step_ms = (uint32_t)(15000.0f / (float)(md_tempo_now() > 20 ? md_tempo_now() : 20));
        uint32_t s = now / (step_ms ? step_ms : 1u);
        if (s != ty_sixteenth) {
            ty_sixteenth = s;
            if (ty_rain > 0 && (int)(ty_rand() * 8.0f) < ty_rain)
                ty_drop_new(150.0f);
        }
    } else {
        ty_rain_acc += dt * (float)ty_rain * 0.9f;
        while (ty_rain_acc >= 1.0f) {
            ty_rain_acc -= 1.0f;
            ty_drop_new(150.0f);
        }
    }
    for (int i = 0; i < TY_DROPS; i++) {
        if (!ty_drop[i].on)
            continue;
        ty_drop[i].vy += 900.0f * dt;
        ty_drop[i].y += ty_drop[i].vy * dt;
        if (ty_drop[i].y >= TY_B) {              /* on a lily pad: it rings, a ripple spreads */
            int d = ty_pad_at(ty_drop[i].x);
            ty_drop[i].on = 0;
            ty_play(d, 60 + (int)(ty_rand() * 40.0f), 260u);
            for (int q = 0; q < 4; q++)          /* (a splash) */
                ty_spark(ty_drop[i].x, TY_B, (ty_rand() - 0.5f) * 160.0f, -120.0f - ty_rand() * 140.0f, 1.8f + ty_rand() * 1.2f, 2.2f, 7, 0);
            for (int k = 0; k < TY_RIPPLES; k++)
                if (!ty_rip[k].on) {
                    ty_rip[k].on = 1, ty_rip[k].x = ty_drop[i].x, ty_rip[k].age = 0;
                    break;
                }
        }
    }
    for (int k = 0; k < TY_RIPPLES; k++)
        if (ty_rip[k].on && (ty_rip[k].age += dt) > 1.2f)
            ty_rip[k].on = 0;
}

/* ------------------------------------------------------------- SWING --- */
/* a pendulum wave (0.66): eight pendulums from one pivot, the longest the slowest and lowest. Pendulum k swings
   (base + k) times in a cycle, so they drift apart into waves and spirals and come together again at its end;
   each one rings its tone when it passes the middle */
#define TY_PENDS 8
static const float TY_CYCLES[] = {20.0f, 30.0f, 45.0f, 60.0f, 90.0f};
#define TY_NCYCLES ((int)(sizeof TY_CYCLES / sizeof TY_CYCLES[0]))
static int ty_cycle = 2, ty_base = 12, ty_one_way;
static float ty_sw_t, ty_sw_last[TY_PENDS], ty_sw_flash[TY_PENDS];

static float ty_swing_x(int k, float t)           /* -1..1: where pendulum k is at time t (released at t = 0) */
{
    float f = (float)(ty_base + k) / TY_CYCLES[ty_cycle];
    return cosf(6.2831853f * f * t);
}
static void ty_swing_release(void)
{
    ty_sw_t = 0;
    for (int k = 0; k < TY_PENDS; k++)
        ty_sw_last[k] = 1.0f, ty_sw_flash[k] = 0;
}
static void ty_swing_step(float dt)
{
    float cyc = TY_CYCLES[ty_cycle];
    ty_sw_t += dt;
    if (ty_sw_t >= cyc)                          /* (together again: the cycle starts over, exactly) */
        ty_sw_t -= cyc;
    for (int k = 0; k < TY_PENDS; k++) {
        float x = ty_swing_x(k, ty_sw_t);
        ty_sw_flash[k] *= 1.0f - 5.0f * dt;
        if ((x < 0) != (ty_sw_last[k] < 0) && (!ty_one_way || x > 0)) {
            ty_sw_flash[k] = 1.0f;
            ty_burst((float)(TY_SX + 360), TY_T + 6.0f + 500.0f - (float)k * 52.0f, k % 6, 4, 120.0f);
            ty_play(k, 64 + 6 * k, 240u);
        }
        ty_sw_last[k] = x;
    }
}

/* ----------------------------------------------------------- FIREFLY --- */
/* steer a firefly to the glowing notes (0.66); each one caught joins a melody that loops in time - the firefly's
   trail is that melody. The edges wrap round; nothing is ever lost but by A */
#define TY_SEEDS 3
#define TY_LOOP 16
#define TY_TRAIL 64
static float ty_fx = -1, ty_fy, ty_feat, ty_fvx = 1, ty_fvy;
static int ty_fdir, ty_loop[TY_LOOP], ty_nloop, ty_loop_pos = -1, ty_loop_rate = 1;   /* rate 0 quarters, 1 eighths, 2 sixteenths */
static struct { float x, y, age; int degree, on; } ty_seed[TY_SEEDS];
static float ty_trail_x[TY_TRAIL], ty_trail_y[TY_TRAIL], ty_trail_acc;
static int ty_trail_w;
static uint32_t ty_loop_step = 0xFFFFFFFFu;
static const char *const TY_RATES[3] = {"QUARTERS", "EIGHTHS", "SIXTEENTHS"};

static int ty_seed_degree(float x, float y) { return ty_pad_at(x) + (y < (TY_T + TY_B) * 0.5f ? 7 : 0); }
static void ty_seed_new(int i)
{
    for (int tries = 0; tries < 8; tries++) {    /* (not right under the firefly) */
        ty_seed[i].x = TY_L + 40.0f + ty_rand() * (TY_R - TY_L - 80.0f);
        ty_seed[i].y = TY_T + 40.0f + ty_rand() * (TY_B - TY_T - 40.0f);
        if (fabsf(ty_seed[i].x - ty_fx) + fabsf(ty_seed[i].y - ty_fy) > 160.0f)
            break;
    }
    ty_seed[i].degree = ty_seed_degree(ty_seed[i].x, ty_seed[i].y);
    ty_seed[i].age = 0, ty_seed[i].on = 1;
}
static void ty_firefly_step(float dt)
{
    static const float dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
    const float sp = 190.0f, top = TY_T + 34.0f, bot = TY_B + 30.0f;   /* (clear of the title) */
    uint32_t now = bk_ms(), beat = (uint32_t)(60000.0f / (float)(md_tempo_now() > 20 ? md_tempo_now() : 20));
    uint32_t step_ms = beat >> ty_loop_rate, s;
    if (ty_fx < 0) {
        ty_fx = (TY_L + TY_R) * 0.5f, ty_fy = (TY_T + TY_B) * 0.5f;
        for (int i = 0; i < TY_TRAIL; i++)
            ty_trail_x[i] = ty_fx, ty_trail_y[i] = ty_fy;
    }
    {   /* (0.68) it turns in a soft curve, not on the spot: the heading eases to the D-pad's, the speed stays */
        float k = fminf(1.0f, 7.0f * dt), m;
        ty_fvx += (dx[ty_fdir] - ty_fvx) * k, ty_fvy += (dy[ty_fdir] - ty_fvy) * k;
        m = sqrtf(ty_fvx * ty_fvx + ty_fvy * ty_fvy);
        if (m > 1e-3f)
            ty_fvx /= m, ty_fvy /= m;
        else
            ty_fvx = dx[ty_fdir], ty_fvy = dy[ty_fdir];
    }
    ty_fx += ty_fvx * sp * dt, ty_fy += ty_fvy * sp * dt;
    if (ty_fx < TY_L) ty_fx += TY_R - TY_L;      /* (round the edges) */
    if (ty_fx > TY_R) ty_fx -= TY_R - TY_L;
    if (ty_fy < top) ty_fy += bot - top;
    if (ty_fy > bot) ty_fy -= bot - top;
    ty_feat *= 1.0f - 4.0f * dt;
    if ((ty_trail_acc += dt) >= 0.03f) {         /* where it was, for the trail */
        ty_trail_acc = 0;
        ty_trail_w = (ty_trail_w + 1) % TY_TRAIL;
        ty_trail_x[ty_trail_w] = ty_fx, ty_trail_y[ty_trail_w] = ty_fy;
    }
    for (int i = 0; i < TY_SEEDS; i++) {
        if (!ty_seed[i].on) {
            ty_seed_new(i);
            continue;
        }
        ty_seed[i].age += dt;
        if ((ty_seed[i].x - ty_fx) * (ty_seed[i].x - ty_fx) + (ty_seed[i].y - ty_fy) * (ty_seed[i].y - ty_fy) < 30.0f * 30.0f) {
            ty_play(ty_seed[i].degree, 110, 260u);   /* caught: heard at once, and into the melody */
            if (ty_nloop == TY_LOOP)
                memmove(ty_loop, ty_loop + 1, (TY_LOOP - 1) * sizeof ty_loop[0]), ty_nloop--;
            ty_loop[ty_nloop++] = ty_seed[i].degree;
            ty_feat = 1.0f;
            ty_burst(ty_seed[i].x, ty_seed[i].y, ty_seed[i].degree % 6, 10, 200.0f);
            ty_seed_new(i);
        }
    }
    s = now / (step_ms ? step_ms : 1u);          /* the melody, round and round in time */
    if (s != ty_loop_step) {
        ty_loop_step = s;
        if (ty_nloop > 0) {
            ty_loop_pos = (ty_loop_pos + 1) % ty_nloop;
            ty_play(ty_loop[ty_loop_pos], 80, (uint32_t)(step_ms * 0.8f));
        }
    }
}

/* ------------------------------------------------------------- WHEEL --- */
/* rhythm circles (0.67): four rings, each spreading its hits as evenly as they go round its steps (Euclid's way:
   3 in 8, 5 in 16 ...), each ring a track of its own - a drum circle of the Biome's voices, in the tempo */
#define TY_RINGS 4
static struct { int steps, hits, rot, on; float flash; } ty_ring_[TY_RINGS] = {
    {16, 4, 0, 1, 0}, {8, 3, 0, 1, 0}, {16, 5, 2, 1, 0}, {12, 2, 3, 0, 0}};
static const int TY_RING_DEG[TY_RINGS] = {0, 4, 7, 2};
static int ty_ring_sel;
static uint32_t ty_wheel_step = 0xFFFFFFFFu, ty_wheel_n;

static int ty_ring_hit(int r, int i)              /* step i of ring r: a hit? (evenly spread, turned by rot) */
{
    int n = ty_ring_[r].steps, k = ty_ring_[r].hits, j = ((i - ty_ring_[r].rot) % n + n) % n;
    return k > 0 && (j * k) % n < k;
}
static void ty_play_on(int track, int degree, int vel, uint32_t gate)   /* ty_play on another track for a moment */
{
    int keep = dream_ui.track;
    dream_ui.track = track;
    ty_play(degree, vel, gate);
    dream_ui.track = keep;
}
static void ty_wheel_step_fn(float dt)
{
    uint32_t step_ms = (uint32_t)(15000.0f / (float)(md_tempo_now() > 20 ? md_tempo_now() : 20)), s;
    int n = md_active_tracks();
    for (int r = 0; r < TY_RINGS; r++)
        ty_ring_[r].flash *= 1.0f - 6.0f * dt;
    s = bk_ms() / (step_ms ? step_ms : 1u);
    if (s == ty_wheel_step)
        return;
    ty_wheel_step = s;
    ty_wheel_n++;
    for (int r = 0; r < TY_RINGS; r++)
        if (ty_ring_[r].on && ty_ring_hit(r, (int)(ty_wheel_n % (uint32_t)ty_ring_[r].steps))) {
            ty_ring_[r].flash = 1.0f;
            {
                float rr = 250.0f - (float)r * 52.0f;
                float a = (float)(ty_wheel_n % (uint32_t)ty_ring_[r].steps) / (float)ty_ring_[r].steps * 6.2831853f - 1.5707963f;
                ty_burst((float)(TY_SX + 360) + cosf(a) * rr, (float)(TY_SY + 330) + sinf(a) * rr, n > 0 ? r % n : 0, 3, 90.0f);
            }
            ty_play_on(n > 0 ? r % n : 0, TY_RING_DEG[r], r == 0 ? 120 : 90, step_ms);
        }
}

/* -------------------------------------------------------------- ECHO --- */
/* a listening game (0.69): four lily pads under the D-pad (LEFT, UP, RIGHT, DOWN: root, third, fifth, octave). The
   pond sings a phrase, you sing it back; right, and it grows by a note and quickens; wrong, and the dream fades */
#define TY_ECHO_MAX 48
static const int TY_ECHO_DEG[4] = {0, 2, 4, 7};
enum { EC_IDLE, EC_LISTEN, EC_YOURS, EC_WON, EC_LOST };
static int ty_ec_state, ty_ec_seq[TY_ECHO_MAX], ty_ec_len, ty_ec_pos, ty_ec_best, ty_ec_wrong = -1, ty_ec_last;
static uint32_t ty_ec_at;
static float ty_ec_flash[4];

static uint32_t ty_ec_gap(void) { return (uint32_t)(ty_ec_len < 14 ? 620 - ty_ec_len * 26 : 260); }
static void ty_ec_pad(int p, int vel)            /* a pad sings and lights, a ring and sparks from it */
{
    static const float ox[4] = {-150, 0, 150, 0}, oy[4] = {0, -150, 0, 150};
    ty_play(TY_ECHO_DEG[p], vel, 320u);
    ty_ec_flash[p] = 1.0f;
    ty_burst((float)(TY_SX + 360) + ox[p], (float)(TY_SY + 320) + oy[p], p == 0 ? 0 : p == 1 ? 2 : p == 2 ? 4 : 1, 5, 150.0f);
}
static void ty_ec_start(void)
{
    ty_ec_len = 1, ty_ec_pos = 0, ty_ec_wrong = -1;
    ty_ec_seq[0] = (int)(ty_rand() * 4.0f) & 3;
    ty_ec_state = EC_LISTEN, ty_ec_at = bk_ms() + 700u;
    bk_notice("LISTEN", "THEN SING IT BACK WITH THE D-PAD");
}
static void ty_echo_step(float dt)
{
    uint32_t now = bk_ms();
    for (int p = 0; p < 4; p++)
        ty_ec_flash[p] *= 1.0f - 4.0f * dt;
    if ((int32_t)(now - ty_ec_at) < 0)
        return;
    if (ty_ec_state == EC_LISTEN) {              /* the pond sings, a pad at a time */
        ty_ec_pad(ty_ec_seq[ty_ec_pos++], 100);
        ty_ec_at = now + ty_ec_gap();
        if (ty_ec_pos >= ty_ec_len)
            ty_ec_state = EC_YOURS, ty_ec_pos = 0;
    } else if (ty_ec_state == EC_WON) {          /* a note more */
        if (ty_ec_len < TY_ECHO_MAX)
            ty_ec_seq[ty_ec_len++] = (int)(ty_rand() * 4.0f) & 3;
        ty_ec_pos = 0, ty_ec_state = EC_LISTEN, ty_ec_at = now + 350u;
    } else if (ty_ec_state == EC_LOST)
        ty_ec_state = EC_IDLE;
}
static void ty_echo_key(int b)
{
    int p = b == BK_LEFT ? 0 : b == BK_UP ? 1 : b == BK_RIGHT ? 2 : b == BK_DOWN ? 3 : -1;
    if (b == BK_A && ty_ec_state != EC_LISTEN) {
        ty_ec_start();
        return;
    }
    if (p < 0 || bk_ms() - bk_down_at[b] > 120u)  /* (a held pad's repeats: not new presses) */
        return;
    if (ty_ec_state != EC_YOURS) {               /* (outside the game: the pads simply play) */
        if (ty_ec_state == EC_IDLE)
            ty_ec_pad(p, 90);
        return;
    }
    ty_ec_pad(p, 110);
    if (p != ty_ec_seq[ty_ec_pos]) {             /* wrong: the dream fades */
        ty_ec_wrong = p, ty_ec_last = ty_ec_len - 1;
        ty_ec_state = EC_LOST, ty_ec_at = bk_ms() + 1800u;
        if (ty_ec_last > ty_ec_best)
            ty_ec_best = ty_ec_last;
        bk_notice("THE DREAM FADES", ty_ec_last > 0 && ty_ec_last >= ty_ec_best ? "A NEW BEST" : "A: AGAIN");
        return;
    }
    if (++ty_ec_pos >= ty_ec_len) {              /* the whole phrase: the next round */
        ty_ec_last = ty_ec_len;
        if (ty_ec_len > ty_ec_best)
            ty_ec_best = ty_ec_len;
        ty_ec_state = EC_WON, ty_ec_at = bk_ms() + 750u;
        for (int k = 0; k < 4; k++)
            ty_burst((float)(TY_SX + 360), (float)(TY_SY + 320), k, 4, 220.0f);
    }
}

/* -------------------------------------------------------------- SEED --- */
/* a dandelion seed on the night wind (0.70): the world drifts by from the right, the seed goes where the D-pad
   pushes it and the wind carries it back. Where it is turns the garden's dials live: up is GLOW, right is WIND.
   Spores of light ring a tone (higher the higher they float) and thicken the FIREFLIES; burrs thicken the MIST */
#define TY_SPORES 6
#define TY_BURRS 3
static float ty_sx = -1, ty_sy, ty_svx, ty_svy, ty_gust, ty_seed_hurt, ty_seed_glow, ty_scroll;
static int ty_seed_streak, ty_seed_best, ty_seed_push[4], ty_seed_saved = 0, ty_seed_mac[4], ty_seed_sent[4] = {-1, -1, -1, -1};
static uint32_t ty_seed_sent_at;
static struct { float x, y, ph; int on, degree; } ty_spore[TY_SPORES];
static struct { float x, y, rot; int on; } ty_burr[TY_BURRS];

static void ty_macro_soft(int k, int v)          /* a dial, only when it moved enough (the engine's queue is small) */
{
    v = clamp(v, 0, 1000);
    if (ty_seed_sent[k] < 0 || abs(v - ty_seed_sent[k]) >= 12) {
        dream_cmd(MD_MACRO, k, v);
        ty_seed_sent[k] = v;
    }
}
static void ty_seed_enter(void)                  /* coming in: the dials as they are, to give back with Y */
{
    const md_patch *p = md_state();
    for (int k = 0; k < 4; k++)
        ty_seed_mac[k] = p->macro[k], ty_seed_sent[k] = p->macro[k];
    ty_seed_saved = 1;
}
static void ty_seed_step(float dt)
{
    const float top = TY_T + 40.0f, bot = TY_B + 20.0f, l = TY_L + 30.0f, r = TY_R - 30.0f;
    uint32_t now = bk_ms();
    if (ty_sx < 0) {
        ty_sx = TY_L + 200.0f, ty_sy = (TY_T + TY_B) * 0.5f;
        if (!ty_seed_saved)
            ty_seed_enter();
    }
    {   /* the D-pad pushes, the wind carries: a soft, floating flight */
        float ax = (float)(ty_seed_push[2] - ty_seed_push[0]) * 900.0f, ay = (float)(ty_seed_push[3] - ty_seed_push[1]) * 900.0f;
        ty_svx += (ax - 40.0f) * dt, ty_svy += (ay + 25.0f) * dt;   /* (the wind leans left, the seed sinks a little) */
        ty_svx *= 1.0f - 2.2f * dt, ty_svy *= 1.0f - 2.2f * dt;
        ty_svy += sinf((float)now * 0.0021f) * 30.0f * dt;          /* (and it bobs) */
        ty_sx += ty_svx * dt, ty_sy += ty_svy * dt;
        if (ty_sx < l) ty_sx = l, ty_svx = fabsf(ty_svx) * 0.3f;
        if (ty_sx > r) ty_sx = r, ty_svx = -fabsf(ty_svx) * 0.3f;
        if (ty_sy < top) ty_sy = top, ty_svy = fabsf(ty_svy) * 0.3f;
        if (ty_sy > bot) ty_sy = bot, ty_svy = -fabsf(ty_svy) * 0.3f;
    }
    ty_gust *= 1.0f - 2.5f * dt, ty_seed_hurt *= 1.0f - 2.0f * dt, ty_seed_glow *= 1.0f - 3.0f * dt;
    ty_scroll += dt * (120.0f + 200.0f * ty_gust);
    for (int i = 0; i < TY_SPORES; i++) {        /* spores of light drift in from the right */
        if (!ty_spore[i].on) {
            if (ty_rand() < dt * 1.2f) {
                ty_spore[i].on = 1, ty_spore[i].x = TY_R + 20.0f, ty_spore[i].y = top + ty_rand() * (bot - top);
                ty_spore[i].ph = ty_rand() * 6.28f;
                ty_spore[i].degree = (int)((bot - ty_spore[i].y) / (bot - top) * 10.0f);
            }
            continue;
        }
        ty_spore[i].x -= dt * (110.0f + 200.0f * ty_gust);
        ty_spore[i].y += sinf((float)now * 0.002f + ty_spore[i].ph) * 22.0f * dt;
        if (ty_spore[i].x < TY_L - 20.0f)
            ty_spore[i].on = 0;
        else if ((ty_spore[i].x - ty_sx) * (ty_spore[i].x - ty_sx) + (ty_spore[i].y - ty_sy) * (ty_spore[i].y - ty_sy) < 34.0f * 34.0f) {
            ty_spore[i].on = 0;                  /* caught: it rings, the fireflies thicken */
            ty_play(ty_spore[i].degree, 100, 300u);
            ty_burst(ty_spore[i].x, ty_spore[i].y, ty_spore[i].degree % 6, 8, 170.0f);
            ty_seed_streak++, ty_seed_glow = 1.0f;
            if (ty_seed_streak > ty_seed_best)
                ty_seed_best = ty_seed_streak;
            ty_macro_soft(0, md_state()->macro[0] + 25);
        }
    }
    for (int i = 0; i < TY_BURRS; i++) {         /* burrs, slower, turning */
        if (!ty_burr[i].on) {
            if (ty_rand() < dt * 0.35f)
                ty_burr[i].on = 1, ty_burr[i].x = TY_R + 30.0f, ty_burr[i].y = top + ty_rand() * (bot - top), ty_burr[i].rot = 0;
            continue;
        }
        ty_burr[i].x -= dt * (80.0f + 150.0f * ty_gust);
        ty_burr[i].rot += dt * 2.0f;
        if (ty_burr[i].x < TY_L - 30.0f)
            ty_burr[i].on = 0;
        else if (ty_seed_hurt < 0.3f && (ty_burr[i].x - ty_sx) * (ty_burr[i].x - ty_sx) + (ty_burr[i].y - ty_sy) * (ty_burr[i].y - ty_sy) < 32.0f * 32.0f) {
            ty_seed_hurt = 1.0f, ty_seed_streak = 0;   /* a burr: the mist thickens, the streak breaks */
            ty_svx -= 260.0f;
            ty_play(0, 70, 500u);
            ty_burst(ty_sx, ty_sy, 8, 6, 120.0f);
            ty_macro_soft(3, md_state()->macro[3] + 60);
        }
    }
    if (now - ty_seed_sent_at >= 60u) {          /* where the seed is, turned into GLOW (height) and WIND (right) */
        ty_seed_sent_at = now;
        ty_macro_soft(2, (int)((bot - ty_sy) / (bot - top) * 1000.0f));
        ty_macro_soft(1, (int)((ty_sx - l) / (r - l) * 1000.0f));
    }
    for (int k = 0; k < 4; k++)                  /* (a pad's push fades unless the key is still held) */
        if (ty_seed_push[k] && !((bk_held >> (k == 0 ? BK_LEFT : k == 1 ? BK_UP : k == 2 ? BK_RIGHT : BK_DOWN)) & 1u))
            ty_seed_push[k] = 0;
}
static void ty_seed_key(int b)
{
    int k = b == BK_LEFT ? 0 : b == BK_UP ? 1 : b == BK_RIGHT ? 2 : b == BK_DOWN ? 3 : -1;
    if (k >= 0)
        ty_seed_push[k] = 1;
    else if (b == BK_A) {                        /* a gust: the world rushes, a chord */
        ty_gust = 1.0f, ty_svx += 160.0f;
        ty_play(0, 90, 400u), ty_play(2, 80, 400u), ty_play(4, 80, 400u);
        for (int q = 0; q < 8; q++)
            ty_spark(ty_sx - 10.0f, ty_sy + (ty_rand() - 0.5f) * 30.0f, -200.0f - ty_rand() * 200.0f, (ty_rand() - 0.5f) * 60.0f, 2.0f, 2.0f, 8, 0);
    } else if (b == BK_Y && ty_seed_saved) {     /* the dials back as they were */
        for (int q = 0; q < 4; q++)
            dream_cmd(MD_MACRO, q, ty_seed_mac[q]), ty_seed_sent[q] = ty_seed_mac[q];
        bk_notice("THE DIALS AS THEY WERE", "");
    }
}

/* ------------------------------------------------------------- JELLY --- */
/* a jellyfish in a deep pond (0.72): A is a pulse of its bell (up, and a soft low note), it sinks slowly between
   pulses; LEFT/RIGHT drift. Depth turns the MIST (down: more space), right turns the FIREFLIES; glowing plankton
   drifts up from below and rings when the bell touches it */
#define TY_PLANKTON 7
static float ty_jx = -1, ty_jy, ty_jvx, ty_jvy, ty_jpulse, ty_jlast_pulse;
static int ty_jdrift, ty_jcaught;
static struct { float x, y, ph; int on, degree; } ty_pk[TY_PLANKTON];

static void ty_jelly_step(float dt)
{
    const float top = TY_T + 50.0f, bot = TY_B + 10.0f, l = TY_L + 40.0f, r = TY_R - 40.0f;
    uint32_t now = bk_ms();
    if (ty_jx < 0) {
        ty_jx = (TY_L + TY_R) * 0.5f, ty_jy = (TY_T + TY_B) * 0.5f;
        if (!ty_seed_saved)
            ty_seed_enter();
    }
    ty_jvy += 34.0f * dt;                        /* (it sinks, slowly) */
    ty_jvx += (float)ty_jdrift * 260.0f * dt;
    ty_jvx *= 1.0f - 1.6f * dt, ty_jvy *= 1.0f - 1.3f * dt;
    ty_jx += ty_jvx * dt, ty_jy += ty_jvy * dt;
    if (ty_jx < l) ty_jx = l, ty_jvx = fabsf(ty_jvx) * 0.4f;
    if (ty_jx > r) ty_jx = r, ty_jvx = -fabsf(ty_jvx) * 0.4f;
    if (ty_jy < top) ty_jy = top, ty_jvy = fabsf(ty_jvy) * 0.3f;
    if (ty_jy > bot) ty_jy = bot, ty_jvy = 0;
    ty_jpulse *= 1.0f - 3.0f * dt;
    if (ty_jdrift && !((bk_held >> (ty_jdrift < 0 ? BK_LEFT : BK_RIGHT)) & 1u))
        ty_jdrift = 0;
    for (int i = 0; i < TY_PLANKTON; i++) {      /* plankton rising from the deep */
        if (!ty_pk[i].on) {
            if (ty_rand() < dt * 1.0f) {
                ty_pk[i].on = 1, ty_pk[i].x = l + ty_rand() * (r - l), ty_pk[i].y = bot + 30.0f, ty_pk[i].ph = ty_rand() * 6.28f;
                ty_pk[i].degree = (int)(ty_rand() * 8.0f);
            }
            continue;
        }
        ty_pk[i].y -= dt * (26.0f + 10.0f * sinf(ty_pk[i].ph));
        ty_pk[i].x += sinf((float)now * 0.0015f + ty_pk[i].ph) * 18.0f * dt;
        if (ty_pk[i].y < top - 30.0f)
            ty_pk[i].on = 0;
        else if ((ty_pk[i].x - ty_jx) * (ty_pk[i].x - ty_jx) + (ty_pk[i].y - ty_jy - 10.0f) * (ty_pk[i].y - ty_jy - 10.0f) < 40.0f * 40.0f) {
            ty_pk[i].on = 0;                     /* touched by the bell: it rings */
            ty_play(ty_pk[i].degree + 7, 95, 420u);
            ty_burst(ty_pk[i].x, ty_pk[i].y, ty_pk[i].degree % 6, 7, 120.0f);
            ty_jcaught++;
        }
    }
    if (now - ty_seed_sent_at >= 60u) {          /* depth: MIST; right: FIREFLIES */
        ty_seed_sent_at = now;
        ty_macro_soft(3, (int)((ty_jy - top) / (bot - top) * 1000.0f));
        ty_macro_soft(0, (int)((ty_jx - l) / (r - l) * 1000.0f));
    }
}
static void ty_jelly_key(int b)
{
    if ((b == BK_A || b == BK_UP) && bk_ms() - bk_down_at[b] > 120u)
        return;                                  /* (a held key's repeats: one pulse per press) */
    if (b == BK_A) {                             /* a pulse of the bell */
        ty_jvy -= 150.0f, ty_jpulse = 1.0f;
        ty_play(0, 70, 500u);
        for (int q = 0; q < 6; q++)
            ty_spark(ty_jx + (ty_rand() - 0.5f) * 40.0f, ty_jy + 30.0f, (ty_rand() - 0.5f) * 60.0f, 60.0f + ty_rand() * 60.0f, 2.0f, 1.6f, 7, 0);
    } else if (b == BK_LEFT || b == BK_RIGHT)
        ty_jdrift = b == BK_RIGHT ? 1 : -1;
    else if (b == BK_UP)                         /* (UP: a pulse too, for those who reach for it) */
        ty_jelly_key(BK_A);
    else if (b == BK_Y && ty_seed_saved) {
        for (int q = 0; q < 4; q++)
            dream_cmd(MD_MACRO, q, ty_seed_mac[q]), ty_seed_sent[q] = ty_seed_mac[q];
        bk_notice("THE DIALS AS THEY WERE", "");
    }
}

/* -------------------------------------------------------------- KEEP --- */
/* R2 + X: what the toys played in the last bar becomes the chosen track's sequence - sixteen steps, each note on
   the sixteenth it came nearest (0.67). R2 + A: the sequence as it was before (once). Not on the kit's track */
#define TY_REC 64
static struct { uint32_t at; int note, vel, track; } ty_rec[TY_REC];
static int ty_rec_w;
static md_sequence ty_undo_seq;
static int ty_undo_track = -1;

static void ty_record(int note, int vel, int track)
{
    ty_rec[ty_rec_w].at = bk_stamp(bk_ms()), ty_rec[ty_rec_w].note = note, ty_rec[ty_rec_w].vel = vel, ty_rec[ty_rec_w].track = track;
    ty_rec_w = (ty_rec_w + 1) % TY_REC;
}
/* the engine takes at most 64 commands between two blocks of sound, a whole sequence is some 100: they wait here
   and go in portions in ty_tick (24 at a time: also with a key's own tick in between, never over the 64) */
#define TY_CMDQ 256
static struct { int c, t, v; } ty_cmdq[TY_CMDQ];
static int ty_cmd_r, ty_cmd_w;
static void ty_cmd(int c, int t, int v)
{
    if (ty_cmd_w - ty_cmd_r < TY_CMDQ)
        ty_cmdq[ty_cmd_w++ % TY_CMDQ].c = c, ty_cmdq[(ty_cmd_w - 1) % TY_CMDQ].t = t, ty_cmdq[(ty_cmd_w - 1) % TY_CMDQ].v = v;
}
static void ty_cmd_drain(void)
{
    for (int k = 0; k < 24 && ty_cmd_r != ty_cmd_w; k++, ty_cmd_r++)
        dream_cmd(ty_cmdq[ty_cmd_r % TY_CMDQ].c, ty_cmdq[ty_cmd_r % TY_CMDQ].t, ty_cmdq[ty_cmd_r % TY_CMDQ].v);
}
static void ty_seq_write(int t, const md_sequence *q)   /* a whole sequence, by the engine's own commands */
{
    ty_cmd(MD_SEQ_LENGTH, t, q->length ? q->length : 16);
    for (int i = 0; i < q->length && i < MD_STEPS; i++) {
        const md_step *st = &q->steps[i];
        ty_cmd(MD_SEQ_STEP, t, i);
        ty_cmd(MD_SEQ_NOTE, t, st->note);
        ty_cmd(MD_SEQ_VELOCITY, t, st->velocity ? st->velocity : 80);
        ty_cmd(MD_SEQ_CHANCE, t, (int)md_step_chance(st));
        ty_cmd(MD_SEQ_GATE, t, md_step_gate(st) ? (int)md_step_gate(st) : 50);
        ty_cmd(MD_SEQ_RULE, t, (int)md_step_rule(st));
    }
    ty_cmd(MD_SEQ_STEP, t, q->edit < q->length ? q->edit : 0);
    ty_cmd(MD_SEQ_ENABLE, t, q->enabled);
}
static void ty_keep(void)
{
    const md_patch *p = md_state();
    int t = dream_ui.track, got = 0;
    uint32_t now = bk_ms(), step_ms = (uint32_t)(15000.0f / (float)(md_tempo_now() > 20 ? md_tempo_now() : 20));
    uint32_t start = now - 16u * step_ms;
    md_sequence q;
    char buf[40];
    if (p->kit.enabled && t == 3) {
        bk_notice("THE KIT'S TRACK", "KEEP WORKS ON THE OTHER TRACKS");
        return;
    }
    memset(&q, 0, sizeof q);
    q.length = 16, q.enabled = 1;
    for (int i = 0; i < 16; i++)
        q.steps[i].velocity = 80, q.steps[i].chance = 100, q.steps[i].gate = 50;
    for (int k = 0; k < TY_REC; k++) {           /* the last bar's notes on this track, each on its nearest sixteenth */
        int i = (ty_rec_w + k) % TY_REC, s;
        if (!ty_rec[i].at || ty_rec[i].track != t || (int32_t)(ty_rec[i].at - start) < 0)
            continue;
        s = (int)((ty_rec[i].at - start + step_ms / 2) / (step_ms ? step_ms : 1u)) % 16;
        if (!q.steps[s].note) {
            q.steps[s].note = (uint8_t)clamp(ty_rec[i].note, 24, 96);
            q.steps[s].velocity = (uint8_t)clamp(ty_rec[i].vel * 100 / 127, 1, 100);
            got++;
        }
    }
    if (!got) {
        bk_notice("NOTHING TO KEEP YET", "PLAY A LITTLE, THEN R2 + X");
        return;
    }
    ty_undo_seq = p->sequence[t], ty_undo_track = t;
    ty_seq_write(t, &q);
    snprintf(buf, sizeof buf, "%d NOTES ON TRACK %d", got, t + 1);
    bk_notice("KEPT AS A SEQUENCE", buf);
    ty_keeps++;
}
static void ty_keep_undo(void)
{
    if (ty_undo_track < 0) {
        bk_notice("NOTHING TO UNDO", "");
        return;
    }
    ty_seq_write(ty_undo_track, &ty_undo_seq);
    bk_notice("SEQUENCE AS BEFORE", "");
    ty_undo_track = -1;
}

/* ---------------------------------------------------------- STARFALL --- */
/* a catching game (0.79): stars fall in the tempo, one lane each (eight scale tones); a lotus cup moved with LEFT/
   RIGHT catches them and each caught one sings. Outside a round they simply fall and ring when caught; in a round
   three missed end it, and every five caught it quickens. The lanes wander like a melody, never far at once */
#define TY_FALL 10
static struct { float y, v, ph; int lane, on; } ty_fs[TY_FALL];
static int ty_fl_lane = 3, ty_fl_last = 3, ty_fl_score, ty_fl_best, ty_fl_lives, ty_fl_round, ty_fl_speed = 0, ty_fl_over;
static uint32_t ty_fl_next;
static float ty_fl_catch, ty_fl_miss;            /* (flashes: the cup lit, the sky blushing) */
static const char *const TY_FL_SPEEDS[3] = {"GENTLE", "STEADY", "QUICK"};

static float ty_fl_x(int lane) { return TY_L + (TY_R - TY_L) * ((float)lane + 0.5f) / 8.0f; }
#define TY_FL_CUP (TY_B - 52.0f)
static void ty_fl_begin(void)
{
    for (int i = 0; i < TY_FALL; i++)
        ty_fs[i].on = 0;
    ty_fl_score = 0, ty_fl_lives = 3, ty_fl_round = 1, ty_fl_over = 0;
    ty_fl_next = bk_ms() + 600u;
}
static void ty_stars_step(float dt)
{
    uint32_t now = bk_ms();
    float beat = 60000.0f / (float)(md_tempo_now() > 20 ? md_tempo_now() : 20);
    float every = beat * (ty_fl_speed == 0 ? 2.0f : ty_fl_speed == 1 ? 1.0f : 0.5f);
    float fall = (150.0f + (float)ty_fl_speed * 50.0f) * (1.0f + 0.06f * (float)(ty_fl_round ? ty_fl_score / 5 : 0));
    ty_fl_catch *= 1.0f - 4.0f * dt, ty_fl_miss *= 1.0f - 2.5f * dt;
    if (every < 180.0f)
        every = 180.0f;
    if (fall > 520.0f)
        fall = 520.0f;
    if ((int32_t)(now - ty_fl_next) >= 0 && !ty_fl_over) {   /* a new star, near the last one's lane */
        for (int i = 0; i < TY_FALL; i++)
            if (!ty_fs[i].on) {
                int step = (int)(ty_rand() * 5.0f) - 2, l = ty_fl_last + step;
                if (l < 0 || l > 7)
                    l = ty_fl_last - step;
                ty_fl_last = l < 0 ? 0 : l > 7 ? 7 : l;
                ty_fs[i].on = 1, ty_fs[i].lane = ty_fl_last, ty_fs[i].y = TY_T + 10.0f, ty_fs[i].v = fall, ty_fs[i].ph = ty_rand() * 6.28f;
                break;
            }
        ty_fl_next = now + (uint32_t)every;
    }
    for (int i = 0; i < TY_FALL; i++) {
        if (!ty_fs[i].on)
            continue;
        ty_fs[i].y += ty_fs[i].v * dt;
        if (ty_fs[i].y >= TY_FL_CUP - 8.0f && ty_fs[i].y < TY_FL_CUP + 14.0f && ty_fs[i].lane == ty_fl_lane) {   /* caught */
            ty_fs[i].on = 0;
            ty_play(ty_fs[i].lane, 96, 280u);
            ty_burst(ty_fl_x(ty_fs[i].lane), TY_FL_CUP - 10.0f, ty_fs[i].lane % 6, 5, 190.0f);
            ty_fl_catch = 1.0f;
            if (ty_fl_round && ++ty_fl_score > ty_fl_best)
                ty_fl_best = ty_fl_score;
        } else if (ty_fs[i].y > TY_B + 20.0f) {   /* missed: it sinks into the pond */
            ty_fs[i].on = 0;
            ty_spark(ty_fl_x(ty_fs[i].lane), TY_B + 10.0f, 0, 0, 8.0f, 2.0f, 7, 1);
            if (ty_fl_round) {
                ty_fl_miss = 1.0f;
                if (--ty_fl_lives <= 0)
                    ty_fl_round = 0, ty_fl_over = 1, ty_fl_next = now + 1500u;
            }
        }
    }
    if (ty_fl_over && (int32_t)(now - ty_fl_next) >= 0)   /* (after a breath, the stars fall again, freely) */
        ty_fl_over = 0;
}
static void ty_stars_key(int b)
{
    if (b == BK_LEFT || b == BK_RIGHT)
        ty_fl_lane = clamp(ty_fl_lane + (b == BK_RIGHT ? 1 : -1), 0, 7);
    else if (b == BK_A) {
        ty_fl_begin();
        bk_notice("STARFALL", "CATCH THEM - THREE MOONS");
    } else if (b == BK_Y) {
        ty_fl_speed = (ty_fl_speed + 1) % 3;
        bk_notice("STARS FALL", TY_FL_SPEEDS[ty_fl_speed]);
    }
}

/* ------------------------------------------------------------- shared --- */
static void ty_open(void)
{
    if (!ty_ready) {                             /* the first visit: two balls already in the air */
        ty_ready = 1;
        ty_throw();
        ty_throw();
    }
    for (int i = 0; i < TY_PLANETS; i++)         /* the planets start together at the gate */
        ty_pl[i].ph = 0;
    ty_swing_release();                          /* the pendulums: released together */
    ty_last_ms = bk_ms();
}

/* in bk_tick: the toy on the sound's clock; away from the playground, what still rings is let go */
static void ty_tick(void)
{
    uint32_t now = bk_ms();
    float dt;
    ty_cmd_drain();                              /* (a KEEP still going in) */
    if (bk_view != V_TOYS || lab_open) {
        ty_release(1);
        return;
    }
    dt = (float)(now - ty_last_ms) / 1000.0f;
    ty_last_ms = now;
    if (dt <= 0)
        return;
    if (dt > 0.05f)
        dt = 0.05f;
    for (int k = 0; k < 8; k++)
        ty_pad_flash[k] *= 1.0f - 3.5f * dt;
    ty_sparks_step(dt);
    while (dt > 0) {                             /* (steps of at most 6 ms: no ball through a wall) */
        float h = dt > 0.006f ? 0.006f : dt;
        if (ty_toy == TY_BOUNCE)
            ty_bounce_step(h);
        else if (ty_toy == TY_ORBITS)
            ty_orbit_step(h);
        else if (ty_toy == TY_RAIN)
            ty_rain_step(h);
        else if (ty_toy == TY_SWING)
            ty_swing_step(h);
        else if (ty_toy == TY_FIREFLY)
            ty_firefly_step(h);
        else if (ty_toy == TY_WHEEL)
            ty_wheel_step_fn(h);
        else if (ty_toy == TY_ECHO)
            ty_echo_step(h);
        else if (ty_toy == TY_SEED)
            ty_seed_step(h);
        else if (ty_toy == TY_STARS)
            ty_stars_step(h);
        else
            ty_jelly_step(h);
        dt -= h;
    }
    ty_release(0);
}

static void ty_key(int b, int times)
{
    static const char *const grav[9] = {"NONE (FLOAT)", "MOON", "LIGHT", "SOFT", "EARTH", "HEAVY", "HEAVIER", "JUPITER", "STONE"};
    char buf[40];
    if (b == BK_L1 || b == BK_R1) {
        ty_release(1);
        ty_toy = (ty_toy + (b == BK_R1 ? 1 : TY_COUNT - 1)) % TY_COUNT;
        ty_switch_at = bk_stamp(bk_ms());
        st_dirty = 1;                            /* (the toy kept for the next start) */
        for (int k = 0; k < TY_SPARKS; k++)      /* (the last toy's sparks stay with it) */
            ty_sp[k].life = 0;
        ty_last_ms = bk_ms();
        if (ty_toy == TY_SWING)
            ty_swing_release();
        bk_notice(TY_NAMES[ty_toy], ty_toy == TY_BOUNCE ? "A: THROW A BALL" : ty_toy == TY_ORBITS ? "A: A PLANET ON / OFF"
                                : ty_toy == TY_RAIN ? "LEFT/RIGHT: THE CLOUD" : ty_toy == TY_SWING ? "A: RELEASE THEM AGAIN"
                                : ty_toy == TY_FIREFLY ? "D-PAD: FLY TO THE NOTES" : ty_toy == TY_WHEEL ? "EACH RING A TRACK"
                                : ty_toy == TY_ECHO ? "A: LISTEN, THEN SING IT BACK" : ty_toy == TY_SEED ? "UP: GLOW   RIGHT: WIND   CATCH THE LIGHT"
                                : ty_toy == TY_STARS ? "LEFT/RIGHT: CATCH   A: A ROUND"
                                : "A: PULSE   DEEP: MIST   RIGHT: FIREFLIES");
        if (ty_toy == TY_SEED || ty_toy == TY_JELLY)
            ty_seed_enter();
        return;
    }
    if (b == BK_X + SQ_SHIFT) {                  /* R2 + X: keep what was played as the track's sequence */
        ty_keep();
        return;
    }
    if (b == BK_A + SQ_SHIFT) {
        ty_keep_undo();
        return;
    }
    if (b >= SQ_SHIFT)
        return;
    if (b == BK_X) {                             /* the next track that plays */
        int n = md_active_tracks();
        ty_release(1);
        dream_ui.track = n > 0 ? (dream_ui.track + 1) % n : 0;
        snprintf(buf, sizeof buf, "TRACK %d", dream_ui.track + 1);
        bk_notice("PLAYS", buf);
        return;
    }
    switch (ty_toy) {
    case TY_BOUNCE:
        if (b == BK_A)
            ty_throw();
        else if (b == BK_LEFT || b == BK_RIGHT) {
            ty_tilt = clamp(ty_tilt + (b == BK_RIGHT ? 1 : -1), -3, 3);
            snprintf(buf, sizeof buf, "%s%d", ty_tilt > 0 ? "RIGHT " : ty_tilt < 0 ? "LEFT " : "", ty_tilt < 0 ? -ty_tilt : ty_tilt);
            bk_notice("TILT", ty_tilt ? buf : "LEVEL");
        } else if (b == BK_UP || b == BK_DOWN) {
            ty_grav = clamp(ty_grav + (b == BK_DOWN ? 1 : -1), 0, 8);
            bk_notice("GRAVITY", grav[ty_grav]);
        } else if (b == BK_Y) {
            for (int i = 0; i < TY_BALLS; i++)
                ty_ball[i].on = 0;
            bk_notice("EMPTY", "A: THROW A BALL");
        }
        break;
    case TY_ORBITS:
        if (b == BK_UP || b == BK_DOWN)
            ty_sel = clamp(ty_sel + (b == BK_DOWN ? 1 : -1), 0, TY_PLANETS - 1);
        else if (b == BK_LEFT || b == BK_RIGHT) {
            ty_pl[ty_sel].ratio = clamp(ty_pl[ty_sel].ratio + (b == BK_RIGHT ? 1 : -1), 0, TY_NRATIOS - 1);
            ty_pl[ty_sel].on = 1;
            snprintf(buf, sizeof buf, "%s A BAR", TY_RATIO_NAMES[ty_pl[ty_sel].ratio]);
            bk_notice("TURNS", buf);
        } else if (b == BK_A) {
            ty_pl[ty_sel].on = !ty_pl[ty_sel].on;
            ty_pl[ty_sel].ph = 0;
            bk_notice(ty_pl[ty_sel].on ? "PLANET ON" : "PLANET RESTS", "");
        } else if (b == BK_Y) {
            ty_pl[ty_sel].degree = (ty_pl[ty_sel].degree + 1) % 12;
            ty_pl[ty_sel].on = 1;
            ty_play(ty_pl[ty_sel].degree, 90, 200u);   /* (its new note, heard at once) */
        }
        break;
    case TY_SWING:
        if (b == BK_A) {
            ty_swing_release();
            bk_notice("RELEASED", "TOGETHER AGAIN");
        } else if (b == BK_LEFT || b == BK_RIGHT) {
            ty_cycle = clamp(ty_cycle + (b == BK_RIGHT ? 1 : -1), 0, TY_NCYCLES - 1);
            ty_swing_release();
            snprintf(buf, sizeof buf, "%d S", (int)TY_CYCLES[ty_cycle]);
            bk_notice("TOGETHER EVERY", buf);
        } else if (b == BK_UP || b == BK_DOWN) {
            ty_base = clamp(ty_base + (b == BK_UP ? 2 : -2), 6, 24);
            ty_swing_release();
            bk_notice(b == BK_UP ? "FASTER" : "SLOWER", "");
        } else if (b == BK_Y) {
            ty_one_way = !ty_one_way;
            bk_notice(ty_one_way ? "ONE WAY" : "BOTH WAYS", ty_one_way ? "A NOTE PER SWING" : "A NOTE EACH WAY");
        }
        break;
    case TY_ECHO:
        ty_echo_key(b);
        break;
    case TY_SEED:
        ty_seed_key(b);
        break;
    case TY_JELLY:
        ty_jelly_key(b);
        break;
    case TY_STARS:
        ty_stars_key(b);
        break;
    case TY_WHEEL:
        if (b == BK_UP || b == BK_DOWN)
            ty_ring_sel = clamp(ty_ring_sel + (b == BK_DOWN ? 1 : -1), 0, TY_RINGS - 1);
        else if (b == BK_LEFT || b == BK_RIGHT) {
            ty_ring_[ty_ring_sel].hits = clamp(ty_ring_[ty_ring_sel].hits + (b == BK_RIGHT ? 1 : -1), 0, ty_ring_[ty_ring_sel].steps);
            ty_ring_[ty_ring_sel].on = ty_ring_[ty_ring_sel].hits > 0;
            snprintf(buf, sizeof buf, "%d IN %d", ty_ring_[ty_ring_sel].hits, ty_ring_[ty_ring_sel].steps);
            bk_notice("HITS", buf);
        } else if (b == BK_Y) {
            int n = ty_ring_[ty_ring_sel].steps % 16 + 1;
            ty_ring_[ty_ring_sel].steps = n < 3 ? 3 : n;
            ty_ring_[ty_ring_sel].hits = clamp(ty_ring_[ty_ring_sel].hits, 0, ty_ring_[ty_ring_sel].steps);
            ty_ring_[ty_ring_sel].rot %= ty_ring_[ty_ring_sel].steps;
            snprintf(buf, sizeof buf, "%d STEPS", ty_ring_[ty_ring_sel].steps);
            bk_notice("RING", buf);
        } else if (b == BK_A) {
            ty_ring_[ty_ring_sel].rot = (ty_ring_[ty_ring_sel].rot + 1) % ty_ring_[ty_ring_sel].steps;
            bk_notice("TURNED", "A STEP ON");
        }
        break;
    case TY_FIREFLY:
        if (b == BK_RIGHT) ty_fdir = 0;
        else if (b == BK_DOWN) ty_fdir = 1;
        else if (b == BK_LEFT) ty_fdir = 2;
        else if (b == BK_UP) ty_fdir = 3;
        else if (b == BK_A) {
            ty_nloop = 0, ty_loop_pos = -1;
            bk_notice("MELODY FORGOTTEN", "CATCH NEW NOTES");
        } else if (b == BK_Y) {
            ty_loop_rate = (ty_loop_rate + 1) % 3;
            bk_notice("MELODY IN", TY_RATES[ty_loop_rate]);
        }
        break;
    default:
        if (b == BK_LEFT || b == BK_RIGHT)
            ty_cloud = fminf(fmaxf(ty_cloud + (b == BK_RIGHT ? 0.04f : -0.04f) * (float)times, 0.05f), 0.95f);
        else if (b == BK_UP || b == BK_DOWN) {
            ty_rain = clamp(ty_rain + (b == BK_UP ? 1 : -1), 0, 8);
            snprintf(buf, sizeof buf, "%d / 8", ty_rain);
            bk_notice(ty_rain ? "RAIN" : "DRY", ty_rain ? buf : "UP: RAIN");
        } else if (b == BK_A) {                  /* thunder: a burst over the cloud, the root below */
            for (int i = 0; i < 7; i++)
                ty_drop_new(320.0f);
            ty_thunder = 1.0f;
            for (int k = 0; k < TY_BOLT; k++)    /* the lightning's zigzag, new each time */
                ty_bolt[k] = (ty_rand() - 0.5f) * 46.0f;
            ty_play(0, 120, 600u);
        } else if (b == BK_Y) {
            ty_in_time = !ty_in_time;
            bk_notice(ty_in_time ? "IN TIME" : "FREE RAIN", ty_in_time ? "ON THE SIXTEENTHS" : "");
        }
        break;
    }
}
uint32_t bk_toy_notes(void) { return ty_notes; }
/* a track's sequence, for the tests: its notes (when it plays) and a fingerprint of all of it */
int bk_seq_notes(int t)
{
    const md_sequence *q = &md_state()->sequence[t % 6];
    int n = 0;
    for (int i = 0; i < q->length && i < MD_STEPS; i++)
        n += q->steps[i].note != 0;
    return q->enabled ? n : -1;
}
uint32_t bk_seq_print(int t)
{
    const md_sequence *q = &md_state()->sequence[t % 6];
    uint32_t h = 2166136261u ^ q->enabled ^ ((uint32_t)q->length << 8);
    for (int i = 0; i < q->length && i < MD_STEPS; i++)
        h = (h ^ q->steps[i].note ^ ((uint32_t)q->steps[i].velocity << 8) ^ ((uint32_t)q->steps[i].chance << 16) ^ ((uint32_t)q->steps[i].gate << 24)) * 16777619u;
    return h;
}
