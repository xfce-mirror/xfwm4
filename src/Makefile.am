bin_PROGRAMS = xfwm4

xfwm4_SOURCES =								\
	client.c							\
	client.h							\
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
	mypixmap.c 							\
	mypixmap.h 							\
	mywindow.c 							\
	mywindow.h 							\
	parserc.c 							\
	parserc.h 							\
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
	@LIBSTARTUP_NOTIFICATION_CFLAGS@				\
	@LIBX11_CFLAGS@							\
	@LIBXFCE4MCS_CLIENT_CFLAGS@					\
	@LIBXFCEGUI4_CFLAGS@						\
	@LIBXPM_CFLAGS@							\
	-I$(top_builddir)/common					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DDATADIR=\"$(datadir)\"					\
        -DPACKAGE_DATADIR=\"$(pkgdatadir)\"

xfwm4_LDADD =								\
	@LIBSTARTUP_NOTIFICATION_LIBS@					\
	@LIBX11_LIBS@							\
	@LIBXFCE4MCS_CLIENT_LIBS@					\
	@LIBXFCEGUI4_LIBS@						\
	@LIBXPM_LIBS@							\
	@RANDR_LIBS@

xfwm4_LDFLAGS =								\
	@LIBXPM_LDFLAGS@

EXTRA_DIST = inline-tabwin-icon.h tabwin-icon.png

noinst_DATA = inline-tabwin-icon.h tabwin-icon.png

inline-tabwin-icon.h: $(srcdir)/tabwin-icon.png
	gdk-pixbuf-csource --raw --build-list 				\
	tabwin_icon_data $(srcdir)/tabwin-icon.png > inline-tabwin-icon.h

