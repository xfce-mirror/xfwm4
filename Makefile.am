@SET_MAKE@

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST =								\
	example.gtkrc-2.0						\
	COMPOSITOR							\
	README.md

SUBDIRS = 								\
	defaults 							\
	helper-dialog 							\
	icons 								\
	common 								\
	settings-dialogs						\
	src 								\
	themes								\
	po

distclean-local:
	rm -rf *.cache po/.intltool-merge-cache.lock

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

if MAINTAINER_MODE

all-local: compile_flags.txt

compile_flags.txt: Makefile
	rm -f $@
	for i in \
	$(PLATFORM_CPPFLAGS) \
	$(GLIB_CFLAGS)  \
	$(GTK_CFLAGS)  \
	$(COMPOSITOR_CFLAGS) \
	$(EPOXY_CFLAGS) \
	$(LIBSTARTUP_NOTIFICATION_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(LIBXFCE4KBD_PRIVATE_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCONF_CFLAGS) \
	$(PRESENT_EXTENSION_CFLAGS) \
	$(RANDR_CFLAGS) \
	$(RENDER_CFLAGS) \
	$(XINERAMA_CFLAGS) \
	-I${top_srcdir} \
	-DHAVE_CONFIG_H=1 \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DHELPERDIR=\"$(HELPER_PATH_PREFIX)\" \
	-DPACKAGE_DATADIR=\"$(pkgdatadir)\" \
	-DG_LOG_DOMAIN=\"xfwm4\"; \
	do \
		echo "$$i" >> $@ || exit 1; \
	done

endif
