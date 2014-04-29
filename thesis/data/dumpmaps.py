#!/usr/bin/python

# Usage: ./dumpmaps PID outbase
#
# Save the VM maps metadata for a running process to a set of
# outbase-x files.  Values of x include:
#
# * maps: A copy of the /proc/PID/maps file.  Each line is a
#   vm_area_struct.
# * pagemap: A list of ranges of pages present in the page table.
#   Each range is half-open (just like in maps).

import sys, os

PGSIZE = 4096

def parse_maps(maps):
    for line in maps:
        fields = line.split()
        start, end = [int(v, 16) for v in fields[0].split("-")]
        used = not fields[1].startswith("---")
        yield start, end, used

def parse_pagemap(maps_list, pagemap):
    # Parse pagemap file.  See fs/proc/task_mmu.c:pagemap_read.
    ranges = []
    for start, end, used in parse_maps(file("/proc/%s/maps" % sys.argv[1])):
        pagemap.seek(8 * start / PGSIZE)
        want = 8 * (end - start) / PGSIZE
        pages = pagemap.read(want)
        if len(pages) != want:
            print >> sys.stderr, "Skipping %x-%x" % (start, end)
            if used:
                ranges.append([start, end])
            continue
        for i in range((end - start) / PGSIZE):
            # Bit 63 is "page present"
            if ord(pages[i*8 + 7]) & 0x80:
                va = start + i * PGSIZE
                if not ranges or ranges[-1][1] != va:
                    ranges.append([va, va + PGSIZE])
                else:
                    ranges[-1][1] = va + PGSIZE
    return ranges

if __name__ == "__main__":
    maps_data = file("/proc/%s/maps" % sys.argv[1]).read()
    file(sys.argv[2] + "-maps", "w").write(maps_data)

    pagemap = file("/proc/%s/pagemap" % sys.argv[1], "rb")
    pagemap_out = file(sys.argv[2] + "-pagemap", "w")
    for start, end in parse_pagemap(parse_maps(maps_data.splitlines()), pagemap):
        print >> pagemap_out, "%x-%x" % (start, end)
