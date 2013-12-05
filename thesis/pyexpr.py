import sys, json, pyexprlib

#
# Data from const.json
#

const = json.load(file("const.json"))

def loc_table(key):
    out = []
    for name, lines in const[key]:
        out.append("%s & %s" % (name, format(lines, ",d")))
    return "\\\\".join(out)

#
# Formatters
#

def bytes(n):
    sufs = ["", "~KB", "~MB", "~GB", "~TB", "~PB"]
    for i, suf in enumerate(sufs):
        p = 1 << (10 * i)
        if n / p < 10 and i > 0:
            return "%.1f%s" % (float(n) / p, suf)
        if n / p < 1024:
            return "%d%s" % (n / p, suf)

def times(a, b):
    x = float(a) / b
    if x > 10:
        return "$%d\\times$" % x
    elif x > 1:
        return "$%1.1f\\times$" % x
    else:
        raise ValueError("times not implemented for small values")

def percent(x):
    x *= 100
    if x < 10:
        return "%.2g\\%%" % x
    return "%d\\%%" % x

#
# Benchmark data
#

sys.path.append("data")
import common, data, glob
from stats import *
linkbench = common.files_to_table(
    *["data/linkbench-%s.raw" % s
      for s in ["linux","xv6-refcache","xv6-shared"]])
fdbench = common.files_to_table(
    *["data/fdbench-%s.raw" % s for s in ["linux","xv6"]])
mailbench = common.files_to_table(
    *["data/mailbench-%s.raw" % s for s in ["xv6"]])

#
# Mscan data
#

import readmscan
class mscan(object):
    xv6 = readmscan.mscan(file('data/mscan-xv6.out'))
    linux = readmscan.mscan(file('data/mscan-linux.out'))

    # We don't use the idempotence numbers in the paper right now.
    # try:
    #     model = readmscan.model_tests(file('data/model.out'))
    # except EnvironmentError:
    #     model = None
    #     xv6_idem = xv6.filter(lambda: False)
    #     print >>sys.stderr
    #     print >>sys.stderr, "\x1b[31m" + "WARNING:" + "\x1b[0m",
    #     print >>sys.stderr, "No data/model.out; idempotence numbers will be 0"
    #     print >>sys.stderr
    # else:
    #     xv6 = xv6.join('', model, '', 'calls', 'path', 'testnum')
    #     linux = linux.join('', model, '', 'calls', 'path', 'testnum')
    #     xv6_idem = xv6.filter(lambda idempotent_projs:
    #                           idempotent_projs != [[],[]])

    calls = xv6.calls
    ntestcases = xv6.total

#
# Generate
#

pyexprlib.generate(sys.argv[1:], globals())
