dnl From Benedikt Meurer (benedikt.meurer@unix-ag.uni-siegen.de)
dnl
dnl if debug support is requested:
dnl
dnl   1) defines DEBUG to 1
dnl   2) adds requested debug level flags to CFLAGS
dnl

AC_DEFUN([BM_DEBUG_SUPPORT],
[
  AC_ARG_ENABLE([debug],
AC_HELP_STRING([--enable-debug[=yes|no|full]], [Build with debugging support])
AC_HELP_STRING([--disable-debug], [Include no debugging support [default]]),
    [], [enable_debug=no])

  AC_MSG_CHECKING([whether to build with debugging support])
  if test x"$enable_debug" != x"no"; then
    AC_DEFINE(DEBUG, 1, Define for debugging support)
    if test x"$enable_debug" = x"full"; then
      AC_DEFINE(DEBUG_TRACE, 1, Define for tracing support)
      CFLAGS="$CFLAGS -g3 -Wall -Werror -DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED"
      AC_MSG_RESULT([full])
    else
      CFLAGS="$CFLAGS -g -Wall -DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED"
      AC_MSG_RESULT([yes])
    fi
  else
    AC_MSG_RESULT([no])
  fi

  AC_ARG_ENABLE([profiling],
AC_HELP_STRING([--enable-profiling],
    [Generate extra code to write profile information])
AC_HELP_STRING([--disable-profiling],
    [No extra code for profiling (default)]),
    [], [enable_profiling=no])

  AC_MSG_CHECKING([whether to build with profiling support])
  if test x"$enable_profiling" != x"no"; then
    CFLAGS="$CFLAGS -pg"
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
  fi

  AC_ARG_ENABLE([asserts],
AC_HELP_STRING([--enable-asserts], [Enable assert statements (default)])
AC_HELP_STRING([--disable-asserts],
    [Disable assert statements (USE WITH CARE!!!)]),
    [], [enable_asserts=yes])

  AC_MSG_CHECKING([whether to enable assert statements])
  if test x"$enable_asserts" != x"yes"; then
    CFLAGS="$CFLAGS -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS"
    AC_MSG_RESULT([no])
  else
    AC_MSG_RESULT([yes])
  fi
])
