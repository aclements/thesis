SLIDES_SIZE="1024,600"
call "common.gnuplot"

set key inside left Left reverse
set log y
set grid back noxtics
set border 1 back
set ytics scale 0
set pointsize 0.5
set decimal locale
set ytics format "%'g"

# Create out own ytics that are inside the graph
set for [i=0:5] label gprintf("%'g",10**i) at graph 0.01, first 10**i offset character 0,0.5
# Hide regular ytics (we don't unset ytics because that hides the grid
# lines, too).
set ytics format ""
set key at graph 0.1,0.97

set lmargin at screen 0
set rmargin at screen 1
set xrange [1984:2016]

set lt 1 lw 2 lc rgb '#73d216'  # Dark green
set lt 2 lw 2 lc rgb '#005fd8'  # Blue
set lt 3 lw 2 lc rgb '#ef2929'  # Red
set lt 4 lw 2 lc rgb '#fcaf3e'  # Orange

# plot \
# "<cd ../data/processors && python moore.py" \
#    using 1:2 smooth sbezier lt 1 lc 1 notitle, \
# "" using 1:2 with p pt 7 lc 1 notitle, \
# 1/0 with lp pt 7 lt 1 lc 1 title "Clock speed (MHz)", \
# \
# "" using 1:3 smooth sbezier lt 1 lc 3 notitle, \
# "" using 1:3 with p pt 7 lc 3 notitle, \
# 1/0 with lp pt 7 lt 1 lc 3 title "Power (watts)", \
# \
# "" using 1:4 smooth sbezier lt 1 lc 2 notitle, \
# "" using 1:4 with p pt 7 lc 2 notitle, \
# 1/0 with lp pt 7 lt 1 lc 2 title "Cores per socket", \
# \
# "" using 1:($2*$4) smooth sbezier lt 1 lc 4 notitle, \
# "" using 1:($2*$4) with p pt 7 lc 4 notitle, \
# 1/0 with lp pt 7 lt 1 lc 4 title "Total megacycles/sec"

# Note that the curves and the key are not in the same order
plot \
"<cd ../data/processors && python moore.py" \
   using 1:($2*$4) smooth sbezier lt 1 lc 4 notitle, \
"" using 1:($2*$4) with p pt 7 lc 4 notitle, \
\
"" using 1:2 smooth sbezier lt 1 lc 1 notitle, \
"" using 1:2 with p pt 7 lc 1 notitle, \
\
"" using 1:3 smooth sbezier lt 1 lc 3 notitle, \
"" using 1:3 with p pt 7 lc 3 notitle, \
\
"" using 1:4 smooth sbezier lt 1 lc 2 notitle, \
"" using 1:4 with p pt 7 lc 2 notitle, \
\
1/0 with lp pt 7 lt 1 lc 1 title "Clock speed (MHz)", \
1/0 with lp pt 7 lt 1 lc 3 title "Power (watts)", \
1/0 with lp pt 7 lt 1 lc 2 title "Cores per socket", \
1/0 with lp pt 7 lt 1 lc 4 title "Total megacycles/sec"
