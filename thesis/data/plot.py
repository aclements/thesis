from __future__ import print_function

import sys
import data
import subprocess
import stats

def stat_bounds():
    def fn(layer):
        return layer.group_by("$x",
                              ("$y", data.pinject("$y", stats.mean, omit=True)),
                              data.pinject("$y", stats.min, omit=True),
                              data.pinject("$y", stats.max, omit=True),
                              data.pinject("$y", stats.pctvar, omit=True))
    return fn

def stat_differential():
    def fn(layer):
        px, py = 0, 0
        out = []
        for row in layer:
            x, y = row["$x"], row["$y"]
            out.append({"$x": x, "$y": float(y - py) / (x - px)})
            px, py = x, y
        return data.Table(out)
    return fn

class Gnuplot(object):
    def __init__(self, table, x, y, layer=(), stats=[stat_bounds()]):
        table = table. \
            filter(lambda r: r[x] is not None and r[y] is not None). \
            sort_by(x). \
            map("*", ("$x", lambda row: row[x]), ("$y", lambda row: row[y]))
        for stat in stats:
            table = table.group_by(layer, ("$layer", stat)).ungroup("$layer")
        self.__curves = table.group_by(layer, "$curve")
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
                if row.get("$y") is None:
                    data.append("")
                else:
                    data.append("%s %s" % (row[self.__x], row["$y"]))
            data.append("e")
            for row in curve["$curve"]:
                if row.get("$y") is not None:
                    data.append("%s %s %s %s" % (row[self.__x], row["$y"],
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

    def data(self, file=sys.stdout, col_headers=False):
        for curve in self.__curves:
            print("# " + self.__title(curve), file=file)
            labels = [self.__x, self.__y]
            indexes = ["row['$x']", "row['$y']"]
            fmt = ["%s", "%s"]
            for prop in curve["$curve"].properties()["name"]:
                if prop in ("$x", "$y") or not prop.startswith("$"):
                    continue
                labels.append(prop[1:])
                indexes.append("row[%r]" % prop)
                if prop == "$pctvar":
                    fmt.append("%1.2g")
                else:
                    fmt.append("%s")
            proj = eval("lambda row: %r %% (%s,)" %
                        (" ".join(fmt), ",".join(indexes)))
            if col_headers:
                print("%s" % " ".join(labels))
            else:
                print("# %s" % " ".join(labels))
            for row in curve["$curve"]:
                print(proj(row), file=file)

            print(file=file)
            print(file=file)
