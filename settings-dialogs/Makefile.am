bin_PROGRAMS = \
xfwm4-settings  \
xfwm4-tweaks-settings  \
xfwm4-workspace-settings 

xfwm4_workspace_settings_SOURCES = \
	workspace-settings.c \
	xfwm4-workspace-dialog_glade.h

xfwm4_workspace_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GLADE_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(LIBXFCONF_CFLAGS) \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\"

xfwm4_workspace_settings_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(GLADE_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(LIBXFCEGUI4_LIBS) \
	$(LIBXFCONF_LIBS)

xfwm4_settings_SOURCES = \
	xfwm4-settings.c \
	xfwm4-dialog_glade.h

xfwm4_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GLADE_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(LIBXFCONF_CFLAGS) \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\"

xfwm4_settings_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(GLADE_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(LIBXFCEGUI4_LIBS) \
	$(LIBXFCONF_LIBS)

xfwm4_tweaks_settings_SOURCES = \
	tweaks-settings.c \
	xfwm4-tweaks-dialog_glade.h

xfwm4_tweaks_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GLADE_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(LIBXFCONF_CFLAGS) \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\"

xfwm4_tweaks_settings_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(GLADE_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(LIBXFCEGUI4_LIBS) \
	$(LIBXFCONF_LIBS)

INCLUDES = \
	-I${top_srcdir}

if MAINTAINER_MODE

BUILT_SOURCES = \
xfwm4-dialog_glade.h \
xfwm4-tweaks-dialog_glade.h \
xfwm4-workspace-dialog_glade.h

xfwm4-workspace-dialog_glade.h: xfwm4-workspace-dialog.glade
	exo-csource --static --name=workspace_dialog_glade $< >$@

xfwm4-dialog_glade.h: xfwm4-dialog.glade
	exo-csource --static --name=xfwm4_dialog_glade $< >$@

xfwm4-tweaks-dialog_glade.h: xfwm4-tweaks-dialog.glade
	exo-csource --static --name=tweaks_dialog_glade $< >$@

endif

desktopdir = $(datadir)/applications
desktop_in_files = \
	xfwm4-settings.desktop.in \
	xfwm4-tweaks-settings.desktop.in \
	xfwm4-workspace-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

settingsdir = $(datadir)/xfce4/settings-dialogs
settings_DATA = $(desktop_DATA)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files)							\
	xfwm4-workspace-dialog.glade				\
	xfwm4-tweaks-dialog.glade					\
	xfwm4-dialog.glade

DISTCLEANFILES = \
	xfwm4-settings.desktop \
	xfwm4-workspace-settings.desktop \
	xfwm4-tweaks-settings.desktop
