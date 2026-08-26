#!/usr/bin/perl

=head1 Usage

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

=cut

#--uses only standards modules, no extra installs
use List::Util qw(max);
use Term::ANSIColor qw(colored :constants);
use Benchmark qw(cmpthese);
my @mon = qw(Jan Feb Mar Apr Mai Jun Jul Aug Sep Okt Nov Des);
my @p = map{ my $w = max(map length($_),split/\n/); s/.+/sprintf"%*s",-$w,$&/ger }
        split /\n\n/, join '', map s/\s+\#.*//r, <DATA>;
@p = map {my $p = $_; [grep !$sett{$_}++, map { ($p = $_==5 ? flip_h($p) : rotate_cw($p) ) } 1..8 ] } @p;
$p[0] = [$p[0][3]];  #brettet har ingen rotasjon (dvs nr3 av 0..3, ingen flipp, ingen forskyvning
for(@p){$_=bits($_) for@$_}
if( $ENV{VERBOSE} > 1 ){ #animer
    for my $p (map @$_, @p[1..$#p]){
	for(0..63){
	    print pat( $p[0][0] | $p),"\n" and sleep_fp(0.001) if not $p[0][0] & $p;
	    $p<<=1;
	}
    }
}
print "\@p:\n".join('',map"@$_\n",@p);
$_ = [ map{ my $p = $_; grep !( $_ & $p[0][0]), grep $_, map $p<<$_, 0..60 } @$_ ] for @p[1..$#p]; #hm 60
print "\@p:\n".join('',map"@$_\n",@p) if $ENV{VERBOSE}>1;
my($funnet, $t, $plev_min, $plev_max);
my $reset = sub { ($funnet, $t, $plev_min, $plev_max) = (0, time_fp(), @_) };

&$reset(3,5), finn_cells(), exit if !$ENV{CMP};

cmpthese(1, {
    finn_enkel   => sub { &$reset();    eval{ finn_enkel()   } },
    finn_prune2  => sub { &$reset(1,2); eval{ finn_prune()   } },
    finn_prune3  => sub { &$reset(1,3); eval{ finn_prune()   } },
    finn_prune4  => sub { &$reset(1,4); eval{ finn_prune()   } },
    finn_prune5  => sub { &$reset(1,5); eval{ finn_prune()   } },
    finn_prune6  => sub { &$reset(1,6); eval{ finn_prune()   } },
    finn_prune7  => sub { &$reset(1,7); eval{ finn_prune()   } },
    finn_prune55 => sub { &$reset(5,5); eval{ finn_prune()   } },
    finn_prune35 => sub { &$reset(3,5); eval{ finn_prune()   } },
    finn_prune56 => sub { &$reset(5,6); eval{ finn_prune()   } },
    finn_while   => sub { &$reset();    eval{ finn_while()   } },
    finn_while_p => sub { &$reset(3,5); eval{ finn_while_p() } },
    finn_cells   => sub { &$reset();    eval{ finn_cells()   } },
});

sub finn_enkel {
    my($n, $brett, $funntall) = @_;
    $n == 9 ? registrer_funn($funntall) : map finn_enkel($n+1, $brett | $_, "$funntall $_"),  grep !( $brett & $_), @{$p[$n]}
}

sub finn_prune {
    #my($n, $brett, $funntall, $p) = (@_, 0, 0, '', \@p);
    my($n, $brett, $funntall, $p) = @_; $p //= \@p;
    if( $plev_min <= $n <= $plev_max ){ #prune
	$p = [ @$p[0..$n-1], map [grep!($brett&$_),@$_], @$p[$n..$#$p] ]
    }
#    my @p = map { $_<$n ? $_ : [grep!($brett & $_), @{$p[$_]}]  } keys@p;
    $n == 9 ? registrer_funn($funntall) : map finn_prune($n+1, $brett | $_, "$funntall $_", $p),  grep !( $brett & $_), @{ $$p[$n] };
}

sub finn_while {
    my @todo = ( [0, 0, ''] );
    while( @todo ){
	my($n, $brett, $funntall) = @{ pop @todo };
	$freq{$n}++;
	registrer_funn($funntall) and next if $n == 9;
	push @todo, map [$n+1, $brett | $_, "$funntall $_"], grep !( $brett & $_ ), @{ $p[$n] };
    }
    print hash_srlz(\%freq,'freq','',1);
}

sub finn_while_p {
    my @todo = ( [0, 0, '', \@p] );
    while( @todo ){
	my($n, $brett, $funntall, $p) = @{ pop @todo };
	if( $plev_min <= $n <= $plev_max ){ #prune
	    $p = [ @$p[0..$n-1], map [grep!($brett&$_),@$_], @$p[$n..$#$p] ]
	}
	$freq{$n}++;
	registrer_funn($funntall) and next if $n == 9;
	push @todo, map [$n+1, $brett | $_, "$funntall $_", $p], grep !( $brett & $_ ), @{ $$p[$n] };
    }
    print hash_srlz(\%freq);
}

sub registrer_funn {
    my $funntall = shift;
    my($dato, $er_dato) = dato($funntall);
    return 1 if !$er_dato and !$ENV{ALLE_TREFF};
    printf "%d   %.1f sek   %d pr/sek   funn: $funntall   dato: %s\n%s",
	++$funnet, time_fp()-$t, $funnet / (time_fp()-$t), $dato, brett($funntall);
    $ENV{STOPP} == $funnet and die;
}

sub dato {
    my $sum = 0; $sum |= $_ for $_[0] =~ /\d+/g;
    my $bits_str = sprintf "%064b", $sum;
    my($m, $d) = map $_<16 ? $mon[6*($_>>3)+$_%8] : 1+7*(($_>>3)-2)+$_%8, grep !substr($bits_str,63-$_,1),0..63;
    ( "$m $d", 0<$d<=($m=~/^F/?29:$m=~/^(Ap|Jun|S|N)/?30:$m=~/^[A-Z]/?31:0) );
}

sub brett {
    $ENV{VERBOSE} or return '';
    my $funntall = shift;
    my @p = $funntall =~ /\d+/g;
    my($a, $str) = ('a', ' ' x 64);
    for my $b ( map sprintf("%064b", $_), @p ){
	substr($b,$_,1) and substr($str,63-$_,1) = $a for 0..63;
	$a++;
    }
    $str=~s/.{8}/$&\n/g;
    my %c = ( b=>RED, c=>GREEN, d=>BLUE, e=>CYAN, f=>MAGENTA, g=>BRIGHT_BLUE, h=>BRIGHT_GREEN, i=>BRIGHT_RED );
    $str =~ s/[a-i]/$c{$&}?$c{$&}.$&.RESET:$&/ger;
}
sub pat { '-' x 10 . ($_[0]^$p[0][0]) . "\n" . reverse(sprintf("%064b",pop))=~y/0/ /r=~s/.{8}/($&)\n/gr . "\n" }
sub rotate_cw { my@l=reverse split/\n/,pop;my@r;my$i=0;while(length$l[0]){s/^.//,$r[$i].=$&for@l;$i++}join"\n",@r  } #clockwise
sub rotate_ccw { my($i,@s);my$w=$_[0]=~/.*/?length$&:die;$i=$w,map$s[--$i].=$_,split// for split/\n/,pop;join"\n",@s }
sub flip_h {join"\n",reverse split"\n",$_[0]}
sub bits {
    my @l = pop =~ /.+/g;
    my $sum = 0;
    for(0 .. $#l){
	my $b = 1 << 8*$_;
	for( $l[$_] =~ /./g ){
	    $sum |= $b if /1/;
	    $b <<= 1;
	}
    }
    $sum
}
sub hash_srlz { my $hr = shift(); die if ref($hr) ne 'HASH'; join'',map sprintf("%-30s %-20s\n",$_,$$hr{$_}) for sort keys%$hr }
sub sleep_fp { eval{require Time::HiRes} or (sleep(shift()),    return);my$s=shift()-73e-6;Time::HiRes::sleep($s>0?$s:0) }
sub time_fp { eval{ require Time::HiRes } or return time(); my($sec,$mic)=Time::HiRes::gettimeofday();  return $sec+$mic/1e6 }

# Cell-based search: instead of "place piece $n anywhere", take the lowest cell
# that is still undecided and either cover it with some unused piece, or spend
# one of the two cells that stay empty (the month and the day). Every solution
# then has exactly one path to it, and a branch that cannot cover that one cell
# dies at once instead of being discovered nine levels down.
#
# Same 24405 solutions and the same per-solution output as finn_while_p, but in
# a different order -- the traversal is not the same walk of the tree.
#
# 13.8M nodes instead of 134M, and ~5x faster end to end.
sub finn_cells {
    my $board = $p[0][0];
    my @cover;                       # $cover[$c][$i] = masks of piece $i covering cell $c
    for my $i (1..8){
	for my $m (@{$p[$i]}){
	    $m & (1<<$_) and push @{$cover[$_][$i]}, $m for 0..63;
	}
    }
    for my $c (0..63){ $cover[$c][$_] ||= [] for 1..8 }

    my @placed = ($board, (0) x 8);  # indexed by piece, so the output order is unchanged
    my $rec;
    $rec = sub {
	my($brett, $used, $holes, $from) = @_;
	return registrer_funn(join ' ', '', @placed) if $used == 255;
	my $c = $from;
	$c++ while ($brett >> $c) & 1;               # lowest undecided cell
	my $cv = $cover[$c];
	for my $i (1..8){
	    my $bit = 1 << ($i-1);
	    next if $used & $bit;                    # skip a used piece as a whole list
	    my $ub = $used | $bit;
	    for my $m (@{$$cv[$i]}){
		next if $brett & $m;
		$placed[$i] = $m;
		$rec->($brett | $m, $ub, $holes, $c+1); # cells below $c are settled
	    }
	}
	$rec->($brett | (1<<$c), $used, $holes+1, $c+1) if $holes < 2;
    };
    $rec->($board, 0, 0, 0);
}

__DATA__
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
