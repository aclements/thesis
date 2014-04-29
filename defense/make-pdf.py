# Run snapshot.py first to create thumbs-svg

import sys, glob, subprocess

class Inkscape(object):
    def __init__(self):
        self.ink = None
        self.count = 0

    def do(self, cmd):
        if not self.ink:
            self.ink = subprocess.Popen(["inkscape", "--shell"], stdin=subprocess.PIPE)
        print >> self.ink.stdin, cmd

        # Ugh.  Inkscape 0.48 has a memory leak
        self.count += 1
        if self.count == 10:
            self.flush()

    def flush(self):
        if not self.ink:
            return
        self.ink.stdin.close()
        if self.ink.wait():
            raise RuntimeError("inkscape exited with non-zero status %s" % self.ink.returncode)
        self.ink = None
        self.count = 0
        print

# Make PDFs
print >> sys.stderr, "Converting to PDF..."
ink = Inkscape()
pdfs = []
for svg in sorted(glob.glob("thumbs-svg/*.svg")):
    pdf = svg[:-3] + "pdf"
    ink.do("%s --export-area-page --export-pdf=%s" % (svg, pdf))
    pdfs.append(pdf)
ink.flush()

# Merge PDFs
print >> sys.stderr, "Merging..."
subprocess.check_call(["pdftk"] + pdfs + ["cat", "output", "slides.pdf"])
