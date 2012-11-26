import sys, json, glob, pyexprlib

#
# Data from const.json
#

const = json.load(file("const.json"))

#
# Throughput Data from microbenchmarks in loops/per-second
#

def microbench_data(fname):
    d = {}
    for l in open('data/'+fname+'.data'):
        if l.startswith("#"):
            continue
        s=l.split()
        d[int(s[0])]=1000000.0*float(s[2])/float(s[1])
    return d;

#linuxMapbench = microbench_data('mapbench-linux')
#xv6Mapbench = microbench_data('mapbench-xv6')
#linuxDirbench = microbench_data('dirbench-linux')
#xv6Dirbench = microbench_data('dirbench-xv6')
#linuxFilebench = microbench_data('filebench-linux')
#xv6Filebench = microbench_data('filebench-xv6')

def fmt_float(f):
    return float('{0:.2f}'.format(f))

#
# mscan results for microbenchmarks
#

mscan = dict((f[5:-6], json.load(file(f))["scope-summary"])
             for f in glob.glob("data/*.mscan"))

def ncommute(experiment, commutes):
    if commutes:
        return (mscan[experiment]["logically unshared/physically unshared"]
                + mscan[experiment]["logically unshared/physically shared"])
    else:
        return mscan[experiment]["logically shared  /physically shared"]

#
# Generate
#

pyexprlib.generate(sys.argv[1:], globals())
