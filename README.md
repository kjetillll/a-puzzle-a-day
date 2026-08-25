A Perl solver for this calendar puzzle: https://www.dragonfjord.com/product/a-puzzle-a-day/

Run:

perl a-puzzle-a-day.pl | tee puzzle-solutions.lst          # ~3 minutes

...to get all 24405 uniqe solutions for all 366 valid days.

Or to get only one particular day with piece layouts with terminal colors:

VERBOSE=1 perl a-puzzle-a-day.pl|grep -A8 'Aug 31'

To output also matching pieces but invalid dates like 'Feb 31' or 'Jan Apr':

ALLE_TREFF=1 perl a-puzzle-a-day.pl

Count the number of solutions for each date:

perl -nle'/\b[A-Z]\w\w \d+/&&print$&' puzzle-solutions.lst |sort|uniq -c|sort -nr

Oct 6th is the hardest with only 7 solutions while Jan 25 is easiest with 216 solutions.

On average over all months, day 6 is hardest with 311 solutions (all months) and day 28
is easiest with 1581 solutions.

To compare runtimes of around 12 different methods/methodparams:

CMP=1 STOPP=100 perl a-puzzle-a-day.pl

Google 'constraint solvers' for how to solve such puzzles in a more general way.
