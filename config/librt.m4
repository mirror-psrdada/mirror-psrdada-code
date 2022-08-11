#
# LIB_LIBRT([ACTION-IF-FOUND [,ACTION-IF-NOT-FOUND]])
#
# This m4 macro checks availability of the Real Time Library
#
# HAVE_LIBRT   - automake conditional
# HAVE_LIBRT   - pre-processor macro in config.h
#
# This macro tries to link using:
#
#    -lrt
#
# ----------------------------------------------------------
AC_DEFUN([LIB_LIBRT],
[
  AC_PROVIDE([LIB_LIBRT])

  dnl Check for librt
  AC_MSG_CHECKING(for librt)
  AC_CHECK_LIB([rt], [clock_gettime], [have_librt="yes"])
  if test "$have_librt" = "yes" ; then
    AC_DEFINE([HAVE_LIBRT], 1, [Have librt])
    [$1]
  else
    AC_MSG_RESULT(no)
    [$2]
  fi

  AM_CONDITIONAL(HAVE_LIBRT, [test x"$have_librt" = xyes])

])

