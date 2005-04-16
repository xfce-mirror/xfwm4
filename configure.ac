dnl configure.ac
dnl
dnl xfwm4 - window manager for the XFce4 desktop environment
dnl

AC_INIT([src/main.c])

AC_PREREQ(2.52)

AM_INIT_AUTOMAKE([xfwm4], [4.2.2])

AM_CONFIG_HEADER([config.h])

AM_MAINTAINER_MODE

dnl check for UNIX variants
AC_AIX
AC_ISC_POSIX
AC_MINIX
AM_CONDITIONAL([HAVE_CYGWIN], [test "`uname | grep \"CYGWIN\"`" != ""])

dnl check for basic programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

dnl Check C Compiler Characteristics
AC_C_CONST
AC_C_INLINE

dnl check for standard header files
AC_HEADER_STDC
AC_CHECK_FUNCS([daemon setsid])
AC_CHECK_FUNCS(opendir)

dnl Check for i18n support
BM_I18N([xfwm4], [ar az be bg bn_IN ca cs de el_GR en_GB es es_MX et eu fa fi fr
                  gu he hi hu it ja ko lt ms nb_NO nl pa pl pt_BR pt_PT ro
		  ru sk sv ta tr uk vi zh_CN zh_TW])

dnl Check for X11 window system
BM_LIBX11_REQUIRE
BM_LIBXPM_REQUIRE

dnl Check for Xext library
AC_CHECK_LIB([Xext], [XShapeCombineShape],
  [
    if ! echo $LIBX11_LIBS | grep -q -- '-lXext'; then
      LIBX11_LIBS="$LIBX11_LIBS -lXext"
    fi
  ], [], [$LIBX11_CFLAGS $LIBX11_LDFLAGS $LIBX11_LIBS])

BM_DEPEND([GTK], [gtk+-2.0], [2.2.0])
BM_DEPEND(LIBXFCE4MCS_CLIENT, libxfce4mcs-client-1.0, 4.2.0)
BM_DEPEND(LIBXFCE4MCS_MANAGER, libxfce4mcs-manager-1.0, 4.2.0)
BM_DEPEND([LIBXFCE4UTIL], [libxfce4util-1.0], [4.2.0])
BM_DEPEND(LIBXFCEGUI4, libxfcegui4-1.0, 4.2.0)
XFCE_MCS_PLUGIN([XFCE_MCS_MANAGER], [4.2.0])

BM_DEPEND_CHECK(LIBSTARTUP_NOTIFICATION, libstartup-notification-1.0, 0.5,
  [startup-notification], [startup notification library], [yes])

dnl
dnl RANDR extension
dnl (please note that Xrandr requires Xrender - and no, it's not a typo ;)
dnl
AC_ARG_ENABLE([randr],
AC_HELP_STRING([--enable-randr], [try to use the randr extension])
AC_HELP_STRING([--disable-randr], [don't try to use the randr extension]),
  [], [enable_randr=yes])
RANDR_LIBS=
if test x"$enable_randr" = x"yes"; then
  ac_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $LIBX11_CFLAGS"
  AC_CHECK_LIB(Xrandr, XRRUpdateConfiguration,
               [AC_CHECK_HEADER(X11/extensions/Xrandr.h,
                                RANDR_LIBS="-lXrandr -lXrender",,
                                [#include <X11/Xlib.h>])],, 
                $LIBS $LIBX11_LDFLAGS $LIBX11_LIBS -lXrender -lXext)
  CFLAGS="$ac_CFLAGS"
fi
AC_SUBST([RANDR_LIBS])

dnl
dnl Xcomposite and related extensions
dnl
ENABLE_COMPOSITOR=""
AC_ARG_ENABLE([compositor],
AC_HELP_STRING([--enable-compositor], [enable compositor in xfwm4])
AC_HELP_STRING([--disable-compositor], [disable compositor in xfwm4 (default)]),
  [], [enable_compositor=no])

if test x"$enable_compositor" = x"yes"; then
    if $PKG_CONFIG --exists xcomposite xfixes xdamage xrender 2> /dev/null; then
        PKG_CHECK_MODULES(COMPOSITOR, xcomposite >= 1.0 xfixes xdamage xrender)
        AC_DEFINE([HAVE_COMPOSITOR], [1], [Define to enable compositor])
        ENABLE_COMPOSITOR="--enable-compositor"
    fi
fi
AC_SUBST(ENABLE_COMPOSITOR)

dnl Check for debugging support
BM_DEBUG_SUPPORT

AC_OUTPUT([ 
xfwm4.spec
Makefile
doc/Makefile
doc/C/Makefile
doc/C/images/Makefile
doc/fr/Makefile
doc/fr/images/Makefile
defaults/Makefile
icons/Makefile
icons/48x48/Makefile
icons/scalable/Makefile
mcs-plugin/Makefile
po/Makefile.in 
src/Makefile
themes/Makefile
themes/default/Makefile
themes/default.keys/Makefile
themes/default-4.0/Makefile
themes/opta/Makefile
themes/symphony/Makefile
themes/tyrex/Makefile
themes/therapy/Makefile
themes/waza/Makefile
themes/wildbush/Makefile
])
