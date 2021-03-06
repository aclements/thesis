call "common.gnuplot" "2x2"

if (TARGET eq "") {
  set term wxt size 640,768
}

eval mpSetup(2, 2)

#set palette gray negative gamma 0.3
shade(gray) = gray**(1/0.3)
set palette model RGB functions shade(gray), shade(gray), 0.5 + 0.5*shade(gray) negative
#set palette defined (0 "#ffffff", 1/6. "#f0f9e8", 2/6. "#ccebc5", 3/6. "#a8ddb5", 4/6. "#7bccc4", 5/6. "#43a2ca", 1 "#0868ac")
#set palette cubehelix cycles -2.5 saturation 1.5 negative gamma 0.3
set zeroaxis
set cbrange [0:0.002]
unset colorbox
set grid front
set ytics format "%.0s%c"
set lt 1 lw 5

eval mpNextSharing
# "inside" overrides setting in common.gnuplot
set key inside left Left reverse samplen 3
eval mpRowLabel("Write latency (cycles)")
set xlabel "\\# writer cores"
plot '../data/sharing.out/ben/ww/kde-write' with image notitle, \
     '<../data/ttt iv.ncores dv.cycles/write ../data/sharing.out/ben/ww/data' \
     with lines title 'mean'

eval mpNextSharing
set xlabel "\\# writer cores"
plot '../data/sharing.out/tom/ww/kde-write' with image notitle, \
     '<../data/ttt iv.ncores dv.cycles/write ../data/sharing.out/tom/ww/data' \
     with lines title 'mean'

eval mpNextSharing
eval mpRowLabel("Read latency (cycles)")
set xlabel "\\# reader cores"
plot '../data/sharing.out/ben/rw/kde-read' with image notitle, \
     '<../data/ttt iv.ncores dv.cycles/read ../data/sharing.out/ben/rw/data' \
     with lines title 'mean'

eval mpNextSharing
set xlabel "\\# reader cores"
plot '../data/sharing.out/tom/rw/kde-read' with image notitle, \
     '<../data/ttt iv.ncores dv.cycles/read ../data/sharing.out/tom/rw/data' \
     with lines title 'mean'

unset multiplot
