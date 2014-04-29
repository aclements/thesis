#!/usr/bin/python

# Usage:
#   ./vmsize mapfile [pagemapfile]
# or
#   ./vmsize pid
#
# Compute the VM metadata size of a tree representation, a skiplist
# representation, and a radix representation of the given address
# space.  If pagemapfile is not given, it is assumed that all pages
# are mapped.

import sys, math
from dumpmaps import parse_maps, parse_pagemap

USERTOP = 0x0000800000000000
PGSIZE = 4096

VMDESC = ["flags", "page", "inode", "start"]
VMDESC_SIZE = len(VMDESC) * 8

VMA = ["flags", "inode", "start", "vm_start", "vm_end", "pages"]
def vma_size(pages):
    # Assumes pages array has to allocate from a power-of-two bucket
    return len(VMA) * 8 + round_up_pow2(pages * 8)

LINUX_VMA = ["mm", "start", "end", "next", "prev", "page_prot", "flags",
             "rb_parent_color", "rb_left", "rb_right",
             "l.rb_parent_color", "l.rb_left", "l.rb_right",
             "l.rb_subtree_last",
             "avc.next", "avc.prev", "anon_vma", "vm_ops", "pgoff", "file",
             "private", "policy"]
LINUX_VMA_SIZE = len(LINUX_VMA) * 8

RADIX_NODE_SIZE = PGSIZE

def round_up_pow2(x):
    if x == 0:
        return 0
    return 1 << int(math.ceil(math.log(x, 2)))

def parse_saved_pagemap(pagemap):
    for line in pagemap:
        start, end = [int(v, 16) for v in line.split("-")]
        yield start, end

def tree_size(maps_list):
    size = 0
    for start, end, used in maps_list:
        if used:
            size += vma_size((end - start) / PGSIZE)
        else:
            size += vma_size(0)
        # Plus left, right pointers
        size += 2 * 8
    return size

def page_table_size(pagemap_list):
    r = Radix(1<<48>>12, 8)
    for start, end in pagemap_list:
        for i in range(start, end, PGSIZE):
            if i > 1<<47:
                i = i & (1 << 48 - 1)
            r.insert(i / PGSIZE)
    return r.space()

def linux_size(maps_list, pagemap_list):
    return LINUX_VMA_SIZE * len(maps_list), page_table_size(pagemap_list)

def skip_list_size(maps_list):
    # Skip graph overhead: ~len(maps_list) interior nodes * two pointers each
    size = len(maps_list) * 8 * 2
    # Compute VMAs size
    for start, end, used in maps_list:
        if used:
            size += vma_size((end - start) / PGSIZE)
        else:
            size += vma_size(0)
        # Plus a next pointer
        size += 8
    return size

class Radix(object):
    def __init__(self, max_val, leaf_size=VMDESC_SIZE):
        self.nlevels = 0
        self.leaf_size = leaf_size
        max_val -= 1
        while max_val:
            if self.nlevels == 0:
                max_val /= RADIX_NODE_SIZE / leaf_size
            else:
                max_val /= RADIX_NODE_SIZE / 8
            self.nlevels += 1

        self.allocated = [set() for n in range(self.nlevels)]

    def insert(self, val):
        val /= RADIX_NODE_SIZE / self.leaf_size
        for level in range(self.nlevels):
            self.allocated[level].add(val)
            val /= RADIX_NODE_SIZE / 8
        assert val == 0

    def space(self):
        return sum(len(level) * RADIX_NODE_SIZE for level in self.allocated)

def radix_size(maps_list, pagemap_list=None):
    r = Radix(USERTOP / PGSIZE)
    for start, end, used in maps_list:
        if start >= USERTOP:
            continue
        if pagemap_list:
            # We'll fill in the precise pages later
            used = False
        if used:
            for i in range(start, end, PGSIZE):
                r.insert(i / PGSIZE)
        else:
            # In terms of node allocation, filling a range is
            # identical to inserting just the first and last
            r.insert(start / PGSIZE)
            r.insert(end / PGSIZE - 1)

    # If we have a precise pagemap, fill in the individual pages
    if pagemap_list:
        for start, end in pagemap_list:
            for i in range(start, end, PGSIZE):
                r.insert(i / PGSIZE)

    return r.space()

def rss(pagemap_list):
    total = 0
    for start, end in pagemap_list:
        total += end - start
    return total

if __name__ == "__main__":
    if sys.argv[1].isdigit():
        maps_list = list(parse_maps(file("/proc/%s/maps" % sys.argv[1])))
        pagemap_list = list(parse_pagemap(
                maps_list, file("/proc/%s/pagemap" % sys.argv[1])))
    else:
        maps_list = list(parse_maps(file(sys.argv[1])))
        if len(sys.argv) > 2:
            pagemap_list = list(parse_saved_pagemap(file(sys.argv[2])))
        else:
            pagemap_list = None
    r = rss(pagemap_list)
    print "RSS:     ", r
    pts = page_table_size(pagemap_list)
    print "PT:      ", pts, "(%g%% of RSS)" % (100.0 * pts / r)
    ts = tree_size(maps_list)
    print "Tree:    ", ts, "(%g%% of RSS)" % (100.0 * ts / r)
    ss = skip_list_size(maps_list)
    print "Skiplist:", ss, "(%g%% of RSS)" % (100.0 * ss / r)
    rs = radix_size(maps_list, pagemap_list)
    print "Radix:   ", rs, \
        "(%gx tree, %g%% of RSS)" % (float(rs) / ts, 100.0 * rs / r)
    ls = linux_size(maps_list, pagemap_list)
    print "Linux:   ", ls[0], "+", ls[1]
