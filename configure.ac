dnl configure.ac
dnl
dnl xfwm4 - window manager for the XFce4 desktop environment
dnl

AC_INIT([src/main.c])

AC_PREREQ(2.52)

AM_INIT_AUTOMAKE([xfwm4], [3.99.4])

AM_CONFIG_HEADER([config.h])

AM_MAINTAINER_MODE

dnl check for UNIX variants
AC_AIX
AC_ISC_POSIX
AC_MINIX

dnl check for basic programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

dnl Check C Compiler Characteristics
AC_C_CONST
AC_C_INLINE

dnl check for standard header files
AC_HEADER_STDC

dnl Check for i18n support
BM_I18N([xfwm4], [ar ca de es es_MX fi fr hu ja ko lt ms nl pl ru ta tr zh_CN zh_TW])

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

BM_DEPEND(LIBXFCE4MCS_CLIENT, libxfce4mcs-client-1.0, 3.99.4)
BM_DEPEND(LIBXFCE4MCS_MANAGER, libxfce4mcs-manager-1.0, 3.99.4)
BM_DEPEND([LIBXFCE4UTIL], [libxfce4util-1.0], [3.99.4])
BM_DEPEND(LIBXFCEGUI4, libxfcegui4-1.0, 3.99.4)
XFCE_MCS_PLUGIN([XFCE_MCS_MANAGER], [3.99.4])

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
  AC_CHECK_LIB(Xrandr, XRRUpdateConfiguration,
               [AC_CHECK_HEADER(X11/extensions/Xrandr.h,
                                RANDR_LIBS="-lXrandr -lXrender",,
				[#include <X11/Xlib.h>])],, 
                $LIBS $LIBX11_LDFLAGS $LIBX11_LIBS -lXrender -lXext)
fi
AC_SUBST([RANDR_LIBS])

dnl Check for debugging support
BM_DEBUG_SUPPORT

AC_OUTPUT([ 
xfwm4.spec
Makefile
doc/Makefile
doc/C/Makefile
doc/C/images/Makefile
defaults/Makefile
mcs-plugin/Makefile
po/Makefile.in 
src/Makefile
themes/Makefile
themes/bbs/Makefile
themes/curve/Makefile
themes/default/Makefile
themes/default.keys/Makefile
themes/exocet/Makefile
themes/meenee/Makefile
themes/microcurve/Makefile
themes/microdeck/Makefile
themes/microdeck2/Makefile
themes/microdeck3/Makefile
themes/tgc/Makefile
])
