call "common.gnuplot" "2col"

set zeroaxis
@xaxis_ben
set yrange [0:]
set xlabel "\\# cores"
set ylabel "page writes/sec/core"
set ytics format "  %.0s%c"
set title "(a) vmlocal throughput"

set key inside top right

plot \
"<../data/ttt --hdr --compute --layer iv.mode --layer iv.kernel --layer iv.krel iv.cores 'dv.page touches/sec' ../data/mapbench-xv6.raw ../data/mapbench-linux.raw ../data/mapbench-bonsai.raw" \
   index "iv.mode=local iv.kernel=xv6" @vmusing @radix title '\sys', \
"" index "iv.mode=local iv.kernel=Linux iv.krel=3.5.7.2" @vmusing @linux title 'Linux', \
"" index "iv.mode=local iv.kernel=Linux iv.krel=2.6.37-amdragon-cbtree-all+" @vmusing @bonsai title 'Bonsai'
