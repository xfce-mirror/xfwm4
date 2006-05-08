bin_PROGRAMS = xfwm4

xfwm4_SOURCES =								\
	client.c							\
	client.h							\
	compositor.c							\
	compositor.h							\
	display.c							\
	display.h							\
	inline-default-icon.h						\
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
	inline-tabwin-icon.h						\
	icons.c								\
	icons.h								\
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
	screen.c							\
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
	ui_style.c							\
	ui_style.h							\
	transients.c							\
	transients.h							\
	workspaces.c							\
	workspaces.h							\
	wireframe.c							\
	wireframe.h

xfwm4_CFLAGS =								\
	@LIBX11_CFLAGS@							\
	@LIBXFCE4MCS_CLIENT_CFLAGS@					\
	@LIBXFCEGUI4_CFLAGS@						\
	@RENDER_CFLAGS@							\
	@LIBSTARTUP_NOTIFICATION_CFLAGS@				\
	@COMPOSITOR_CFLAGS@						\
	-I$(top_builddir)/common					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DDATADIR=\"$(datadir)\"					\
	-DPACKAGE_DATADIR=\"$(pkgdatadir)\"

xfwm4_LDADD =								\
	@LIBX11_LIBS@							\
	@LIBXFCE4MCS_CLIENT_LIBS@					\
	@LIBXFCEGUI4_LIBS@						\
	@LIBSTARTUP_NOTIFICATION_LIBS@					\
	@RENDER_LIBS@							\
	@COMPOSITOR_LIBS@						\
	@RANDR_LIBS@

EXTRA_DIST = inline-tabwin-icon.h tabwin-icon.png                       \
	inline-default-icon.h default_icon.png				\
	gen-color-table.pl

noinst_DATA = inline-tabwin-icon.h tabwin-icon.png

build.h:
	echo "#define BUILD_ID \""`date +%H%M-%y%m%d`"\"" > build.h

inline-tabwin-icon.h: $(srcdir)/tabwin-icon.png
	gdk-pixbuf-csource --raw --build-list 				\
	tabwin_icon_data $(srcdir)/tabwin-icon.png > inline-tabwin-icon.h

inline-default-icon.h: $(srcdir)/default_icon.png
	gdk-pixbuf-csource --raw --build-list 				\
	default_icon_data $(srcdir)/default_icon.png > inline-default-icon.h

