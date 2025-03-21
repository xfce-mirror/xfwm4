bin_PROGRAMS = \
	xfwm4-settings  \
	xfwm4-tweaks-settings  \
	xfwm4-workspace-settings

xfwm4_workspace_settings_SOURCES = \
	workspace-settings.c \
	xfwm4-resources.c

xfwm4_workspace_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCONF_CFLAGS) \
	$(LIBWNCK_CFLAGS) \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE

xfwm4_workspace_settings_LDADD = \
	$(top_builddir)/common/libxfwm-common.la \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCONF_LIBS) \
	$(LIBWNCK_LIBS)

xfwm4_settings_SOURCES = \
	xfwm4-settings.c \
	xfwm4-settings.h \
	xfwm4-resources.c \
	range-debouncer.c \
	range-debouncer.h

xfwm4_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4KBD_PRIVATE_CFLAGS) \
	$(LIBXFCONF_CFLAGS) \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\"

xfwm4_settings_LDADD = \
	$(top_builddir)/common/libxfwm-common.la \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4KBD_PRIVATE_LIBS) \
	$(LIBXFCONF_LIBS)

xfwm4_tweaks_settings_SOURCES = \
	tweaks-settings.c \
	xfwm4-resources.c \
	range-debouncer.c \
	range-debouncer.h

xfwm4_tweaks_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCONF_CFLAGS) \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\"

xfwm4_tweaks_settings_LDADD = \
	$(top_builddir)/common/libxfwm-common.la \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCONF_LIBS)

AM_CPPFLAGS = \
	-I${top_srcdir} \
	$(PLATFORM_CPPFLAGS)

if MAINTAINER_MODE

BUILT_SOURCES = \
	xfwm4-resources.c

xfwm4-resources.c:
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) $(srcdir)/xfwm4.gresource.xml --target=$@ --generate-source

endif

desktopdir = $(datadir)/applications
desktop_in_files = \
	xfce-wm-settings.desktop.in \
	xfce-wmtweaks-settings.desktop.in \
	xfce-workspaces-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST = \
	$(desktop_in_files)  \
	xfwm4-workspace-dialog.glade  \
	xfwm4-tweaks-dialog.glade  \
	xfwm4-dialog.glade \
	xfwm4.gresource.xml \
	monitor-icon.svg \
	meson.build \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_DATA)

%.desktop: %.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@
