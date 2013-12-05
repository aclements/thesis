call "common.gnuplot" "3col"

set zeroaxis
@xaxis_ben
set yrange [0:]
set xlabel "\\# cores"
set ylabel "emails/sec/core"
set ytics format " %.0s%c"
set title "(c) Mail server throughput"

set key at graph 1,0.73

plot \
"< bash -c \"../data/mailbench ../data/mailbench-xv6.raw\"" \
   index "iv.alt=all" using 1:($2/$1) with lp title "Commutative APIs", \
"" index "iv.alt=none" using 1:($2/$1) with lp title "Regular APIs", \
"<echo '1 316'" with p pt 7 linecolor 2 ps 1 title ""
