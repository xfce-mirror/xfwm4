dnl From Benedikt Meurer (benedikt.meurer@unix-ag.uni-siegen.de)
dnl
dnl if debug support is requested:
dnl
dnl   1) defines DEBUG to 1
dnl   2) adds requested debug level flags to CFLAGS
dnl

AC_DEFUN([BM_DEBUG_SUPPORT],
[
  AC_ARG_ENABLE(debug,
AC_HELP_STRING([--enable-debug[=yes|no|full]], [Build with debugging support])
AC_HELP_STRING([--disable-debug], [Include no debugging support [default]]),
    [ac_cv_debug=$enableval], [ac_cv_debug=no])
  AC_MSG_CHECKING([whether to build with debugging support])
  if test "x$ac_cv_debug" != "xno"; then
    AC_DEFINE(DEBUG, 1, Define for debugging support)
    if test "x$ac_cv_debug" == "xfull"; then
      CFLAGS="$CFLAGS -g3 -Wall -Werror"
      AC_MSG_RESULT([full])
    else
      CFLAGS="$CFLAGS -g -Wall -Werror"
      AC_MSG_RESULT([yes])
    fi
  else
    AC_MSG_RESULT([no])
  fi
])
