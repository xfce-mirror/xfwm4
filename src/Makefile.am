bin_PROGRAMS = xfwm4

xfwm4_SOURCES =								\
	client.c							\
	client.h							\
	debug.h 							\
	events.c 							\
	events.h 							\
	frame.c 							\
	frame.h 							\
	hints.c 							\
	hints.h 							\
	keyboard.c 							\
	keyboard.h 							\
	main.c 								\
	main.h 								\
	menu.c 								\
	menu.h 								\
	misc.c 								\
	misc.h 								\
	mywindow.c 							\
	mywindow.h 							\
	parserc.c 							\
	parserc.h 							\
	pixmap.c 							\
	pixmap.h 							\
	session.c 							\
	session.h 							\
	settings.c 							\
	settings.h 							\
	startup_notification.c 						\
	startup_notification.h 						\
	spinning_cursor.c 						\
	spinning_cursor.h 						\
	tabwin.c 							\
	tabwin.h 							\
	workspaces.c 							\
	workspaces.h
	
xfwm4_CFLAGS =								\
	@CFLAGS@							\
	@GLIB_CFLAGS@							\
	@GTK_CFLAGS@							\
	@LIBSTARTUP_NOTIFICATION_CFLAGS@				\
	@LIBXFCE4MCS_CLIENT_CFLAGS@					\
	@LIBXFCEGUI4_CFLAGS@						\
	@LIBXPM_CFLAGS@							\
	-I$(top_builddir)/common					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DG_DISABLE_DEPRECATED						\
	-DGTK_DISABLE_DEPRECATED					\
	-DGDK_DISABLE_DEPRECATED					\
	-DGDK_PIXBUF_DISABLE_DEPRECATED					\
	-DDATADIR=\"$(pkgdatadir)\"

xfwm4_LDADD =								\
	@LIBS@								\
	@GLIB_LIBS@							\
	@GTK_LIBS@							\
	@LIBSTARTUP_NOTIFICATION_LIBS@					\
	@LIBXFCE4MCS_CLIENT_LIBS@					\
	@LIBXFCEGUI4_LIBS@						\
	@LIBXPM_LIBS@

xfwm4_LDFLAGS =								\
	@LIBXPM_LDFLAGS@

EXTRA_DIST = inline-tabwin-icon.h tabwin-icon.png

noinst_DATA = inline-tabwin-icon.h tabwin-icon.png

inline-tabwin-icon.h: $(srcdir)/tabwin-icon.png
	gdk-pixbuf-csource --raw --build-list 				\
	tabwin_icon_data $(srcdir)/tabwin-icon.png > inline-tabwin-icon.h

