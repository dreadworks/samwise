#!/usr/bin/env fish
#
#  build the publisher confirms
#  publisher/consumer example
#


function __build_pubconf_lib_rabbit
  echo "creating temporary dir"
  mkdir .tmp
  cd .tmp

  git clone https://github.com/alanxz/rabbitmq-c
  and cmake rabbitmq-c
  and cmake --build

  echo "compile source file"
  make

  echo "copying header files"
  cp rabbitmq-c/librabbitmq/amqp*.h ../include

  echo "copying library files"
  cp librabbitmq/*.so ../lib

  echo "moving up"
  cd ..

  rm -rf .tmp
  return $build_stat
end


function __build_pubconf
  # clean
  make clean
  rm -rf include lib

  # setup
  mkdir lib
  mkdir include

  # build deps
  __build_pubconf_lib_rabbit

  # compile
  make
end

__build_pubconf
