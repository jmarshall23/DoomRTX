#
# $Id: Makefile.am,v 1.31 2004/03/05 08:01:55 bagder Exp $
#

AUTOMAKE_OPTIONS = foreign no-dependencies

man_MANS = \
	curl.1 \
	curl-config.1

GENHTMLPAGES = \
	curl.html \
	curl-config.html

HTMLPAGES = $(GENHTMLPAGES) index.html

PDFPAGES = \
	curl.pdf \
	curl-config.pdf

SUBDIRS = examples libcurl

CLEANFILES = $(GENHTMLPAGES) $(PDFPAGES)

EXTRA_DIST = MANUAL BUGS CONTRIBUTE FAQ FEATURES INTERNALS SSLCERTS	\
	     README.win32 RESOURCES TODO TheArtOfHttpScripting THANKS	\
	     VERSIONS KNOWN_BUGS BINDINGS $(man_MANS) $(HTMLPAGES) \
	     HISTORY INSTALL libcurl-the-guide $(PDFPAGES)

MAN2HTML= roffit < $< >$@

SUFFIXES = .1 .html .pdf

html: $(HTMLPAGES)
	cd libcurl; make html

pdf: $(PDFPAGES)
	cd libcurl; make pdf

.1.html:
	$(MAN2HTML)

.1.pdf:
	@(foo=`echo $@ | sed -e 's/\.[0-9]$$//g'`; \
	groff -Tps -man $< >$$foo.ps; \
	ps2pdf $$foo.ps $@; \
	rm $$foo.ps; \
	echo "converted $< to $@")

