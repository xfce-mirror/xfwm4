@SET_MAKE@

noinst_DATA = example.xfwm4rc

EXTRA_DIST = example.xfwm4rc.in example.gtkrc-2.0 xfwm4.spec

SUBDIRS = po doc src mcs-plugin defaults themes

distclean-local:
	rm -rf example.xfwm4rc xfwm4.spec *.cache

example.xfwm4rc: Makefile example.xfwm4rc.in
	@sed -e "s%_pkgdatadir_%$(pkgdatadir)%g" $(srcdir)/example.xfwm4rc.in > example.xfwm4rc

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

html: Makefile
	make -C doc html

ACLOCAL_AMFLAGS = -I m4
