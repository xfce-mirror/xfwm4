xfwm4_plugindir = $(libdir)/libxfce4mcs/settings/

xfwm4_plugin_LTLIBRARIES = xfwm4_plugin.la
xfwm4_plugin_la_LDFLAGS =  -module -avoid-version

xfwm4_plugin_la_SOURCES = xfwm4_plugin.c 

xfwm4_plugin_la_CFLAGS = @CFLAGS@ @X_CFLAGS@ @BASE_DEPENDENCIES_CFLAGS@ @MCS_MANAGER_CFLAGS@ -DG_DISABLE_DEPRECATED -DSYSCONFDIR=\"$(sysconfdir)\" -DDATADIR=\"$(pkgdatadir)\" 
xfwm4_plugin_la_LIBADD = @LIBS@ @X_LIBS@ @BASE_DEPENDENCIES_LIBS@ @MCS_MANAGER_LIBS@

IMAGES=icon.png
VARIABLES=default_icon_data $(srcdir)/$(IMAGES)

noinst_DATA = inline-icon.h

inline-icon.h: $(IMAGES)
	$(GDK_PIXBUF_CSOURCE) --raw --build-list $(VARIABLES) > $(srcdir)/inline-icon.h


