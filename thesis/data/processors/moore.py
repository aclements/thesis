import collections
import datetime
import json
import re
import csv
import os
import itertools

class Proc(collections.namedtuple(
        'Proc', 'name date clock_mhz cores tdp_watts product_id')):

    def dominates(self, other):
        """Return True if self strictly dominates other."""
        cmps = [cmp(getattr(self, field), getattr(other, field))
                for field in ('clock_mhz', 'cores', 'tdp_watts')]
        return all(c >= 0 for c in cmps) and any(c > 0 for c in cmps)

def parse_odata_date(s):
    m = re.match(r'/Date\(([0-9]+)\)/', s)
    return datetime.date.fromtimestamp(int(m.group(1)) / 1000)

def read_ark(fp):
    d = json.load(fp)['d']
    for rec in d:
        if 'Phi' in rec['ProductName']:
            # These reach to higher core counts, but I'm not sure I
            # would consider the "general purpose".
            continue
        if rec['LaunchDate'] is None:
            # XXX Lots of these have BornOnDate
            continue
        if rec['MaxTDP'] is None:
            continue
        date = parse_odata_date(rec['LaunchDate'])
        yield Proc(name=rec['ProductName'], date=date,
                   clock_mhz=rec['ClockSpeedMhz'],
                   cores=rec['CoreCount'], # * (rec['MaxCPUs'] or 1),
                   tdp_watts=rec['MaxTDP'],
                   product_id=('Intel', rec['ProductId']))

def read_cpudb(path):
    x86s = {row['microarchitecture_id']
            for row in csv.DictReader(
                    open(os.path.join(path, 'microarchitecture.csv')))
            if row['isa'] in ('x86-32', 'x86-64')}
    manu = {row['manufacturer_id']
            for row in csv.DictReader(
                    open(os.path.join(path, 'manufacturer.csv')))
            if row['name'] in ('AMD', 'Intel')}
    for rec in csv.DictReader(open(os.path.join(path, 'processor.csv'))):
        # if rec['microarchitecture_id'] not in x86s:
        #     continue
        if rec['manufacturer_id'] not in manu:
            continue
        if rec['date'].startswith('1982-'):
            # Meh.  Points before 1985 are just to make the smoothing
            # pretty (we don't actually show them), and the 80286
            # messes with our pretty smoothing.
            continue
        date = rec['date']
        if not date:
            continue
        date = datetime.date(*map(int, date.split('-')))
        if not rec['tdp']:
            continue

        m = re.match(r'http://ark.intel.com/Product\.aspx\?id=([0-9]+)',
                     rec['source'])
        if m:
            product_id = ('Intel', int(m.group(1)))
        else:
            product_id = None

        yield Proc(name=rec['model'], date=date,
                   clock_mhz=float(rec['clock']),
                   cores=int(rec['hw_ncores']),
                   tdp_watts=float(rec['tdp']),
                   product_id=product_id)

def dedup(ark, cpudb):
    ids = set()
    for proc in ark:
        yield proc
        ids.add(proc.product_id)
    for proc in cpudb:
        if proc.product_id is None or proc.product_id not in ids:
            yield proc

def dedominate_month(procs):
    """From each month, remove processors strictly dominated by another.

    This usually weeds out multiple speeds of the same basic model.
    The result is somewhat messy.  Not recommended.
    """
    groups = {}
    for proc in procs:
        key = proc.date.replace(day=1)
        groups.setdefault(key, []).append(proc)
    for procs in groups.itervalues():
        for proc in procs:
            if not any(other.dominates(proc) for other in procs):
                yield proc

def dedominate_past(procs):
    """Remove processors strictly dominated by an earlier processor.

    This focuses on "top of the line" processors.
    """
    groups = {}
    for proc in procs:
        groups.setdefault(proc.date, []).append(proc)
    kept = []
    for date, procs in sorted(groups.iteritems()):
        for proc in procs:
            # Is this processor is dominated by an earlier processor
            # or one released at the same time?
            if not any(other.dominates(proc) for other in kept + procs):
                kept.append(proc)
                yield proc

for proc in dedominate_past(dedup(
        read_ark(open('ark/processors.json')),
        read_cpudb('cpudb'))):
    df = proc.date.year + (proc.date.replace(year=1).toordinal() / 365.0)
    print df, proc.clock_mhz, proc.tdp_watts, proc.cores, proc.name.encode('utf-8')
