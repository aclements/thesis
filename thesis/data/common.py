import sys

import data
import textformat

def argv_to_table():
    files = []
    for fname in sys.argv[1:]:
        files.append(file(fname))
    if not files:
        files = [sys.stdin]
    raw = []
    for fp in files:
        raw.extend(textformat.parse(fp))
    return data.Table(raw)

def files_to_table(*fnames):
    raw = []
    for fname in fnames:
        raw.extend(textformat.parse(file(fname)))
    return data.Table(raw)
