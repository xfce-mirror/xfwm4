@SET_MAKE@

EXTRA_DIST =								\
	example.gtkrc-2.0						\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	COMPOSITOR

DISTCLEANFILES =							\
	intltool-extract						\
	intltool-merge							\
	intltool-update							\
	xfwm4.spec

SUBDIRS = po doc icons src mcs-plugin defaults themes

distclean-local:
	rm -rf *.cache

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

html: Makefile
	make -C doc html

dist-bz2: dist
	zcat $(PACKAGE)-$(VERSION).tar.gz | \
	 bzip2 --best -c > $(PACKAGE)-$(VERSION).tar.bz2

distcheck-bz2: distcheck
	zcat $(PACKAGE)-$(VERSION).tar.gz | \
	 bzip2 --best -c > $(PACKAGE)-$(VERSION).tar.bz2
