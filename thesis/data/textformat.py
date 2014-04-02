from __future__ import print_function

__all__ = ["parse"]

import re, shlex

def parse(fp):
    """Parse benchmark results in text format from a file-like object
    and return a list of dictionaries, with each dictionary containing
    one benchmark result.

    This format is intended to be a good balance between user- and
    machine-readable data.  It's designed so the output of small
    benchmarks is easy to copy-paste a data file, while still scaling
    to large benchmarks with complex configurations and many dependent
    variables.

    Each benchmark result is formatted as

      # --[config-name]=[value] --[config-name]=[value] ...
      [value] [result-name]
      [value] [result-name]
      ...

    All values are DWIM: if the value looks like an integer or a
    floating-point number, it will be parsed as such.  If it is "true"
    or "false" (case-insensitive), it will be parsed as a boolean.
    Otherwise, it will be kept as a string.

    The first line gives the input configuration of the benchmark as
    key/value pairs.  These should be escaped using shell escaping
    syntax.  If the value is true, the "=true" part may be omitted.
    The configuration line may be split across multiple lines, where
    each begins with a "#".  The configuration syntax is intended to
    be a valid command-line to the benchmark to reproduce the
    configuration, including arguments that take on default values.
    It may include things that the benchmark can't easily control,
    such as the running kernel; if the benchmark does accept such
    command-line arguments, it should abort if the argument is
    specified and does not match reality.

    The lines following the configuration give output results, with
    one result on each line.  The result value is anything up to the
    first space and the result name is everything that following.
    Since this is line-delimited, it does not require shell escaping.
    Multiple results may be given on the same line by separating them
    with commas.

    Benchmark results may be grouped by preceding them with headings
    of the form

      == [config-name]=[value] [config-name]=[value] ==

    The configuration values given in the heading are propagated to
    all results in the group.  The syntax is identical to the
    per-result configuration values, except that the "--"s are
    optional.  Groups may be further subdivided using the same syntax,
    but by increasing the number of "="s at the beginning and end of
    the line.  E.g.,

      = a=1 =
      == b=2 ==
      # Here a is 1 and b is 2

    Lines beginning with "#" but not "# --" are ignored.

    By convention, each "run" of benchmark results---where all
    configuration is held constant except a small set of configuration
    variables---should be assigned a configuration variable called
    "id", which gives the ISO 8601 date/time when the run was started.

    In the returned dictionary, all keys for configuration inputs have
    "iv." prepended (for "independent variable"), with the exception
    of "id", and all result outputs have "dv." prepended (for
    "dependent variable")."""

    res = []
    for (conf, block) in headerBlocks(fp):
        if not block.strip():
            continue
        row = dict(conf)
        row.update(dict(parseDV(block, parseVal)))
        res.append(row)
    return res

def maybeFloat(s):
    try:
        return float(s)
    except ValueError:
        return s

def maybeNum(s):
    if s.strip().isdigit():
        return int(s)
    return maybeFloat(s)

def parseVal(s):
    if s.lower() == "true":
        return True
    if s.lower() == "false":
        return False
    return maybeNum(s)

def headerBlocks(fp):
    headMaps = [[] for l in range(10)]
    headMap = []
    confMap = []
    lines = []
    for line in fp:
        oline = line
        line = line.strip()
        prevMap = None
        m = re.match("(=+) (.*?) \\1", line)
        if m:
            # XXX Generate missing ids
            prevMap = flattenMap(headMap + confMap)
            level = len(m.group(1))
            headMaps = (headMaps + [[] for l in range(level)])[:level]
            headMaps.append(parseMap(m.group(2), False))
            headMap = []
            for m in headMaps:
                headMap.extend(m)
            headMap = flattenMap(headMap)
            confMap = []
        elif line.startswith("# --"):
            prevMap = flattenMap(headMap + confMap)
            confMap = confMap + parseMap(line[1:], True)
        elif line.startswith("#"):
            continue

        if prevMap is not None:
            if "".join(lines).strip():
                yield (prevMap, "".join(lines))
            lines = []
        else:
            lines.append(oline)

    if "".join(lines).strip():
        yield (flattenMap(headMap + confMap), "".join(lines))

def parseMap(s, dashes):
    res = []
    for tok in shlex.split(s, False, True):
        if "=" not in tok:
            tok += "=true"
        if dashes or tok.startswith("--"):
            assert tok.startswith("--")
            tok = tok[2:]
        k, v = tok.split("=", 1)
        res.append((k if k == "id" else "iv." + k, parseVal(v)))
    return res

def flattenMap(expMap):
    # XXX Check that repeated keys agree and flatten
    return expMap

def parseDV(s, vParser = lambda x:x):
    s = s.replace(",", "\n")
    s = re.sub(r"\(([0-9]+ [^)]+)\)", r"\n\1", s)
    for line in s.splitlines():
        if not line.strip():
            continue
        v, k = line.strip().split(" ", 1)
        yield "dv." + k.strip(), vParser(v.strip())
