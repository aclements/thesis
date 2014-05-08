call "common.gnuplot" "2col"

set zeroaxis
@xaxis_ben
set yrange [0:]
set xlabel "\\# cores"
set ylabel "\\gpcode{link}s+\\gpcode{unlink}s/sec/core"
set ytics format " %.0s%c"
# Ugh.  There is no way DWIM %s
set ytics add ("1.5M" 1500000,"2.5M" 2500000)
set title "(a) statbench \\gpcode{fstat} throughput"

plot \
"<../data/ttt --layer iv.st_nlink --layer iv.FS_NLINK_REFCOUNT iv.cores dv.stats/sec ../data/linkbench-xv6-[sr]*.raw" \
   index "iv.st_nlink=False iv.FS_NLINK_REFCOUNT=refcache::" using 1:($2/$1) with lp title "Without \\gpcode{st\\_nlink}", \
"" index "iv.st_nlink=True iv.FS_NLINK_REFCOUNT=::" using 1:($2/$1) with lp title "With shared \\gpcode{st\\_nlink}", \
"" index "iv.st_nlink=True iv.FS_NLINK_REFCOUNT=refcache::" using 1:($2/$1) with lp title "With \\refcache\\ \\gpcode{st\\_nlink}", \
"<../data/ttt --layer iv.st_nlink iv.cores dv.stats/sec ../data/linkbench-linux.raw" using 1:($1 == 2 ? $2/$1 : NaN) with p pt 7 linecolor 2 ps 1 title ""
