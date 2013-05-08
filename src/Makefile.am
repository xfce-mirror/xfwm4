bin_PROGRAMS = xfwm4

xfwm4_SOURCES =								\
	client.c							\
	client.h							\
	compositor.c							\
	compositor.h							\
	cycle.c								\
	cycle.h								\
	display.c							\
	display.h							\
	event_filter.c							\
	event_filter.h							\
	events.c							\
	events.h							\
	focus.c								\
	focus.h								\
	frame.c								\
	frame.h								\
	hints.c								\
	hints.h								\
	icons.c								\
	icons.h								\
	inline-default-icon.h						\
	keyboard.c							\
	keyboard.h							\
	main.c								\
	menu.c								\
	menu.h								\
	misc.c								\
	misc.h								\
	moveresize.c							\
	moveresize.h							\
	mypixmap.c							\
	mypixmap.h							\
	mywindow.c							\
	mywindow.h							\
	netwm.c								\
	netwm.h								\
	parserc.c							\
	parserc.h							\
	placement.c							\
	placement.h							\
	poswin.c							\
	poswin.h							\
	screen.c							\
	screen.h							\
	session.c							\
	session.h							\
	settings.c							\
	settings.h							\
	spinning_cursor.c						\
	spinning_cursor.h						\
	stacking.c							\
	stacking.h							\
	startup_notification.c						\
	startup_notification.h						\
	tabwin.c							\
	tabwin.h							\
	terminate.c							\
	terminate.h							\
	transients.c							\
	transients.h							\
	ui_style.c							\
	ui_style.h							\
	wireframe.c							\
	wireframe.h							\
	workspaces.c							\
	workspaces.h							\
	xsync.c								\
	xsync.h								\
	xpm-color-table.h

xfwm4_CFLAGS =								\
	$(GTK_CFLAGS) 							\
	$(GLIB_CFLAGS) 							\
	$(LIBX11_CFLAGS)						\
	$(LIBXFCONF_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4KBD_PRIVATE_CFLAGS)					\
	$(RENDER_CFLAGS)						\
	$(LIBDRM_CFLAGS)						\
	$(LIBSTARTUP_NOTIFICATION_CFLAGS)				\
	$(COMPOSITOR_CFLAGS)						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DDATADIR=\"$(datadir)\"					\
	-DHELPERDIR=\"$(HELPER_PATH_PREFIX)\"				\
	-DPACKAGE_DATADIR=\"$(pkgdatadir)\"				\
	-DG_LOG_DOMAIN=\"xfwm4\"

xfwm4_LDADD =								\
	$(GTK_LIBS) 							\
	$(GLIB_LIBS) 							\
	$(LIBX11_LIBS)							\
	$(LIBX11_LDFLAGS)						\
	$(LIBXFCONF_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCE4UI_LIBS)						\
	$(LIBXFCE4KBD_PRIVATE_LIBS)					\
	$(LIBSTARTUP_NOTIFICATION_LIBS)					\
	$(RENDER_LIBS)							\
	$(COMPOSITOR_LIBS)						\
	$(RANDR_LIBS) 							\
	$(MATH_LIBS)

EXTRA_DIST = 								\
	default_icon.png						\
	default_icon.svg						\
	gen-color-table.pl

if MAINTAINER_MODE

BUILT_SOURCES =								\
	inline-default-icon.h

inline-default-icon.h: $(srcdir)/default_icon.png
	$(AM_V_GEN) gdk-pixbuf-csource --raw --build-list 		\
	default_icon_data $(srcdir)/default_icon.png > inline-default-icon.h

endif
