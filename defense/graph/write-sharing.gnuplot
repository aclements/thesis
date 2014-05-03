call "common.gnuplot"

set zeroaxis
@xaxis_ben
set xlabel "1 writer + N readers"
set ylabel "Cycles to read"
set format y "%.0s%c"
set ytics add ("1.5k" 1500, "2.5k" 2500, "3.5k" 3500)

plot '<../../thesis/data/ttt iv.ncores dv.cycles/read ../../thesis/data/sharing.out/ben/rw/data' \
     with lines title '' lt 1 lw 3
