#
#  install all required dependencies to ../usr
#


#
#  generic download and autotools build
#
function __deps_build_generic \
  -a dir name archive url

  echo "building $name"
  mkdir .tmp
  pushd .tmp

  wget $url
  and tar xzf $archive
  and pushd $name
  and ./configure "--prefix=$dir"
  and make
  and make install
  and popd
  or begin
    if [ (basename (pwd)) = "$name" ]
      popd
    end

    popd
    echo "installing $name failed"
    return -1
  end

  popd
  rm -rf .tmp
  echo

end


#
#  install zeromq
#  http://zeromq.org
#
function __deps_libzmq \
  -a dir

  __deps_build_generic \
    $dir \
    zeromq-4.0.5 \
    zeromq-4.0.5.tar.gz \
    http://download.zeromq.org/zeromq-4.0.5.tar.gz

end


#
#  install czmq
#  http://czmq.zeromq.org
#
function __deps_libczmq \
  -a dir

  __deps_build_generic \
    $dir \
    czmq-3.0.0 \
    czmq-3.0.0-rc1.tar.gz \
    http://download.zeromq.org/czmq-3.0.0-rc1.tar.gz

end


#
#  install the rabbitmq c library
#  https://github.com/alanxz/rabbitmq-c
#
function __deps_librabbitmq \
  -a dir

  echo "building librabbitmq"
  mkdir .tmp
  pushd .tmp

  wget https://github.com/alanxz/rabbitmq-c/releases/download/v0.5.2/rabbitmq-c-0.5.2.tar.gz
  and tar xzf rabbitmq-c-0.5.2.tar.gz
  and cd rabbitmq-c-0.5.2
  and mkdir build
  and cd build
  and cmake "-DCMAKE_INSTALL_PREFIX=$dir" ..
  and cmake --build . --target install
  and find "$dir" -name "librabbitmq.so*" -exec cp -p -d "{}" "$dir/lib/" \;
  or begin
    popd
    echo "installing rabbitmq-c failed"
    return -1
  end

  popd
  rm -rf .tmp
  echo
end


#
#  install check, a unit testing framework
#  http://check.sourceforge.net
#
function __deps_check \
  -a dir

  __deps_build_generic \
    $dir \
    check-0.9.14 \
    check-0.9.14.tar.gz \
    http://downloads.sourceforge.net/project/check/check/0.9.14/check-0.9.14.tar.gz

end



function __deps \
  -a cflags
  echo "resolving dependencies"

  if [ "tools" = (basename (pwd)) ]
    echo "moving up to /samwise"
    cd ..
  end

  pushd ../
  set -l dir (pwd)"/usr"
  popd

  if [ -d "$dir" ]
    rm -rf "$dir"
  end

  if [ -d .tmp ]
    echo "cleaning up older state"
    rm -rf .tmp
  end

  mkdir "$dir"
  mkdir "$dir/lib"

  set -Ux LDFLAGS "-L$dir/lib"

  __deps_libzmq "$dir"
  and __deps_libczmq "$dir"
  and __deps_librabbitmq "$dir"
  and __deps_check "$dir"
  or echo "something went wrong, exiting."

end


__deps
