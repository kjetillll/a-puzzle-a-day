# Speeding up `finn_while_p`: `sub finn_cells`

Stored as `a-puzzle-a-day-cells.pl` — a standalone copy of `a-puzzle-a-day.pl` with
`finn_cells` added, used as the default and also registered in the `cmpthese` list.
Verified end to end: **35.9s vs 180.8s, same 24405-solution set**.

## What I measured

Full search, all 24405 solutions:

| variant | time | nodes |
|---|---|---|
| `finn_while_p` (baseline) | 180.8s | 134.1M |
| + fail-fast on empty later level | 184.3s | 129.8M |
| + prune at every level, `@freq` array, no dead slice copy | 197.1s | 24.3M |
| cell-based, naive | 263.8s | 13.8M |
| **cell-based, tuned (`finn_cells`)** | **35.9s** | **13.8M** |

The instructive part is rows 2–4: **every micro-optimization I tried cut node counts
and still lost.** Pruning at every level visits 5.5x fewer nodes and runs *slower*,
because the prune itself is O(all remaining placements) per node. In Perl the
per-node cost dominates, so buying node reductions with per-node work is a losing
trade. The redundant `grep !($brett & $_)` after a prune, `%freq` vs `@freq`,
arrayref path vs string concat — all noise.

## What actually worked

Two independent things, and you need both:

**1. Cell-based instead of piece-based (10x fewer nodes).** `finn_while_p` asks
"where can piece *n* go?" — which lets it place pieces 0–5 in ways that leave an
unfillable hole and only discover it at level 8. Cell-based asks "what covers *this
cell*?": take the lowest undecided cell and either cover it with an unused piece or
spend one of the two cells that stay empty. A branch that can't cover that cell dies
immediately, and each solution has exactly one path to it. 134M → 13.8M nodes.

**2. Three constant factors (7.3x faster per node).** Naive cell-based was *slower*
than the baseline despite 10x fewer nodes. The fixes:

- `$cover[$c][$i]` holds bare masks — the naive version stored `[$piece,$mask]`
  pairs and paid `my($i,$m)=@$pm` on every candidate, hundreds per node.
- Skip a used piece as a whole list rather than testing entry by entry.
- Pass the scan position down. Cells below the one just decided are settled, so
  finding the next empty cell is amortized O(64) over a path instead of O(64) per
  node.

## Caveats

- **Solution order changes.** It's a different walk of the tree. Each solution still
  prints in the exact same format (masks stay indexed by piece, so `funn:` is still
  board, p1…p8), and the set is identical — but the sequence differs. The
  `sort | uniq -c` pipeline in the main README is unaffected; `STOPP=1` now gives a
  different first solution.
- `a-puzzle-a-day.pl` itself is untouched.
- Untested at this scale: `most-constrained-cell` ordering (pick the empty cell with
  fewest candidates) would cut nodes further, but given rows 2–4 above the per-node
  cost would likely eat it in Perl. It should pay off in the C or Rust ports, where
  per-node cost is ~50x lower.
