call "common.gnuplot" "2x2"

# Our Y tic labels are somewhat long, so increase colgap
mp_colgap=mp_colgap*1.1
eval mpSetup(2, 2)

set zeroaxis

set logscale x 2
set xrange [32*1024:128*1024*1024]
set format x ""
set xtics 2**3
set for [pow = 15:30:3] xtics add (fmtbinary(2**pow) 2**pow)

set key inside left Left reverse

mpNextMemscan = '\
    eval mpNext; \
    if (mp_nplot>1) {unset key}; \
    if (mp_nplot==0) {set title "80-core Intel"}; \
    if (mp_nplot==1) {set title "48-core AMD"}; \
    if (mp_nplot>1) {unset title}'
mp_rowgap=0.1                   # No xlabel between rows

eval mpNextMemscan
eval mpRowLabel("Read latency (cycles)")
plot for [cores in "80 40 10"] \
     "<../data/ttt --hdr --layer=iv.ncores iv.size dv.cycles/op ../data/memscan.out/ben/read/data" \
     index 'iv.ncores='.cores \
     using 'iv.size':'min' with lp lw 2 title cores." cores"

eval mpNextMemscan
plot for [cores in "48 24 6"] \
     "<../data/ttt --hdr --layer=iv.ncores iv.size dv.cycles/op ../data/memscan.out/tom/read/data" \
     index 'iv.ncores='.cores \
     using 'iv.size':'min' with lp lw 2 title cores." cores"

set xlabel "Thread working set size"

eval mpNextMemscan
eval mpRowLabel("Write latency (cycles)")
plot for [cores in "80 40 10"] \
     "<../data/ttt --hdr --layer=iv.ncores iv.size dv.cycles/op ../data/memscan.out/ben/write/data" \
     index 'iv.ncores='.cores \
     using 'iv.size':'min' with lp lw 2 title cores." cores"

eval mpNextMemscan
plot for [cores in "48 24 6"] \
     "<../data/ttt --hdr --layer=iv.ncores iv.size dv.cycles/op ../data/memscan.out/tom/write/data" \
     index 'iv.ncores='.cores \
     using 'iv.size':'min' with lp lw 2 title cores." cores"

unset multiplot
