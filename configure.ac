AC_PREREQ([2.69])
AC_INIT([tsschecker], m4_esyscmd([git rev-list --count HEAD | tr -d '\n']), [tihmstar@gmail.com])

AC_CANONICAL_SYSTEM
AC_CANONICAL_HOST
AM_PROG_LIBTOOL

AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])


AC_DEFINE([VERSION_COMMIT_COUNT], "m4_esyscmd([git rev-list --count HEAD | tr -d '\n'])", [Git commit count])
AC_DEFINE([VERSION_COMMIT_SHA], "m4_esyscmd([git rev-parse HEAD | tr -d '\n'])", [Git commit sha])
AC_SUBST([VERSION_COMMIT_COUNT], ["m4_esyscmd([git rev-list --count HEAD | tr -d '\n'])"])
AC_SUBST([VERSION_COMMIT_SHA], ["m4_esyscmd([git rev-parse HEAD | tr -d '\n'])"])


# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

crypto_backend="none"
# Check for operating system
AC_MSG_CHECKING([whether we need platform-specific build settings])
case $host_os in
  darwin* )
    AC_MSG_RESULT([${host_os}])
    AC_DEFINE(HAVE_COMMCRYPTO, 1, [Define if you have CommonCrypto (macOS)])
    crypto_backend="CommCrypto"
  ;;
  *mingw32*|*cygwin*|*msys*)
		AC_MSG_RESULT([${host_os}])
    LDFLAGS+=" -no-undefined -lws2_32"
		win32=true
	;;
	*)
		AC_MSG_RESULT([${host_os}])
  ;;
esac
AM_CONDITIONAL(WIN32, test x$win32 == xtrue)

CXXFLAGS+=" -std=c++17"
CFLAGS+=" -std=c11"
# Versioning.

# Checks for libraries.
LIBCURL_REQUIRES_STR="libcurl >= 7.0.0"
LIBFRAGMENTZIP_REQUIRES_STR="libfragmentzip >= 65"
LIBGENERAL_REQUIRES_STR="libgeneral >= 75"
LIBIRECOVERY_REQUIRES_STR="libirecovery-1.0 >= 1.0.0"
LIBPLIST_REQUIRES_STR="libplist-2.0 >= 2.3.0"
OPENSSL_REQUIRES_STR="openssl >= 0.9.8"

PKG_CHECK_MODULES(libcurl, $LIBCURL_REQUIRES_STR)
PKG_CHECK_MODULES(libfragmentzip, $LIBFRAGMENTZIP_REQUIRES_STR)
PKG_CHECK_MODULES(libgeneral, $LIBGENERAL_REQUIRES_STR)
PKG_CHECK_MODULES(libirecovery, $LIBIRECOVERY_REQUIRES_STR)
PKG_CHECK_MODULES(libplist, $LIBPLIST_REQUIRES_STR)
PKG_CHECK_MODULES(openssl, $OPENSSL_REQUIRES_STR, have_openssl=yes, have_openssl=no)


AC_SUBST([libcurl_requires], [$LIBCURL_REQUIRES_STR])
AC_SUBST([libfragmentzip_requires], [$LIBFRAGMENTZIP_REQUIRES_STR])
AC_SUBST([libgeneral_requires], [$LIBGENERAL_REQUIRES_STR])
AC_SUBST([libirecovery_requires], [$LIBIRECOVERY_REQUIRES_STR])
AC_SUBST([libplist_requires], [$LIBPLIST_REQUIRES_STR])

AC_ARG_ENABLE([debug],
            [AS_HELP_STRING([--enable-debug],
            [enable debug build(default is no)])],
            [debug_build=true],
            [debug_build=false])

AC_ARG_WITH([openssl],
            [AS_HELP_STRING([--without-openssl],
            [do not build with openssl @<:@default=yes@:>@])],
            [with_openssl=no],
            [with_openssl=yes])

if test "x$debug_build" = "true"; then
  echo "*** Note: debug build requested ***"
  CFLAGS+=" -g -O0 -DDEBUG=1"
  CXXFLAGS+=" -g -O0 -DDEBUG=1"
fi

if test "x$with_openssl" == "xyes"; then
  if test "x$have_openssl" = "xyes"; then
    AC_DEFINE(HAVE_OPENSSL, 1, [Define if you have openssl])
    AC_SUBST([openssl_requires], [$OPENSSL_REQUIRES_STR])
    AC_SUBST(openssl_CFLAGS)
    AC_SUBST(openssl_LIBS)
    crypto_backend="openssl"
  else
    echo "*** Note: openssl has been disabled ***"
  fi
else
  if test "x$with_openssl" == "xyes"; then
    AC_MSG_ERROR([requested building with openssl, but library could not be found])
  fi
fi

if test "x$crypto_backend" != "xnone"; then
  AC_DEFINE(HAVE_CRYPTO, 1, [Define if you have either openssl or CommCrypto])
fi


AC_CONFIG_FILES([Makefile
                 include/Makefile
                 libtsschecker.pc
                 tsschecker/Makefile])

AC_OUTPUT


echo "
Configuration for $PACKAGE-$VERSION:
-------------------------------------------

  install prefix ..........: $prefix
  debug build .............: $debug_build
  crypto backend ..........: $crypto_backend"

echo "  compiler ................: ${CC}

  Now type 'make' to build $PACKAGE-$VERSION,
  and then 'make install' for installation.
"
