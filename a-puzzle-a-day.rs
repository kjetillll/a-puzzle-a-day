//! a-puzzle-a-day.rs -- Rust translation of a-puzzle-a-day.pl
//!
//! A solver for <https://www.dragonfjord.com/product/a-puzzle-a-day/>
//!
//! Board and pieces are 8x8 bitmasks in a u64: bit (8*row + col), row 0 on top,
//! col 0 on the left. Table level 0 is the board (the *blocked* cells), levels
//! 1..8 are the pieces, each holding every legal placement.
//!
//! Where the Perl threads a "$funntall" string of decimal masks through the
//! search, this carries a `[u64; 9]` instead; the printed output is the same,
//! but `dato`/`brett` read the array rather than re-parsing digits.
//!
//! Env vars, same as the Perl: VERBOSE, CMP, STOPP, ALLE_TREFF.
//!
//!     rustc -O -o a-puzzle-a-day a-puzzle-a-day.rs

use std::collections::BTreeMap;
use std::env;
use std::io::{self, BufWriter, Write};
use std::time::Instant;

// Term::ANSIColor :constants
const RED: &str = "\x1b[31m";
const GREEN: &str = "\x1b[32m";
const BLUE: &str = "\x1b[34m";
const CYAN: &str = "\x1b[36m";
const MAGENTA: &str = "\x1b[35m";
const BRIGHT_BLUE: &str = "\x1b[94m";
const BRIGHT_GREEN: &str = "\x1b[92m";
const BRIGHT_RED: &str = "\x1b[91m";
const RESET: &str = "\x1b[0m";

const NP: usize = 9; // board + 8 pieces

const MON: [&str; 12] = [
    "Jan", "Feb", "Mar", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Des",
];

/// The Perl `__DATA__` section, verbatim. Spelled out with explicit `\n` rather
/// than as a multi-line literal: a `\` line-continuation would eat the leading
/// spaces of the following line, which here are part of the picture.
const DATA: &str = concat!(
    "      11\n",
    "      11\n",
    "       1\n",
    "       1\n",
    "       1\n",
    "       1\n",
    "   11111\n",
    "11111111\n",
    "\n",
    "1    # 4<<16\n",
    "1    # 4<<8\n",
    "111  # 4+2+1  = 4+2+1+(4<<16)+(4<<8) = 263175\n",
    "\n",
    "11   # 4<<9+4<<8\n",
    " 111 # 4+2+1      = 4+2+1+(4<<9)+(4<<8) = 3079\n",
    "\n",
    "   1 # 1<<8\n",
    "1111 # 8+4+2+1    = 271\n",
    "\n",
    " 11  # 2<<8 + 1<<8 + 4+2+1 = 775\n",
    "111\n",
    "\n",
    "11\n",
    " 1\n",
    " 11\n",
    "\n",
    "1 1\n",
    "111\n",
    "\n",
    "  1\n",
    "1111\n",
    "\n",
    "111\n",
    "111\n",
);

/// Raised where the Perl says `die`.
struct Stop;

type R = Result<(), Stop>;

// ------------------------------------------------------------------ helpers

/// Perl's `split /\n/`: trailing empty fields are dropped.
fn lines_of(s: &str) -> Vec<&str> {
    let mut v: Vec<&str> = s.split('\n').collect();
    while v.last() == Some(&"") {
        v.pop();
    }
    v
}

/// rotate_cw: rows bottom-to-top become the new columns left-to-right.
fn rotate_cw(s: &str) -> String {
    let g: Vec<Vec<char>> = lines_of(s).iter().map(|l| l.chars().collect()).collect();
    let (h, w) = (g.len(), g[0].len());
    (0..w)
        .map(|col| (0..h).rev().map(|k| g[k][col]).collect::<String>())
        .collect::<Vec<_>>()
        .join("\n")
}

/// flip_h: reverse the line order.
fn flip_h(s: &str) -> String {
    let mut l = lines_of(s);
    l.reverse();
    l.join("\n")
}

/// Line i occupies bits 8*i..; within a line the leftmost char is the lowest bit.
fn bits(s: &str) -> u64 {
    let mut sum = 0u64;
    for (i, line) in lines_of(s).iter().enumerate() {
        let mut b = 1u64 << (8 * i);
        for c in line.chars() {
            if c == '1' {
                sum |= b;
            }
            b <<= 1;
        }
    }
    sum
}

/// Perl numeric context: leading integer, else 0 (so "Apr" is 0).
fn numify(s: &str) -> i64 {
    let t = s.trim_start();
    let end = t
        .char_indices()
        .position(|(i, c)| !(c.is_ascii_digit() || (i == 0 && (c == '-' || c == '+'))))
        .unwrap_or(t.len());
    t[..end].parse().unwrap_or(0)
}

fn env_num(k: &str) -> i64 {
    env::var(k).map(|v| numify(&v)).unwrap_or(0)
}

/// Perl boolean context: unset, "" and "0" are false.
fn env_true(k: &str) -> bool {
    matches!(env::var(k), Ok(v) if !v.is_empty() && v != "0")
}

fn color_of(c: char) -> Option<&'static str> {
    Some(match c {
        'b' => RED,
        'c' => GREEN,
        'd' => BLUE,
        'e' => CYAN,
        'f' => MAGENTA,
        'g' => BRIGHT_BLUE,
        'h' => BRIGHT_GREEN,
        'i' => BRIGHT_RED,
        _ => return None,
    })
}

// -------------------------------------------------------------------- setup

/// Strip comments, split into blocks on a blank line, pad every line of a block
/// to the block's width, generate the 8 rotations/flips and drop duplicates
/// (`%sett` is global in the Perl, so duplicates are dropped across pieces too),
/// then convert each variant to a bitmask.
fn parse_data() -> Vec<Vec<u64>> {
    let mut joined = String::new();
    for line in lines_of(DATA) {
        // s/\s+#.*// -- cut from the start of the whitespace run before the first #
        let cut = line.find('#').and_then(|h| {
            let head = &line[..h];
            let trimmed = head.trim_end();
            (trimmed.len() < head.len()).then_some(trimmed.len())
        });
        joined.push_str(cut.map_or(line, |c| &line[..c]));
        joined.push('\n');
    }

    let mut sett: Vec<String> = Vec::new();
    let mut tab: Vec<Vec<u64>> = Vec::new();

    for blk in joined.split("\n\n") {
        let l = lines_of(blk);
        if l.is_empty() {
            continue;
        }
        let w = l.iter().map(|x| x.len()).max().unwrap();
        // s/.+/sprintf"%*s",-$w,$&/ge
        let padded = l
            .iter()
            .map(|x| format!("{:<w$}", x, w = w))
            .collect::<Vec<_>>()
            .join("\n");

        // map { $p = $_==5 ? flip_h($p) : rotate_cw($p) } 1..8
        // => rot90, rot180, rot270, rot360(=original), flip, flip+90, +180, +270
        let mut var = Vec::new();
        let mut cur = padded;
        for k in 1..=8 {
            cur = if k == 5 { flip_h(&cur) } else { rotate_cw(&cur) };
            if !sett.iter().any(|s| *s == cur) {
                var.push(bits(&cur));
                sett.push(cur.clone());
            }
        }
        tab.push(var);
    }

    // $p[0] = [$p[0][3]] -- the board has no rotation (nr 3 of 0..3)
    tab[0] = vec![tab[0][3]];
    tab
}

/// Every piece variant, shifted 0..60, minus those hitting the board.
fn expand(tab: &mut [Vec<u64>]) {
    let board = tab[0][0];
    for lvl in tab.iter_mut().skip(1) {
        *lvl = lvl
            .iter()
            .flat_map(|&v| (0..=60).map(move |s| v << s)) // hm 60
            .filter(|&x| x != 0) // grep $_
            .filter(|x| x & board == 0) // grep !($_ & $p[0][0])
            .collect();
    }
}

// ---------------------------------------------------------------------- ctx

struct Ctx {
    /// `tabs[0]` is the full table; `tabs[d+1]` is the scratch a node at depth
    /// `d` prunes into. A pruned table is only ever read at levels >= d, so the
    /// levels the Perl copies from the parent are dead and left untouched.
    tabs: Vec<Vec<Vec<u64>>>,
    funnet: i64,
    t: Instant,
    plev_min: usize,
    plev_max: usize,
    freq: BTreeMap<usize, i64>,
    stopp: i64,
    alle_treff: bool,
    verbose_true: bool,
    out: BufWriter<io::Stdout>,
}

impl Ctx {
    fn new(base: Vec<Vec<u64>>) -> Ctx {
        let empty: Vec<Vec<u64>> = base.iter().map(|_| Vec::new()).collect();
        let mut tabs = vec![base];
        tabs.resize(NP + 2, empty);
        Ctx {
            tabs,
            funnet: 0,
            t: Instant::now(),
            plev_min: 0,
            plev_max: 0,
            freq: BTreeMap::new(),
            stopp: env_num("STOPP"),
            alle_treff: env_true("ALLE_TREFF"),
            verbose_true: env_true("VERBOSE"),
            out: BufWriter::new(io::stdout()),
        }
    }

    fn reset(&mut self, a: usize, b: usize) {
        self.funnet = 0;
        self.t = Instant::now();
        self.plev_min = a;
        self.plev_max = b;
    }

    fn elapsed(&self) -> f64 {
        self.t.elapsed().as_secs_f64()
    }

    /// `[ @$p[0..$n-1], map [grep !($brett&$_), @$_], @$p[$n..$#$p] ]`
    /// Placements from level n on that clash with the board so far are dropped.
    /// Empty levels are kept, so indexes stay aligned.
    fn prune_into(&mut self, src: usize, dst: usize, n: usize, brett: u64) {
        debug_assert!(src < dst);
        let (lo, hi) = self.tabs.split_at_mut(dst);
        let s = &lo[src];
        let d = &mut hi[0];
        for i in n..NP {
            d[i].clear();
            d[i].extend(s[i].iter().copied().filter(|v| brett & v == 0));
        }
    }

    // ---------------------------------------------------------- searching

    fn finn_enkel(&mut self, n: usize, brett: u64, placed: &mut [u64; NP]) -> R {
        if n == NP {
            return self.registrer_funn(placed);
        }
        for i in 0..self.tabs[0][n].len() {
            let v = self.tabs[0][n][i];
            if brett & v != 0 {
                continue;
            }
            placed[n] = v;
            self.finn_enkel(n + 1, brett | v, placed)?;
        }
        Ok(())
    }

    fn finn_prune(&mut self, n: usize, brett: u64, placed: &mut [u64; NP], ti: usize) -> R {
        let ti = if self.plev_min <= n && n <= self.plev_max {
            self.prune_into(ti, n + 1, n, brett); // prune
            n + 1
        } else {
            ti
        };
        if n == NP {
            return self.registrer_funn(placed);
        }
        for i in 0..self.tabs[ti][n].len() {
            let v = self.tabs[ti][n][i];
            if brett & v != 0 {
                continue;
            }
            placed[n] = v;
            self.finn_prune(n + 1, brett | v, placed, ti)?;
        }
        Ok(())
    }

    fn finn_while(&mut self) -> R {
        let mut todo = vec![(0usize, 0u64, [0u64; NP])];
        while let Some((n, brett, mut placed)) = todo.pop() {
            *self.freq.entry(n).or_insert(0) += 1;
            if n == NP {
                self.registrer_funn(&placed)?;
                continue;
            }
            for i in 0..self.tabs[0][n].len() {
                let v = self.tabs[0][n][i];
                if brett & v != 0 {
                    continue;
                }
                placed[n] = v;
                todo.push((n + 1, brett | v, placed));
            }
        }
        self.hash_srlz();
        Ok(())
    }

    fn finn_while_p(&mut self) -> R {
        // The stack is LIFO, so a subtree is fully drained before its parent's
        // next sibling is popped -- which is what makes one scratch table per
        // depth safe to share between siblings, exactly as the Perl shares one
        // arrayref between them.
        let mut todo = vec![(0usize, 0u64, [0u64; NP], 0usize)];
        while let Some((n, brett, mut placed, ti)) = todo.pop() {
            let ti = if self.plev_min <= n && n <= self.plev_max {
                self.prune_into(ti, n + 1, n, brett); // prune
                n + 1
            } else {
                ti
            };
            *self.freq.entry(n).or_insert(0) += 1;
            if n == NP {
                self.registrer_funn(&placed)?;
                continue;
            }
            for i in 0..self.tabs[ti][n].len() {
                let v = self.tabs[ti][n][i];
                if brett & v != 0 {
                    continue;
                }
                placed[n] = v;
                todo.push((n + 1, brett | v, placed, ti));
            }
        }
        self.hash_srlz();
        Ok(())
    }

    // ----------------------------------------------------------- reporting

    fn registrer_funn(&mut self, placed: &[u64; NP]) -> R {
        let (datostr, er_dato) = dato(placed);
        if !er_dato && !self.alle_treff {
            return Ok(());
        }
        let dt = self.elapsed();
        self.funnet += 1;
        write!(
            self.out,
            "{}   {:.1} sek   {} pr/sek   funn: ",
            self.funnet,
            dt,
            (self.funnet as f64 / dt) as i64
        )
        .unwrap();
        for v in placed {
            write!(self.out, " {}", v).unwrap();
        }
        writeln!(self.out, "   dato: {}", datostr).unwrap();
        self.brett(placed);

        if self.stopp == self.funnet {
            return Err(Stop);
        }
        Ok(())
    }

    /// The solved board, one letter per piece, coloured.
    fn brett(&mut self, placed: &[u64; NP]) {
        if !self.verbose_true {
            return;
        }
        let mut str = [b' '; 64];
        for (k, &v) in placed.iter().enumerate() {
            let a = b'a' + k as u8;
            for (i, s) in str.iter_mut().enumerate() {
                if (v >> i) & 1 != 0 {
                    *s = a;
                }
            }
        }
        for (i, &c) in str.iter().enumerate() {
            match color_of(c as char) {
                Some(col) => write!(self.out, "{}{}{}", col, c as char, RESET).unwrap(),
                None => write!(self.out, "{}", c as char).unwrap(),
            }
            if i % 8 == 7 {
                writeln!(self.out).unwrap();
            }
        }
    }

    fn dump_p(&mut self) {
        writeln!(self.out, "@p:").unwrap();
        for i in 0..NP {
            let line = self.tabs[0][i]
                .iter()
                .map(|v| v.to_string())
                .collect::<Vec<_>>()
                .join(" ");
            writeln!(self.out, "{}", line).unwrap();
        }
    }

    fn hash_srlz(&mut self) {
        for (k, v) in &self.freq {
            writeln!(self.out, "{:<30} {:<20}", k, v).unwrap();
        }
    }

    fn pat(&mut self, x: u64) {
        writeln!(self.out, "----------{}", x ^ self.tabs[0][0][0]).unwrap();
        for r in 0..8 {
            write!(self.out, "(").unwrap();
            for c in 0..8 {
                write!(self.out, "{}", if (x >> (r * 8 + c)) & 1 != 0 { '1' } else { ' ' }).unwrap();
            }
            writeln!(self.out, ")").unwrap();
        }
        writeln!(self.out).unwrap();
    }

    fn animer(&mut self) {
        let board = self.tabs[0][0][0];
        for i in 1..NP {
            for j in 0..self.tabs[0][i].len() {
                let mut v = self.tabs[0][i][j];
                for _ in 0..64 {
                    if board & v == 0 {
                        self.pat(board | v);
                        writeln!(self.out).unwrap();
                        self.out.flush().unwrap();
                        std::thread::sleep(std::time::Duration::from_micros(1000));
                    }
                    v <<= 1;
                }
            }
        }
    }
}

/// The two cells left uncovered: `m` is the lower bit, `d` the higher. Bits
/// 0..15 are months, 16.. are days, so a real date is a month in `m` and a day
/// in `d`; "Jan Apr" or "8 25" mean the pieces fit but the leftovers are not a
/// date.
fn dato(placed: &[u64; NP]) -> (String, bool) {
    let sum = placed.iter().fold(0u64, |a, b| a | b);
    let free: Vec<String> = (0..64)
        .filter(|i| (sum >> i) & 1 == 0)
        .map(|i| {
            if i < 16 {
                MON[6 * (i >> 3) + i % 8].to_string()
            } else {
                (1 + 7 * ((i >> 3) - 2) + i % 8).to_string()
            }
        })
        .collect();
    let m = free.first().map_or("", |s| s.as_str());
    let d = free.get(1).map_or("", |s| s.as_str());

    let lim = if m.starts_with('F') {
        29
    } else if m.starts_with("Ap") || m.starts_with("Jun") || m.starts_with('S') || m.starts_with('N')
    {
        30
    } else if m.starts_with(|c: char| c.is_ascii_uppercase()) {
        31
    } else {
        0
    };
    let dn = numify(d);
    (format!("{} {}", m, d), 0 < dn && dn <= lim)
}

// -------------------------------------------------------------- benchmarks

const BENCH: [(&str, usize, usize); 12] = [
    ("finn_enkel", 0, 0),
    ("finn_prune2", 1, 2),
    ("finn_prune3", 1, 3),
    ("finn_prune4", 1, 4),
    ("finn_prune5", 1, 5),
    ("finn_prune6", 1, 6),
    ("finn_prune7", 1, 7),
    ("finn_prune55", 5, 5),
    ("finn_prune35", 3, 5),
    ("finn_prune56", 5, 6),
    ("finn_while", 0, 0),
    ("finn_while_p", 3, 5),
];

/// printf's `%.3g`: three significant digits, trailing zeros dropped.
fn g3(x: f64) -> String {
    let e = if x == 0.0 { 0 } else { x.abs().log10().floor() as i32 };
    let s = format!("{:.*}", (2 - e).max(0) as usize, x);
    match s.contains('.') {
        true => s.trim_end_matches('0').trim_end_matches('.').to_string(),
        false => s,
    }
}

/// Roughly `Benchmark::cmpthese`'s table: rows slowest first, pairwise % faster.
fn cmpthese(ctx: &mut Ctx) {
    let mut secs = Vec::new();
    for (b, &(_, lo, hi)) in BENCH.iter().enumerate() {
        let t0 = Instant::now();
        ctx.reset(lo, hi);
        let mut placed = [0u64; NP];
        let _ = match b {
            0 => ctx.finn_enkel(0, 0, &mut placed), // eval { ... }
            10 => ctx.finn_while(),
            11 => ctx.finn_while_p(),
            _ => ctx.finn_prune(0, 0, &mut placed, 0),
        };
        secs.push(t0.elapsed().as_secs_f64().max(1e-9));
    }

    let mut ord: Vec<usize> = (0..BENCH.len()).collect();
    ord.sort_by(|&a, &b| secs[b].partial_cmp(&secs[a]).unwrap()); // slowest first
    let rate: Vec<f64> = ord.iter().map(|&i| 1.0 / secs[i]).collect();
    let names: Vec<&str> = ord.iter().map(|&i| BENCH[i].0).collect();

    let per_sec = *rate.last().unwrap() >= 1.0;
    let rate_hdr = if per_sec { "Rate" } else { "s/iter" };
    let rstr: Vec<String> = rate
        .iter()
        .map(|&r| {
            if per_sec {
                format!("{}/s", g3(r))
            } else {
                g3(1.0 / r)
            }
        })
        .collect();

    let cell: Vec<Vec<String>> = (0..names.len())
        .map(|r| {
            (0..names.len())
                .map(|c| {
                    if r == c {
                        "--".to_string()
                    } else {
                        format!("{:.0}%", 100.0 * (rate[r] / rate[c] - 1.0))
                    }
                })
                .collect()
        })
        .collect();

    let namew = names.iter().map(|n| n.len()).max().unwrap();
    let ratew = rstr.iter().map(|s| s.len()).max().unwrap().max(rate_hdr.len());
    let colw: Vec<usize> = (0..names.len())
        .map(|c| {
            cell.iter()
                .map(|row| row[c].len())
                .chain([names[c].len()])
                .max()
                .unwrap()
        })
        .collect();

    writeln!(ctx.out).unwrap();
    write!(ctx.out, "{:namew$} {:>ratew$}", "", rate_hdr).unwrap();
    for (c, n) in names.iter().enumerate() {
        write!(ctx.out, " {:>w$}", n, w = colw[c]).unwrap();
    }
    writeln!(ctx.out).unwrap();
    for (r, n) in names.iter().enumerate() {
        write!(ctx.out, "{:<namew$} {:>ratew$}", n, rstr[r]).unwrap();
        for c in 0..names.len() {
            write!(ctx.out, " {:>w$}", cell[r][c], w = colw[c]).unwrap();
        }
        writeln!(ctx.out).unwrap();
    }
}

// -------------------------------------------------------------------- main

fn main() {
    let verbose = env_num("VERBOSE");
    let mut ctx = Ctx::new(parse_data());

    if verbose > 1 {
        ctx.animer();
    }
    ctx.dump_p();
    expand(&mut ctx.tabs[0]);
    if verbose > 1 {
        ctx.dump_p();
    }

    // give the scratch tables the shape of the expanded one
    let shape: Vec<usize> = ctx.tabs[0].iter().map(|l| l.len()).collect();
    for d in 1..ctx.tabs.len() {
        ctx.tabs[d] = shape.iter().map(|&n| Vec::with_capacity(n)).collect();
    }

    if !env_true("CMP") {
        ctx.reset(3, 5);
        let mut placed = [0u64; NP];
        let _ = ctx.finn_prune(0, 0, &mut placed, 0);
        ctx.out.flush().unwrap();
        return;
    }

    cmpthese(&mut ctx);
    ctx.out.flush().unwrap();
}
