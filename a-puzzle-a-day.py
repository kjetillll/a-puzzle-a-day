#!/usr/bin/env python3
"""a-puzzle-a-day.py -- Python translation of a-puzzle-a-day.pl

A solver for https://www.dragonfjord.com/product/a-puzzle-a-day/

Board and pieces are 8x8 bitmasks in an int: bit (8*row + col), row 0 on top,
col 0 on the left. Table level 0 is the board (the *blocked* cells), levels 1..8
are the pieces, each holding every legal placement.

Where the Perl threads a "$funntall" string of decimal masks through the search,
this carries a list of 9 ints; the printed output is the same, but dato() and
brett() read the list rather than re-parsing digits.

Python ints are unbounded, so every shift is masked back to 64 bits to get the
truncation the Perl gets from its 64-bit UVs -- without it a piece shifted off
the top would keep its high bits, miss the board collision test and survive.

Env vars, same as the Perl: VERBOSE, CMP, STOPP, ALLE_TREFF.

    VERBOSE=1 python3 a-puzzle-a-day.py | grep -A8 'Feb 22'
"""

import os
import re
import sys
import time

# Term::ANSIColor :constants
RED = "\033[31m"
GREEN = "\033[32m"
BLUE = "\033[34m"
CYAN = "\033[36m"
MAGENTA = "\033[35m"
BRIGHT_BLUE = "\033[94m"
BRIGHT_GREEN = "\033[92m"
BRIGHT_RED = "\033[91m"
RESET = "\033[0m"

COLOR = {"b": RED, "c": GREEN, "d": BLUE, "e": CYAN,
         "f": MAGENTA, "g": BRIGHT_BLUE, "h": BRIGHT_GREEN, "i": BRIGHT_RED}

MASK64 = (1 << 64) - 1
NP = 9  # board + 8 pieces

MON = ["Jan", "Feb", "Mar", "Apr", "Mai", "Jun",
       "Jul", "Aug", "Sep", "Okt", "Nov", "Des"]

# The Perl __DATA__ section, verbatim. A backslash-newline in a Python string
# elides just the newline, so the leading spaces below are preserved.
DATA = """\
      11
      11
       1
       1
       1
       1
   11111
11111111

1    # 4<<16
1    # 4<<8
111  # 4+2+1  = 4+2+1+(4<<16)+(4<<8) = 263175

11   # 4<<9+4<<8
 111 # 4+2+1      = 4+2+1+(4<<9)+(4<<8) = 3079

   1 # 1<<8
1111 # 8+4+2+1    = 271

 11  # 2<<8 + 1<<8 + 4+2+1 = 775
111

11
 1
 11

1 1
111

  1
1111

111
111
"""


class Stop(Exception):
    """Raised where the Perl says `die`."""


# --------------------------------------------------------------- state

p = []            # @p -- the table
funnet = 0        # $funnet
t = 0.0           # $t
plev_min = 0      # $plev_min
plev_max = 0      # $plev_max
freq = {}         # %freq

stopp = 0
alle_treff = False

out = sys.stdout


# --------------------------------------------------------------- helpers

def numify(s):
    """Perl numeric context: leading integer, else 0 (so "Apr" is 0)."""
    m = re.match(r"\s*([+-]?\d+)", s or "")
    return int(m.group(1)) if m else 0


def env_num(k):
    return numify(os.environ.get(k))


def env_true(k):
    """Perl boolean context: unset, "" and "0" are false."""
    v = os.environ.get(k)
    return bool(v) and v != "0"


def lines_of(s):
    """Perl's split /\\n/: trailing empty fields are dropped."""
    v = s.split("\n")
    while v and v[-1] == "":
        v.pop()
    return v


def rotate_cw(s):
    """Rows bottom-to-top become the new columns left-to-right."""
    g = lines_of(s)
    return "\n".join("".join(g[k][col] for k in range(len(g) - 1, -1, -1))
                     for col in range(len(g[0])))


def flip_h(s):
    """Reverse the line order."""
    return "\n".join(reversed(lines_of(s)))


def bits(s):
    """Line i occupies bits 8*i..; leftmost char of a line is its lowest bit."""
    total = 0
    for i, line in enumerate(lines_of(s)):
        b = 1 << (8 * i)
        for c in line:
            if c == "1":
                total |= b
            b <<= 1
    return total & MASK64


# ----------------------------------------------------------------- setup

def parse_data():
    """Strip comments, split into blocks on a blank line, pad every line of a
    block to the block's width, generate the 8 rotations/flips and drop
    duplicates (%sett is global in the Perl, so duplicates are dropped across
    pieces too), then convert each variant to a bitmask."""
    joined = "".join(re.sub(r"\s+#.*", "", line) + "\n" for line in lines_of(DATA))

    sett = set()
    tab = []
    for blk in joined.split("\n\n"):
        lines = lines_of(blk)
        if not lines:
            continue
        w = max(len(x) for x in lines)
        padded = "\n".join(x.ljust(w) for x in lines)  # s/.+/sprintf"%*s",-$w,$&/ge

        # map { $p = $_==5 ? flip_h($p) : rotate_cw($p) } 1..8
        # => rot90, rot180, rot270, rot360(=original), flip, flip+90, +180, +270
        var, cur = [], padded
        for k in range(1, 9):
            cur = flip_h(cur) if k == 5 else rotate_cw(cur)
            if cur not in sett:
                sett.add(cur)
                var.append(bits(cur))
        tab.append(var)

    # $p[0] = [$p[0][3]] -- the board has no rotation (nr 3 of 0..3)
    tab[0] = [tab[0][3]]
    return tab


def expand(tab):
    """Every piece variant, shifted 0..60, minus those hitting the board."""
    board = tab[0][0]
    for i in range(1, NP):
        lvl = []
        for v in tab[i]:
            for s in range(61):                  # hm 60
                x = (v << s) & MASK64
                if not x:                        # grep $_
                    continue
                if x & board:                    # grep !($_ & $p[0][0])
                    continue
                lvl.append(x)
        tab[i] = lvl


def reset(a=0, b=0):
    global funnet, t, plev_min, plev_max
    funnet, t, plev_min, plev_max = 0, time.monotonic(), a, b


# -------------------------------------------------------------- reporting

def dato(placed):
    """The two cells left uncovered: m is the lower bit, d the higher. Bits
    0..15 are months, 16.. are days, so a real date is a month in m and a day in
    d; "Jan Apr" or "8 25" mean the pieces fit but the leftovers are not a date.
    """
    total = 0
    for v in placed:
        total |= v
    free = [MON[6 * (i >> 3) + i % 8] if i < 16 else str(1 + 7 * ((i >> 3) - 2) + i % 8)
            for i in range(64) if not (total >> i) & 1]
    m = free[0] if free else ""
    d = free[1] if len(free) > 1 else ""

    if m.startswith("F"):
        lim = 29
    elif m.startswith(("Ap", "Jun", "S", "N")):
        lim = 30
    elif m[:1].isupper():
        lim = 31
    else:
        lim = 0
    return "%s %s" % (m, d), 0 < numify(d) <= lim


def brett(placed):
    """The solved board, one letter per piece, coloured."""
    if not env_true("VERBOSE"):
        return ""
    cells = [" "] * 64
    for k, v in enumerate(placed):
        a = chr(ord("a") + k)
        for i in range(64):
            if (v >> i) & 1:
                cells[i] = a
    res = []
    for i, c in enumerate(cells):
        col = COLOR.get(c)
        res.append(col + c + RESET if col else c)
        if i % 8 == 7:
            res.append("\n")
    return "".join(res)


def registrer_funn(placed):
    global funnet
    datostr, er_dato = dato(placed)
    if not er_dato and not alle_treff:
        return
    dt = time.monotonic() - t
    funnet += 1
    out.write("%d   %.1f sek   %d pr/sek   funn: %s   dato: %s\n%s" % (
        funnet, dt, int(funnet / dt),
        "".join(" %d" % v for v in placed), datostr, brett(placed)))
    if stopp == funnet:
        raise Stop


def pat(x):
    rows = ["-" * 10 + str(x ^ p[0][0])]
    for r in range(8):
        rows.append("(" + "".join("1" if (x >> (r * 8 + c)) & 1 else " "
                                  for c in range(8)) + ")")
    return "\n".join(rows) + "\n\n"


def animer():
    board = p[0][0]
    for i in range(1, NP):
        for v in p[i]:
            for _ in range(64):
                if not board & v:
                    out.write(pat(board | v) + "\n")
                    out.flush()
                    time.sleep(0.001)
                v = (v << 1) & MASK64


def dump_p():
    out.write("@p:\n" + "".join(" ".join(map(str, lvl)) + "\n" for lvl in p))


def hash_srlz():
    out.write("".join("%-30s %-20s\n" % (k, freq[k]) for k in sorted(freq)))


# ------------------------------------------------------------- searching

def finn_enkel(n=0, brett_=0, placed=None):
    if placed is None:
        placed = [0] * NP
    if n == NP:
        registrer_funn(placed)
        return
    for v in p[n]:
        if brett_ & v:
            continue
        placed[n] = v
        finn_enkel(n + 1, brett_ | v, placed)


def finn_prune(n=0, brett_=0, placed=None, tab=None):
    if placed is None:
        placed = [0] * NP
    if tab is None:
        tab = p                                          # $p //= \@p
    if plev_min <= n <= plev_max:                        # prune
        tab = tab[:n] + [[v for v in lvl if not brett_ & v] for lvl in tab[n:]]
    if n == NP:
        registrer_funn(placed)
        return
    for v in tab[n]:
        if brett_ & v:
            continue
        placed[n] = v
        finn_prune(n + 1, brett_ | v, placed, tab)


def finn_while():
    todo = [(0, 0, [0] * NP)]
    while todo:
        n, brett_, placed = todo.pop()
        freq[n] = freq.get(n, 0) + 1
        if n == NP:
            registrer_funn(placed)
            continue
        for v in p[n]:
            if brett_ & v:
                continue
            nxt = placed[:]
            nxt[n] = v
            todo.append((n + 1, brett_ | v, nxt))
    hash_srlz()


def finn_while_p():
    todo = [(0, 0, [0] * NP, p)]
    while todo:
        n, brett_, placed, tab = todo.pop()
        if plev_min <= n <= plev_max:                    # prune
            tab = tab[:n] + [[v for v in lvl if not brett_ & v] for lvl in tab[n:]]
        freq[n] = freq.get(n, 0) + 1
        if n == NP:
            registrer_funn(placed)
            continue
        for v in tab[n]:
            if brett_ & v:
                continue
            nxt = placed[:]
            nxt[n] = v
            todo.append((n + 1, brett_ | v, nxt, tab))
    hash_srlz()


# ------------------------------------------------------------ benchmarks

BENCH = [
    ("finn_enkel", None, None), ("finn_prune2", 1, 2), ("finn_prune3", 1, 3),
    ("finn_prune4", 1, 4), ("finn_prune5", 1, 5), ("finn_prune6", 1, 6),
    ("finn_prune7", 1, 7), ("finn_prune55", 5, 5), ("finn_prune35", 3, 5),
    ("finn_prune56", 5, 6), ("finn_while", None, None), ("finn_while_p", 3, 5),
]


def g3(x):
    """printf's %.3g: three significant digits, trailing zeros dropped."""
    s = "%.3g" % x
    return s


def cmpthese():
    secs = []
    for i, (name, lo, hi) in enumerate(BENCH):
        t0 = time.monotonic()
        reset(*(() if lo is None else (lo, hi)))
        try:                                             # eval { ... }
            if i == 0:
                finn_enkel()
            elif i == 10:
                finn_while()
            elif i == 11:
                finn_while_p()
            else:
                finn_prune()
        except Stop:
            pass
        secs.append(max(time.monotonic() - t0, 1e-9))

    order = sorted(range(len(BENCH)), key=lambda i: -secs[i])   # slowest first
    names = [BENCH[i][0] for i in order]
    rate = [1 / secs[i] for i in order]

    per_sec = rate[-1] >= 1
    rate_hdr = "Rate" if per_sec else "s/iter"
    rstr = [g3(r) + "/s" if per_sec else g3(1 / r) for r in rate]
    cell = [["--" if r == c else "%.0f%%" % (100 * (rate[r] / rate[c] - 1))
             for c in range(len(names))] for r in range(len(names))]

    namew = max(len(n) for n in names)
    ratew = max([len(rate_hdr)] + [len(s) for s in rstr])
    colw = [max([len(names[c])] + [len(cell[r][c]) for r in range(len(names))])
            for c in range(len(names))]

    out.write("\n" + " " * namew + " " + rate_hdr.rjust(ratew))
    out.write("".join(" " + names[c].rjust(colw[c]) for c in range(len(names))) + "\n")
    for r, name in enumerate(names):
        out.write(name.ljust(namew) + " " + rstr[r].rjust(ratew))
        out.write("".join(" " + cell[r][c].rjust(colw[c]) for c in range(len(names))) + "\n")


# ------------------------------------------------------------------ main

def main():
    global p, stopp, alle_treff
    verbose = env_num("VERBOSE")
    stopp = env_num("STOPP")
    alle_treff = env_true("ALLE_TREFF")

    p = parse_data()
    if verbose > 1:
        animer()
    dump_p()
    expand(p)
    if verbose > 1:
        dump_p()

    if not env_true("CMP"):
        reset(3, 5)
        try:
            finn_prune()
        except Stop:
            pass
        return

    cmpthese()


if __name__ == "__main__":
    try:
        main()
    finally:
        out.flush()
