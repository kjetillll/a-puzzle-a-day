#!/usr/bin/perl
use Term::ANSIColor qw(colored :constants);
my @mon = qw(Jan Feb Mar Apr Mai Jun Jul Aug Sep Okt Nov Des);
my @p = map{ my $w = (sort{$b<=>$a}(map length,split/\n/))[0]; s/.+/sprintf"%*s",-$w,$&/ger }
        split /\n\n/, join '', map s/\s+\#.*//r, <DATA>;
@p = map {my $p = $_; [grep !$sett{$_}++, map { ($p = $_==5 ? flip_h($p) : rotate_cw($p) ) } 1..8 ] } @p;
$p[0] = [$p[0][3]];  #brettet har ingen rotasjon (dvs nr3 av 0..3, ingen flipp, ingen forskyvning
for(@p){$_=bits($_) for@$_}
print "\@p:\n".join('',map"@$_\n",@p);
$_ = [ map{ my $p = $_; grep !( $_ & $p[0][0]), grep $_, map $p<<$_, 0..60 } @$_ ] for @p[1..$#p]; #hm 60
print "\@p:\n".join('',map"@$_\n",@p) if $ENV{VERBOSE}>1;
my($funnet, $t) = (0, time_fp());

finn();

sub finn {
    my($n, $brett, $funntall) = @_;
    $n == 9 ? registrer_funn($funntall) : map finn($n+1, $brett | $_, "$funntall $_"),  grep !( $brett & $_), @{$p[$n]}
}

sub registrer_funn {
    my $funntall = shift;
    my $dato = dato($funntall);
    printf "%d   %.1f sek   %d pr/sek   funn: $funntall   dato: %s\n%s",
	++$funnet, time_fp()-$t, $funnet / (time_fp()-$t), $dato, brett($funntall);
    $ENV{STOPP} == $funnet and die;
}

sub dato {
    my $sum = 0; $sum |= $_ for $_[0] =~ /\d+/g;
    my $bits_str = sprintf "%064b", $sum;
    join' ', map $_<16 ? $mon[6*($_>>3)+$_%8] : 1+7*(($_>>3)-2)+$_%8, grep !substr($bits_str,63-$_,1), 0..63;
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
sub time_fp { eval{ require Time::HiRes } or return time(); my($sec,$mic)=Time::HiRes::gettimeofday();  return $sec+$mic/1e6 }
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
