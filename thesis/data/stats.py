__all__ = ["mean", "min", "max", "stddev"]

import math

def mean(xs, default=None):
    """Return the arithmetic mean of 'xs'.  'xs' may be any iterator.
    If 'xs' is empty, returns 'default'."""

    total = 0
    count = 0
    for val in xs:
        total += val
        count += 1
    if count == 0:
        return default
    return float(total) / count

bmin = min
def min(xs, default=None):
    """Return the minimum value of 'xs'.  'xs' may be any iterator.
    If 'xs' is empty, returns 'default'."""

    try:
        return bmin(xs)
    except ValueError:
        return default

bmax = max
def max(xs, default=None):
    """Return the maximum value of 'xs'.  'xs' may be any iterator.
    If 'xs' is empty, returns 'default'."""

    try:
        return bmax(xs)
    except ValueError:
        return default

def stddev(xs, default=None):
    """"Return the sample standard deviation of 'xs'.  This uses a
    single-pass algorithm, so 'xs' may be any iterator.  If 'xs' is
    empty, returns 'default'."""

    # Based on Wikipedia's presentation of Welford 1962.
    A = Q = k = 0
    for x in xs:
        Anext = A + float(x - A)/k
        Q += (x - A)*(x - Anext)
        A = Anext
        k += 1
    if k == 0:
        return default
    return math.sqrt(Q/(k - 1))

def pctvar(xs, default=None):
    """Return the percent variance of the maximum relative to the mean.
    'xs' may be any iterator.  If 'xs' is empty, returns 'default'."""
    total = 0
    count = 0
    maxval = 0
    for val in xs:
        total += val
        count += 1
        if val > maxval:
            maxval = val
    if count == 0:
        return default
    meanval = float(total) / count
    return 100 * (maxval / meanval - 1)
