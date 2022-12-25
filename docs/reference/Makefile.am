# The name of the module.
DOC_MODULE=xfwm4

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.xml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="XFCONF_DISABLE_DEPRECATED"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--xml-mode --output-format=xml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

# Used for dependencies
HFILE_GLOB =
CFILE_GLOB =

# Header files to ignore when scanning
IGNORE_HFILES=

# Extra files to add when scanning (relative to $srcdir)
EXTRA_HFILES =

# Images to copy into HTML directory
HTML_IMAGES = 

# Extra SGML files that are included by DOC_MAIN_SGML_FILE
content_files = \
    xfconf/general.xml \
    xfconf/policies.xml \
    xfconf/fences.xml

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
GTKDOC_CFLAGS = \
       -I$(top_srcdir) \
       -I$(top_builddir)

include $(top_srcdir)/gtk-doc.make

CLEANFILES += html xml xfwm4-decl-list.txt xfwm4-decl.txt \
              xfwm4-overrides.txt xfwm4-sections.txt
