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
# VM size data
#

import vmsize

vmsizes = {}
for workload in ["chrome-used", "firefox-10.0.6-init", "firefox-10.0.6-used",
                 "eurosys-apache", "eurosys-mysql"]:
    maps = list(vmsize.parse_maps(file("data/vmsize/%s-maps" % workload)))
    pagemap = list(vmsize.parse_saved_pagemap(file(
                "data/vmsize/%s-pagemap" % workload)))
    vmsizes[workload] = {"tree": vmsize.tree_size(maps),
                         "skip": vmsize.skip_list_size(maps),
                         "linux": vmsize.linux_size(maps, pagemap),
                         "radix": vmsize.radix_size(maps, pagemap),
                         "rss": vmsize.rss(pagemap)}

def vmsize_row(workload):
    return "%s & %s & %s & %s & ($%.1f\\times$)" % (
        bytes(vmsizes[workload]["rss"]),
        bytes(vmsizes[workload]["linux"][0]),
        bytes(vmsizes[workload]["linux"][1]),
        bytes(vmsizes[workload]["radix"]),
        float(vmsizes[workload]["radix"]) / sum(vmsizes[workload]["linux"]))

#
# Generate
#

pyexprlib.generate(sys.argv[1:], globals())
