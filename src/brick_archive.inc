/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick, by Kid Bunka: the ARCHIVE, voices to fetch for VOICES.
 *
 * mutabledreams/archive/catalogue.tsv lists poems that are free to use (made on the Mac by
 * tools/gen_archive_catalogue.py: LibriVox recordings on archive.org, authors dead more than 70 years, a free
 * licence). The ARCHIVE view shows them by basket; A fetches one with the system's curl (its own process, fixed
 * arguments, no shell) into mutabledreams/samples/archive/<item>/, checks its size and writes a .txt beside it
 * (title; author with years, reader, licence, source) that VOICES and the radio credits read. A on a fetched poem
 * puts it into VOICES. Only what the catalogue lists can be fetched. MD_ARCHIVE_BASE replaces
 * https://archive.org/download (the tests fetch from file:// so). Included by brick_views.inc. */
#include <spawn.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
extern char **environ;

#define AR_MAX 400
typedef struct {
    char basket[24], item[96], file[96], title[96], author[64], born[8], died[8], reader[64], licence[96];
    int secs, bytes, have;
} ar_entry;
static ar_entry ar_e[AR_MAX];
static int ar_n = -1, ar_basket, ar_pos[8], ar_top[8];
static char ar_baskets[8][24];
static int ar_nbaskets;
static pid_t ar_pid;                             /* a fetch running: its curl */
static int ar_fetching = -1;                     /* its entry */
static char ar_status[96];
static uint32_t ar_status_at;

static void ar_say(const char *s)
{
    snprintf(ar_status, sizeof ar_status, "%s", s);
    ar_status_at = bk_ms();
}

static int ar_safe(const char *s)                /* item and file names: plain, so the path and the URL stay plain */
{
    if (!*s || strstr(s, ".."))
        return 0;
    for (; *s; s++)
        if (!(isalnum((unsigned char)*s) || *s == '_' || *s == '-' || *s == '.'))
            return 0;
    return 1;
}

static void ar_path(const ar_entry *e, char *out, size_t n, const char *ext)
{
    char d[700];
    vo_dir(d, sizeof d);
    snprintf(out, n, "%s/archive/%s/%s%s", d, e->item, e->file, ext);
}

static void ar_check_have(void)
{
    struct stat st;
    char p[900];
    for (int i = 0; i < ar_n; i++) {
        ar_path(&ar_e[i], p, sizeof p, "");
        ar_e[i].have = !stat(p, &st) && st.st_size > 0;
    }
}

static void ar_load(void)
{
    char path[700], line[1024];
    FILE *f;
    ar_n = 0, ar_nbaskets = 0;
    if (!bk_dir[0])
        return;
    snprintf(path, sizeof path, "%s/../archive/catalogue.tsv", bk_dir);
    if (getenv("MD_ARCHIVE_CATALOGUE"))
        snprintf(path, sizeof path, "%s", getenv("MD_ARCHIVE_CATALOGUE"));
    if (!(f = fopen(path, "r")))
        return;
    while (fgets(line, sizeof line, f) && ar_n < AR_MAX) {
        char *col[11], *s = line;
        int c = 0;
        ar_entry *e = &ar_e[ar_n];
        if (line[0] == '#')
            continue;
        line[strcspn(line, "\r\n")] = 0;
        while (c < 11) {
            col[c++] = s;
            if (!(s = strchr(s, '\t')))
                break;
            *s++ = 0;
        }
        if (c < 11)
            continue;
        snprintf(e->basket, sizeof e->basket, "%s", col[0]);
        snprintf(e->item, sizeof e->item, "%s", col[1]);
        snprintf(e->file, sizeof e->file, "%s", col[2]);
        snprintf(e->title, sizeof e->title, "%s", col[3]);
        snprintf(e->author, sizeof e->author, "%s", col[4]);
        snprintf(e->born, sizeof e->born, "%s", col[5]);
        snprintf(e->died, sizeof e->died, "%s", col[6]);
        snprintf(e->reader, sizeof e->reader, "%s", col[7]);
        e->secs = atoi(col[8]), e->bytes = atoi(col[9]);
        snprintf(e->licence, sizeof e->licence, "%s", col[10]);
        if (!ar_safe(e->item) || !ar_safe(e->file))
            continue;
        {
            int b;
            for (b = 0; b < ar_nbaskets && strcmp(ar_baskets[b], e->basket); b++)
                ;
            if (b == ar_nbaskets && ar_nbaskets < 8)
                snprintf(ar_baskets[ar_nbaskets++], 24, "%s", e->basket);
        }
        ar_n++;
    }
    fclose(f);
    ar_check_have();
}

/* the entries of basket b: their indexes into idx, their count */
static int ar_list(int b, int *idx)
{
    int n = 0;
    for (int i = 0; i < ar_n; i++)
        if (b < ar_nbaskets && !strcmp(ar_e[i].basket, ar_baskets[b]))
            idx[n++] = i;
    return n;
}

static void ar_credit(const ar_entry *e, char *out, size_t n)
{
    snprintf(out, n, "%s (%s-%s)%s%s, LibriVox, %s", e->author, e->born, e->died, e->reader[0] ? ", read by " : "", e->reader,
             strstr(e->licence, "/zero/") ? "CC0" : strstr(e->licence, "/by/") ? "CC-BY" : "public domain");
}

static void ar_fetch(int i)
{
    ar_entry *e = &ar_e[i];
    char part[900], dir[900], url[400], *argv[16];
    const char *base = getenv("MD_ARCHIVE_BASE") ? getenv("MD_ARCHIVE_BASE") : "https://archive.org/download";
    int a = 0;
    if (ar_pid)
        return;
    ar_path(e, dir, sizeof dir, "");
    *strrchr(dir, '/') = 0;
    {   /* samples/, samples/archive/, samples/archive/<item>/ */
        char d[700];
        vo_dir(d, sizeof d);
        mkdir(d, 0755);
        strncat(d, "/archive", sizeof d - strlen(d) - 1);
        mkdir(d, 0755);
        mkdir(dir, 0755);
    }
    ar_path(e, part, sizeof part, ".part");
    remove(part);
    snprintf(url, sizeof url, "%s/%s/%s", base, e->item, e->file);
    argv[a++] = "curl", argv[a++] = "-fsSL", argv[a++] = "--max-time", argv[a++] = "300", argv[a++] = "--retry", argv[a++] = "2";
    argv[a++] = "-A", argv[a++] = "MutableDreams-Brick/0.29", argv[a++] = "-o", argv[a++] = part, argv[a++] = url, argv[a] = 0;
    if (posix_spawnp(&ar_pid, "curl", 0, 0, argv, environ)) {
        ar_pid = 0;
        ar_say("NO CURL ON THIS DEVICE");
        return;
    }
    ar_fetching = i;
    ar_say("FETCHING ...");
}

static void ar_cancel(void)
{
    if (ar_pid) {
        kill(ar_pid, SIGTERM);
        waitpid(ar_pid, 0, 0);
        ar_pid = 0;
        if (ar_fetching >= 0) {
            char part[900];
            ar_path(&ar_e[ar_fetching], part, sizeof part, ".part");
            remove(part);
        }
        ar_fetching = -1;
        ar_say("STOPPED");
    }
}

/* a fetched entry into VOICES (its file, and LEVEL if it was off) */
static void ar_use(int i)
{
    char rel[200];
    snprintf(rel, sizeof rel, "archive/%s/%s", ar_e[i].item, ar_e[i].file);
    vo_scan();
    for (int k = 0; k < vo_nfiles; k++)
        if (!strcmp(vo_names[k], rel)) {
            lab_set(P_VO_FILE, (float)k);
            if (lab_v[P_VO_LEVEL] <= 0)
                lab_set(P_VO_LEVEL, 70);
            ar_say("NOW IN VOICES");
            return;
        }
    ar_say("NOT FOUND IN VOICES");
}

/* the poem i is what VOICES speaks now */
static int ar_playing(int i)
{
    char rel[200];
    int f = (int)lab_v[P_VO_FILE];
    snprintf(rel, sizeof rel, "archive/%s/%s", ar_e[i].item, ar_e[i].file);
    return lab_v[P_VO_LEVEL] > 0 && vo_nfiles > 0 && f >= 0 && f < vo_nfiles && !strcmp(vo_names[f], rel);
}

/* the fetch, followed (from bk_tick): finished, checked, named */
static void ar_poll(void)
{
    int status;
    pid_t r;
    if (!ar_pid || (r = waitpid(ar_pid, &status, WNOHANG)) == 0)
        return;
    ar_pid = 0;
    if (ar_fetching < 0)
        return;
    {
        ar_entry *e = &ar_e[ar_fetching];
        char part[900], fin[900], side[900];
        struct stat st;
        int code = r > 0 && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        ar_path(e, part, sizeof part, ".part");
        ar_path(e, fin, sizeof fin, "");
        snprintf(side, sizeof side, "%.*s.txt", (int)(strlen(fin) - 4), fin);
        if (code == 127)
            ar_say("NO CURL ON THIS DEVICE");
        else if (code) {
            char m[96];
            snprintf(m, sizeof m, "%s (CURL %d)", code == 6 || code == 7 ? "NO CONNECTION" : code == 28 ? "TOO SLOW, TRY AGAIN" :
                                                   code == 22 ? "NOT FOUND AT THE ARCHIVE" : "FETCH FAILED", code);
            ar_say(m);
        } else if (stat(part, &st) || (e->bytes > 0 && llabs((long long)st.st_size - e->bytes) > e->bytes / 50 + 1024))
            ar_say("WRONG SIZE: NOT KEPT");
        else if (rename(part, fin))
            ar_say("CANNOT STORE IT");
        else {
            FILE *f = fopen(side, "w");
            if (f) {                             /* the credit beside the voice: VOICES, the garden and the radio read it */
                char credit[300];
                ar_credit(e, credit, sizeof credit);
                fprintf(f, "%s\n%s\nhttps://archive.org/details/%s\n%s\n", e->title, credit, e->item, e->licence);
                fclose(f);
            }
            e->have = 1;
            ar_say("FETCHED. A: PLAY IT");
            vo_nfiles = -1;
        }
        remove(part);
        if (!e->have) {                          /* (nothing kept: no empty folder left behind) */
            *strrchr(fin, '/') = 0;
            rmdir(fin);
        }
        ar_fetching = -1;
    }
}

static float ar_progress(void)
{
    char part[900];
    struct stat st;
    if (ar_fetching < 0 || !ar_pid)
        return 0;
    ar_path(&ar_e[ar_fetching], part, sizeof part, ".part");
    if (stat(part, &st) || ar_e[ar_fetching].bytes <= 0)
        return 0;
    return (float)st.st_size / (float)ar_e[ar_fetching].bytes;
}

/* the SAMPLER: everything VOICES can do with a poem, on one page of the Archive (X opens it while a poem speaks):
   how it is cut and played, its grains, its tide, and the modules it may go into. A row of the MODULES opens its
   module when it is closed (its MIX up to 60 %, the rack's input on), so one turn is enough to hear it. */
enum { AR_LAB, AR_INTO, AR_RACK };
typedef struct { const char *sect, *name; int p, kind, mix; } ar_srow;
static const ar_srow AR_ROWS[] = {
    {"VOICE", "HOW IT IS CUT", P_VO_MODE, AR_LAB, 0}, {"VOICE", "WHAT STARTS IT", P_VO_TRIGGER, AR_LAB, 0},
    {"VOICE", "PITCH", P_VO_PITCH, AR_LAB, 0}, {"VOICE", "BACKWARDS", P_VO_REVERSE, AR_LAB, 0},
    {"VOICE", "OLD RADIO", P_VO_RADIO, AR_LAB, 0}, {"VOICE", "WIDTH", P_VO_SPREAD, AR_LAB, 0},
    {"VOICE", "LEVEL", P_VO_LEVEL, AR_LAB, 0},
    {"GRAINS", "WHERE IN THE POEM", P_VO_POSITION, AR_LAB, 0}, {"GRAINS", "WANDERING (SCAN)", P_VO_SCAN, AR_LAB, 0},
    {"GRAINS", "GRAIN SIZE", P_VO_SIZE, AR_LAB, 0}, {"GRAINS", "GRAINS PER SECOND", P_VO_GRAINS, AR_LAB, 0},
    {"GRAINS", "SPRAY", P_VO_SPRAY, AR_LAB, 0}, {"GRAINS", "SCATTER IN THE SCALE", P_VO_SCATTER, AR_LAB, 0},
    {"GRAINS", "DENSITY / CHANCE", P_VO_DENSITY, AR_LAB, 0}, {"GRAINS", "DEJA VU (MARBLES)", P_VO_DEJAVU, AR_LAB, 0},
    {"TIDE", "TIDE RATE", P_VO_TIDE_RATE, AR_LAB, 0}, {"TIDE", "RISE / FALL", P_VO_TIDE_SLOPE, AR_LAB, 0},
    {"TIDE", "SHAPE", P_VO_TIDE_SHAPE, AR_LAB, 0}, {"TIDE", "SMOOTH / RIPPLE", P_VO_TIDE_SMOOTH, AR_LAB, 0},
    {"TIDE", "DEPTH", P_VO_TIDE_DEPTH, AR_LAB, 0}, {"TIDE", "IT MOVES", P_VO_TIDE_TO, AR_LAB, 0},
    {"MODULES", "INTO CLOUD", P_VO_TO_CL, AR_INTO, P_CL_MIX}, {"MODULES", "INTO PARTICLES (CLOUDS)", P_VO_TO_PA, AR_INTO, P_PA_MIX},
    {"MODULES", "INTO SEEDS (BEADS)", P_VO_TO_BE, AR_INTO, P_BE_MIX}, {"MODULES", "INTO SHIMMER", P_VO_TO_SH, AR_INTO, P_SH_MIX},
    {"MODULES", "RINGS RESONATOR", P_RA_RES_MIX, AR_RACK, 0}, {"MODULES", "RINGS MODEL", P_RA_RES_MODEL, AR_RACK, 0},
    {"MODULES", "RINGS NOTE", P_RA_RES_NOTE, AR_RACK, 0}, {"MODULES", "TAPE ECHO", P_RA_ECHO_MIX, AR_RACK, 0},
    {"MODULES", "FILTER SWEEP", P_RA_SWEEP, AR_RACK, 0},
};
#define AR_NROWS ((int)(sizeof AR_ROWS / sizeof AR_ROWS[0]))
static int ar_smp, ar_row;
static uint32_t ar_row_at;                        /* (when a value last turned: the picture answers) */

static void ar_smp_turn(int dir, int times)
{
    const ar_srow *r = &AR_ROWS[ar_row];
    int p = r->p;
    float v = lab_v[p] + (float)(dir * times) * LAB_P[p].step;
    if (LAB_P[p].unit == U_ENUM && v > LAB_P[p].hi)  /* (choices go round) */
        v = LAB_P[p].lo;
    else if (LAB_P[p].unit == U_ENUM && v < LAB_P[p].lo)
        v = LAB_P[p].hi;
    lab_set(p, v);
    if (r->kind == AR_INTO && lab_v[p] > 0 && lab_v[r->mix] <= 0)
        lab_set(r->mix, 60);                      /* the module opens */
    if (r->kind == AR_RACK && lab_v[P_RA_IN_VOICES] < 0.5f)
        lab_set(P_RA_IN_VOICES, 1);               /* the voice into RACK A */
    if (p == P_VO_MODE && (int)lab_v[P_VO_MODE] == 4)
        ar_say("GRAINS: L/R ON 'WHERE IN THE POEM'");
    ar_row_at = bk_ms() ? bk_ms() : 1u;
    bk_turns++, bk_turn_dir = dir;
}

static void ar_key(int b)
{
    int idx[AR_MAX], n;
    if (ar_n < 0)
        ar_load();
    if (!ar_nbaskets)
        return;
    n = ar_list(ar_basket, idx);
    if (ar_smp) {                                /* the SAMPLER page */
        if (b == BK_UP || b == BK_DOWN)
            ar_row = (ar_row + (b == BK_DOWN ? 1 : -1) + AR_NROWS) % AR_NROWS;
        else if (b == BK_L1 || b == BK_R1) {      /* the next / last section */
            int st[8], ns = 0, ci = 0;
            for (int k = 0; k < AR_NROWS; k++)
                if (!k || strcmp(AR_ROWS[k].sect, AR_ROWS[k - 1].sect)) {
                    if (k <= ar_row)
                        ci = ns;
                    st[ns++] = k;
                }
            ar_row = st[(ci + (b == BK_R1 ? 1 : ns - 1)) % ns];
        } else if (b == BK_LEFT || b == BK_RIGHT)
            ar_smp_turn(b == BK_RIGHT ? 1 : -1, 1);
        else if (b == BK_X)
            ar_smp = 0;
        else if (b == BK_A && n) {               /* still: the chosen poem speaks (into VOICES first) or falls silent */
            int i = idx[ar_pos[ar_basket]];
            if (ar_e[i].have && !ar_playing(i))
                ar_use(i);
            else if (lab_v[P_VO_LEVEL] > 0) {
                lab_set(P_VO_LEVEL, 0);
                ar_say("SILENT. A: AGAIN");
            } else {
                lab_set(P_VO_LEVEL, 70);
                ar_say("IT SPEAKS");
            }
        } else if (b == BK_Y) {                  /* GRAINS on and off: the quickest way into the sampler */
            lab_set(P_VO_MODE, (int)lab_v[P_VO_MODE] == 4 ? 0 : 4);
            ar_say((int)lab_v[P_VO_MODE] == 4 ? "GRAINS OF THE POEM" : "WHOLE SENTENCES AGAIN");
        }
        return;
    }
    if (b == BK_L1 || b == BK_R1)
        ar_basket = (ar_basket + ar_nbaskets + (b == BK_R1 ? 1 : -1)) % ar_nbaskets;
    else if ((b == BK_UP || b == BK_DOWN) && n)
        ar_pos[ar_basket] = clamp(ar_pos[ar_basket] + (b == BK_DOWN ? 1 : -1), 0, n - 1);
    else if ((b == BK_LEFT || b == BK_RIGHT) && n)  /* a page */
        ar_pos[ar_basket] = clamp(ar_pos[ar_basket] + (b == BK_RIGHT ? 12 : -12), 0, n - 1);
    else if (b == BK_A && n) {                   /* fetch it; play it; playing: let it fall silent */
        int i = idx[ar_pos[ar_basket]];
        if (ar_e[i].have && ar_playing(i)) {
            lab_set(P_VO_LEVEL, 0);
            ar_say("SILENT. A: AGAIN");
        } else if (ar_e[i].have)
            ar_use(i);
        else if (ar_pid)
            ar_say("ONE AT A TIME: WAIT OR X STOPS");
        else
            ar_fetch(i);
    } else if (b == BK_X) {                      /* a fetch: stop it; a voice speaking: cut it another way */
        if (ar_pid)
            ar_cancel();
        else {                                    /* the SAMPLER opens (it works on what VOICES holds) */
            ar_smp = 1;
            if (lab_v[P_VO_LEVEL] <= 0)
                ar_say("A: LET IT SPEAK");
        }
    } else if (b == BK_Y) {                      /* the voice into the granular cloud, and out again */
        if (lab_v[P_VO_TO_CL] < 40) {
            lab_set(P_VO_TO_CL, 85);
            if (lab_v[P_CL_MIX] < 55)
                lab_set(P_CL_MIX, 60);
            ar_say("INTO THE CLOUD: GRAINS OF IT");
        } else {
            lab_set(P_VO_TO_CL, 0);
            ar_say("OUT OF THE CLOUD");
        }
    }
}

int bk_archive_have(int i) { if (ar_n < 0) ar_load(); return i >= 0 && i < ar_n ? ar_e[i].have : -1; }
int bk_archive_count(void) { if (ar_n < 0) ar_load(); return ar_n; }
int bk_archive_busy(void) { return ar_pid != 0; }
const char *bk_archive_status(void) { return ar_status; }
