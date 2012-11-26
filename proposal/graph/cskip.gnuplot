load "common.gnuplot"

set zeroaxis
set key left top
@xaxis_tom
set yrange [0:]
set xlabel "\\# cores performing lookups"
set ylabel "Total throughput (lookups/sec)"
#set ytics format " $%.1t*10^{%T}$" add ("0" 0)
set ytics format "  %.0s%c"

plot  "../data/cskip.data" every :::0::0 using ($1):($2) title "0 writers" with lp lw 2 pt 1,\
      "../data/cskip.data" every :::1::1 using ($1):($2) title "1 writers" with lp lw 2 pt 6,\
      "../data/cskip.data" every :::2::2 using ($1):($2) title "5 writers" with lp lw 2 pt 2
