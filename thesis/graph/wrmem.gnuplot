call "common.gnuplot" "4.05in,1.8in"

set zeroaxis
@xaxis_ben
set yrange [0:]
set xlabel "\\# cores"
set ylabel "jobs/hour/core"
#set ytics format " $%.1t*10^{%T}$" add ("0" 0)
# set ytics format "  %.0s%c"

set key outside Left reverse

plot \
"<../data/wrmem-to-text ../data/wrmem-*.raw | ../data/ttt --hdr --layer iv.malloc --layer iv.kernel --layer iv.krel iv.cores dv.jobs/hour -" \
   index "iv.malloc=8388608 iv.kernel=xv6" @vmusing title "\\sys/8~MB" @radix lw 3,\
"" index "iv.malloc=65536 iv.kernel=xv6" @vmusing title "\\sys/64~KB" @radix lw 1,\
"" index "iv.malloc=8388608 iv.kernel=Linux iv.krel=3.5.7.2" @vmusing title "Linux/8~MB" @linux lw 3,\
"" index "iv.malloc=65536 iv.kernel=Linux iv.krel=3.5.7.2" @vmusing title "Linux/64~KB" @linux lw 1,\
"" index "iv.malloc=8388608 iv.kernel=Linux iv.krel=2.6.37" @vmusing title "Bonsai/8~MB" @bonsai lw 3,\
"" index "iv.malloc=65536 iv.kernel=Linux iv.krel=2.6.37" @vmusing title "Bonsai/64~KB" @bonsai lw 1
