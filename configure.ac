AC_INIT([ztbackup], [1.0], [gkdada@comcast.net])
PKG_CHECK_MODULES([LIBSSH2], [libssh2])
PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([LIBSMB],  [smbclient])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CXX
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
  Makefile
  src/Makefile
])
AC_OUTPUT

