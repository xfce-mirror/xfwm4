# mcs-plugin/Makefile.am
#

xfwm4_plugindir = $(libdir)/xfce4/mcs-plugins

xfwm4_plugin_LTLIBRARIES = xfwm4_plugin.la

xfwm4_plugin_la_LDFLAGS =						\
	-avoid-version							\
	-module

if HAVE_CYGWIN
xfwm4_plugin_la_LDFLAGS +=                                              \
	-no-undefined                                                   \
	-export-symbols $(datadir)/xfce4/devel/mcs-manager.def
endif

xfwm4_plugin_la_SOURCES =						\
	xfwm4_plugin.c							\
	xfwm4_plugin.h							\
	xfwm4_shortcuteditor.c						\
	xfwm4_shortcuteditor.h

xfwm4_plugin_la_CFLAGS =						\
	@LIBX11_CFLAGS@							\
	@LIBXFCE4MCS_MANAGER_CFLAGS@					\
	@LIBXFCEGUI4_CFLAGS@						\
	@XFCE_MCS_MANAGER_CFLAGS@					\
	-I$(top_builddir)/common					\
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\"				\
	-DSYSCONFDIR=\"$(sysconfdir)\"					\
	-DDATADIR=\"$(datadir)\"					\
        -DPACKAGE_DATADIR=\"$(pkgdatadir)\"

xfwm4_plugin_la_LIBADD =						\
	@LIBXFCEGUI4_LIBS@						\
	@LIBXFCE4MCS_MANAGER_LIBS@					\
	@LIBS@

if HAVE_CYGWIN
xfwm4_plugin_la_LIBADD +=						\
	@LIBX11_LIBS@
endif

workspaces_plugindir = $(libdir)/xfce4/mcs-plugins

workspaces_plugin_LTLIBRARIES = workspaces_plugin.la

workspaces_plugin_la_LDFLAGS =						\
	-avoid-version							\
	-module

if HAVE_CYGWIN
workspaces_plugin_la_LDFLAGS +=                                         \
	-no-undefined                                                   \
	-export-symbols $(datadir)/xfce4/devel/mcs-manager.def
endif

workspaces_plugin_la_SOURCES =						\
	margins.c							\
	margins.h							\
	workspaces.c							\
	workspaces.h							\
	plugin.c							\
	plugin.h							\
	monitor-icon.h

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
	@LIBXFCEGUI4_LIBS@						\
	@LIBXFCE4MCS_MANAGER_LIBS@					\
	@LIBS@

if HAVE_CYGWIN
workspaces_plugin_la_LIBADD +=                                          \
	@LIBX11_LIBS@
endif

noinst_DATA =								\
	monitor-icon.h							\
	monitor-icon.png

monitor-icon.h: $(srcdir)/monitor-icon.png
	gdk-pixbuf-csource --raw --build-list				\
	monitor_icon_data $(srcdir)/monitor-icon.png > monitor-icon.h

# .desktop files
desktopdir = $(datadir)/applications
desktop_in_files =							\
	xfce-wm-settings.desktop.in					\
	xfce-workspaces-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	$(desktop_in_files)						\
	$(noinst_DATA)

DISTCLEANFILES =							\
	$(desktop_DATA)
