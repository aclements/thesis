#!/usr/bin/python

import argparse
import readmscan
import sys

parser = argparse.ArgumentParser()
parser.add_argument('-s', '--mscan', nargs=2, action='append', required=True,
                    metavar=('SYSNAME', 'MSCAN'),
                    help='mscan --check-testcases output')
args = parser.parse_args()

def nfmt(v):
    return '{0:,}'.format(v)

# Color for 100% scalable
print r'\definecolor{myfull-hsb}{hsb}{0.34065,1,0.91}'
# Color blended towards for 100% scalable
print r'\definecolor{mygreen-hsb}{hsb}{0.34065,1,0.91}'
# Color for 0% scalable
print r'\definecolor{myred-hsb}{hsb}{0,0.8,1}'

def heat_map_color(frac):
    if frac == 1:
        # Convert myfull-hsb to rgb space since that's what TikZ needs.
        setup = r'\colorlet{mycolor}{rgb:myfull-hsb,1}'
        fill = (r'mycolor,path picture={\foreach \x in {-20,...,20} '
                r'\draw[color=mycolor!70,line width=0.45em] '
                r'(\x,1)-- +(20,-20);}')
    else:
        # Do the blend in hsb space, then convert to rgb
        setup = r'\colorlet{mycolor}{rgb:mygreen-hsb!%d!myred-hsb,1}' \
                % (frac * 50)
        fill = 'mycolor'
    return setup,fill

calls = [
    # Things that deal with names
    'open', 'link', 'unlink', 'rename', 'stat',
    # Things that deal with FDs
    'fstat', 'lseek', 'close', 'pipe', 'read', 'write', 'pread', 'pwrite',
    # VM
    'mmap', 'munmap', 'mprotect', 'memread', 'memwrite']

def concat(lists):
    res = []
    for l in lists:
        res.extend(l)
    return res

testsets = []
for sysname, fname in args.mscan:
    testset = readmscan.mscan(file(fname))
    # Uncomment the following to count paths instead of test cases
    #testset = testset.group('suite calls path'.split(), ('shared', concat))
    testsets.append((sysname, testset))
    assert set(testset.calls) == set(calls)
atestset = testsets[0][1]

for idx, (s0, s1) in enumerate(zip(testsets[0][1].col('shared'),
                                   testsets[1][1].col('shared'))):
    if len(s1) > 0 and len(s0) == 0:
        print >>sys.stderr, "Second mscan output not scalable:\n", \
                            testsets[1][1].row(idx)

pad = 1
pos = 0
print r'\begin{tikzpicture}[x=\baselineskip,y=\baselineskip]'

# counts = readmscan.Tikz.text(atestset.table_ul(calls).mapget('total'))
# counts.left_labels().top_labels().caption(
#     'Commutative test cases (%s total)' %
#     nfmt(atestset.total))
# pos += counts.width + pad

for i, (sysname, testset) in enumerate(testsets):
    print r'\begin{scope}[shift={(%g,0)}]' % pos
    table = testset.table_ul(calls)
    hmap = readmscan.Tikz.heatmap(table.mapget('nonshared_frac'),
                                  heat_map_color)
    if i == 0:
        hmap.left_labels()
    hmap.top_labels().caption(
        r'%s (%s of %s cases scale)' %
        (sysname, nfmt(testset.nonshared), nfmt(testset.total)))
    print r'\begin{scope}[font=\fontsize{6pt}{1em}\selectfont]'
    hmap.overlay(table.map(lambda ts: None if ts.shared == 0 else ts.shared))
    print r'\end{scope}'
    pos += hmap.width + pad
    print r'\end{scope}'

print r'\begin{scope}[shift={(%g,%g)}]' % (pos - 2, -hmap.height / 2)
readmscan.Tikz.heatbar(hmap.height / 2, heat_map_color, side='left')
print r'\end{scope}'

print r'\end{tikzpicture}'
