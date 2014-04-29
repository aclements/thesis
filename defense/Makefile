all: ben-big.jpg

# From http://www.aixtensions.fr/index.php?mo=71&rub=3&so=3#
ben-big.jpg:
	wget 'http://www.aixtensions.fr/pop_image.php?id=443&format=orig&lang=1' -O $@

.PHONY: fetch-fonts
fetch-fonts:
	./fetch-fonts

.PHONY: httpd
httpd:
	python -m SimpleHTTPServer

slides.tar.gz: index.html external.html slides.css favicon.png \
	  slides.svg formalism.png \
	  engine.js jquery-1.10.2.js slides.js \
	  fonts/faces.css $(wildcard fonts/*.woff)
	tar czf $@ --xform s,^,slides/, $^
