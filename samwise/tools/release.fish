#!/usr/bin/env fish

function __release_change_header \
  -a major minor patch

  cp include/sam.h .backup/
  sed \
    -e "s/^\(#define SAM_VERSION_MAJOR\) *\d*.*/\1 $major/" \
    -e "s/^\(#define SAM_VERSION_MINOR\) *\d*.*/\1 $minor/" \
    -e "s/^\(#define SAM_VERSION_PATCH\) *\d*.*/\1 $patch/" \
  < .backup/sam.h > include/sam.h
end


function __release_change_doxyfile \
  -a vers

  cp Doxyfile .backup/
  sed -e "s/^\(PROJECT_NUMBER *=\).*/\1 $vers/" \
  < .backup/Doxyfile > Doxyfile
end


function __release_change_configure \
  -a vers pkg_name addr

  cp configure.ac .backup/
  sed -e "s|AC_INIT.*|AC_INIT($pkg_name, $vers, $addr)|" \
  < .backup/configure.ac > configure.ac

end


function __release_change_samctl \
  -a vers pkg_name addr

  cp src/samctl.c .backup/samctl.c
  sed \
    -e "s/ *\*argp_program_version.*/    *argp_program_version = \"$vers\",/"         \
    -e "s| *\*argp_program_bug_address.*|    *argp_program_bug_address = \"$addr\";|" \
  < .backup/samctl.c > src/samctl.c

end


function __release \
  -d "release a new version" \
  -a major minor patch cflags

  if begin;             \
       [ -z "$major" ]; \
    or [ -z "$minor" ]; \
    or [ -z "$patch" ]; end

    echo "usage: <prog> major minor patch"; echo
    echo "where"
    echo "  <prog>: ./release.fish or ./tools/release.fish"
    echo "  major, minor, patch: version numbers"
    exit 1
  end

  if [ "tools" = (basename (pwd)) ]
    echo "moving up to /samwise"
    cd ..
  end

  set -l vers "$major.$minor.$patch"
  set -l pkg_name "samwise"
  set -l addr "http://github.com/dreadworks/samwise/issues"

  echo "relasing version $vers"
  echo "continue? [y]"
  read user_input
  if [ "y" != "$user_input" ]
    echo "aborting"
    exit 0
  end

  echo "creating backup directory"
  rm -rf .backup
  mkdir .backup

  echo "changing include/sam.h"
  __release_change_header $major $minor $patch

  echo "changing Doxyfile"
  __release_change_doxyfile $vers

  echo "changing configure.ac"
  __release_change_configure $vers $pkg_name $addr

  echo "changing src/samctl.c"
  __release_change_samctl $vers $pkg_name $addr

end


__release $argv
