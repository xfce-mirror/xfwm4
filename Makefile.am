@SET_MAKE@

EXTRA_DIST =								\
	example.gtkrc-2.0						\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	xfwm4.spec							\
	COMPOSITOR

DISTCLEANFILES =							\
	intltool-extract						\
	intltool-merge							\
	intltool-update							\
	xfwm4.spec

SUBDIRS = 								\
	defaults 							\
	doc 								\
	helper-dialog 							\
	icons 								\
	po 								\
	settings-dialogs						\
	src 								\
	themes

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

snapshot: dist
	mv $(PACKAGE)-$(VERSION).tar.gz \
	$(PACKAGE)-$(VERSION)-r@REVISION@.tar.gz

snapshot-bz2: dist-bz2
	mv $(PACKAGE)-$(VERSION).tar.bz2 \
	$(PACKAGE)-$(VERSION)-r@REVISION@.tar.bz2
