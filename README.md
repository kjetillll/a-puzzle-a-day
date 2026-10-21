# Solver for the a-puzzle-a-day calendar puzzle

A solver in different languages for this calendar puzzle: https://www.dragonfjord.com/product/a-puzzle-a-day/

Run the Perl program like this in Linux, WSL2 or any sensible *nix terminal:

```bash
VERBOSE=1 perl a-puzzle-a-day.pl | tee puzzle-solutions.lst         # ~3 minutes
```

...to get all 24405 unique solutions for all 366 valid days.

Or to get only one particular day, with piece layouts in terminal colors:

```bash
VERBOSE=1 perl a-puzzle-a-day.pl | grep -A8 'Feb 22'
```

To also output matching pieces for invalid dates like 'Feb 31' or 'Jan Apr':

```bash
ALLE_TREFF=1 perl a-puzzle-a-day.pl
```

Count the number of solutions for each date:

```bash
perl -nle'/\b[A-Z]\w\w \d+/&&print$&' puzzle-solutions.lst | sort | uniq -c | sort -nr
```

To compare the runtimes of around 12 different methods/method parameters:

```bash
CMP=1 STOPP=100 perl a-puzzle-a-day.pl
```

## Translations to other languages

To run the 10x faster Java program translated by Claude:

```bash
VERBOSE=1 java APuzzleADay.java | grep -A8 'Feb 22'
```

Or the 35x faster C program, also translated by Claude:

```bash
cc -O2 -o a-puzzle-a-day a-puzzle-a-day.c
VERBOSE=1 ./a-puzzle-a-day | grep -A8 'Feb 22'
```

Or the 28x faster Rust program, also translated by Claude:

```bash
rustc -O -o a-puzzle-a-day-rs a-puzzle-a-day.rs
VERBOSE=1 ./a-puzzle-a-day-rs | grep -A8 'Feb 22'
```

Or the Python program, also translated by Claude, which is only 1.3x faster:

```bash
VERBOSE=1 python3 a-puzzle-a-day.py | grep -A8 'Feb 22'
```


## Solutions / difficulty per day

Oct 6th is the hardest, with only 7 solutions, while Jan 25 is the easiest, with 216 solutions.

Averaged over all months, day 6 is the hardest with 311 solutions, and day 28 is the easiest with 1581 solutions.

The two side-by-side lists below are ordered by date and by number of solutions, respectively.

<pre>
Date↓    Solutions              Date    Solutions↓
------- ----------              ------- ----------
Jan 01          64              Oct 06           7
Jan 02         109              Apr 06           8
Jan 03          47              Jul 06          12
Jan 04         103              Oct 05          13
Jan 05          83              May 24          14
Jan 06          24              Oct 12          16
Jan 07         188              May 22          16
Jan 08          79              Mar 06          16
Jan 09         127              Mar 01          17
Jan 10          77              Mar 30          18
Jan 11          74              Sep 02          18
Jan 12          60              Jul 02          19
Jan 13         129              Sep 12          20
Jan 14          88              Sep 14          21
Jan 15          87              Feb 24          21
Jan 16          70              Jul 24          22
Jan 17         133              Oct 24          22
Jan 18          69              Feb 03          22
Jan 19         104              Mar 18          22
Jan 20         195              May 06          23
Jan 21         104              May 12          23
Jan 22          78              Jun 06          24
Jan 23         188              Mar 24          24
Jan 24          48              Jan 06          24
Jan 25         216              Mar 02          24
Jan 26          84              Jul 22          24
Jan 27          85              Aug 24          24
Jan 28         145              Sep 24          24
Jan 29          74              Sep 05          25
Jan 30         119              Oct 21          25
Jan 31         170              Feb 22          25
Feb 01          73              Feb 11          26
Feb 02          45              Oct 18          26
Feb 03          22              Apr 24          26
Feb 04          41              Dec 01          26
Feb 05          49              Oct 16          26
Feb 06          27              Feb 16          26
Feb 07          88              Sep 06          27
Feb 08          48              Mar 10          27
Feb 09          32              Feb 06          27
Feb 10          47              Oct 22          27
Feb 11          26              Jul 03          27
Feb 12          31              Oct 11          28
Feb 13          59              Oct 03          28
Feb 14          31              Feb 15          28
Feb 15          28              Jul 15          28
Feb 16          26              Mar 22          29
Feb 17          55              Mar 21          29
Feb 18          35              Oct 27          29
Feb 19          45              Oct 30          29
Feb 20          67              Feb 21          29
Feb 21          29              Sep 22          29
Feb 22          25              Mar 16          30
Feb 23          81              Dec 24          30
Feb 24          21              Feb 12          31
Feb 25          78              Feb 14          31
Feb 26          63              Jun 24          31
Feb 27          47              Dec 05          32
Feb 28          81              May 03          32
Feb 29          64              Feb 09          32
Mar 01          17              Mar 05          32
Mar 02          24              Dec 02          32
Mar 03          39              Sep 19          33
Mar 04          57              May 09          33
Mar 05          32              Nov 06          33
Mar 06          16              Sep 18          34
Mar 07          86              Jul 05          34
Mar 08          66              Oct 02          34
Mar 09          71              Sep 01          34
Mar 10          27              Feb 18          35
Mar 11          40              Nov 24          35
Mar 12          40              Jun 22          35
Mar 13          53              Oct 26          36
Mar 14          44              Sep 11          36
Mar 15          39              May 21          36
Mar 16          30              Oct 09          36
Mar 17          60              May 18          36
Mar 18          22              Sep 30          36
Mar 19          52              Jul 12          37
Mar 20          61              Jul 26          38
Mar 21          29              Dec 18          38
Mar 22          29              Jun 16          38
Mar 23          92              Sep 04          38
Mar 24          24              Sep 21          38
Mar 25          61              Sep 27          39
Mar 26          72              Mar 15          39
Mar 27          49              Mar 03          39
Mar 28          80              Dec 14          39
Mar 29          49              Jun 30          39
Mar 30          18              Nov 22          40
Mar 31         107              Jul 10          40
Apr 01          55              Mar 12          40
Apr 02          49              Aug 03          40
Apr 03          78              Jul 16          40
Apr 04          56              Mar 11          40
Apr 05          63              Aug 22          41
Apr 06           8              Feb 04          41
Apr 07         104              Apr 18          41
Apr 08         111              May 08          42
Apr 09          52              Jun 18          42
Apr 10          88              Jul 29          42
Apr 11          68              Sep 16          42
Apr 12          47              May 16          43
Apr 13          63              Jul 18          43
Apr 14          62              Jul 11          43
Apr 15          59              Oct 15          43
Apr 16          61              Sep 09          43
Apr 17          62              Jun 13          44
Apr 18          41              Dec 11          44
Apr 19          52              Aug 06          44
Apr 20         123              Jun 10          44
Apr 21          76              Mar 14          44
Apr 22          49              Dec 21          44
Apr 23         145              Feb 19          45
Apr 24          26              Jun 12          45
Apr 25         116              Feb 02          45
Apr 26          81              Dec 04          45
Apr 27          68              Sep 13          46
Apr 28         140              May 19          46
Apr 29          84              May 04          47
Apr 30          52              Feb 27          47
May 01          57              May 27          47
May 02          62              Jan 03          47
May 03          32              Feb 10          47
May 04          47              Apr 12          47
May 05          56              May 10          48
May 06          23              Jul 14          48
May 07         116              Sep 26          48
May 08          42              Dec 26          48
May 09          33              Jan 24          48
May 10          48              Feb 08          48
May 11          51              Sep 29          48
May 12          23              Jun 04          48
May 13          77              Oct 17          48
May 14          53              Mar 29          49
May 15          56              Feb 05          49
May 16          43              Apr 02          49
May 17          67              Apr 22          49
May 18          36              Mar 27          49
May 19          46              Sep 03          49
May 20          62              Jun 02          49
May 21          36              Dec 22          49
May 22          16              Jun 05          50
May 23          87              Oct 19          50
May 24          14              Oct 08          50
May 25         101              Sep 08          50
May 26          53              Sep 10          51
May 27          47              Jul 30          51
May 28         130              May 11          51
May 29          66              Oct 13          52
May 30          79              Apr 30          52
May 31          87              Apr 09          52
Jun 01          56              Sep 17          52
Jun 02          49              Jul 21          52
Jun 03          54              Mar 19          52
Jun 04          48              Apr 19          52
Jun 05          50              Jun 09          52
Jun 06          24              May 26          53
Jun 07         191              Aug 21          53
Jun 08          85              Mar 13          53
Jun 09          52              May 14          53
Jun 10          44              Oct 14          54
Jun 11          78              Sep 25          54
Jun 12          45              Jun 03          54
Jun 13          44              Dec 29          54
Jun 14          61              Apr 01          55
Jun 15          57              Feb 17          55
Jun 16          38              May 15          56
Jun 17          75              Apr 04          56
Jun 18          42              May 05          56
Jun 19          86              Oct 29          56
Jun 20         114              Jun 01          56
Jun 21          57              Aug 11          57
Jun 22          35              Mar 04          57
Jun 23         102              May 01          57
Jun 24          31              Jun 15          57
Jun 25         150              Jun 21          57
Jun 26          73              Jun 29          57
Jun 27          73              Oct 01          58
Jun 28         163              Sep 15          58
Jun 29          57              Apr 15          59
Jun 30          39              Aug 12          59
Jul 01          99              Feb 13          59
Jul 02          19              Dec 19          59
Jul 03          27              Oct 04          59
Jul 04          70              Oct 10          59
Jul 05          34              Jan 12          60
Jul 06          12              Mar 17          60
Jul 07         125              Dec 30          60
Jul 08          63              Jul 19          61
Jul 09          72              Apr 16          61
Jul 10          40              Jun 14          61
Jul 11          43              Mar 20          61
Jul 12          37              Mar 25          61
Jul 13          70              Apr 17          62
Jul 14          48              May 02          62
Jul 15          28              May 20          62
Jul 16          40              Apr 14          62
Jul 17         109              Feb 26          63
Jul 18          43              Jul 08          63
Jul 19          61              Apr 13          63
Jul 20         108              Apr 05          63
Jul 21          52              Feb 29          64
Jul 22          24              Jan 01          64
Jul 23          78              Nov 16          64
Jul 24          22              Aug 18          65
Jul 25          97              Dec 06          66
Jul 26          38              May 29          66
Jul 27          68              Mar 08          66
Jul 28         133              Dec 10          66
Jul 29          42              May 17          67
Jul 30          51              Feb 20          67
Jul 31          82              Oct 20          67
Aug 01          68              Dec 03          67
Aug 02          85              Dec 08          67
Aug 03          40              Nov 15          67
Aug 04          94              Jul 27          68
Aug 05          76              Nov 04          68
Aug 06          44              Aug 01          68
Aug 07         172              Apr 27          68
Aug 08         105              Dec 16          68
Aug 09          88              Apr 11          68
Aug 10          87              Jan 18          69
Aug 11          57              Sep 20          70
Aug 12          59              Aug 14          70
Aug 13         120              Jul 13          70
Aug 14          70              Jul 04          70
Aug 15          72              Jan 16          70
Aug 16          85              Dec 27          71
Aug 17         107              Mar 09          71
Aug 18          65              Jul 09          72
Aug 19         100              Mar 26          72
Aug 20         116              Nov 09          72
Aug 21          53              Aug 15          72
Aug 22          41              Jun 26          73
Aug 23         129              Jun 27          73
Aug 24          24              Feb 01          73
Aug 25         161              Dec 09          73
Aug 26          84              Jan 11          74
Aug 27         111              Jan 29          74
Aug 28         189              Nov 30          74
Aug 29          82              Jun 17          75
Aug 30          78              Nov 03          75
Aug 31         151              Nov 14          76
Sep 01          34              Aug 05          76
Sep 02          18              Apr 21          76
Sep 03          49              Dec 31          77
Sep 04          38              May 13          77
Sep 05          25              Jan 10          77
Sep 06          27              Apr 03          78
Sep 07          92              Jun 11          78
Sep 08          50              Feb 25          78
Sep 09          43              Dec 12          78
Sep 10          51              Nov 13          78
Sep 11          36              Aug 30          78
Sep 12          20              Jan 22          78
Sep 13          46              Sep 28          78
Sep 14          21              Jul 23          78
Sep 15          58              May 30          79
Sep 16          42              Jan 08          79
Sep 17          52              Mar 28          80
Sep 18          34              Nov 18          81
Sep 19          33              Dec 15          81
Sep 20          70              Feb 28          81
Sep 21          38              Apr 26          81
Sep 22          29              Feb 23          81
Sep 23          85              Nov 19          82
Sep 24          24              Jul 31          82
Sep 25          54              Dec 17          82
Sep 26          48              Nov 01          82
Sep 27          39              Aug 29          82
Sep 28          78              Jan 05          83
Sep 29          48              Jan 26          84
Sep 30          36              Aug 26          84
Oct 01          58              Apr 29          84
Oct 02          34              Oct 25          85
Oct 03          28              Jan 27          85
Oct 04          59              Nov 29          85
Oct 05          13              Aug 02          85
Oct 06           7              Sep 23          85
Oct 07          92              Aug 16          85
Oct 08          50              Jun 08          85
Oct 09          36              Nov 27          86
Oct 10          59              Jun 19          86
Oct 11          28              Mar 07          86
Oct 12          16              May 23          87
Oct 13          52              May 31          87
Oct 14          54              Oct 23          87
Oct 15          43              Jan 15          87
Oct 16          26              Aug 10          87
Oct 17          48              Jan 14          88
Oct 18          26              Apr 10          88
Oct 19          50              Feb 07          88
Oct 20          67              Aug 09          88
Oct 21          25              Nov 02          91
Oct 22          27              Sep 07          92
Oct 23          87              Dec 25          92
Oct 24          22              Nov 12          92
Oct 25          85              Mar 23          92
Oct 26          36              Oct 07          92
Oct 27          29              Aug 04          94
Oct 28          95              Oct 28          95
Oct 29          56              Jul 25          97
Oct 30          29              Nov 10          97
Oct 31         106              Dec 13          99
Nov 01          82              Jul 01          99
Nov 02          91              Aug 19         100
Nov 03          75              May 25         101
Nov 04          68              Nov 23         101
Nov 05         178              Nov 11         102
Nov 06          33              Jun 23         102
Nov 07         179              Dec 20         103
Nov 08         108              Jan 04         103
Nov 09          72              Jan 19         104
Nov 10          97              Nov 21         104
Nov 11         102              Apr 07         104
Nov 12          92              Jan 21         104
Nov 13          78              Aug 08         105
Nov 14          76              Nov 26         106
Nov 15          67              Oct 31         106
Nov 16          64              Mar 31         107
Nov 17         115              Aug 17         107
Nov 18          81              Jul 20         108
Nov 19          82              Nov 08         108
Nov 20         164              Jan 02         109
Nov 21         104              Jul 17         109
Nov 22          40              Apr 08         111
Nov 23         101              Aug 27         111
Nov 24          35              Jun 20         114
Nov 25         155              Dec 23         115
Nov 26         106              Nov 17         115
Nov 27          86              Aug 20         116
Nov 28         183              Apr 25         116
Nov 29          85              May 07         116
Nov 30          74              Jan 30         119
Dec 01          26              Aug 13         120
Dec 02          32              Apr 20         123
Dec 03          67              Dec 07         125
Dec 04          45              Jul 07         125
Dec 05          32              Jan 09         127
Dec 06          66              Jan 13         129
Dec 07         125              Aug 23         129
Dec 08          67              May 28         130
Dec 09          73              Jul 28         133
Dec 10          66              Jan 17         133
Dec 11          44              Apr 28         140
Dec 12          78              Apr 23         145
Dec 13          99              Jan 28         145
Dec 14          39              Jun 25         150
Dec 15          81              Aug 31         151
Dec 16          68              Nov 25         155
Dec 17          82              Aug 25         161
Dec 18          38              Jun 28         163
Dec 19          59              Nov 20         164
Dec 20         103              Dec 28         164
Dec 21          44              Jan 31         170
Dec 22          49              Aug 07         172
Dec 23         115              Nov 05         178
Dec 24          30              Nov 07         179
Dec 25          92              Nov 28         183
Dec 26          48              Jan 07         188
Dec 27          71              Jan 23         188
Dec 28         164              Aug 28         189
Dec 29          54              Jun 07         191
Dec 30          60              Jan 20         195
Dec 31          77              Jan 25         216

## To do

Google 'constraint programming solvers' to perhaps find proper generalized tools for such problems, and learn a bit about how to set up such puzzles/problems in a more general way.

