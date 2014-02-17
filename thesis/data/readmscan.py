import sys
import json
import collections
import itertools

class Relation(object):
    def __init__(self, cols, base=None):
        if cols is None and base is not None:
            cols = base.__cols
        if cols is None:
            raise ValueError('Must specify columns')
        self.__ncols = len(cols)
        self.__cols = cols
        self.__colnums = range(len(cols))
        self.__data = tuple([] for col in cols)

    def __getitem__(self, idx):
        if idx >= len(self):
            raise IndexError(idx)
        return self.row(idx)

    def add(self, *vals):
        if len(vals) != self.__ncols:
            raise ValueError('Wrong number of columns')
        for i in self.__colnums:
            self.__data[i].append(vals[i])

    def col(self, name):
        return self.__data[self.__cols.index(name)]

    def row(self, i):
        return [self.__data[col][i] for col in self.__colnums]

    def __len__(self):
        return len(self.__data[0])

    def __proj(self, cols):
        return eval('lambda i: (%s)' %
                    ','.join('data[%d][i]' % self.__cols.index(col)
                             for col in cols),
                    {'data': self.__data})

    def index(self, *cols):
        res = {}
        reltype = type(self)
        if len(cols) == 1:
            idx = self.col(cols[0])
        else:
            idx = itertools.izip(*[self.col(col) for col in cols])
        for i, val in enumerate(idx):
            try:
                rel = res[val]
            except KeyError:
                rel = reltype(None, base=self)
                res[val] = rel

            for j in self.__colnums:
                rel.__data[j].append(self.__data[j][i])
        return res

    def filter(self, pred):
        args = []
        for i in range(pred.__code__.co_argcount):
            args.append('data[%d][i]' %
                        self.__cols.index(pred.__code__.co_varnames[i]))
        predx = eval('lambda i: pred(%s)' % ','.join(args),
                     {'pred': pred, 'data': self.__data})

        res = type(self)(None, base=self)
        for i in xrange(len(self)):
            if predx(i):
                for j in self.__colnums:
                    res.__data[j].append(self.__data[j][i])
        return res

    def group(self, groupby, *agg):
        cols = list(groupby) + [col for col, fn in agg]
        res = type(self)(cols, base=self)
        aggidx = [self.__cols.index(col) for col, fn in agg]
        proj = self.__proj(groupby)

        # Group rows
        key_to_row = {}
        for i in xrange(len(self)):
            key = proj(i)
            rown = key_to_row.get(key)
            if rown is None:
                rown = len(res)
                key_to_row[key] = rown
                for j in range(len(groupby)):
                    res.__data[j].append(key[j])
                for j in range(len(agg)):
                    res.__data[j + len(groupby)].append([])
            for j in range(len(agg)):
                res.__data[j + len(groupby)][rown].append(self.__data[aggidx[j]][i])

        # Aggregate
        for col, fn in agg:
            j = cols.index(col)
            for i, vals in enumerate(res.__data[j]):
                res.__data[j][i] = fn(vals)

        return res

    def join(self, selfname, other, othername, *cols):
        """Perform an inner equi-join between self and other on cols.

        The returned table will have the type of self and will be
        ordered like self.  Columns other than those listed in cols
        will be prefixed by selfname and othername depending on their
        source table.
        """

        # Construct joined schema and row constructor
        schema = []
        code = ['def joiner(i, odata, j):']
        for col in cols:
            code.append(' resdata[%d].append(data[%d][i])' %
                        (len(schema), self.__cols.index(col)))
            schema.append(col)
        for prefix, relcols, data, idx in [
                (selfname, self.__cols, 'data', 'i'),
                (othername, other.__cols, 'odata', 'j')]:
            for coli, col in enumerate(relcols):
                if col in cols:
                    continue
                code.append(' resdata[%d].append(%s[%d][%s])' %
                            (len(schema), data, coli, idx))
                schema.append(prefix + col)
        res = type(self)(schema, base=self)
        data = self.__data
        resdata = res.__data
        exec '\n'.join(code) in locals()

        # Perform join
        proj = self.__proj(cols)
        oindex = other.index(*cols)
        for i in xrange(len(self)):
            orows = oindex.get(proj(i))
            if orows is None:
                continue
            # Join row(i) with rows in orows
            for j in xrange(len(orows)):
                joiner(i, orows.__data, j)
        return res

    def str_table(self, width_limit=20):
        def trim(width, obj):
            s = str(obj)
            if len(s) > width:
                return s[:width - 3] + '...'
            else:
                return s.ljust(width)
        widths = [min(max(len(name), max(len(str(val)) for val in data)),
                      width_limit)
                  for name, data in zip(self.__cols, self.__data)]
        out = [' '.join(trim(w, name) for w, name in zip(widths, self.__cols))]
        for irow in xrange(len(self)):
            out.append(' '.join(trim(w, self.__data[icol][irow])
                                for w, icol in zip(widths, self.__colnums)))
        return '\n'.join(out)

def canon_tuple(vals):
    return tuple(sorted(vals))

def mscan(fp):
    """Parse an mscan.out into a TestSet."""

    data = json.load(fp)

    suite = None
    calls = []
    rel = TestSet('suite calls path testnum shared'.split())

    for testcase in data['testcases']:
        name = testcase['name']
        tsuite, rest = name.split('-', 1)
        if suite is None:
            suite = tsuite
        elif suite != tsuite:
            raise ValueError('Suite mismatch: %r vs %r' % (suite, tsuite))

        call1, call2, path, testnum = rest.split('_')
        for call in [call1, call2]:
            if call not in calls:
                calls.append(call)

        callset = canon_tuple([call1, call2])
        rel.add(suite, callset, path, int(testnum), testcase['shared'])

    rel.calls = calls
    return rel

def model_tests(fp):
    """Parse a model.out file into a Relation of tests."""

    rel = Relation('calls path testnum diverge idempotent_projs idempotence_unknown test'.split())
    for callsetname, callset in json.load(fp)['tests'].iteritems():
        calls = canon_tuple(callsetname.split('_'))
        for pathid, pathinfo in callset.iteritems():
            if 'tests' not in pathinfo:
                continue
            for testnum, test in enumerate(pathinfo['tests']):
                rel.add(calls, pathid, testnum,
                        pathinfo['diverge'],
                        test.get('idempotent_projs', None),
                        test.get('idempotence_unknown', 0),
                        test)
    return rel

class TestSet(Relation):
    def __init__(self, *args, **kwargs):
        if 'base' in kwargs:
            self.calls = kwargs['base'].calls
        else:
            self.calls = None
        super(TestSet, self).__init__(*args, **kwargs)

    def __str__(self):
        return "%d+%d/%d" % (self.shared, self.nonshared, self.total)

    @property
    def shared(self):
        n = 0
        for val in self.col('shared'):
            if len(val):
                n += 1
        return n

    @property
    def nonshared(self):
        return len(self) - self.shared

    @property
    def nonshared_frac(self):
        return self.nonshared / float(self.total)

    @property
    def total(self):
        return len(self)

    def table_ul(self, row_labels=None):
        """Return an upper-left Table of TestSet relations.

        row_labels, if provided, is a list of call names to use for
        the rows.  If omitted, it defaults to self.calls.  The column
        labels will be the reverse of this list.  The returned table
        is upper-left triangular and empty cells will be filled with
        None.
        """

        if row_labels is None:
            row_labels = self.calls

        callsets = self.index('calls')
        rows = []
        for call1i, call1 in enumerate(row_labels):
            row = []
            for call2 in reversed(row_labels[call1i:]):
                row.append(callsets.get(canon_tuple([call1, call2]), None))
            row.extend([None] * (len(row_labels) - len(row)))
            rows.append(row)
        return Table(rows, list(reversed(row_labels)), list(row_labels))

class Table(object):
    def __init__(self, rows, col_labels, row_labels):
        self.rows, self.col_labels, self.row_labels \
            = rows, col_labels, row_labels

    def __str__(self):
        return self.text()

    def text(self, shade=False):
        table = [[label] + row
                 for label, row in zip(self.row_labels, self.rows)]
        table = [[''] + self.col_labels] + table

        widths = []
        for row in table:
            if len(row) > len(widths):
                widths.extend([0] * (len(row) - len(widths)))
            for coli, col in enumerate(row):
                if col is not None:
                    widths[coli] = max(widths[coli], len(str(col)))

        lines = []
        for row in table:
            text = ''
            for w, v in zip(widths, row):
                cell = '%*s' % (w, '' if v is None else v)
                if shade and not v:
                    cell = '\033[1;30m' + cell + '\033[0m'
                text += cell + ' '
            lines.append(text)
        return '\n'.join(lines)

    def map(self, fn):
        nrows = [[v if v is None else fn(v)
                   for v in row] for row in self.rows]
        return Table(nrows, self.col_labels, self.row_labels)

    def mapget(self, field):
        return self.map(lambda v: getattr(v, field))

    def get(self, x, y):
        if x < 0 or y < 0 or y >= len(self.rows) or x >= len(self.rows[y]):
            return None
        return self.rows[y][x]

class Tikz(object):
    def __init__(self, width, height, xspace, yspace, table, fp):
        self.width, self.height, self.xspace, self.yspace \
            = width, height, xspace, yspace
        self.table, self.fp = table, fp

    @classmethod
    def text(cls, table, fp=None):
        if fp is None:
            fp = sys.stdout

        width = height = 0
        for y, row in enumerate(table.rows):
            for x, val in enumerate(row):
                if val is not None:
                    print >> fp, r'\path (1.5*%d+1.5,-%d-0.5) node[anchor=mid east] {%s};' % (x, y, val)
                    width, height = max(x+1, width), max(y+1, height)

        return cls(1.5*(width), height, 1.5, 1, table, fp)

    @classmethod
    def heatmap(cls, table, colorfn, fp=None):
        if fp is None:
            fp = sys.stdout

        # We go to great lengths here to render nicely in PDF engines
        # that approximate anti-aliasing using alpha blending (which
        # seems to be all of them except Acrobat).  If we simply
        # render rectangles with shared edges, the background color
        # will bleed through.  Hence, we need to overlap edges.  At
        # the same time, if two shapes overlap and share an edge, the
        # bottom of the two shapes will bleed through.  See
        #   http://bugs.ghostscript.com/show_bug.cgi?id=689557
        # To fix this, we bevel the edges of each cell, except where
        # that bevel would cover something already drawn.  Also, since
        # some of the fills are complex, we trace the boundary of each
        # region to minimize the number of fill operations.

        filled = set()
        for y, row in enumerate(table.rows):
            for x, val in enumerate(row):
                if val is None or (x,y) in filled:
                    continue

                # Treat (x,y) as the top-left coordinate on grid.
                # Trace clockwise around and bevel.
                edge, vert = [], collections.defaultdict(list)
                nx, ny = x, y

                # (dx, dy, chcekdx, checkdy)
                dirs = [(+1, +0,  +0, -1), # Right
                        (+0, -1,  -1, -1), # Up
                        (-1, +0,  -1, +0), # Left
                        (+0, +1,  +0, +0)] # Down
                curdir = 1

                while (nx, ny) != (x, y) or len(edge) == 0:
                    edge.append((nx, ny))
                    for curdir in range(curdir - 1, curdir + 2):
                        dx, dy, checkdx, checkdy = dirs[curdir % 4]
                        check = (nx+checkdx, ny+checkdy)
                        if table.get(*check) != val:
                            if check not in filled and \
                               table.get(*check) is not None:
                                edge.append((nx + dx*0.5 + dy*0.5,
                                             ny + dy*0.5 - dx*0.5))

                            if dy > 0: vert[ny].append(nx)
                            nx += dx
                            ny += dy
                            if dy < 0: vert[ny].append(nx)
                            break
                    else:
                        assert 0

                # Clean up degenerate convex bevels
                torm = []
                for i, (p1, p2, p3) in enumerate(zip(edge, edge[1:], edge[2:])):
                    if p1 == p3:
                        torm.extend([i, i+1])
                for i in reversed(torm):
                    del edge[i]

                # Add filled region to 'filled'
                for fy, verts in vert.items():
                    verts.sort()
                    assert len(set(verts)) == len(verts)
                    for left, right in zip(verts[::2], verts[1::2]):
                        for fx in range(left, right):
                            if table.get(fx, fy) == val:
                                filled.add((fx, fy))

                # Finally, trace the edge
                precolor, color = colorfn(val)
                if precolor:
                    print >> fp, precolor
                print >> fp, r'\fill[fill=%s] %s -- cycle;' % \
                    (color, ' -- '.join('(%g,%g)' % (x,-y) for x,y in edge))

        for y, row in enumerate(table.rows):
            if y % 4 == 0 and y != 0:
                print >> fp, r'\draw[color=white] (0,%g) -- (%g,%g);' % \
                    (-y, len(table.rows) - y + 1, -y)
                print >> fp, r'\draw[color=white] (%g,0) -- (%g,%g);' % \
                    (y, y, -(len(table.rows) - y + 1))

        width = max(pt[0]+1 for pt in filled)
        height = max(pt[1]+1 for pt in filled)
        return cls(width, height, 1, 1, table, fp)

    @classmethod
    def heatbar(cls, height, colorfn, fp=None,
                segments=50, height100=0.75, width=1, side='right'):
        if fp is None:
            fp = sys.stdout

        height_rest = height - height100
        if side == 'right':
            lx, lanchor, align = 1, 'west', 'left'
        else:
            lx, lanchor, align = 0, 'east', 'right'

        # Draw 100% box
        precolor, color = colorfn(1)
        if precolor:
            print >> fp, precolor
        print >> fp, r'\draw[fill=%s] (0,0) rectangle +(1,-%g);' % \
            (color, height100)
        print >> fp, \
            r'\draw (%g,-%g) node[anchor=north %s,inner ysep=0,align=%s,font=\scriptsize] {All tests\\conflict-free};' % \
            (lx, 0, lanchor, align)

        # Draw gradient
        for segment in range(segments):
            frac = segment / float(segments)
            precolor, color = colorfn(frac)
            if precolor:
                print >> fp, precolor
            print >> fp, r'\fill[fill=%s] (0,%g) rectangle (%g,%g);' % \
              (color, frac * height_rest - height, width, height_rest - height)
        print >> fp, \
            r'\draw (%g,%g) node[anchor=base %s,align=%s,font=\scriptsize] {All tests\\conflicted};' % \
            (lx, -height, lanchor, align)

        # Box the gradient
        print >> fp, r'\draw (0,%g) rectangle (%g,%g);' % \
            (-height, width, height_rest - height)

        return cls(width, height, None, None, None, fp)

    def top_labels(self):
        for i, l in enumerate(self.table.col_labels):
            print >> self.fp, \
                r'\path (%g,0) node[anchor=mid west,rotate=90] {%s};' % \
                (self.xspace * i + self.yspace / 2.0, l)
        return self

    def left_labels(self):
        for i, l in enumerate(self.table.row_labels):
            print >> self.fp, \
                r'\path (0,-%d-0.5) node[anchor=mid east] {%s};' % (i, l)
        return self

    def caption(self, caption):
        print >> self.fp, r'\path (%g,-%g-0.5) node[anchor=north] {%s};' % \
            (self.width / 2.0, self.height, caption)
        return self

    def overlay(self, table):
        for y, row in enumerate(table.rows):
            for x, val in enumerate(row):
                if val is None:
                    continue
                print >> self.fp, r'\path (%g,%g) node[inner sep=0] {%s};' % \
                    ((x+0.5)*self.xspace, -(y+0.5)*self.yspace, val)
        return self
