dnl configure.ac
dnl
dnl xfwm4 - window manager for the XFce4 desktop environment
dnl

AC_INIT(src/main.c)

AC_PREREQ(2.52)

AM_INIT_AUTOMAKE(xfwm4, 0.1)

AM_CONFIG_HEADER(config.h)

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
BM_I18N([xfwm4], [ar de es es_MX fi fr nl pl zh_CN])

dnl Check for X11 window system
BM_LIBX11_REQUIRE
BM_LIBXPM_REQUIRE

BM_DEPEND(LIBXFCE4MCS_CLIENT, libxfce4mcs-client-1.0, 0.0.3)
BM_DEPEND(LIBXFCE4MCS_MANAGER, libxfce4mcs-manager-1.0, 0.0.3)
BM_DEPEND(LIBXFCEGUI4, libxfcegui4-1.0, 0.0.16)
XFCE_MCS_PLUGIN([XFCE_MCS_MANAGER], [0.2.0])

BM_DEPEND_CHECK(LIBSTARTUP_NOTIFICATION, libstartup-notification-1.0, 0.4,
  [startup-notification], [startup notification library], [yes])

dnl Check for debugging support
BM_DEBUG_SUPPORT

AC_OUTPUT([ 
xfwm4.spec
Makefile
defaults/Makefile
mcs-plugin/Makefile
po/Makefile.in 
src/Makefile
themes/Makefile
themes/adept/Makefile
themes/agua/Makefile
themes/atlanta/Makefile
themes/atlanta2/Makefile
themes/b5/Makefile
themes/b6/Makefile
themes/bbs/Makefile
themes/beastie/Makefile
themes/biz/Makefile
themes/coldsteel/Makefile
themes/coolclean/Makefile
themes/crux/Makefile
themes/cruxish/Makefile
themes/curve/Makefile
themes/default/Makefile
themes/default.keys/Makefile
themes/eazel-blue/Makefile
themes/elberg/Makefile
themes/exocet/Makefile
themes/fbx/Makefile
themes/g2/Makefile
themes/gaudy/Makefile
themes/gnububble/Makefile
themes/gorilla/Makefile
themes/gtk/Makefile
themes/iceg/Makefile
themes/kde/Makefile
themes/keramik/Makefile
themes/kindaker/Makefile
themes/koynacity/Makefile
themes/linea/Makefile
themes/meenee/Makefile
themes/metabox/Makefile
themes/microcurve/Makefile
themes/microdeck/Makefile
themes/microdeck2/Makefile
themes/microdeck3/Makefile
themes/microgui/Makefile
themes/mofit/Makefile
themes/next/Makefile
themes/ops/Makefile
themes/oroborus/Makefile
themes/perl/Makefile
themes/pills/Makefile
themes/piranha/Makefile
themes/platinum/Makefile
themes/quiet-purple/Makefile
themes/quinx/Makefile
themes/r9x/Makefile
themes/redmond/Makefile
themes/redmondxp/Makefile
themes/retro/Makefile
themes/silverado/Makefile
themes/slick/Makefile
themes/slimline/Makefile
themes/tabs/Makefile
themes/tgc/Makefile
themes/totem/Makefile
themes/trench/Makefile
themes/triviality/Makefile
themes/tux/Makefile
themes/variation/Makefile
themes/xfce/Makefile
])
