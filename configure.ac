#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT([parsify], [0.1], [dustin@clou.es])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

: ${CFLAGS="-g -O0"}

# Checks for programs.
AC_PROG_CC([clang gcc], [-O0])
#AC_PROG_CC_C99([clang gcc], [-O0])

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lncurses':
AC_CHECK_LIB([ncurses], [main])

AC_ARG_ENABLE([hval-stats],
    [ --enable-hval-stats    enable hval allocation stats ],
    [ hval_stats=${enableval}], [hval_stats=no])

AC_ARG_ENABLE([gc-reporting],
    [ --enable-gc-reporting   enable runtime gc reporting ],
    [ gc_reporting=${enableval}], [gc_reporting=no])

if test "x${hval_stats}" == xyes; then
    AC_DEFINE([HVAL_STATS], 1, [hval stats enabled])
fi

if test "x${gc_reporting}" == xyes; then
    AC_DEFINE([GC_REPORTING], 1, [gc reporting enabled])
fi

PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 tests/Makefile])
AC_OUTPUT

echo "
    Object Alloc. Stats...: $hval_stats
    GC Stats..............: $gc_reporting
    C Compiler............: $CC $CFLAGS
    Linker................: $LD $LDFLAGS $LIBS
"
