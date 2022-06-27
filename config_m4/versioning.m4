# Set application version based on the git version

#Default
HP8753_VERSION="$PACKAGE_VERSION" #Unknown (no GIT repository detected)"
FILE_VERSION=`cat $srcdir/VERSION`

AC_CHECK_PROG(ff_git,git,yes,no)

#Normalize
HP8753_VERSION_NORMALIZED=`echo $HP8753_VERSION | sed s/dev//g | sed s/RC.*//g | tr -d v`

#Substs
AC_SUBST([HP8753_VERSION], ["$HP8753_VERSION"])
AC_SUBST([HP8753_VERSION_NORMALIZED], ["$HP8753_VERSION_NORMALIZED"])

AC_MSG_CHECKING([the build version])
AC_MSG_RESULT([$HP8753_VERSION ($HP8753_VERSION_NORMALIZED)])

AC_MSG_CHECKING([the build number])
if test $ff_git = no
then
	AC_MSG_RESULT([git not found!])
else

	if test -d $srcdir/.git ; then
		#Try to retrieve the build number
		_HP8753_GIT_BUILD=`git log -1 --pretty=%H`
		_HP8753_GIT_BRANCH=`git rev-parse --abbrev-ref HEAD`
		_HP8753_GIT_DESCRIBE=`git describe --abbrev=40`

		AC_SUBST([HP8753_BUILD], ["$_HP8753_GIT_BUILD"])
		AC_SUBST([HP8753_BRANCH], ["$_HP8753_GIT_BRANCH"])
		AC_SUBST([HP8753_DESCRIBE], ["$_HP8753_GIT_DESCRIBE"])

	fi

	AC_MSG_RESULT([$_HP8753_GIT_BUILD])
fi
