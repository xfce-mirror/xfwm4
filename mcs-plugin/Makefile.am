xfwm4_plugindir = $(libdir)/xfce-mcs-manager/plugins

xfwm4_plugin_LTLIBRARIES = xfwm4_plugin.la
xfwm4_plugin_la_LDFLAGS =  -module -avoid-version

xfwm4_plugin_la_SOURCES = xfwm4_plugin.c xfwm4-icon.h

xfwm4_plugin_la_CFLAGS = @CFLAGS@ @X_CFLAGS@ @BASE_DEPENDENCIES_CFLAGS@ @MCS_MANAGER_CFLAGS@ @XFCE_MCS_MANAGER_CFLAGS@ -I$(top_builddir)/common -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" -DG_DISABLE_DEPRECATED -DSYSCONFDIR=\"$(sysconfdir)\" -DDATADIR=\"$(pkgdatadir)\" 
xfwm4_plugin_la_LIBADD = @LIBS@ @X_LIBS@ @BASE_DEPENDENCIES_LIBS@ @MCS_MANAGER_LIBS@ @XFCE_MCS_MANAGER_LIBS@

margins_plugindir = $(libdir)/xfce-mcs-manager/plugins

margins_plugin_LTLIBRARIES = margins_plugin.la
margins_plugin_la_LDFLAGS =  -module -avoid-version

margins_plugin_la_SOURCES = margins_plugin.c margins-icon.h

margins_plugin_la_CFLAGS = @CFLAGS@ @X_CFLAGS@ @BASE_DEPENDENCIES_CFLAGS@ @MCS_MANAGER_CFLAGS@ @XFCE_MCS_MANAGER_CFLAGS@ -I$(top_builddir)/common -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" -DG_DISABLE_DEPRECATED -DSYSCONFDIR=\"$(sysconfdir)\" -DDATADIR=\"$(pkgdatadir)\" 
margins_plugin_la_LIBADD = @LIBS@ @X_LIBS@ @BASE_DEPENDENCIES_LIBS@ @MCS_MANAGER_LIBS@ @XFCE_MCS_MANAGER_LIBS@

EXTRA_DIST = xfwm4-icon.h margins-icon.h monitor-icon.h xfwm4-icon.png margins-icon.png monitor-icon.png

noinst_DATA = xfwm4-icon.h margins-icon.h monitor-icon.h xfwm4-icon.png margins-icon.png monitor-icon.png

xfwm4-icon.h: $(IMAGES)
	$(GDK_PIXBUF_CSOURCE) --raw --build-list xfwm4_icon_data $(srcdir)/xfwm4-icon.png > $(srcdir)/xfwm4-icon.h

margins-icon.h: $(IMAGES)
	$(GDK_PIXBUF_CSOURCE) --raw --build-list margins_icon_data $(srcdir)/margins-icon.png > $(srcdir)/margins-icon.h

monitor-icon.h: $(IMAGES)
	$(GDK_PIXBUF_CSOURCE) --raw --build-list monitor_icon_data $(srcdir)/monitor-icon.png > $(srcdir)/monitor-icon.h

