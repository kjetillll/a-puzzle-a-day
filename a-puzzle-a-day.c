/*
 * a-puzzle-a-day.c -- C translation of a-puzzle-a-day.pl
 *
 * A solver for https://www.dragonfjord.com/product/a-puzzle-a-day/
 *
 * Board and pieces are 8x8 bitmasks in a uint64_t: bit (8*row + col), row 0 on
 * top, col 0 on the left. p[0] is the board (the *blocked* cells), p[1..8] are
 * the pieces, each holding every legal placement.
 *
 * Where the Perl threads a "$funntall" string of decimal masks through the
 * search, this carries a uint64_t placed[9] instead; the printed output is the
 * same, but dato()/brett() read the array rather than re-parsing digits.
 *
 * Env vars, same as the Perl: VERBOSE, CMP, STOPP, ALLE_TREFF.
 *
 *   cc -O2 -o a-puzzle-a-day a-puzzle-a-day.c
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>

/* Term::ANSIColor :constants */
#define RED           "\033[31m"
#define GREEN         "\033[32m"
#define BLUE          "\033[34m"
#define CYAN          "\033[36m"
#define MAGENTA       "\033[35m"
#define BRIGHT_BLUE   "\033[94m"
#define BRIGHT_GREEN  "\033[92m"
#define BRIGHT_RED    "\033[91m"
#define RESET         "\033[0m"

#define NP 9                    /* board + 8 pieces */
#define MAXVAR 8                /* rotations/flips per piece */

static const char *mon[12] = { "Jan","Feb","Mar","Apr","Mai","Jun",
                               "Jul","Aug","Sep","Okt","Nov","Des" };

/* The Perl __DATA__ section, verbatim. */
static const char DATA[] =
    "      11\n"
    "      11\n"
    "       1\n"
    "       1\n"
    "       1\n"
    "       1\n"
    "   11111\n"
    "11111111\n"
    "\n"
    "1    # 4<<16\n"
    "1    # 4<<8\n"
    "111  # 4+2+1  = 4+2+1+(4<<16)+(4<<8) = 263175\n"
    "\n"
    "11   # 4<<9+4<<8\n"
    " 111 # 4+2+1      = 4+2+1+(4<<9)+(4<<8) = 3079\n"
    "\n"
    "   1 # 1<<8\n"
    "1111 # 8+4+2+1    = 271\n"
    "\n"
    " 11  # 2<<8 + 1<<8 + 4+2+1 = 775\n"
    "111\n"
    "\n"
    "11\n"
    " 1\n"
    " 11\n"
    "\n"
    "1 1\n"
    "111\n"
    "\n"
    "  1\n"
    "1111\n"
    "\n"
    "111\n"
    "111\n";

/* ---------------------------------------------------------------- tables */

typedef struct { int n; uint64_t *v; } Level;   /* one piece: count + placements */
typedef struct { Level l[NP]; } Tab;            /* @p */

static Tab base;                                /* the unpruned table */
static uint64_t *poolbuf[NP + 1][NP];           /* scratch, one set per depth */
static Tab tabpool[NP + 1];                     /* persistent tables for finn_while_p */

/* ------------------------------------------------------------- state */

static long long funnet;
static double t;
static int plev_min, plev_max;
static long long freq[NP + 2];
static int freq_seen[NP + 2];

static int verbose, stopp, alle_treff, use_cmp;

static jmp_buf died;                            /* where the Perl says `die` */
static int die_ok;

/* --------------------------------------------------------------- helpers */

static double time_fp(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void sleep_fp(double s)
{
    struct timespec ts;
    ts.tv_sec = (time_t)s;
    ts.tv_nsec = (long)((s - ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
}

/* Perl numeric context: leading integer, else 0 (so "Apr" is 0). */
static int numify(const char *s)
{
    if (!s) return 0;
    while (isspace((unsigned char)*s)) s++;
    return (*s == '-' || *s == '+' || isdigit((unsigned char)*s)) ? atoi(s) : 0;
}

static int env_num(const char *k) { return numify(getenv(k)); }

/* Perl boolean context: unset, "" and "0" are false. */
static int env_true(const char *k)
{
    const char *v = getenv(k);
    return v && *v && strcmp(v, "0") != 0;
}

static void *xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) { fputs("out of memory\n", stderr); exit(1); }
    return p;
}

/* Split on '\n', dropping trailing empty fields like Perl's split /\n/. */
static int split_lines(char *s, char **out, int max)
{
    int n = 0;
    char *q = s;
    for (;;) {
        char *e = strchr(q, '\n');
        if (n >= max) break;
        if (!e) { if (*q) out[n++] = q; break; }
        *e = '\0';
        out[n++] = q;
        q = e + 1;
    }
    while (n > 0 && out[n - 1][0] == '\0') n--;   /* trailing empties */
    return n;
}

/* rotate_cw: rows bottom-to-top become the new columns left-to-right. */
static char *rotate_cw(const char *s)
{
    char *copy = strdup(s), *lines[64];
    int h = split_lines(copy, lines, 64);
    int w = (int)strlen(lines[0]);
    char *r = xmalloc((size_t)w * (h + 1) + 1);
    int col, k, o = 0;

    for (col = 0; col < w; col++) {
        for (k = h - 1; k >= 0; k--) r[o++] = lines[k][col];
        if (col < w - 1) r[o++] = '\n';
    }
    r[o] = '\0';
    free(copy);
    return r;
}

/* flip_h: reverse the line order. */
static char *flip_h(const char *s)
{
    char *copy = strdup(s), *lines[64];
    int h = split_lines(copy, lines, 64);
    size_t len = strlen(s) + 1;
    char *r = xmalloc(len);
    int i;

    r[0] = '\0';
    for (i = h - 1; i >= 0; i--) {
        strcat(r, lines[i]);
        if (i > 0) strcat(r, "\n");
    }
    free(copy);
    return r;
}

/* Line i occupies bits 8*i..; within a line the leftmost char is the lowest bit. */
static uint64_t bits(const char *s)
{
    char *copy = strdup(s), *lines[64];
    int h = split_lines(copy, lines, 64);
    uint64_t sum = 0;
    int i, j;

    for (i = 0; i < h; i++) {
        uint64_t b = (uint64_t)1 << (8 * i);
        for (j = 0; lines[i][j]; j++, b <<= 1)
            if (lines[i][j] == '1') sum |= b;
    }
    free(copy);
    return sum;
}

/* ----------------------------------------------------------------- setup */

/*
 * Strip comments, split into blocks on a blank line, pad every line of a block
 * to the block's width, generate the 8 rotations/flips and drop duplicates
 * (%sett is global in the Perl, so duplicates are dropped across pieces too),
 * then convert each variant to a bitmask.
 */
static void parse_data(void)
{
    char *work = xmalloc(sizeof DATA), *seen[NP * MAXVAR];
    char *blocks[NP], *q;
    uint64_t var[NP][MAXVAR];
    int nvar[NP] = {0};
    int nseen = 0, nblk = 0, o = 0, i, j, k;
    const char *r = DATA;

    /* strip a trailing comment from each line, as the Perl's s/\s+#.* does */
    while (*r) {
        const char *e = strchr(r, '\n');
        size_t len = e ? (size_t)(e - r) : strlen(r);
        const char *h = memchr(r, '#', len);
        if (h && h > r && isspace((unsigned char)h[-1])) {
            while (h > r && isspace((unsigned char)h[-1])) h--;
            len = (size_t)(h - r);
        }
        memcpy(work + o, r, len);
        o += (int)len;
        work[o++] = '\n';
        if (!e) break;
        r = e + 1;
    }
    work[o] = '\0';

    /* split /\n\n/ */
    q = work;
    while (*q && nblk < NP) {
        char *e = strstr(q, "\n\n");
        blocks[nblk++] = q;
        if (!e) break;
        *e = '\0';
        q = e + 2;
    }

    for (i = 0; i < nblk; i++) {
        char *copy = strdup(blocks[i]), *lines[16], padded[512];
        int h = split_lines(copy, lines, 16), w = 0, po = 0;
        char *cur;

        for (j = 0; j < h; j++) { int L = (int)strlen(lines[j]); if (L > w) w = L; }
        for (j = 0; j < h; j++) {                       /* s/.+/sprintf"%*s",-$w,$&/ge */
            int L = (int)strlen(lines[j]);
            memcpy(padded + po, lines[j], L);
            po += L;
            while (L++ < w) padded[po++] = ' ';
            if (j < h - 1) padded[po++] = '\n';
        }
        padded[po] = '\0';
        free(copy);

        /* map { $p = $_==5 ? flip_h($p) : rotate_cw($p) } 1..8
           => rot90, rot180, rot270, rot360(=original), flip, flip+90, +180, +270 */
        cur = strdup(padded);
        for (k = 1; k <= MAXVAR; k++) {
            char *next = (k == 5) ? flip_h(cur) : rotate_cw(cur);
            int dup = 0;
            free(cur);
            cur = next;
            for (j = 0; j < nseen; j++) if (!strcmp(seen[j], cur)) { dup = 1; break; }
            if (dup) continue;
            seen[nseen++] = strdup(cur);
            var[i][nvar[i]++] = bits(cur);
        }
        free(cur);
    }

    /* $p[0] = [$p[0][3]] -- the board has no rotation (nr 3 of 0..3) */
    var[0][0] = var[0][3];
    nvar[0] = 1;

    for (i = 0; i < NP; i++) {
        base.l[i].n = nvar[i];
        base.l[i].v = xmalloc(sizeof(uint64_t) * nvar[i]);
        memcpy(base.l[i].v, var[i], sizeof(uint64_t) * nvar[i]);
    }
    for (j = 0; j < nseen; j++) free(seen[j]);
    free(work);
}

/* Every piece variant, shifted 0..60, minus those hitting the board. */
static void expand(void)
{
    uint64_t board = base.l[0].v[0];
    int i, j, s, d;

    for (i = 1; i < NP; i++) {
        int cap = base.l[i].n * 61, c = 0;
        uint64_t *out = xmalloc(sizeof(uint64_t) * cap);
        for (j = 0; j < base.l[i].n; j++)
            for (s = 0; s <= 60; s++) {                 /* hm 60 */
                uint64_t x = base.l[i].v[j] << s;
                if (!x) continue;                       /* grep $_ */
                if (x & board) continue;                /* grep !($_ & $p[0][0]) */
                out[c++] = x;
            }
        free(base.l[i].v);
        base.l[i].v = out;
        base.l[i].n = c;
    }

    for (d = 0; d <= NP; d++)
        for (i = 0; i < NP; i++)
            poolbuf[d][i] = xmalloc(sizeof(uint64_t) * (base.l[i].n ? base.l[i].n : 1));
}

static void reset_state(int a, int b)
{
    funnet = 0;
    t = time_fp();
    plev_min = a;
    plev_max = b;
}

/* --------------------------------------------------------------- output */

/*
 * The two cells left uncovered: m is the lower bit, d the higher. Bits 0..15
 * are months, 16.. are days, so a real date is a month in m and a day in d;
 * "Jan Apr" or "8 25" mean the pieces fit but the leftovers are not a date.
 */
static void dato(const uint64_t *placed, char *out, int *er_dato)
{
    char m[8] = "", d[8] = "", cell[8];
    uint64_t sum = 0;
    int i, have = 0, lim, dn;

    for (i = 0; i < NP; i++) sum |= placed[i];
    for (i = 0; i < 64; i++) {
        if ((sum >> i) & 1) continue;
        if (i < 16) snprintf(cell, sizeof cell, "%s", mon[6 * (i >> 3) + i % 8]);
        else        snprintf(cell, sizeof cell, "%d", 1 + 7 * ((i >> 3) - 2) + i % 8);
        if      (have == 0) memcpy(m, cell, sizeof cell);
        else if (have == 1) memcpy(d, cell, sizeof cell);
        have++;
    }
    sprintf(out, "%s %s", m, d);

    lim = m[0] == 'F' ? 29
        : (!strncmp(m, "Ap", 2) || !strncmp(m, "Jun", 3) || m[0] == 'S' || m[0] == 'N') ? 30
        : (m[0] >= 'A' && m[0] <= 'Z') ? 31
        : 0;
    dn = numify(d);
    *er_dato = (0 < dn && dn <= lim);
}

static const char *color_of(char c)
{
    switch (c) {
        case 'b': return RED;          case 'c': return GREEN;
        case 'd': return BLUE;         case 'e': return CYAN;
        case 'f': return MAGENTA;      case 'g': return BRIGHT_BLUE;
        case 'h': return BRIGHT_GREEN; case 'i': return BRIGHT_RED;
        default:  return NULL;
    }
}

/* The solved board, one letter per piece, coloured. */
static void brett_print(const uint64_t *placed)
{
    char str[64];
    char a = 'a';
    int i, k;

    if (!env_true("VERBOSE")) return;
    memset(str, ' ', sizeof str);
    for (k = 0; k < NP; k++, a++)
        for (i = 0; i < 64; i++)
            if ((placed[k] >> i) & 1) str[i] = a;

    for (i = 0; i < 64; i++) {
        const char *c = color_of(str[i]);
        if (c) printf("%s%c%s", c, str[i], RESET);
        else   putchar(str[i]);
        if (i % 8 == 7) putchar('\n');
    }
}

static int registrer_funn(const uint64_t *placed)
{
    char datostr[32];
    int er_dato, i;
    double dt;

    dato(placed, datostr, &er_dato);
    if (!er_dato && !alle_treff) return 1;

    dt = time_fp() - t;
    ++funnet;
    printf("%lld   %.1f sek   %lld pr/sek   funn: ", funnet, dt, (long long)(funnet / dt));
    for (i = 0; i < NP; i++) printf(" %" PRIu64, placed[i]);
    printf("   dato: %s\n", datostr);
    brett_print(placed);

    if (stopp == funnet && die_ok) longjmp(died, 1);
    return 0;
}

static void pat(uint64_t x)
{
    int r, c;
    printf("----------%" PRIu64 "\n", x ^ base.l[0].v[0]);
    for (r = 0; r < 8; r++) {
        putchar('(');
        for (c = 0; c < 8; c++) putchar(((x >> (r * 8 + c)) & 1) ? '1' : ' ');
        printf(")\n");
    }
    putchar('\n');
}

static void animer(void)
{
    uint64_t board = base.l[0].v[0];
    int i, j, k;

    for (i = 1; i < NP; i++)
        for (j = 0; j < base.l[i].n; j++) {
            uint64_t v = base.l[i].v[j];
            for (k = 0; k < 64; k++, v <<= 1)
                if (!(board & v)) { pat(board | v); putchar('\n'); sleep_fp(0.001); }
        }
}

static void dump_p(void)
{
    int i, j;
    printf("@p:\n");
    for (i = 0; i < NP; i++) {
        for (j = 0; j < base.l[i].n; j++)
            printf(j ? " %" PRIu64 : "%" PRIu64, base.l[i].v[j]);
        putchar('\n');
    }
}

static void hash_srlz(void)
{
    char kb[32], vb[32];
    int i;
    for (i = 0; i < (int)(sizeof freq / sizeof *freq); i++) {
        if (!freq_seen[i]) continue;
        sprintf(kb, "%d", i);
        sprintf(vb, "%lld", freq[i]);
        printf("%-30s %-20s\n", kb, vb);
    }
}

/* ------------------------------------------------------------- searching */

/* [ @$p[0..$n-1], map [grep !($brett&$_), @$_], @$p[$n..$#$p] ]
   Levels below n are shared as-is; from n on, placements clashing with the
   board so far are dropped. Empty levels are kept, so indexes stay aligned. */
static void prune_tab(int depth, Tab *dst, const Tab *src, int n, uint64_t brett)
{
    int i, j;
    for (i = 0; i < n; i++) dst->l[i] = src->l[i];
    for (i = n; i < NP; i++) {
        uint64_t *out = poolbuf[depth][i];
        int c = 0;
        for (j = 0; j < src->l[i].n; j++)
            if (!(brett & src->l[i].v[j])) out[c++] = src->l[i].v[j];
        dst->l[i].v = out;
        dst->l[i].n = c;
    }
}

static void finn_enkel(int n, uint64_t brett, uint64_t *placed)
{
    int i;
    if (n == NP) { registrer_funn(placed); return; }
    for (i = 0; i < base.l[n].n; i++) {
        uint64_t v = base.l[n].v[i];
        if (brett & v) continue;
        placed[n] = v;
        finn_enkel(n + 1, brett | v, placed);
    }
}

static void finn_prune(int n, uint64_t brett, uint64_t *placed, const Tab *p)
{
    Tab local;
    int i;

    if (plev_min <= n && n <= plev_max) {        /* prune */
        prune_tab(n, &local, p, n, brett);
        p = &local;
    }
    if (n == NP) { registrer_funn(placed); return; }
    for (i = 0; i < p->l[n].n; i++) {
        uint64_t v = p->l[n].v[i];
        if (brett & v) continue;
        placed[n] = v;
        finn_prune(n + 1, brett | v, placed, p);
    }
}

typedef struct { int n; uint64_t brett; uint64_t placed[NP]; const Tab *p; } Node;

static Node *stack;
static int stack_n, stack_cap;

static void push_node(int n, uint64_t brett, const uint64_t *placed, const Tab *p)
{
    if (stack_n == stack_cap) {
        stack_cap = stack_cap ? stack_cap * 2 : 1024;
        stack = realloc(stack, sizeof(Node) * stack_cap);
        if (!stack) { fputs("out of memory\n", stderr); exit(1); }
    }
    stack[stack_n].n = n;
    stack[stack_n].brett = brett;
    memcpy(stack[stack_n].placed, placed, sizeof(uint64_t) * NP);
    stack[stack_n].p = p;
    stack_n++;
}

static void finn_while(void)
{
    uint64_t placed[NP] = {0};

    stack_n = 0;
    push_node(0, 0, placed, &base);
    while (stack_n) {
        Node nd = stack[--stack_n];
        int i;
        freq[nd.n]++;
        freq_seen[nd.n] = 1;
        if (nd.n == NP) { registrer_funn(nd.placed); continue; }
        for (i = 0; i < base.l[nd.n].n; i++) {
            uint64_t v = base.l[nd.n].v[i];
            if (nd.brett & v) continue;
            nd.placed[nd.n] = v;
            push_node(nd.n + 1, nd.brett | v, nd.placed, &base);
        }
    }
    hash_srlz();
}

static void finn_while_p(void)
{
    uint64_t placed[NP] = {0};

    stack_n = 0;
    push_node(0, 0, placed, &base);
    while (stack_n) {
        Node nd = stack[--stack_n];
        const Tab *p = nd.p;
        int i;

        if (plev_min <= nd.n && nd.n <= plev_max) {     /* prune */
            prune_tab(nd.n, &tabpool[nd.n], p, nd.n, nd.brett);
            p = &tabpool[nd.n];
        }
        freq[nd.n]++;
        freq_seen[nd.n] = 1;
        if (nd.n == NP) { registrer_funn(nd.placed); continue; }
        for (i = 0; i < p->l[nd.n].n; i++) {
            uint64_t v = p->l[nd.n].v[i];
            if (nd.brett & v) continue;
            nd.placed[nd.n] = v;
            push_node(nd.n + 1, nd.brett | v, nd.placed, p);
        }
    }
    hash_srlz();
}

/* ------------------------------------------------------------ benchmarks */

#define NBENCH 12
static const char *bench_name[NBENCH] = {
    "finn_enkel", "finn_prune2", "finn_prune3", "finn_prune4", "finn_prune5",
    "finn_prune6", "finn_prune7", "finn_prune55", "finn_prune35", "finn_prune56",
    "finn_while", "finn_while_p"
};
static const int bench_plev[NBENCH][2] = {
    {0,0}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6}, {1,7}, {5,5}, {3,5}, {5,6}, {0,0}, {3,5}
};

static void run_bench(int b)
{
    uint64_t placed[NP] = {0};
    reset_state(bench_plev[b][0], bench_plev[b][1]);
    die_ok = 1;
    if (!setjmp(died)) {                          /* eval { ... } */
        if (b == 0)        finn_enkel(0, 0, placed);
        else if (b == 10)  finn_while();
        else if (b == 11)  finn_while_p();
        else               finn_prune(0, 0, placed, &base);
    }
    die_ok = 0;
}

/* Roughly Benchmark::cmpthese's table: rows slowest first, pairwise % faster. */
static void cmpthese(void)
{
    double secs[NBENCH], rate[NBENCH];
    int ord[NBENCH], i, j, b;
    char cell[NBENCH][NBENCH][16], rstr[NBENCH][24];
    int colw[NBENCH], namew = 0, ratew, per_sec;
    const char *rate_hdr;

    for (b = 0; b < NBENCH; b++) {
        double t0 = time_fp();
        run_bench(b);
        secs[b] = time_fp() - t0;
        if (secs[b] < 1e-9) secs[b] = 1e-9;
    }

    for (i = 0; i < NBENCH; i++) ord[i] = i;
    for (i = 0; i < NBENCH; i++)                  /* slowest first */
        for (j = i + 1; j < NBENCH; j++)
            if (secs[ord[j]] > secs[ord[i]]) { int x = ord[i]; ord[i] = ord[j]; ord[j] = x; }
    for (i = 0; i < NBENCH; i++) rate[i] = 1 / secs[ord[i]];

    per_sec = rate[NBENCH - 1] >= 1;
    rate_hdr = per_sec ? "Rate" : "s/iter";
    ratew = (int)strlen(rate_hdr);

    for (i = 0; i < NBENCH; i++) {
        int L = (int)strlen(bench_name[ord[i]]);
        if (L > namew) namew = L;
        if (per_sec) snprintf(rstr[i], sizeof rstr[i], "%.3g/s", rate[i]);
        else         snprintf(rstr[i], sizeof rstr[i], "%.3g", 1 / rate[i]);
        L = (int)strlen(rstr[i]);
        if (L > ratew) ratew = L;
    }
    for (i = 0; i < NBENCH; i++) {
        colw[i] = (int)strlen(bench_name[ord[i]]);
        for (j = 0; j < NBENCH; j++) {
            int L;
            if (i == j) snprintf(cell[j][i], sizeof cell[j][i], "--");
            else snprintf(cell[j][i], sizeof cell[j][i], "%.0f%%", 100 * (rate[j] / rate[i] - 1));
            L = (int)strlen(cell[j][i]);
            if (L > colw[i]) colw[i] = L;
        }
    }

    putchar('\n');
    printf("%*s %*s", namew, "", ratew, rate_hdr);
    for (i = 0; i < NBENCH; i++) printf(" %*s", colw[i], bench_name[ord[i]]);
    putchar('\n');
    for (j = 0; j < NBENCH; j++) {
        printf("%-*s %*s", namew, bench_name[ord[j]], ratew, rstr[j]);
        for (i = 0; i < NBENCH; i++) printf(" %*s", colw[i], cell[j][i]);
        putchar('\n');
    }
}

/* ------------------------------------------------------------------ main */

int main(void)
{
    uint64_t placed[NP] = {0};

    verbose    = env_num("VERBOSE");
    stopp      = env_num("STOPP");
    alle_treff = env_true("ALLE_TREFF");
    use_cmp    = env_true("CMP");

    parse_data();
    if (verbose > 1) animer();
    dump_p();
    expand();
    if (verbose > 1) dump_p();

    if (!use_cmp) {
        reset_state(3, 5);
        die_ok = 1;
        if (!setjmp(died)) finn_prune(0, 0, placed, &base);
        return 0;
    }

    cmpthese();
    return 0;
}
