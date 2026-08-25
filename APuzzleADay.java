import java.util.*;

/**
 * Java translation of a-puzzle-a-day.pl
 *
 * Board and pieces are 8x8 bitmasks in a long: bit (8*row + col), row 0 = top,
 * col 0 = left. p[0] is the board (the *blocked* cells), p[1..8] are the pieces,
 * each held as an array of all legal placements.
 *
 * Env vars, same as the Perl: VERBOSE, CMP, STOPP, ALLE_TREFF.
 */
public class APuzzleADay {

    // Term::ANSIColor :constants
    static final String RED = "\033[31m", GREEN = "\033[32m", BLUE = "\033[34m",
            CYAN = "\033[36m", MAGENTA = "\033[35m", BRIGHT_BLUE = "\033[94m",
            BRIGHT_GREEN = "\033[92m", BRIGHT_RED = "\033[91m", RESET = "\033[0m";

    static final String[] MON = {"Jan", "Feb", "Mar", "Apr", "Mai", "Jun",
                                 "Jul", "Aug", "Sep", "Okt", "Nov", "Des"};

    /** The Perl __DATA__ section, verbatim. */
    static final String DATA =
            "      11\n" +
            "      11\n" +
            "       1\n" +
            "       1\n" +
            "       1\n" +
            "       1\n" +
            "   11111\n" +
            "11111111\n" +
            "\n" +
            "1    # 4<<16\n" +
            "1    # 4<<8\n" +
            "111  # 4+2+1  = 4+2+1+(4<<16)+(4<<8) = 263175\n" +
            "\n" +
            "11   # 4<<9+4<<8\n" +
            " 111 # 4+2+1      = 4+2+1+(4<<9)+(4<<8) = 3079\n" +
            "\n" +
            "   1 # 1<<8\n" +
            "1111 # 8+4+2+1    = 271\n" +
            "\n" +
            " 11  # 2<<8 + 1<<8 + 4+2+1 = 775\n" +
            "111\n" +
            "\n" +
            "11\n" +
            " 1\n" +
            " 11\n" +
            "\n" +
            "1 1\n" +
            "111\n" +
            "\n" +
            "  1\n" +
            "1111\n" +
            "\n" +
            "111\n" +
            "111\n";

    private final List<long[]> p = new ArrayList<>();   // @p
    private long funnet;                                // $funnet
    private double t;                                   // $t
    private int plevMin, plevMax;                       // $plev_min, $plev_max
    private final Map<Integer, Long> freq = new TreeMap<>();

    private final int verbose = envNum("VERBOSE");
    private final long stopp = envNum("STOPP");

    /** Thrown where the Perl says `die`. */
    static class Die extends RuntimeException {
        Die() { super(null, null, false, false); }
    }

    public static void main(String[] args) {
        new APuzzleADay().run();
    }

    void run() {
        parseData();

        if (verbose > 1) animer();
        System.out.print("@p:\n" + dumpP());
        expand();
        if (verbose > 1) System.out.print("@p:\n" + dumpP());

        if (!envTrue("CMP")) {
            reset(3, 5);
            try { finnPrune(0, 0L, "", p); } catch (Die e) { /* STOPP */ }
            return;
        }

        cmpthese();
    }

    // ---------------------------------------------------------------- setup

    /**
     * Perl lines 5-9: strip comments, split into blocks on a blank line, pad every
     * line of a block to the block's width, generate the 8 rotations/flips, drop
     * duplicates (%sett is global, so duplicates are dropped across pieces too),
     * then convert each variant to a bitmask.
     */
    private void parseData() {
        // map s/\s+\#.*//r, <DATA>   -- one line at a time, then join ''
        String[] raw = DATA.split("\n", -1);
        StringBuilder joined = new StringBuilder();
        for (int i = 0; i < raw.length - 1; i++)          // last element is the "" after the final \n
            joined.append(raw[i].replaceFirst("\\s+#.*", "")).append('\n');

        Set<String> sett = new HashSet<>();               // %sett
        List<List<String>> variants = new ArrayList<>();

        for (String blk : joined.toString().split("\n\n")) {
            String[] lines = blk.split("\n", -1);
            int w = 0;
            for (String l : lines) w = Math.max(w, l.length());
            for (int i = 0; i < lines.length; i++)
                if (!lines[i].isEmpty()) lines[i] = padRight(lines[i], w);   // s/.+/%-*s/g

            // map { $p = $_==5 ? flip_h($p) : rotate_cw($p) } 1..8
            // => rot90, rot180, rot270, rot360(=original), flip, flip+90, +180, +270
            List<String> vs = new ArrayList<>();
            String cur = String.join("\n", lines);
            for (int i = 1; i <= 8; i++) {
                cur = (i == 5) ? flipH(cur) : rotateCw(cur);
                if (sett.add(cur)) vs.add(cur);
            }
            variants.add(vs);
        }

        // $p[0] = [$p[0][3]] -- the board has no rotation (nr 3 of 0..3)
        variants.set(0, new ArrayList<>(Collections.singletonList(variants.get(0).get(3))));

        for (List<String> vs : variants) {
            long[] a = new long[vs.size()];
            for (int i = 0; i < vs.size(); i++) a[i] = bits(vs.get(i));
            p.add(a);
        }
    }

    /** Perl line 19: every piece variant, shifted 0..60, minus those hitting the board. */
    private void expand() {
        long board = p.get(0)[0];
        for (int i = 1; i < p.size(); i++) {
            List<Long> out = new ArrayList<>();
            for (long v : p.get(i))
                for (int s = 0; s <= 60; s++) {          // hm 60
                    long x = v << s;
                    if (x == 0) continue;                // grep $_
                    if ((x & board) != 0) continue;      // grep !($_ & $p[0][0])
                    out.add(x);
                }
            long[] a = new long[out.size()];
            for (int j = 0; j < a.length; j++) a[j] = out.get(j);
            p.set(i, a);
        }
    }

    private void reset(int... plev) {
        funnet = 0;
        t = timeFp();
        plevMin = plev.length > 0 ? plev[0] : 0;
        plevMax = plev.length > 1 ? plev[1] : 0;
    }

    // ------------------------------------------------------------- searching

    /** $plev_min <= $n <= $plev_max -- a Perl 5.32+ chained comparison. */
    private boolean prune(int n) {
        return plevMin <= n && n <= plevMax;
    }

    private void finnEnkel(int n, long brett, String funntall) {
        if (n == 9) { registrerFunn(funntall); return; }
        for (long v : p.get(n))
            if ((brett & v) == 0)
                finnEnkel(n + 1, brett | v, funntall + " " + Long.toUnsignedString(v));
    }

    private void finnPrune(int n, long brett, String funntall, List<long[]> pp) {
        if (pp == null) pp = p;                                  // $p //= \@p
        if (prune(n)) pp = pruned(pp, n, brett);
        if (n == 9) { registrerFunn(funntall); return; }
        for (long v : pp.get(n))
            if ((brett & v) == 0)
                finnPrune(n + 1, brett | v, funntall + " " + Long.toUnsignedString(v), pp);
    }

    private void finnWhile() {
        Deque<Object[]> todo = new ArrayDeque<>();
        todo.push(new Object[]{0, 0L, ""});
        while (!todo.isEmpty()) {
            Object[] s = todo.pop();
            int n = (Integer) s[0];
            long brett = (Long) s[1];
            String funntall = (String) s[2];
            freq.merge(n, 1L, Long::sum);
            if (n == 9) { registrerFunn(funntall); continue; }
            for (long v : p.get(n))
                if ((brett & v) == 0)
                    todo.push(new Object[]{n + 1, brett | v, funntall + " " + Long.toUnsignedString(v)});
        }
        System.out.print(hashSrlz(freq));
    }

    private void finnWhileP() {
        Deque<Object[]> todo = new ArrayDeque<>();
        todo.push(new Object[]{0, 0L, "", p});
        while (!todo.isEmpty()) {
            Object[] s = todo.pop();
            int n = (Integer) s[0];
            long brett = (Long) s[1];
            String funntall = (String) s[2];
            @SuppressWarnings("unchecked") List<long[]> pp = (List<long[]>) s[3];
            if (prune(n)) pp = pruned(pp, n, brett);
            freq.merge(n, 1L, Long::sum);
            if (n == 9) { registrerFunn(funntall); continue; }
            for (long v : pp.get(n))
                if ((brett & v) == 0)
                    todo.push(new Object[]{n + 1, brett | v,
                            funntall + " " + Long.toUnsignedString(v), pp});
        }
        System.out.print(hashSrlz(freq));
    }

    /**
     * [ @$p[0..$n-1], map [grep !($brett&$_), @$_], @$p[$n..$#$p] ]
     * Levels below n are shared as-is; from n on, placements clashing with the
     * board so far are dropped. Empty levels are kept, so indexes stay aligned.
     */
    private List<long[]> pruned(List<long[]> pp, int n, long brett) {
        List<long[]> np = new ArrayList<>(pp.size());
        for (int i = 0; i < n; i++) np.add(pp.get(i));
        for (int i = n; i < pp.size(); i++) {
            long[] src = pp.get(i);
            long[] dst = new long[src.length];
            int c = 0;
            for (long v : src) if ((brett & v) == 0) dst[c++] = v;
            np.add(c == src.length ? src : Arrays.copyOf(dst, c));
        }
        return np;
    }

    // -------------------------------------------------------------- reporting

    private void registrerFunn(String funntall) {
        Dato d = dato(funntall);
        if (!d.erDato && !envTrue("ALLE_TREFF")) return;
        double dt = timeFp() - t;
        ++funnet;
        System.out.printf("%d   %.1f sek   %d pr/sek   funn: %s   dato: %s\n%s",
                funnet, dt, (long) (funnet / dt), funntall, d.str, brett(funntall));
        if (stopp == funnet) throw new Die();
    }

    static class Dato {
        final String str;
        final boolean erDato;
        Dato(String str, boolean erDato) { this.str = str; this.erDato = erDato; }
    }

    /**
     * The two cells left uncovered: $m is the lower bit, $d the higher. Bits 0..15
     * are months, 16.. are days, so a real date is a month in $m and a day in $d;
     * "Jan Apr" or "8 25" mean the pieces fit but the leftovers are not a date.
     *
     * Validity is `0 < $d <= days_in($m)` -- a chained comparison. A month/month
     * pair gives $d = 0 numerically, a day/day pair gives a limit of 0, so both
     * fail; only a real month with a day in range passes.
     */
    private Dato dato(String funntall) {
        long sum = 0;
        for (long v : nums(funntall)) sum |= v;
        List<String> free = new ArrayList<>();
        for (int i = 0; i < 64; i++) {
            if (((sum >>> i) & 1) != 0) continue;
            free.add(i < 16 ? MON[6 * (i >> 3) + i % 8]
                            : String.valueOf(1 + 7 * ((i >> 3) - 2) + i % 8));
        }
        String m = free.size() > 0 ? free.get(0) : "";
        String d = free.size() > 1 ? free.get(1) : "";
        int lim = m.startsWith("F") ? 29
                : m.startsWith("Ap") || m.startsWith("Jun") || m.startsWith("S") || m.startsWith("N") ? 30
                : !m.isEmpty() && m.charAt(0) >= 'A' && m.charAt(0) <= 'Z' ? 31
                : 0;
        int dn = numify(d);
        return new Dato(m + " " + d, 0 < dn && dn <= lim);
    }

    /** The solved board, one letter per piece, coloured. */
    private String brett(String funntall) {
        if (!envTrue("VERBOSE")) return "";
        char[] str = new char[64];
        Arrays.fill(str, ' ');
        char a = 'a';
        for (long v : nums(funntall)) {
            for (int i = 0; i < 64; i++) if (((v >>> i) & 1) != 0) str[i] = a;
            a++;
        }
        Map<Character, String> c = new HashMap<>();
        c.put('b', RED);         c.put('c', GREEN);        c.put('d', BLUE);
        c.put('e', CYAN);        c.put('f', MAGENTA);      c.put('g', BRIGHT_BLUE);
        c.put('h', BRIGHT_GREEN); c.put('i', BRIGHT_RED);

        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 64; i++) {
            String col = c.get(str[i]);
            sb.append(col == null ? String.valueOf(str[i]) : col + str[i] + RESET);
            if (i % 8 == 7) sb.append('\n');
        }
        return sb.toString();
    }

    private String pat(long x) {
        StringBuilder sb = new StringBuilder("----------")
                .append(Long.toUnsignedString(x ^ p.get(0)[0])).append('\n');
        for (int r = 0; r < 8; r++) {
            sb.append('(');
            for (int col = 0; col < 8; col++)
                sb.append(((x >>> (r * 8 + col)) & 1) != 0 ? '1' : ' ');
            sb.append(")\n");
        }
        return sb.append('\n').toString();
    }

    private void animer() {
        long board = p.get(0)[0];
        for (int i = 1; i < p.size(); i++)
            for (long v0 : p.get(i)) {
                long v = v0;
                for (int k = 0; k < 64; k++) {
                    if ((board & v) == 0) {
                        System.out.print(pat(board | v) + "\n");
                        sleepFp(0.001);
                    }
                    v <<= 1;
                }
            }
    }

    private String dumpP() {
        StringBuilder sb = new StringBuilder();
        for (long[] a : p) {
            StringJoiner sj = new StringJoiner(" ");
            for (long v : a) sj.add(Long.toUnsignedString(v));
            sb.append(sj).append('\n');
        }
        return sb.toString();
    }

    private static String hashSrlz(Map<Integer, Long> h) {
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<Integer, Long> e : h.entrySet())
            sb.append(String.format("%-30s %-20s\n", e.getKey(), e.getValue()));
        return sb.toString();
    }

    // ------------------------------------------------------------ benchmarks

    private interface Body { void run(); }

    private void ev(Body b) { try { b.run(); } catch (Die e) { /* eval{} */ } }

    private void cmpthese() {
        LinkedHashMap<String, Body> benches = new LinkedHashMap<>();
        benches.put("finn_enkel",   () -> { reset();     ev(() -> finnEnkel(0, 0L, "")); });
        benches.put("finn_prune2",  () -> { reset(1, 2); ev(() -> finnPrune(0, 0L, "", null)); });
        benches.put("finn_prune3",  () -> { reset(1, 3); ev(() -> finnPrune(0, 0L, "", null)); });
        benches.put("finn_prune4",  () -> { reset(1, 4); ev(() -> finnPrune(0, 0L, "", null)); });
        benches.put("finn_prune5",  () -> { reset(1, 5); ev(() -> finnPrune(0, 0L, "", null)); });
        benches.put("finn_prune6",  () -> { reset(1, 6); ev(() -> finnPrune(0, 0L, "", null)); });
        benches.put("finn_prune7",  () -> { reset(1, 7); ev(() -> finnPrune(0, 0L, "", null)); });
        benches.put("finn_prune55", () -> { reset(5, 5); ev(() -> finnPrune(0, 0L, "", null)); });
        benches.put("finn_prune35", () -> { reset(3, 5); ev(() -> finnPrune(0, 0L, "", null)); });
        benches.put("finn_prune56", () -> { reset(5, 6); ev(() -> finnPrune(0, 0L, "", null)); });
        benches.put("finn_while",   () -> { reset();     ev(this::finnWhile); });
        benches.put("finn_while_p", () -> { reset(3, 5); ev(this::finnWhileP); });

        Map<String, Double> secs = new LinkedHashMap<>();
        for (Map.Entry<String, Body> e : benches.entrySet()) {
            double t0 = timeFp();
            e.getValue().run();
            secs.put(e.getKey(), Math.max(timeFp() - t0, 1e-9));
        }
        printCmpTable(secs);
    }

    /** Roughly Benchmark::cmpthese's table: rows slowest first, pairwise % faster. */
    private static void printCmpTable(Map<String, Double> secs) {
        List<String> names = new ArrayList<>(secs.keySet());
        names.sort(Comparator.comparingDouble(secs::get).reversed());   // slowest = lowest rate
        double[] rate = new double[names.size()];
        for (int i = 0; i < names.size(); i++) rate[i] = 1 / secs.get(names.get(i));

        boolean perSec = rate[names.size() - 1] >= 1;
        String rateHdr = perSec ? "Rate" : "s/iter";
        int nameW = 0;
        for (String n : names) nameW = Math.max(nameW, n.length());

        String[][] cell = new String[names.size()][names.size()];
        for (int r = 0; r < names.size(); r++)
            for (int c = 0; c < names.size(); c++)
                cell[r][c] = r == c ? "--" : String.format("%.0f%%", 100 * (rate[r] / rate[c] - 1));

        int[] colW = new int[names.size()];
        for (int c = 0; c < names.size(); c++) {
            colW[c] = names.get(c).length();
            for (int r = 0; r < names.size(); r++) colW[c] = Math.max(colW[c], cell[r][c].length());
        }
        int rateW = rateHdr.length();
        String[] rateStr = new String[names.size()];
        for (int i = 0; i < names.size(); i++) {
            rateStr[i] = perSec ? String.format("%.3g/s", rate[i])
                                : String.format("%.3g", 1 / rate[i]);
            rateW = Math.max(rateW, rateStr[i].length());
        }

        System.out.println();
        StringBuilder hdr = new StringBuilder(String.format("%" + nameW + "s %" + rateW + "s", "", rateHdr));
        for (int c = 0; c < names.size(); c++) hdr.append(String.format(" %" + colW[c] + "s", names.get(c)));
        System.out.println(hdr);
        for (int r = 0; r < names.size(); r++) {
            StringBuilder row = new StringBuilder(
                    String.format("%-" + nameW + "s %" + rateW + "s", names.get(r), rateStr[r]));
            for (int c = 0; c < names.size(); c++) row.append(String.format(" %" + colW[c] + "s", cell[r][c]));
            System.out.println(row);
        }
    }

    // --------------------------------------------------------------- helpers

    /** rotate_cw: rows bottom-to-top become the new columns left-to-right. */
    static String rotateCw(String s) {
        String[] l = s.split("\n");
        int h = l.length, w = l[0].length();
        StringBuilder[] r = new StringBuilder[w];
        for (int i = 0; i < w; i++) r[i] = new StringBuilder(h);
        for (int col = 0; col < w; col++)
            for (int row = h - 1; row >= 0; row--)
                r[col].append(l[row].charAt(col));
        return String.join("\n", r);
    }

    /** flip_h: reverse the line order. */
    static String flipH(String s) {
        String[] l = s.split("\n");
        for (int i = 0, j = l.length - 1; i < j; i++, j--) { String x = l[i]; l[i] = l[j]; l[j] = x; }
        return String.join("\n", l);
    }

    /** Line i occupies bits 8*i .. ; within a line the leftmost char is the lowest bit. */
    static long bits(String s) {
        long sum = 0;
        int row = 0;
        for (String line : s.split("\n")) {
            if (line.isEmpty()) continue;               // /.+/g skips empty lines
            long b = 1L << (8 * row);
            for (int i = 0; i < line.length(); i++, b <<= 1)
                if (line.charAt(i) == '1') sum |= b;
            row++;
        }
        return sum;
    }

    static String padRight(String s, int w) {
        StringBuilder sb = new StringBuilder(s);
        while (sb.length() < w) sb.append(' ');
        return sb.toString();
    }

    /** $funntall =~ /\d+/g */
    static long[] nums(String funntall) {
        String s = funntall.trim();
        if (s.isEmpty()) return new long[0];
        String[] tok = s.split("\\s+");
        long[] a = new long[tok.length];
        for (int i = 0; i < tok.length; i++) a[i] = Long.parseUnsignedLong(tok[i]);
        return a;
    }

    static double timeFp() { return System.nanoTime() / 1e9; }

    static void sleepFp(double s) {
        try { Thread.sleep((long) (s * 1000), (int) (s * 1e9 % 1e6)); }
        catch (InterruptedException e) { Thread.currentThread().interrupt(); }
    }

    /** Perl numeric context: leading integer, else 0 (so "Apr" is 0). */
    static int numify(String s) {
        if (s == null) return 0;
        java.util.regex.Matcher m = java.util.regex.Pattern.compile("^\\s*([+-]?\\d+)").matcher(s);
        return m.find() ? Integer.parseInt(m.group(1)) : 0;
    }

    static int envNum(String k) {
        return numify(System.getenv(k));
    }

    /** Perl boolean context: undef, "" and "0" are false. */
    static boolean envTrue(String k) {
        String v = System.getenv(k);
        return v != null && !v.isEmpty() && !v.equals("0");
    }
}
