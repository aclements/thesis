TIKZ_FONT="',10'"
call "common.gnuplot" "6in,4in"

if (TARGET eq "") {
  set term wxt size 640,768
}

eval mpSetup(2, 2)

#set palette gray negative gamma 0.3
#set palette defined (0 "#ffffff", 1/6. "#f0f9e8", 2/6. "#ccebc5", 3/6. "#a8ddb5", 4/6. "#7bccc4", 5/6. "#43a2ca", 1 "#0868ac")
set palette cubehelix cycles -2.5 saturation 1.5 negative gamma 0.3
set zeroaxis
set cbrange [0:0.001]
unset colorbox
set grid front
set ytics format "%.0s%c"
set lt 1 lw 5

eval mpNext
# "inside" overrides setting in common.gnuplot
set key inside left Left reverse samplen 3
eval mpRowLabel("Write latency (cycles)")
set xlabel "\\# writer cores"
plot '../data/sharing.out/ben/ww/kde-write' with image notitle, \
     '<../data/text-to-table ../data/sharing.out/ben/ww/data' \
     using 'iv.ncores':'dv.cycles/write' with lines title 'mean'

eval mpNext
set xlabel "\\# writer cores"
plot '../data/sharing.out/tom/ww/kde-write' with image notitle, \
     '<../data/text-to-table ../data/sharing.out/tom/ww/data' \
     using 'iv.ncores':'dv.cycles/write' with lines title 'mean'

eval mpNext
eval mpRowLabel("Read latency (cycles)")
set xlabel "\\# reader cores"
plot '../data/sharing.out/ben/rw/kde-read' with image notitle, \
     '<../data/text-to-table ../data/sharing.out/ben/rw/data' \
     using 'iv.ncores':'dv.cycles/read' with lines title 'mean'

eval mpNext
set xlabel "\\# reader cores"
plot '../data/sharing.out/tom/rw/kde-read' with image notitle, \
     '<../data/text-to-table ../data/sharing.out/tom/rw/data' \
     using 'iv.ncores':'dv.cycles/read' with lines title 'mean'

unset multiplot
