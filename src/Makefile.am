bin_PROGRAMS = xfwm4

xfwm4_SOURCES =								\
	client.c							\
	client.h							\
	inline-default-icon.h						\
	events.c							\
	events.h							\
	focus.c								\
	focus.h								\
	frame.c								\
	frame.h								\
	hints.c								\
	hints.h								\
	inline-tabwin-icon.h						\
	icons.c		        					\
	icons.h			        				\
	keyboard.c							\
	keyboard.h							\
	main.c								\
	menu.c								\
	menu.h								\
	misc.c								\
	misc.h								\
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
	transients.c							\
	transients.h							\
	workspaces.c							\
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

EXTRA_DIST = inline-tabwin-icon.h tabwin-icon.png                       \
        inline-default-icon.h default_icon.png

noinst_DATA = inline-tabwin-icon.h tabwin-icon.png

inline-tabwin-icon.h: $(srcdir)/tabwin-icon.png
	gdk-pixbuf-csource --raw --build-list 				\
	tabwin_icon_data $(srcdir)/tabwin-icon.png > inline-tabwin-icon.h

inline-default-icon.h: $(srcdir)/default_icon.png
	gdk-pixbuf-csource --raw --build-list 				\
	default_icon_data $(srcdir)/default_icon.png > inline-default-icon.h

