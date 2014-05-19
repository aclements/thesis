#call "common.gnuplot" "2col"
# 6" wide body
#call "common.gnuplot" "2.95in,3.8in"
# 5.5" wide body
call "common.gnuplot" "2.7in,3.8in"

mp_startx=0.25
mp_starty=0.095                 # Matching title alignment with fdbench
mp_width=0.7
mp_rowgap=0.04
eval mpSetup(1,2)

set zeroaxis
@xaxis_ben
set yrange [0:]
set ytics format " %.0s%c"
# Ugh.  There is no way DWIM %s
set ytics add ("1.5M" 1500000,"2.5M" 2500000)
set title "(a) \\gpcmd{statbench} throughput"

eval mpNext
set xtics format ""
unset xlabel
set ylabel "\\gpcode{fstat}s/sec/core"
plot \
"<../data/ttt --layer iv.st_nlink --layer iv.FS_NLINK_REFCOUNT iv.cores dv.stats/sec ../data/linkbench-xv6-[sr]*.raw" \
   index "iv.st_nlink=False iv.FS_NLINK_REFCOUNT=refcache::" using 1:($2/$1) with lp title "\\gpcode{fstatx}+\\refcache", \
"" index "iv.st_nlink=True iv.FS_NLINK_REFCOUNT=refcache::" using 1:($2/$1) with lp title "\\gpcode{fstat}+\\refcache", \
"" index "iv.st_nlink=True iv.FS_NLINK_REFCOUNT=::" using 1:($2/$1) with lp title "\\gpcode{fstat}+Shared counter", \
"<../data/ttt --layer iv.st_nlink iv.cores dv.stats/sec ../data/linkbench-linux.raw" using 1:($1 == 2 ? $2/$1 : NaN) with p pt 7 linecolor 2 ps 1 title ""

eval mpNext
set xtics format "%g"
set xlabel "\\# cores"
unset title
unset key
set ylabel "\\gpcode{link}s+\\gpcode{unlink}s/sec/core"
plot \
"<../data/ttt --layer iv.st_nlink --layer iv.FS_NLINK_REFCOUNT iv.cores dv.links/sec ../data/linkbench-xv6-[sr]*.raw" \
   index "iv.st_nlink=False iv.FS_NLINK_REFCOUNT=refcache::" using 1:($2/$1) with lp title "No \\gpcode{st\\_nlink}", \
"" index "iv.st_nlink=True iv.FS_NLINK_REFCOUNT=refcache::" using 1:($2/$1) with lp title "\\refcache\\ \\gpcode{st\\_nlink}", \
"" index "iv.st_nlink=True iv.FS_NLINK_REFCOUNT=::" using 1:($2/$1) with lp title "Shared \\gpcode{st\\_nlink}", \
"<../data/ttt --layer iv.st_nlink iv.cores dv.links/sec ../data/linkbench-linux.raw" using 1:($1 == 2 ? $2/$1 : NaN) with p pt 7 linecolor 2 ps 1 title ""
