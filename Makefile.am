@SET_MAKE@

noinst_DATA = example.xfwm4rc

EXTRA_DIST = example.xfwm4rc.in example.gtkrc-2.0 xfwm4.spec.in

SUBDIRS = po common src mcs-plugin defaults themes

distclean-local:
	rm -rf example.xfwm4rc xfwm4.spec *.cache

example.xfwm4rc:
	@sed -e "s%_pkgdatadir_%$(pkgdatadir)%g" example.xfwm4rc.in > example.xfwm4rc

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

ACLOCAL_AMFLAGS = -I m4
