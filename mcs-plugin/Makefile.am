xfwm4_plugindir = $(libdir)/xfce-mcs-manager/plugins

xfwm4_plugin_LTLIBRARIES = xfwm4_plugin.la
xfwm4_plugin_la_LDFLAGS =  -module -avoid-version

xfwm4_plugin_la_SOURCES = xfwm4_plugin.c inline-icon.h

xfwm4_plugin_la_CFLAGS = @CFLAGS@ @X_CFLAGS@ @BASE_DEPENDENCIES_CFLAGS@ @MCS_MANAGER_CFLAGS@ @XFCE_MCS_MANAGER_CFLAGS@ -I$(top_builddir)/common -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" -DG_DISABLE_DEPRECATED -DSYSCONFDIR=\"$(sysconfdir)\" -DDATADIR=\"$(pkgdatadir)\" 
xfwm4_plugin_la_LIBADD = @LIBS@ @X_LIBS@ @BASE_DEPENDENCIES_LIBS@ @MCS_MANAGER_LIBS@ @XFCE_MCS_MANAGER_LIBS@

IMAGES=icon.png
VARIABLES=default_icon_data $(srcdir)/$(IMAGES)

EXTRA_DIST = inline-icon.h icon.png

noinst_DATA = inline-icon.h icon.png

inline-icon.h: $(IMAGES)
	$(GDK_PIXBUF_CSOURCE) --raw --build-list $(VARIABLES) > $(srcdir)/inline-icon.h

