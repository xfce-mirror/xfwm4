bin_PROGRAMS = xfwm4
xfwm4_SOURCES = \
	client.c \
	client.h \
	debug.h \
	events.c \
	events.h \
	frame.c \
	frame.h \
	hints.c \
	hints.h \
	keyboard.c \
	keyboard.h \
	main.c \
	main.h \
	menu.c \
	menu.h \
	misc.c \
	misc.h \
	parserc.c \
	parserc.h \
	pixmap.c \
	pixmap.h \
	session.c \
	session.h \
	settings.c \
	settings.h \
	tabwin.c \
	tabwin.h \
	workspaces.c \
	workspaces.h
	
xfwm4_CFLAGS = @CFLAGS@ @X_CFLAGS@ @BASE_DEPENDENCIES_CFLAGS@ @MCS_CLIENT_CFLAGS@ -I$(top_builddir)/common -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DDATADIR=\"$(pkgdatadir)\"
xfwm4_LDADD = @LIBS@ @X_LIBS@ @BASE_DEPENDENCIES_LIBS@ @MCS_CLIENT_LIBS@

IMAGES=tabwin-icon.png
VARIABLES=tabwin_icon_data $(srcdir)/$(IMAGES)

EXTRA_DIST = inline-tabwin-icon.h tabwin-icon.png

noinst_DATA = inline-tabwin-icon.h tabwin-icon.png

inline-tabwin-icon.h: $(IMAGES)
	$(GDK_PIXBUF_CSOURCE) --raw --build-list $(VARIABLES) > $(srcdir)/inline-tabwin-icon.h

