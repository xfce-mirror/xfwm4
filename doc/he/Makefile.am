SUBDIRS = \
  	 images

noinst_DATA = \
	xfwm4.xml

docdir=$(datadir)/xfce4/doc/he

doc_DATA= \
	xfwm4.html

EXTRA_DIST = $(noinst_DATA) $(doc_DATA)

html: $(srcdir)/xfwm4.xml
	xsltproc $(top_srcdir)/doc/xfce-nochunk.xsl \
		$(srcdir)/xfwm4.xml
