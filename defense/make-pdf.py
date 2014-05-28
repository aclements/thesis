# Run snapshot.py first to create thumbs-svg

import sys, os, glob, subprocess, pdf

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
pdfPaths = []
for svg in sorted(glob.glob("thumbs-svg/*.svg")):
    pdfPath = svg[:-3] + "pdf"
    ink.do("%s --export-area-page --export-pdf=%s" % (svg, pdfPath))
    pdfPaths.append(pdfPath)
ink.flush()

# Merge PDFs
print >> sys.stderr, "Merging..."
subprocess.check_call(["pdftk"] + pdfPaths + ["cat", "output", "slides.pdf"])

# Set viewer preferences
f = pdf.File('slides.pdf')
catalogR = f.trailer['Root']
catalog = pdf.deref(catalogR)
catalog['PageLayout'] = pdf.Name('SinglePage')
if 'ViewerPreferences' in catalog:
    raise ValueError('Already has viewer preferences')
catalog['ViewerPreferences'] = {'FitWindow': True}
catalogR.replace(catalog)
f.write_to('slides.pdf')

# Linearize
print >> sys.stderr, "Linearizing..."
subprocess.check_call(['qpdf', '--linearize', 'slides.pdf', 'slides2.pdf'])
os.rename('slides2.pdf', 'slides.pdf')
