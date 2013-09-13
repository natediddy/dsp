AC_PREREQ([2.50])
AC_INIT(dsp, 1.3.0)
AC_CONFIG_SRCDIR([dsp.c])

AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign])

AM_MAINTAINER_MODE

AC_PROG_CC

AC_CHECK_LIB(curl, curl_easy_init, [LIBS="$LIBS -lcurl"], [
  AC_ERROR([libcurl is required to build dsp, if you do not have it,
            you can get it from http://curl.haxx.se/download.html/ or
            from your system package manager (if you have one)])
])

AC_CHECK_HEADERS([sys/ioctl.h unistd.h windows.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
