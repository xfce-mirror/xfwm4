#!/bin/sh
#
# $Id$
#
# Copyright (c) 2002-2005
#         The Xfce development team. All rights reserved.
#
# Written for Xfce by Benedikt Meurer <benny@xfce.org>.
#

# Set locale to C to avoid troubles with "svn info" output parsing
LC_ALL=C
LANG=C
export LC_ALL LANG

(type xdt-autogen) >/dev/null 2>&1 || {
  cat >&2 <<EOF
autogen.sh: You don't seem to have the Xfce development tools installed on
            your system, which are required to build this software.
            Please install the xfce4-dev-tools package first, it is available
            from http://www.xfce.org/.
EOF
  exit 1
}

# verify that po/LINGUAS is present
(test -f po/LINGUAS) >/dev/null 2>&1 || {
  cat >&2 <<EOF
autogen.sh: The file po/LINGUAS could not be found. Please check your snapshot
            or try to checkout again.
EOF
  exit 1
}

echo "Creating configure.ac"

# substitute revision and linguas
linguas=`sed -e '/^#/d' po/LINGUAS`
revision=`git rev-parse --short HEAD`
sed -e "s/@LINGUAS@/${linguas}/g" \
    -e "s/@REVISION@/${revision}/g" \
    < "configure.ac.in" > "configure.ac"

exec xdt-autogen $@

# vi:set ts=2 sw=2 et ai:
