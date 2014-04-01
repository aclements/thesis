TARGET="`echo $TARGET`"

set macros

if ("$0" eq "" || "$0"[0:1] eq "$$"[0:1]) {
  SIZE="3.05in,1.8in"
} else {
  if ("$0" eq "3col") {
    SIZE="2.25in,1.6in"
  } else {
    SIZE="$0"
  }
}

TIKZ_FONT=exists("TIKZ_FONT") ? TIKZ_FONT : "',7'"
if (TARGET eq "paper-tikz") {
  set term tikz size @SIZE font @TIKZ_FONT
  set output
  set pointsize 1.5
} else {
  if (TARGET eq "pdf") {
    set term pdfcairo size @SIZE linewidth 2 rounded font ',10'
    set output
  } else {
    if (!(TARGET eq "")) {
      print sprintf("Unknown target %s!", TARGET)
    }
  }
}

set ytics nomirror
set xtics nomirror
set grid back lt 0 lt rgb 'grey'
set border 3 back

xaxis_tom = 'set xrange [1:48]; \
          set xtics 6 add (1); \
          set xlabel "\\# cores"'
xaxis_ben = 'set xrange [1:80]; \
          set xtics 10 add (1); \
          set xlabel "\\# cores"'

fmtbinary(x) = x < 2**10 ? sprintf("%d", x) \
             : x < 2**20 ? sprintf("%d~KB", x/2**10) \
             : x < 2**30 ? sprintf("%d~MB", x/2**20) \
             : x < 2**40 ? sprintf("%d~GB", x/2**30) \
             : x < 2**50 ? sprintf("%d~TB", x/2**40) \
             : "too big"

set linetype 1 lw 2 lc rgb '#00dd00'
set linetype 2 lw 2 lc rgb '#0000ff'
set linetype 3 lw 2 lc rgb '#ff0000'

# Since most of our graphs are scalability graphs, put key in top left
#set key at graph 0,1 left top Left reverse
# Bottom right for per-core scalability graphs
set key at graph 1,0.85

#
# Multiplot stuff
#

mp_startx=0.15                  # Left edge of col 0 graph
mp_width=0.825                  # Total width of graph area
mp_colgap=0.1                   # Gap between columns
# The screen coordinate of the left edge of column col
mp_lmargin(col)=mp_startx + col*((mp_width+mp_colgap)/mp_ncols)

# Set up a multiplot with w columns and h rows
mpSetup(w,h) = sprintf('\
    mp_nplot=0; \
    mp_ncols=real(%d); \
    set multiplot layout %d,%d', w, h, w)
# Start the next graph in the multiplot
mpNext = '\
    set lmargin at screen mp_lmargin(mp_nplot%2); \
    set rmargin at screen mp_lmargin(mp_nplot%2+1)-mp_colgap; \
    unset label 1; \
    if (mp_nplot%2) {unset ylabel}; \
    if (mp_nplot>0) {unset key}; \
    if (mp_nplot%2==0) {@xaxis_ben} else {@xaxis_tom}; \
    if (mp_nplot==0) {set title "80-core Intel"}; \
    if (mp_nplot==1) {set title "48-core AMD"}; \
    if (mp_nplot>1) {unset title}; \
    mp_nplot=mp_nplot+1'
# Set Y axis row label such that it aligns regardless of tic width
mpRowLabel(lbl) = \
    sprintf('set label 1 "%s" at graph -0.25,0.5 center rotate',lbl)
