TIKZ_FONT="',10'"
call "common.gnuplot" "6in,8in"

if (TARGET eq "") {
  set term wxt size 640,768
}

NPLOT=0
STARTX=0.15
WIDTH=0.825
COLGAP=0.1
NCOLS=2.0
LMARGIN(col)=STARTX + col*((WIDTH+COLGAP)/NCOLS)
NEXT='\
      set lmargin at screen LMARGIN(NPLOT%2); \
      set rmargin at screen LMARGIN(NPLOT%2+1)-COLGAP; \
      unset label 1; \
      if (NPLOT%2) {unset ylabel}; \
      if (NPLOT>0) {unset key}; \
      if (NPLOT>1) {unset title}; \
      NPLOT=NPLOT+1'

set multiplot layout 3,2
#set palette gray negative gamma 0.3
#set palette defined (0 "#ffffff", 1/6. "#f0f9e8", 2/6. "#ccebc5", 3/6. "#a8ddb5", 4/6. "#7bccc4", 5/6. "#43a2ca", 1 "#0868ac")
set palette cubehelix cycles -2.5 saturation 1.5 negative gamma 0.3
set zeroaxis
set cbrange [0:0.001]
unset colorbox
set grid front
set ytics format "%.0s%c"
set lt 1 lw 5

@NEXT
set title "80-core Intel"
set label 1 "Write after write latency (cycles)" at graph -0.25,0.5 center rotate
@xaxis_ben
set xlabel "\\# writer cores"
plot '../data/sharing.out/ben/ww/kde-write' with image notitle, \
     '<../data/text-to-table ../data/sharing.out/ben/ww/data' \
     using 'iv.ncores':'dv.cycles/write' with lines title 'mean'

@NEXT
set title "48-core AMD"
@xaxis_tom
set xlabel "\\# writer cores"
plot '../data/sharing.out/tom/ww/kde-write' with image notitle, \
     '<../data/text-to-table ../data/sharing.out/tom/ww/data' \
     using 'iv.ncores':'dv.cycles/write' with lines title 'mean'

@NEXT
set label 1 "Read after write latency (cycles)" at graph -0.25,0.5 center rotate
@xaxis_ben
set xlabel "\\# reader cores"
plot '../data/sharing.out/ben/rw/kde-read' with image notitle, \
     '<../data/text-to-table ../data/sharing.out/ben/rw/data' \
     using 'iv.ncores':'dv.cycles/read' with lines title 'mean'

@NEXT
@xaxis_tom
set xlabel "\\# reader cores"
plot '../data/sharing.out/tom/rw/kde-read' with image notitle, \
     '<../data/text-to-table ../data/sharing.out/tom/rw/data' \
     using 'iv.ncores':'dv.cycles/read' with lines title 'mean'

set cbrange [0:2]

@NEXT
set label 1 "Read after read latency (cycles)" at graph -0.25,0.5 center rotate
@xaxis_ben
set xlabel "\\# reader cores"
plot '../data/sharing.out/ben/rr/kde-read' with image notitle, \
     '<../data/text-to-table ../data/sharing.out/ben/rr/data' \
     using 'iv.ncores':'dv.cycles/read' with lines title 'mean'

@NEXT
@xaxis_tom
set xlabel "\\# reader cores"
plot '../data/sharing.out/tom/rr/kde-read' with image notitle, \
     '<../data/text-to-table ../data/sharing.out/tom/rr/data' \
     using 'iv.ncores':'dv.cycles/read' with lines title 'mean'

unset multiplot