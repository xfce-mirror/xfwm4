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
  if test x"$ac_cv_debug" != x"no"; then
    AC_DEFINE(DEBUG, 1, Define for debugging support)
    if test x"$ac_cv_debug" = x"full"; then
      AC_DEFINE(DEBUG_TRACE, 1, Define for tracing support)
      CFLAGS="$CFLAGS -g3 -Wall -Werror -DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED"
      AC_MSG_RESULT([full])
    else
      CFLAGS="$CFLAGS -g -Wall -DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED"
      AC_MSG_RESULT([yes])
    fi
  else
    CFLAGS="$CFLAGS -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS"
    AC_MSG_RESULT([no])
  fi
])
