
function __deps_libzmq \
  -a dir

  echo "building libzmq"
  mkdir .tmp
  pushd .tmp

  wget http://download.zeromq.org/zeromq-4.0.5.tar.gz
  and tar xzf zeromq-4.0.5.tar.gz
  and pushd zeromq-4.0.5
  and ./configure "--prefix=$dir"
  and make
  and make install
  and popd
  or begin
    if [ (basename (pwd)) = "zeromq-4.0.5" ]
      popd
    end

    popd
    echo "installing libzmq failed"
    return -1
  end

  popd
  rm -rf .tmp
  echo
end


function __deps_libczmq \
  -a dir

  echo "building czmq"
  mkdir .tmp
  pushd .tmp

  wget http://download.zeromq.org/czmq-3.0.0-rc1.tar.gz
  and tar xzf czmq-3.0.0-rc1.tar.gz
  and pushd czmq-3.0.0
  and ./configure "--prefix=$dir"
  and make
  and make install
  and popd
  or begin
    if [ (basename (pwd)) = "czmq-3.0.0" ]
      popd
    end

    popd
    echo "installing czmq failed"
    return -1
  end

  popd
  rm -rf .tmp
  echo
end


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
  or echo "something went wrong, exiting."

end


__deps
