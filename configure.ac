m4_include([VERSION]) #Force reconf on VERSION change
AC_INIT([HP8753],[m4_esyscmd_s(cat VERSION)],[nv3z@gmail.com],[HP8753],[http://github.com/vk2bea/HP8753])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

AC_USE_SYSTEM_EXTENSIONS

AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_CXX
LT_PATH_LD
AM_PROG_CC_C_O
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

PKG_PROG_PKG_CONFIG

AUTOCONF_ENV="SHELL=/bin/sh"

LT_INIT
AC_ENABLE_STATIC

# Some useful default flags
CFLAGS="-Werror -Wall $CFLAGS"
CXXFLAGS="-Werror -Wall $CXXFLAGS"

PKG_PROG_PKG_CONFIG
if test -z "$PKG_CONFIG"; then
  AC_MSG_WARN([Cannot find pkg-config, make sure it is installed in your PATH])
fi

AC_DEFINE([__STDC_FORMAT_MACROS], [], [Description])

# Debug
m4_include([config_m4/debug.m4])

# Check for profiling mode
m4_include([config_m4/profiling.m4])

# Checking libs
m4_include([config_m4/versioning.m4])

# ghostscript
AC_CHECK_PROG(GS_CHECK,gs,yes)
if test x"$GS_CHECK" != x"yes" ; then
   AC_MSG_ERROR([Please install gs before installing.])
fi
# libgpib
AC_CHECK_LIB(gpib,ibask,,AC_MSG_ERROR([Please install Linux GPIB driver before continuing.]))

# check for Noto font
fc-list -q "Noto Emoji"
if [[ $? -ne 0 ]]; then
   AC_MSG_ERROR([Please install fonts-noto-color-emoji before proceeding.])
else
   echo "checking for Emoji font... yes"   
fi

YELP_HELP_INIT

PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([GTK3], [gtk+-3.0])
PKG_CHECK_MODULES([SQLITE3], [sqlite3])

# files required for building

AC_CHECK_HEADER(ghostscript/ierrors.h, , [AC_MSG_ERROR([Couldn't find ghostscript headers. Please install ghostscript development package] )])
AC_CHECK_HEADER(gpib/gpib_user.h, , [AC_MSG_ERROR([Couldn't find linux gpib headers. Please install libgpib development package] )])


# Output files
AC_CONFIG_FILES([
	Makefile
	src/Makefile
    help/Makefile
    data/Makefile
])

AC_OUTPUT
