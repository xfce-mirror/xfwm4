bin_PROGRAMS = \
xfwm4-settings  \
xfwm4-tweaks-settings  \
xfwm4-workspace-settings

noinst_DATA = \
	monitor-icon.png \
	monitor-icon.svg \
	monitor-icon.h

xfwm4_workspace_settings_SOURCES = \
	workspace-settings.c \
	xfwm4-workspace-dialog_glade.h \
	monitor-icon.png \
	monitor-icon.svg \
	monitor-icon.h

xfwm4_workspace_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(LIBXFCONF_CFLAGS) \
	$(LIBWNCK_CFLAGS) \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE

xfwm4_workspace_settings_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCEGUI4_LIBS) \
	$(LIBXFCONF_LIBS) \
	$(LIBWNCK_LIBS)

xfwm4_settings_SOURCES = \
	xfwm4-settings.c \
	xfwm4-settings.h \
	xfwm4-dialog_glade.h

xfwm4_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(LIBXFCE4KBD_PRIVATE_CFLAGS) \
	$(LIBXFCONF_CFLAGS) \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\"

xfwm4_settings_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(LIBXFCEGUI4_LIBS) \
	$(LIBXFCE4KBD_PRIVATE_LIBS) \
	$(LIBXFCONF_LIBS)

xfwm4_tweaks_settings_SOURCES = \
	tweaks-settings.c \
	xfwm4-tweaks-dialog_glade.h

xfwm4_tweaks_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(LIBXFCONF_CFLAGS) \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\"

xfwm4_tweaks_settings_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(LIBXFCEGUI4_LIBS) \
	$(LIBXFCONF_LIBS)

INCLUDES = \
	-I${top_srcdir}

if MAINTAINER_MODE

BUILT_SOURCES = \
monitor-icon.h \
xfwm4-dialog_ui.h \
xfwm4-tweaks-dialog_ui.h \
xfwm4-workspace-dialog_ui.h

xfwm4-workspace-dialog_ui.h: xfwm4-workspace-dialog.glade
	exo-csource --static --strip-comments --strip-content --name=workspace_dialog_ui $< >$@

xfwm4-dialog_ui.h: xfwm4-dialog.glade
	exo-csource --static --strip-comments --strip-content --name=xfwm4_dialog_ui $< >$@

xfwm4-tweaks-dialog_ui.h: xfwm4-tweaks-dialog.glade
	exo-csource --static --strip-comments --strip-content --name=tweaks_dialog_ui $< >$@

monitor-icon.h: $(srcdir)/monitor-icon.png
	gdk-pixbuf-csource --raw --build-list \
	monitor_icon_data $(srcdir)/monitor-icon.png > monitor-icon.h

endif

desktopdir = $(datadir)/applications
desktop_in_files = \
	xfce-wm-settings.desktop.in \
	xfce-wmtweaks-settings.desktop.in \
	xfce-workspaces-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files)  \
	xfwm4-workspace-dialog.glade  \
	xfwm4-tweaks-dialog.glade  \
	xfwm4-dialog.glade

DISTCLEANFILES = \
	$(desktop_DATA)
