call "common.gnuplot" "2col"

set zeroaxis
@xaxis_ben
set yrange [0:]
set xlabel "\\# cores"
set ylabel "\\gpcode{open}s/sec/core"
set ytics format "  %.0s%c"
set title "(b) openbench throughput"

plot \
"< bash -c \"../data/fdbench ../data/fdbench-xv6.raw\"" \
   index "iv.any_fd=True"  using 1:($2/$1) with lp title "Any FD", \
"" index "iv.any_fd=False" using 1:($2/$1) with lp title "Lowest FD", \
"<echo '1 740478'" with p pt 7 linecolor 2 ps 1 title ""
