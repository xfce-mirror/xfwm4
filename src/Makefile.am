bin_PROGRAMS = xfwm4
xfwm4_SOURCES = \
	client.c \
	events.c \
	frame.c \
	gtk_style.c \
	gtktoxevent.c \
	hints.c \
	keyboard.c \
	main.c \
	menu.c \
	misc.c \
	parserc.c \
	pixmap.c \
	settings.c \
	workspaces.c \
	client.h \
	debug.h \
	events.h \
	frame.h \
	gtk_style.h \
	gtktoxevent.h \
	hints.h \
	keyboard.h \
	main.h \
	menu.h \
	misc.h \
	parserc.h \
	pixmap.h \
	settings.h \
	workspaces.h \
	xsettings-common.c \
	xsettings-common.h \
	xsettings-client.c \
	xsettings-client.h

xfwm4_CFLAGS = @CFLAGS@ @X_CFLAGS@ @BASE_DEPENDENCIES_CFLAGS@ \
-DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED \
-DGDK_PIXBUF_DISABLE_DEPRECATED -DDATADIR=\"$(pkgdatadir)\"
xfwm4_LDADD = @LIBS@ @X_LIBS@  @BASE_DEPENDENCIES_LIBS@
