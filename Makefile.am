@SET_MAKE@

EXTRA_DIST = example.gtkrc-2.0 xfwm4.spec COMPOSITOR

SUBDIRS = po doc icons src mcs-plugin defaults themes

clean-revision:
	rm -r svn-revision.txt

svn-revision.txt:
	svnversion $(top_srcdir) > svn-revision.txt

distclean-local:
	rm -rf *.cache svn-revision.txt

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

snapshot: clean-revision svn-revision.txt dist
	mv $(PACKAGE)-$(VERSION).tar.gz \
	$(PACKAGE)-$(VERSION)-r$(REVISION).tar.gz

snapshot-bz2: clean-revision svn-revision.txt dist-bz2
	mv $(PACKAGE)-$(VERSION).tar.bz2 \
	$(PACKAGE)-$(VERSION)-r$(REVISION).tar.bz2
