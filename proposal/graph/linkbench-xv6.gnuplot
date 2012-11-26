call "common.gnuplot"

set zeroaxis
set key left top reverse Left
@xaxis_tom
set yrange [0:]
set xlabel "\\# cores"
set ylabel "Total throughput (stats/sec)"
#set ytics format " $%.1t*10^{%T}$" add ("0" 0)
set ytics format "  %.0s%c"

plot \
"../data/linkbench.data" index "iv.st_nlink=False" \
  title "Commutative stat" with lp lw 2 pt 1, \
"" index "iv.st_nlink=True" \
  title "Non-commutative stat" with lp lw 2 pt 2
