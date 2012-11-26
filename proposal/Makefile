RUBBER=rubber -Wrefs -Wmisc

GRAPHS=$(filter-out %/common.gnuplot,$(wildcard graph/*.gnuplot))
FIGS=$(wildcard *.svg)

pdf: proposal.pdf
.PHONY: pdf

graph/%.tex: graph/%.gnuplot graph/common.gnuplot data/*
	(cd $(<D) && TARGET=paper-tikz gnuplot $(<F)) > $@.tmp
	mv $@.tmp $@
CLEAN+=$(GRAPHS:.gnuplot=.tex)

gnuplot-lua-tikz.sty:
	gnuplot -e 'set term tikz createstyle'
	rm gnuplot-lua-tikz.tex t-gnuplot-lua-tikz.tex
CLEAN+=gnuplot-lua-tikz.sty gnuplot-lua-tikz-common.tex

%.pdf: %.svg
	inkscape -z --export-text-to-path --export-area-drawing --export-pdf=$@ $<

pyexpr.tex:
	python2.7 pyexpr.py $(SPELLTEX) > $@.tmp
	if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi
.PHONY: pyexpr.tex
CLEAN+=pyexpr.tex

gitinfo.tex:
	echo "\def\gitid{$$(git describe --always --dirty=+)}" > $@.tmp
	cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@
.PHONY: gitinfo.tex
CLEAN+=gitinfo.tex gitinfo.tex.tmp

proposal.pdf: $(GRAPHS:.gnuplot=.tex) gnuplot-lua-tikz.sty \
	   gitinfo.tex pyexpr.tex $(FIGS:.svg=.pdf)
	-@./lint $(SPELLTEX)
	$(RUBBER) --pdf proposal.tex
.PHONY: proposal.pdf

proposal.ps: proposal.pdf
	acroread -toPostScript $<

.PHONY: lint
lint:
	./lint $(SPELLTEX)

clean:
	$(RUBBER) --pdf --clean proposal.tex
	rm -f $(CLEAN)
.PHONY: clean

SPELLTEX := $(shell ./bin/get-tex-files.sh proposal.tex)
spell:
	@ for i in $(SPELLTEX); do aspell --mode=tex -p ./aspell.words -c $$i; done
	@ for i in $(SPELLTEX); do perl bin/double.pl $$i; done
	@ for i in $(SPELLTEX); do perl bin/abbrv.pl  $$i; done
	@ bash bin/hyphens.sh $(SPELLTEX)
	@ ( head -1 aspell.words ; tail -n +2 aspell.words | sort ) > aspell.words~
	@ mv aspell.words~ aspell.words
.PHONY: spell

%-bw.pdf: %.pdf
	./bin/pdftobw $< $@
