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
dnl BM_DEPEND_CHECK(var, pkg, version)
dnl
AC_DEFUN([BM_DEPEND_CHECK],
[
  AC_MSG_CHECKING([for $2 >= $3])
  if $PKG_CONFIG --atleast-version=$3 $2 2> /dev/null; then
    AC_MSG_RESULT([yes])
    BM_DEPEND([$1], [$2], [$3])
    AC_DEFINE([HAVE_$1], [1], [Define if you have $2 >= $3])
  else
    AC_MSG_RESULT([no])
  fi
])

