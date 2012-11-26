load "common.gnuplot"

set zeroaxis
set key left top
set y2tics nomirror
set border 1+2+8
@xaxis_tom
set xlabel "\\# cores performing reads"
set yrange [0:6000000]
set ylabel "Total throughput (ops/sec)"
#set ytics 1000000 format " $%.0t*10^{%T}$" add ("0" 0)
set ytics 1000000 format "  %.0s%c"
set y2label "Read cost (cycles)"
set y2range [0:3000]

!../../bench/summarize ops_vs_ncores < ../../bench/write-sharing-tom.data > .write-line.data
plot ".write-line.data" \
     index "minwait=5000 rw=true wait=5000 readwait=15000" \
     using ($1-1):3 title "Throughput" with lp lw 2 pt 1 axis x1y1, \
     "" index "minwait=5000 rw=true wait=5000 readwait=15000" \
     using ($1-1):2 title "Cycles" with lp lw 2 pt 6 axis x1y2
