from __future__ import print_function

import sys
import data
import subprocess
import stats

class Gnuplot(object):
    def __init__(self, table, x, y, layer=()):
        self.__curves = table. \
            filter(lambda r: r[x] is not None and r[y] is not None). \
            sort_by(x). \
            group_by(layer + (x,),
                     data.pinject(y, stats.mean, omit=True),
                     data.pinject(y, stats.min, omit=True),
                     data.pinject(y, stats.max, omit=True)). \
            group_by(layer, "$curve")
        self.__x = x
        self.__y = y

    @staticmethod
    def __string(x):
        return '"' + x.replace("\\", "\\\\").replace("\n", "\\n").replace("\"", "\\\"") + '"'

    def __title(self, curve):
        title = []
        for prop in self.__curves.properties():
            if prop["name"] != "$curve":
                title.append("%s=%s" % (prop["name"], curve[prop["name"]]))
        return " ".join(title)

    def command_list(self):
        plots = []
        data = []
        for i, curve in enumerate(self.__curves):
            title = self.__title(curve)
            plots.append("'-' title %s with lp ls %d pt %d" %
                         (self.__string(title), i+1, i+1))
            plots.append("'-' title '' with errorbars ls %d pt %d" % (i+1, i+1))
            for row in curve["$curve"]:
                if row.get("$mean") is None:
                    data.append("")
                else:
                    data.append("%s %s" % (row[self.__x], row["$mean"]))
            data.append("e")
            for row in curve["$curve"]:
                if row.get("$mean") is not None:
                    data.append("%s %s %s %s" % (row[self.__x], row["$mean"],
                                                 row["$min"], row["$max"]))
            data.append("e")
        return ["set xlabel %s" % self.__string(self.__x),
                "set ylabel %s" % self.__string(self.__y),
                "plot %s" % ",".join(plots)] + data

    def png(self, file=sys.stdout):
        p = subprocess.Popen("gnuplot", stdout=file, stdin=subprocess.PIPE)
        print("set terminal pngcairo\n"
              "set style line 11 lc rgb '#808080' lt 1\n"
              "set border 3 back ls 11\n"
              "set tics nomirror\n"
              "set style line 12 lc rgb '#808080' lt 0 lw 1\n"
              "set grid back ls 12",
              file=p.stdin)
        print("\n".join(self.command_list()), file=p.stdin)
        p.stdin.close()
        p.wait()

    def data(self, file=sys.stdout):
        for curve in self.__curves:
            print("# " + self.__title(curve), file=file)
            print("# %s %s (mean min max)" % (self.__x, self.__y), file=file)
            for row in curve["$curve"]:
                print("%s %s %s %s" % (row[self.__x], row["$mean"],
                                       row["$min"], row["$max"]), file=file)
            print(file=file)
            print(file=file)
