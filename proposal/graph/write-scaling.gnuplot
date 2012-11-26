load "common.gnuplot"

set zeroaxis
set key left top

set logscale x 2

set xrange [16*1024:64*1024*1024]
set format x ""
set xtics 4
set for [pow = 0:30:2] xtics add (fmtbinary(2**pow) 2**pow)

set xlabel "Thread working set size"
set ylabel "Write cost (cycles)"

!../../bench/summarize ops_vs_size < ../../bench/memscan-tom.data > .write-scaling.data
plot for [cores in "48 24 6"] \
     "./.write-scaling.data" index "op=write ncores=".cores." shared=false" \
     with lp lw 2 title cores." cores"
