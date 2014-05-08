call "common.gnuplot" "5in,2.2in"

set zeroaxis
@xaxis_ben
set yrange [0:]
set xlabel "\\# cores"
set ylabel "emails/sec/core"
set ytics format " %.0s%c"
#set title "(c) Mail server throughput"

#set key at graph 1,0.73
set key outside Left reverse

plot \
"< bash -c \"../data/mailbench ../data/mailbench-xv6.raw\"" \
   index "iv.alt=all" using 1:($2/$1) with lp title "Commutative APIs", \
"" index "iv.alt=none" using 1:($2/$1) with lp title "Regular APIs", \
"<../data/ttt iv.cores dv.messages/sec ../data/mailbench-linux.raw" \
   using 1:($1 == 1 ? $2/$1 : NaN) with p pt 7 linecolor 2 ps 1 title ""
