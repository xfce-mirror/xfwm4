defaultsdir = $(pkgdatadir)
defaults_DATA = defaults
EXTRA_DIST = defaults.in

distclean-local:
	rm -f defaults
defaults :
	@sed -e "s%_pkgdatadir_%$(pkgdatadir)%g" defaults.in > defaults
