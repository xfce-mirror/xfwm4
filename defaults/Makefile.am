defaultsdir = $(pkgdatadir)
defaults_DATA = defaults
EXTRA_DIST = defaults.in

distclean-local:
	rm -f defaults
	
defaults : Makefile defaults.in
	@sed -e "s%_pkgdatadir_%$(pkgdatadir)%g" $(srcdir)/defaults.in > defaults
