#!/bin/sh

#
# NOTE: Some distribution ship different version of auto tools using
# postfixing (e.g."automake-1.6"). If you want to use those, simply
# set the variable $ACLOCAL, $AUTOHEADER, $AUTOMAKE and $AUTOCONF 
# to the proper command to use.
#
# if unset, default commands will be used.
#

if test "x$LIBTOOLIZE" = "x"; then
	LIBTOOLIZE=libtoolize
fi

if test "x$ACLOCAL" = "x"; then
	ACLOCAL=aclocal
fi

if test "x$AUTOHEADER" = "x"; then
	AUTOHEADER=autoheader
fi

if test "x$AUTOMAKE" = "x"; then
	AUTOMAKE=automake
fi

if test "x$AUTOCONF" = "x"; then
	AUTOCONF=autoconf
fi

$LIBTOOLIZE -c -f \
&& $ACLOCAL --verbose \
&& $AUTOHEADER \
&& $AUTOMAKE --add-missing --copy --include-deps --foreign --gnu --verbose \
&& $AUTOCONF \
&& ./configure $@
