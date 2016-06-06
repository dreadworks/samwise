#
#  install all required dependencies to ../usr
#


#
#  generic download and autotools build
#
function __deps_build_generic \
  -a dir name url

  echo "building $name"
  mkdir .tmp
  pushd .tmp

  set -l archive tmp.tar.gz

  wget $url -O $archive
  and tar xzf $archive
  and pushd $name
  and if [ -f autogen.sh ]
    ./autogen.sh
  end
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
    zeromq4-x-4.0.5 \
    https://github.com/zeromq/zeromq4-x/archive/v4.0.5.tar.gz

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
    https://github.com/zeromq/czmq/archive/v3.0.0.tar.gz

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
#  install berkeleydb
#  oracle.com
#
function __deps_libdb \
  -a dir

  echo "building berkeleydb"
  mkdir .tmp
  pushd .tmp

  wget http://download.oracle.com/berkeley-db/db-6.1.19.tar.gz
  and tar xzf db-6.1.19.tar.gz

  # TODO OSX
  and cd db-6.1.19/build_unix
  and ../dist/configure --prefix $dir
  and make
  and make install
  or begin
    popd
    echo "installing berkeley db failed"
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
    http://downloads.sourceforge.net/project/check/check/0.9.14/check-0.9.14.tar.gz

end



function __deps \
  -a dir
  echo "resolving dependencies"

  if [ "tools" = (basename (pwd)) ]
    echo "moving up to /samwise"
    cd ..
  end

  if [ -z "$dir" ]
    pushd ../
    set dir (pwd)"/usr"
    popd
  end

  echo "going to install to $dir"

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
  set -gx LD_LIBRARY_PATH "$dir/lib:$LD_LIBRARY_PATH"
  set -gx C_INCLUDE_PATH "$dir/include:$C_INCLUDE_PATH"

  __deps_libzmq "$dir"
  and __deps_libczmq "$dir"
  and __deps_librabbitmq "$dir"
  and __deps_libdb "$dir"
  and __deps_check "$dir"
  or begin
    echo "something went wrong, exiting."
    return 2
  end

end


__deps $argv
