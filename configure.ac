dnl configure.ac
dnl
dnl xfwm4 - window manager for the Xfce4 desktop environment
dnl

m4_define([glib_minimum_version], [2.72.0])
m4_define([gtk_minimum_version], [3.24.0])
m4_define([xfce_minimum_version], [4.8.0])
m4_define([libxfce4ui_minimum_version], [4.12.0])
m4_define([libxfce4kbd_private_minimum_version], [4.12.0])
m4_define([xfconf_minimum_version], [4.13.0])
m4_define([xfconf_legacy_version], [4.12.0])
m4_define([xcomposite_minimum_version], [0.2])
m4_define([wnck_minimum_version], [3.14])
m4_define([startup_notification_minimum_version], [0.5])
m4_define([libepoxy_minimum_version], [1.0])
m4_define([xpresent_minimum_version], [1.0])
m4_define([presentproto_minimum_version], [1.1])

dnl version info
XDT_VERSION_INIT([4.20.0], [git])

dnl init autoconf
m4_define([copyright_year], [2025])
AC_COPYRIGHT([Copyright (c) 2002-copyright_year()
        The Xfce development team. All rights reserved.

Written for Xfce by Olivier Fourdan <fourdan@xfce.org>.])
AC_INIT([xfwm4], [xdt_version], [https://gitlab.xfce.org/xfce/xfwm4])
AM_INIT_AUTOMAKE([1.8 dist-bzip2 tar-ustar no-dist-gzip foreign])

AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_HEADERS([config.h])

AM_MAINTAINER_MODE()
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_DEFINE([VERSION_FULL], [PACKAGE_VERSION], [Alias for VERSION and PACKAGE_VERSION for meson compatibility])
COPYRIGHT_YEAR=copyright_year()
AC_DEFINE_UNQUOTED([COPYRIGHT_YEAR], ["$COPYRIGHT_YEAR"], [Copyright year])
AC_SUBST([COPYRIGHT_YEAR])

dnl set helper path prefix
AC_ARG_WITH([helper-path-prefix],
            [AS_HELP_STRING([--with-helper-path-prefix=PATH],
                            [Path prefix under which helper executables will be installed (default: $libdir)])],
            [HELPER_PATH_PREFIX="$withval"],
            [HELPER_PATH_PREFIX="$libdir"])
AC_SUBST([HELPER_PATH_PREFIX])

dnl check for UNIX variants
AC_USE_SYSTEM_EXTENSIONS

dnl check for basic programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

dnl check for libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

dnl Check C Compiler Characteristics
AC_C_INLINE

dnl check for required headers
AC_CHECK_HEADERS([errno.h fcntl.h math.h memory.h signal.h stdarg.h \
                  stdio.h stdlib.h string.h sys/stat.h sys/time.h sys/types.h \
                  sys/wait.h time.h unistd.h])

dnl Math lib is required (if available)
MATH_LIBS=""
AC_CHECK_LIB(m, ceil, MATH_LIBS="-lm")
AC_SUBST([MATH_LIBS])

dnl Check for i18n support
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])

AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

dnl Check for X11 window system
XDT_CHECK_LIBX11_REQUIRE

dnl Check for Xext library
AC_CHECK_LIB([Xext], [XShapeCombineShape],
  [
    if ! echo $LIBX11_LIBS | grep -q -- '-lXext'; then
      LIBX11_LIBS="$LIBX11_LIBS -lXext"
    fi
  ], [], [$LIBX11_CFLAGS $LIBX11_LDFLAGS $LIBX11_LIBS])


AC_ARG_ENABLE([xi2],
  AS_HELP_STRING([--enable-xi2], [enable support for the X Input 2 extension])
  AS_HELP_STRING([--disable-xi2], [don't enable the X Input 2 extension (default)]),
  [enable_xi2=yes], [])
have_xi2="no"
if test x"$enable_xi2" = x"yes"; then
dnl Check for Xi2 library
  AC_CHECK_LIB([Xi], [XISelectEvents],
  [
    if ! echo $LIBX11_LIBS | grep -q -- '-lXi'; then
      have_xi2="yes"
      LIBX11_LIBS="$LIBX11_LIBS -lXi"
      AC_DEFINE([HAVE_XI2], [1], [Define to enable XI2])
    fi
  ],
  [
    AC_MSG_ERROR([X Input 2 extension support requested but not available])
  ],
  [$LIBX11_CFLAGS $LIBX11_LDFLAGS $LIBX11_LIBS])
fi

dnl Check for XRes library
AC_CHECK_LIB([XRes], [XResQueryClientIds],
  [
    if ! echo $LIBX11_LIBS | grep -q -- '-lXRes'; then
      LIBX11_LIBS="$LIBX11_LIBS -lXRes"
      AC_DEFINE([HAVE_XRES], [1], [Define to enable XRes])
    fi
  ], [], [$LIBX11_CFLAGS $LIBX11_LDFLAGS $LIBX11_LIBS])

XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [gtk_minimum_version])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [xfce_minimum_version])
XDT_CHECK_PACKAGE([LIBXFCE4UI], libxfce4ui-2, [libxfce4ui_minimum_version])
XDT_CHECK_PACKAGE([LIBXFCE4KBD_PRIVATE], libxfce4kbd-private-3, [libxfce4kbd_private_minimum_version])
XDT_CHECK_PACKAGE([LIBXFCONF], libxfconf-0, [xfconf_minimum_version],,
[
  AC_DEFINE([XFCONF_LEGACY], [], [Use dbus-glib provided by xfconf 4.12 to obtain array type])
  XDT_CHECK_PACKAGE([LIBXFCONF], [libxfconf-0], [xfconf_legacy_version])
])
XDT_CHECK_PACKAGE([LIBWNCK], [libwnck-3.0], [wnck_minimum_version])
XDT_CHECK_PACKAGE([XINERAMA], [xinerama], [0])

AS_IF([test "x$USE_MAINTAINER_MODE" = "xyes"],
[
  AC_PATH_PROG([GLIB_COMPILE_RESOURCES], [glib-compile-resources])
  AS_IF([test -z "$GLIB_COMPILE_RESOURCES"],
  [
    echo '*** The program "glib-compile-resources" is required to build when --enable-maintainer-mode'
    echo '*** is specified.'
    exit 1
  ])
])

dnl
dnl Sync to vblank support
dnl
EPOXY_FOUND="no"
XDT_CHECK_OPTIONAL_PACKAGE([EPOXY],
                       [epoxy], [libepoxy_minimum_version],
                       [epoxy],
                       [library for handling OpenGL function pointer management], [yes])

dnl
dnl Startup notification support
dnl
LIBSTARTUP_NOTIFICATION_FOUND="no"
XDT_CHECK_OPTIONAL_PACKAGE([LIBSTARTUP_NOTIFICATION],
                       [libstartup-notification-1.0], [startup_notification_minimum_version],
                       [startup-notification],
                       [startup notification library], [yes])


dnl
dnl XSync support
dnl
XSYNC_LIBS=
AC_ARG_ENABLE([xsync],
AS_HELP_STRING([--enable-xsync], [try to use the xsync extension])
AS_HELP_STRING([--disable-xsync], [don't try to use the xsync extension]),
  [], [enable_xsync=yes])
have_xsync="no"
if test x"$enable_xsync" = x"yes"; then
  AC_CHECK_LIB([Xext], [XSyncCreateFence],
      [ have_xsync="yes"
        XSYNC_LIBS=" -lXext"
        AC_DEFINE([HAVE_XSYNC], [1], [Define to enable xsync])
      ],[])
fi
AC_SUBST([XSYNC_LIBS])

dnl
dnl Render support
dnl
AC_ARG_ENABLE([render],
AS_HELP_STRING([--enable-render], [try to use the render extension])
AS_HELP_STRING([--disable-render], [don't try to use the render extension]),
  [], [enable_render=yes])
have_render="no"
RENDER_LIBS=
if test x"$enable_render" = x"yes"; then
  if $PKG_CONFIG --print-errors --exists xrender 2>&1; then
    PKG_CHECK_MODULES(RENDER, xrender)
    have_render="yes"
    AC_DEFINE([HAVE_RENDER], [1], [Define to enable render])
  else
    dnl fallback to check for libXrender
    AC_CHECK_LIB([Xrender], [XRenderCreatePicture],
        [ have_render="yes"
          RENDER_LIBS=" -lXrender"
          AC_DEFINE([HAVE_RENDER], [1], [Define to enable render])
        ],[])
  fi
fi
AC_SUBST([RENDER_LIBS])

dnl
dnl RANDR extension
dnl (please note that Xrandr requires Xrender - and no, it's not a typo ;)
dnl
AC_ARG_ENABLE([randr],
AS_HELP_STRING([--enable-randr], [try to use the randr extension (requires render)])
AS_HELP_STRING([--disable-randr], [don't try to use the randr extension]),
  [], [enable_randr=yes])
RANDR_LIBS=
have_xrandr="no"
if test x"$enable_randr" = x"yes"; then
  if test x"$have_render" = x"yes"; then
    have_xrandr="no"
    ac_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $LIBX11_CFLAGS"
    AC_CHECK_LIB(Xrandr, XRRUpdateConfiguration,
                 [AC_CHECK_HEADER(X11/extensions/Xrandr.h,
                                  RANDR_LIBS="-lXrandr -lXrender"

                                  AC_DEFINE([HAVE_RANDR], [1], [Define to enable xrandr])
                                  have_xrandr="yes",,
                                  [#include <X11/Xlib.h>])],,
                  $LIBS $LIBX11_LDFLAGS $LIBX11_LIBS -lXrender -lXext)
    CFLAGS="$ac_CFLAGS"
  fi
fi
AC_SUBST([RANDR_LIBS])

dnl
dnl XPresent support
dnl
AC_ARG_ENABLE([xpresent],
AS_HELP_STRING([--enable-xpresent], [try to use the xpresent extension])
AS_HELP_STRING([--disable-xpresent], [don't try to use the xpresent extension]),
  [], [enable_xpresent=yes])
have_xpresent="no"
XPRESENT_LIBS=
if test x"$enable_xpresent" = x"yes"; then
  if $PKG_CONFIG --print-errors --exists xpresent 2>&1; then
    PKG_CHECK_MODULES(PRESENT_EXTENSION, presentproto >= [presentproto_minimum_version] xpresent)
    have_xpresent="yes"
    AC_DEFINE([HAVE_PRESENT_EXTENSION], [1], [Define to enable xpresent])
  fi
fi
AC_SUBST([PRESENT_EXTENSION_LIBS])

dnl
dnl Xcomposite and related extensions
dnl
compositor="no"
ENABLE_COMPOSITOR=""

AC_ARG_ENABLE([compositor],
AS_HELP_STRING([--enable-compositor], [enable compositor in xfwm4 (default)])
AS_HELP_STRING([--disable-compositor], [disable compositor in xfwm4]),
  [], [enable_compositor=yes])

if test x"$enable_compositor" = x"yes"; then
  if test x"$have_render" = x"yes"; then
    if $PKG_CONFIG --print-errors --exists xcomposite xfixes xdamage xrender 2>&1; then
      PKG_CHECK_MODULES(COMPOSITOR, xcomposite >= [xcomposite_minimum_version] xfixes xdamage)
      AC_DEFINE([HAVE_COMPOSITOR], [1], [Define to enable compositor])
      ENABLE_COMPOSITOR="--enable-compositor"
      AC_DEFINE([HAVE_COMPOSITOR], [1], [Define to enable compositor])
      compositor="yes"
    fi
  fi
fi
AC_SUBST(ENABLE_COMPOSITOR)

dnl
dnl position window
dnl
AC_ARG_ENABLE([poswin],
AS_HELP_STRING([--enable-poswin], [enable position window on move/resize]),
  [], [enable_poswin=no])

if test x"$enable_poswin" = x"yes"; then
  AC_DEFINE([SHOW_POSITION], [1], [Define to enable position window on move/resize])
fi

dnl Check for debugging support
XDT_FEATURE_DEBUG([xdt_debug_default])


m4_define([xfwm4_revision], [ifelse(xdt_version_build, [], [unknown], [xdt_version_build])])
REVISION=xfwm4_revision
AC_DEFINE_UNQUOTED([REVISION], "$REVISION", [git id])
AC_SUBST([REVISION])

AC_CONFIG_FILES([
Makefile
defaults/Makefile
helper-dialog/Makefile
icons/Makefile
icons/16x16/Makefile
icons/22x22/Makefile
icons/48x48/Makefile
icons/128x128/Makefile
icons/scalable/Makefile
po/Makefile.in
common/Makefile
settings-dialogs/Makefile
src/Makefile
themes/Makefile
themes/daloa/Makefile
themes/default/Makefile
themes/default-hdpi/Makefile
themes/default-xhdpi/Makefile
themes/kokodi/Makefile
themes/moheli/Makefile
])
AC_OUTPUT

echo
echo "Build Configuration for $PACKAGE version $VERSION revision $REVISION:"
echo "  Startup notification support: $LIBSTARTUP_NOTIFICATION_FOUND"
echo "  XSync support:                $have_xsync"
echo "  Render support:               $have_render"
echo "  Xrandr support:               $have_xrandr"
echo "  Xpresent support:             $have_xpresent"
echo "  Xi2 support:                  $have_xi2"
echo "  Embedded compositor:          $compositor"
echo "  Epoxy support:                $EPOXY_FOUND"
echo "  Position window:              $enable_poswin"
echo
