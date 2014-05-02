call "common.gnuplot" "3in,2in"
set zeroaxis
set xlabel "Cores"
set ylabel "Messages/second"
set xrange [1:48]
set xtics (1, 6, 12, 18, 24, 30, 36, 42, 48)
set yrange [0:10000]
set format y "%.0s%c"
unset key
set title 'Exim mail server'

plot \
     "exim.data" every :::1::1 using 1:($1*$2) title "Exim" with l ls 1 lc 1 lw 3
