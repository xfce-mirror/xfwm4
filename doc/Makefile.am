SUBDIRS = \
	C \
	fr

noinst_DATA = \
	xfce-nochunk.xsl

EXTRA_DIST = $(noinst_DATA)

html: Makefile
	make -C C html
	make -C fr html
