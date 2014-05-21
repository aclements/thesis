RUBBER=rubber -Wrefs -Wmisc

GRAPHS=$(filter-out %/common.gnuplot,$(wildcard graph/*.gnuplot))
FIGS=$(wildcard *.svg)

pdf: thesis.pdf
.PHONY: pdf

ifeq ($(V),)
quiet = @printf "  %-7s %s\n" "$1" "$@";
Q = @
else
quiet =
Q =
endif

graph/%.tex: graph/%.gnuplot graph/common.gnuplot $(shell find data -type f)
	$(call quiet,GNUPLOT)
	$(Q)(cd $(<D) && TARGET=paper-tikz gnuplot $(<F)) > $@.tmp
	$(Q)mv $@.tmp $@

gnuplot-lua-tikz.sty:
	$(Q)gnuplot -e 'set term tikz createstyle'
	$(Q)rm -f gnuplot-lua-tikz.tex t-gnuplot-lua-tikz.tex

%.pdf: %.svg
	$(call quiet,INKSCAPE)
	sed -re 's/(font-family:)(Times New Roman)/\1Minion Pro,\2/g' < $< > $*.mp.svg
	inkscape -z --export-pdf=$@ $(INKARGS) $*.mp.svg
	rm $*.mp.svg

figures/refcache.pdf: INKARGS+=--export-area-drawing

all-figures: $(addprefix figures/,machines.pdf refcache.pdf radix.pdf as.pdf)

# linuxcomp.tex: data/filebench-*.data data/dirbench-*.data mklinuxcomp
# 	./mklinuxcomp > $@~ && mv $@~ $@

pyexpr.tex:
	$(call quiet,PYEXPR)
	$(Q)python2.7 pyexpr.py $(SPELLTEX) > $@.tmp
	$(Q)if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi
.PHONY: pyexpr.tex
CLEAN+=pyexpr.tex

gitinfo.tex:
	$(call quiet,GITINFO)
	$(Q)echo "\def\gitid{$$(git describe --always --dirty=+)}" > $@.tmp
	$(Q)cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@
.PHONY: gitinfo.tex
CLEAN+=gitinfo.tex gitinfo.tex.tmp

thesis.pdf: $(GRAPHS:.gnuplot=.tex) gnuplot-lua-tikz.sty \
	   gitinfo.tex pyexpr.tex figures/testcases-linux.tex \
	   figures/testcases-sv6.tex \
	   FORCE
	-@./lint $(SPELLTEX)
	$(Q)./check-fonts
	$(call quiet,LATEX)
	$(Q)export TEXMFHOME=$$PWD/fonts/texmf TEXMFVAR=$$PWD/fonts/texmfvar; \
	if which latexrun >/dev/null 2>&1; then \
		latexrun --bibtex-args=-min-crossrefs=100 thesis.tex; \
	else \
		( echo '#!/bin/sh' ; echo 'exec '$(shell which bibtex)' -min-crossrefs=100 "$$@"' ) > bibtex; \
		chmod 755 bibtex; \
		PATH=.:$$PATH $(RUBBER) --pdf thesis.tex; \
	fi
.PHONY: FORCE

thesis.ps: thesis.pdf
	acroread -toPostScript $<

.PHONY: lint
lint:
	./lint $(SPELLTEX)

clean:
	if which latexrun >/dev/null 2>&1; then \
		latexrun --clean-all; \
	else \
		$(RUBBER) --pdf --clean thesis.tex; \
	fi
	rm -f $(GRAPHS:.gnuplot=.tex) gnuplot-lua-tikz.sty gnuplot-lua-tikz-common.tex
.PHONY: clean

view: thesis.pdf
	evince thesis.pdf &
.PHONY: view

SPELLTEX := $(shell ./bin/get-tex-files.sh thesis.tex)
spell:
	@ for i in $(SPELLTEX) *.bbl; do aspell \
		--mode=tex \
		--add-tex-command="pyexpr p" \
		--add-tex-command="XXX op" \
		--add-tex-command="code p" \
		--add-tex-command="cmd p" \
		--add-tex-command="cref p" \
		--add-tex-command="Cref p" \
		--add-tex-command="thiscref p" \
		--add-tex-command="Thiscref p" \
		--add-tex-command="lcnamecref p" \
		-p ./aspell.words -c $$i; done
	@ for i in $(SPELLTEX); do perl bin/double.pl $$i; done
	@ bash bin/hyphens.sh $(SPELLTEX)
	@ ( head -1 aspell.words ; tail -n +2 aspell.words | sort ) > aspell.words~
	@ mv aspell.words~ aspell.words
.PHONY: spell

%-bw.pdf: %.pdf
	./bin/pdftobw $< $@

figures/testcases.tex: data/testcase-tbl.py \
			data/readmscan.py \
			data/mscan-linux.out \
			data/mscan-xv6.out
	$(call quiet,TESTCASE)
	$(Q)python data/testcase-tbl.py \
		--mscan 'Linux' data/mscan-linux.out \
		--mscan '\sys' data/mscan-xv6.out \
		> $@

figures/testcases-linux.tex: data/testcase-tbl.py \
			data/readmscan.py \
			data/mscan-linux.out
	$(call quiet,TESTCASE)
	$(Q)python data/testcase-tbl.py \
		--mscan 'Linux' data/mscan-linux.out \
		> $@

figures/testcases-sv6.tex: data/testcase-tbl.py \
			data/readmscan.py \
			data/mscan-xv6.out
	$(call quiet,TESTCASE)
	$(Q)python data/testcase-tbl.py \
		--mscan '\sys' data/mscan-xv6.out \
		> $@

# The following rules must be manually invoked because they are
# expensive or require additional environment
code/fmt.tex:
	pygmentize -f latex -S default \
		| grep -vw 'PY@tok@m' \
		| grep -vw 'PY@tok@mi' \
		| grep -vw 'PY@tok@o' \
		| grep -vw 'PY@tok@mo' \
		| grep -vw 'PY@tok@kt' \
		> $@~
	mv $@~ $@

code/%.tex: code/%.py
	pygmentize -f latex $< \
		| sed -e 's,PY{o+ow},PY{k},g' \
		> $@

code/%.tex: code/%.c
	pygmentize -f latex $< \
		| sed -e 's,PY{o+ow},PY{k},g' \
		> $@

figures/tree.tex:
	if ! ./figures/mktree --tex > $@~; then \
		echo "Are Commuter and Z3 in your PYTHONPATH?" >&2; \
		false; \
	fi
	mv $@~ $@
