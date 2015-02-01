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
  -a vers

  set -l pkg_name "samwise"
  set -l addr "http://source.xing.com/felix-hamann/samwise/issues"

  sed \
    -e "s/\[FULL-PACKAGE-NAME\]/$pkg_name/" \
    -e "s/\[VERSION\]/$vers/" \
    -e "s|\[BUG-REPORT-ADDRESS\]|$addr|" \
    -e '10i\
AM_INIT_AUTOMAKE' \
  < configure.scan > configure.ac

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
  autoscan
  __release_change_configure $vers

  echo "creating Makefile"
  cp ../AUTHORS ./
  cp ../README.md ./README
  touch NEWS ChangeLog
  autoreconf -iv

  echo "cleaning up"
  rm AUTHORS NEWS README ChangeLog COPYING

end


__release $argv
