call "common.gnuplot"

set zeroaxis
@xaxis_ben
set yrange [0:]
set xlabel "\\# cores"
set ylabel "Total forks/msec"
set ytics format " %.0s%c"

plot \
"../data/forktest.data" every :::0::0 with lp title "xv6",\
"../data/forktest.data" every :::1::1 with lp title "Linux"
