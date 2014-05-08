call "common.gnuplot" "5in,2.2in"

set zeroaxis
set key outside Left reverse
@xaxis_ben
set xlabel "\\# cores performing lookups"
set ylabel "Lookups/sec/core"
#set ytics format " $%.1t*10^{%T}$" add ("0" 0)
set ytics format "  %.0s%c"

plot \
"< ../data/ttt --layer iv.bench --layer iv.writers iv.readers dv.searches/sec ../data/skiplist.raw" \
   index "iv.bench=skip iv.writers=0" using 1:($2/$1) title "0 writers" with lp lw 2 pt 1, \
"" index "iv.bench=skip iv.writers=1" using 1:($2/$1) title "1 writer" with lp lw 2 pt 6, \
"" index "iv.bench=skip iv.writers=5" using 1:($2/$1) title "5 writers" with lp lw 2 pt 2
