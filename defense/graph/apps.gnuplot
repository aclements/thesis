call "common.gnuplot" "3in,2in"
set zeroaxis
set xlabel "Cores"
set ylabel "Normalized throughput"
set xrange [1:48]
set xtics (1, 6, 12, 18, 24, 30, 36, 42, 48)
set format y "%.0s%c"

# plot \
#      "exim.data" every :::1::1 using 1:($1*$2/747.20) title "Exim" with l lw 3, \
#      "mosbench.data" index "stock-gmake" using 2:($2*$3/0.00160207) title "gmake" with l lw 3

# Normalized to 8 cores, since Exim tanks out to 6, then levels
plot \
     "mosbench.data" index "stock-gmake" using 2:($2*$3/0.00148757) title "gmake" with l ls 1 lw 3, \
     "exim.data" every :::1::1 using 1:($1*$2/430.20) title "Exim" with l ls 1 lc 1 lw 3
