PROPOSAL = p.tex
BASE := $(basename $(PROPOSAL))
BIOS = bio-kaashoek.pdf bio-rtm.pdf bio-nickolai.pdf bio-kohler.pdf
PIECEMEAL = submit-summary.pdf submit-description.pdf submit-references.pdf \
	    submit-personnel.pdf submit-commitment.pdf submit-datamgmt.pdf \
	    submit-conflicts.pdf submit-postdoc.pdf
EVERYTHING = $(BASE).pdf $(PIECEMEAL) $(BIOS)
SPELLTEX := $(shell ./bin/get-tex-files.sh $(PROPOSAL))

OURENV = TEXINPUTS="sty:" 

LATEX = $(OURENV) pdflatex
BIBTEX = bibtex --min-crossrefs=100

all: $(BASE).pdf $(BIOS)
# all: $(EVERYTHING)
pieces: $(PIECEMEAL)
bios: $(BIOS)

# parts depend on the whole
$(PIECEMEAL): $(BASE).pdf

XFIGS := $(wildcard figures/*.fig)
DIAFIGS := $(wildcard figures/*.dia)
EPSFIGS := $(DIAFIGS:.dia=.eps)
PDFFIGS := $(DIAFIGS:.dia=.pdf)
TEXFIGS := $(XFIGS:.fig=.tex)
XGRAPHS := graph/linkbench-xv6.gnuplot
FIGGRAPHS := $(XGRAPHS:.gnuplot=.fig)
EPSGRAPHS := $(XGRAPHS:.gnuplot=.eps)
PDFGRAPHS := $(XGRAPHS:.gnuplot=.pdf)
TEXGRAPHS := $(XGRAPHS:.gnuplot=.tex)

TEX := $(wildcard *.tex) $(TEXFIGS)
BIBS := $(wildcard *.bib)

DEPS := $(TEX) pyexpr.tex $(BIBS) $(PDFFIGS) $(TEXGRAPHS)

GENDATA = graph/single-uni-speedup.data \
          graph/single-zipf-speedup.data \
	  graph/par-uni-throttle-speedup.data


.PRECIOUS: $(FIGGRAPHS) $(GENDATA)

graph/%.tex: graph/%.gnuplot graph/common.gnuplot data/*
	(cd $(<D) && TARGET=paper-tikz gnuplot $(<F)) > $@.tmp
	mv $@.tmp $@

gnuplot-lua-tikz.sty:
	gnuplot -e 'set term tikz createstyle'
	rm gnuplot-lua-tikz.tex t-gnuplot-lua-tikz.tex

%.pdf: %.svg
        inkscape -z --export-text-to-path --export-pdf=$@ $<

pyexpr.tex:
	python2.7 pyexpr.py $(SPELLTEX) > $@.tmp
	if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi
.PHONY: pyexpr.tex

RERUN = egrep -q '(^LaTeX Warning:|\(natbib\)).* Rerun'
UNDEFINED = egrep -q '^(LaTeX|Package natbib) Warning:.* undefined'

%.pdf: %.tex
	$(LATEX) $<
	! $(UNDEFINED) $*.log || $(LATEX) $<
	! $(RERUN) $*.log || $(LATEX) $<
	! $(RERUN) $*.log || $(LATEX) $<

$(BASE).pdf: $(DEPS)
	test ! -s $*.aux || $(BIBTEX) $* || rm -f $*.aux $*.bbl
	$(LATEX) $(BASE)
	@if test ! -f $(BASE).bbl || $(RERUN) $(BASE).log \
			|| $(UNDEFINED) $(BASE).log; then \
		set -x; \
		$(BIBTEX) $(BASE); \
		$(LATEX) $(BASE); \
		$(LATEX) $(BASE); \
		$(LATEX) $(BASE); \
	fi
	test ! -f .xpdf-running || xpdf -remote $(BASE)-server -reload
	touch $(BASE).dvi

$(PIECEMEAL): %.pdf : $(DEPS)
	$(LATEX) $*
#	! grep -q 'Rerun' $*.log || $(LATEX) $*
#	! grep -q 'Rerun' $*.log || $(LATEX) $*

BIO_RULES = \
	usr=`expr $@ : 'bio-\([a-z]*\).pdf'`;			\
	(test ! -s $*.aux || ./bcv -$$usr || rm -f $*.aux)	\
	&& $(LATEX) $*						\
	&& (if grep -q 'Rerun' $*.log; then			\
		./bcv -$$usr					\
		latex $*;					\
		grep -q 'Rerun' $*.log && $(LATEX) $*; :;	\
		grep -q 'Rerun' $*.log && $(LATEX) $*; :;	\
	fi); 							\
	rm -f bu-$${usr}*.bbl bu-$${usr}*.blg bu-$${usr}*.aux

$(BIOS) : %.pdf : %.tex $(BIBS)
	$(BIO_RULES)

xpdf: $(BASE).pdf
	if test -f .xpdf-running; then			\
		xpdf -remote $(BASE)-server -quit;	\
		sleep 1;				\
	fi
	touch .xpdf-running
	(xpdf -remote $(BASE)-server $(BASE).pdf; rm -f .xpdf-running) &

spell:
	@ for i in $(SPELLTEX); do aspell --mode=tex -p ./aspell.words -c $$i; done
	@ for i in $(SPELLTEX); do perl bin/double.pl $$i; done
	@ for i in $(SPELLTEX); do perl bin/abbrv.pl  $$i; done
	@ bash bin/hyphens.sh $(SPELLTEX)
	@ ( head -1 aspell.words ; tail -n +2 aspell.words | sort ) > aspell.words~
	@ mv aspell.words~ aspell.words

clean:
	-rm -f *.aux *.dvi *.log *.blg *.bbl *.bak *.lof *.lot *.toc *.brf *.out
	-rm -f $(EVERYTHING)
	-rm -f $(PIECEMEAL:.pdf=.tex)
	-rm -f $(BASE_NORED).pdf
	-rm -f $(EPSFIGS) $(PDFFIGS) $(EPSGRAPHS) $(PDFGRAPHS)

.PHONY: clean spell
