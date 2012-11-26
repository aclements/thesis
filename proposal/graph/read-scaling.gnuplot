load "common.gnuplot"

set zeroaxis
set key left top

set logscale x 2

set xrange [16*1024:64*1024*1024]
set format x ""
set xtics 4
# This graph is stacked with write-scaling.gnuplot
#set xlabel "Thread working set size"
#set for [pow = 0:30:2] xtics add (fmtbinary(2**pow) 2**pow)

set ylabel "Read cost (cycles)"

!../../bench/summarize ops_vs_size < ../../bench/memscan-tom.data > .read-scaling.data
plot for [cores in "48 24 6"] \
     "./.read-scaling.data" index "op=read ncores=".cores." shared=true" \
     with lp lw 2 title cores." cores"
