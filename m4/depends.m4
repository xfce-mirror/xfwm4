dnl From Benedikt Meurer (benedikt.meurer@unix-ag.uni-siegen.de)
dnl
dnl

AC_DEFUN([BM_DEPEND],
[
  PKG_CHECK_MODULES([$1], [$2 >= $3])
  $1_REQUIRED_VERSION=$3
  AC_SUBST($1_REQUIRED_VERSION)
])

dnl
dnl BM_DEPEND_CHECK(var, pkg, version, name, helpstring, default)
dnl
AC_DEFUN([BM_DEPEND_CHECK],
[
  AC_ARG_ENABLE([$4],
AC_HELP_STRING([--enable-$4], [Enable checking for $5 (default=$6)])
AC_HELP_STRING([--disable-$4], [Disable checking for $5]),
    [ac_cv_$1_check=$enableval], [ac_cv_$1_check=$6])

  if test x"$ac_cv_$1_check" = x"yes"; then
    AC_MSG_CHECKING([for $2 >= $3])
    if $PKG_CONFIG --atleast-version=$3 $2 2> /dev/null; then
      AC_MSG_RESULT([yes])
      BM_DEPEND([$1], [$2], [$3])
      AC_DEFINE([HAVE_$1], [1], [Define if you have $2 >= $3])
    else
      AC_MSG_RESULT([no])
    fi
  fi
])

dnl
dnl XFCE_PANEL_PLUGIN(var, version)
dnl
dnl Sets $var_CFLAGS, $var_LIBS and $var_PLUGINSDIR
dnl
AC_DEFUN([XFCE_PANEL_PLUGIN],
[
  BM_DEPEND([$1], [xfce4-panel-1.0], [$2])

  dnl Check where to put the plugins to
  AC_MSG_CHECKING([where to install panel plugins])
  $1_PLUGINSDIR=`$PKG_CONFIG --variable=pluginsdir xfce4-panel-1.0`
  AC_SUBST([$1_PLUGINSDIR])
  AC_MSG_RESULT([$$1_PLUGINSDIR])
])

dnl
dnl XFCE_MCS_PLUGIN(var, version)
dnl
dnl sets $var_CFLAGS, $var_LIBS and $var_PLUGINSDIR
dnl
AC_DEFUN([XFCE_MCS_PLUGIN],
[
  BM_DEPEND([$1], [xfce-mcs-manager], [$2])

  dnl Check where to put the plugins to
  AC_MSG_CHECKING([where to install MCS plugins])
  $1_PLUGINSDIR=`$PKG_CONFIG --variable=pluginsdir xfce-mcs-manager`
  AC_SUBST([$1_PLUGINSDIR])
  AC_MSG_RESULT([$$1_PLUGINSDIR])
])
