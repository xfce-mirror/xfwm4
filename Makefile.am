@SET_MAKE@

EXTRA_DIST = example.gtkrc-2.0 xfwm4.spec

SUBDIRS = po doc icons src mcs-plugin defaults themes

distclean-local:
	rm -rf xfwm4.spec *.cache

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

html: Makefile
	make -C doc html

ACLOCAL_AMFLAGS = -I m4
