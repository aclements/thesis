call "common.gnuplot"

set zeroaxis
@xaxis_ben
set xlabel "1 writer + N readers"
set ylabel "Cycles to read"
set format y "%.0s%c"

!./summarize ops_vs_ncores < write-sharing.data > .write-sharing.data
plot \
     '.write-sharing.data' \
     using ($1):2 title '' with points lc '#000000' ps 0.5, \
     '.write-sharing.data' \
     using ($1):2 title '' smooth sbezier lt 1 lw 3
