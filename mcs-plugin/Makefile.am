# mcs-plugin/Makefile.am
#

xfwm4_plugindir = @XFCE_MCS_MANAGER_PLUGINSDIR@

xfwm4_plugin_LTLIBRARIES = xfwm4_plugin.la

xfwm4_plugin_la_LDFLAGS =						\
	-avoid-version							\
	-export-dynamic							\
	-module

xfwm4_plugin_la_SOURCES =						\
	xfwm4_plugin.c							\
	xfwm4-icon.h

xfwm4_plugin_la_CFLAGS =						\
	@LIBX11_CFLAGS@							\
	@LIBXFCE4MCS_MANAGER_CFLAGS@					\
	@LIBXFCEGUI4_CFLAGS@						\
	@XFCE_MCS_MANAGER_CFLAGS@					\
	-I$(top_builddir)/common					\
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\"				\
	-DSYSCONFDIR=\"$(sysconfdir)\"					\
	-DDATADIR=\"$(pkgdatadir)\" 

xfwm4_plugin_la_LIBADD =						\
	@LIBS@

workspaces_plugindir = @XFCE_MCS_MANAGER_PLUGINSDIR@

workspaces_plugin_LTLIBRARIES = workspaces_plugin.la

workspaces_plugin_la_LDFLAGS =						\
	-avoid-version							\
	-export-dynamic							\
	-module

workspaces_plugin_la_SOURCES =						\
	margins.c						\
	margins.h						\
	workspaces.c						\
	workspaces.h						\
	plugin.c						\
	plugin.h						\
	workspaces-icon.h						\
	monitor-icon.h						\
	margins-icon.h

workspaces_plugin_la_CFLAGS =						\
	@LIBX11_CFLAGS@							\
	@LIBXFCE4MCS_MANAGER_CFLAGS@					\
	@LIBXFCEGUI4_CFLAGS@						\
	@XFCE_MCS_MANAGER_CFLAGS@					\
	-I$(top_builddir)/common					\
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\"				\
	-DSYSCONFDIR=\"$(sysconfdir)\"					\
	-DDATADIR=\"$(pkgdatadir)\" 

workspaces_plugin_la_LIBADD =						\
	@LIBS@

noinst_DATA =								\
	xfwm4-icon.h							\
	margins-icon.h							\
	monitor-icon.h							\
	workspaces-icon.h 						\
	xfwm4-icon.png							\
	workspaces.png						\
	margins-icon.png						\
	monitor-icon.png

EXTRA_DIST = $(noinst_DATA)

xfwm4-icon.h: $(srcdir)/xfwm4-icon.png
	gdk-pixbuf-csource --raw --build-list				\
	xfwm4_icon_data $(srcdir)/xfwm4-icon.png > xfwm4-icon.h

margins-icon.h: $(srcdir)/margins-icon.png
	gdk-pixbuf-csource --raw --build-list				\
	margins_icon_data $(srcdir)/margins-icon.png > margins-icon.h

workspaces-icon.h: $(srcdir)/workspaces.png
	gdk-pixbuf-csource --raw --build-list 				\
	workspaces_icon_data $(srcdir)/workspaces.png > workspaces-icon.h

monitor-icon.h: $(srcdir)/monitor-icon.png
	gdk-pixbuf-csource --raw --build-list				\
	monitor_icon_data $(srcdir)/monitor-icon.png > monitor-icon.h

