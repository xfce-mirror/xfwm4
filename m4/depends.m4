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
dnl BM_DEPEND_CHECK(var, pkg, version, name, helpstring)
dnl
AC_DEFUN([BM_DEPEND_CHECK],
[
  AC_ARG_ENABLE([$4-check],
AC_HELP_STRING([--enable-$4-check], [Enable checking for $5 (default)])
AC_HELP_STRING([--disable-$4-check], [Disable checking for $5]),
    [ac_cv_$1_check=$enableval], [ac_cv_$1_check=yes])

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

