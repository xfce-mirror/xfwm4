dnl From Benedikt Meurer (benedikt.meurer@unix-ag.uni-siegen.de)
dnl
dnl Workaround for some broken ELF systems
dnl

AC_DEFUN([BM_RPATH_SUPPORT],
[
  AC_ARG_ENABLE(rpath,
AC_HELP_STRING([--enable-rpath], [Specify run path to the ELF linker (default)])
AC_HELP_STRING([--disable-rpath], [Do not use -rpath (use with care!!)]),
    [ac_cv_rpath=$enableval], [ac_cv_rpath=yes])
  AC_MSG_CHECKING([whether to use -rpath])
  LD_RPATH=
  if test "x$ac_cv_rpath" != "xno"; then
    LD_RPATH="-Wl,-R"
    AC_MSG_RESULT([yes])
  else
    LD_RPATH=""
    AC_MSG_RESULT([no])
  fi
])
