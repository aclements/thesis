call "common.gnuplot"

set zeroaxis
@xaxis_ben
set yrange [0:]
set xlabel "\\# cores"
set ylabel "Total Emails/msec"
set ytics format " %.0s%c"

plot \
"../data/usocket.data" every :::0::0 with lp title "Unordered datagram socket",\
"../data/usocket.data" every :::1::1 with lp title "Linux w. 1 datagram socket",\
"../data/usocket.data" every :::2::2 with lp title "Linux w. $n$ datagram sockets",\
"../data/usocket.data" every :::3::3 with lp title "Linux w. $n$ streams"
