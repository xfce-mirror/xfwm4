@SET_MAKE@

EXTRA_DIST = example.xfwm4rc example.xfwm4rc.in example.gtkrc-2.0 xfwm4.spec xfwm4.spec.in

SUBDIRS = src defaults themes

distclean-local:
	rm -f example.xfwm4rc xfwm4.spec

example.xfwm4rc :
	@sed -e "s%_pkgdatadir_%$(pkgdatadir)%g" example.xfwm4rc.in > example.xfwm4rc

rpm : dist
	rpm -ta $(PACKAGE)-$(VERSION).tar.gz
