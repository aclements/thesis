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

if (TARGET eq "paper-tikz") {
  set term tikz size @SIZE font ',8' #background '#ff0000'
  set output
  set pointsize 1.5
} else {
  if (TARGET eq "pdf") {
    set term pdfcairo size @SIZE linewidth 2 rounded font ',10'
    set output
  } else {
    if (TARGET eq "slides") {
      set term svg size 720,500 font "Open Sans,20" dashed linewidth 2 enhanced
#      set output
      set output "|sed 's/<svg/& style=\"font-weight:300\"/'"
    } else {
      if (!(TARGET eq "")) {
        print sprintf("Unknown target %s!", TARGET)
      }
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

if (TARGET eq "slides") {
  set style line 1 lt 1 lc rgb "#8ae234" lw 3
  set style line 2 lt 1 lc rgb "#000000" lw 3
}

# Since most of our graphs are scalability graphs, put key in top left
set key at graph 0,1 left top Left reverse

#
# Clean up SVG output
#

# Based on
# http://youinfinitesnake.blogspot.com/2011/02/attractive-scientific-plots-with.html

# Line style for axes
set style line 80 lt 1
set style line 80 lt rgb "#808080"

# Line style for grid
set style line 81 lt 3  # Dotted
set style line 81 lt rgb "#808080" lw 0.5

if (TARGET eq "slides") {
  set grid back linestyle 81
  set border 3 back linestyle 80
}
