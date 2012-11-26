call "common.gnuplot" "2.125in,1.61in"

set zeroaxis
set key right bottom
@xaxis_tom
set yrange [0:]
set xlabel "\\# cores performing VM operations"
set ylabel "Total throughput (loops/sec)"
#set ytics format " $%.1t*10^{%T}$" add ("0" 0)
set ytics format "  %.0s%c"

plot  "../data/mapbench-xv6.data" every :::0::0 using ($1):($1*$3*1000000/$2) title ""  with lp lw 2 pt 1
