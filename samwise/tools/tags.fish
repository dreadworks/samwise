
function __tags_find \
  -a loc

  find "$loc" -name "*.c" -print -or -name "*.h" -print \
    | xargs etags --append
end

function __tags
  echo "creating tags"

  if [ "tools" = (basename (pwd)) ]
    echo "moving up to /samwise"
    cd ..
  end

  pushd ../
  set -l dir (pwd)"/.tags"
  popd

  rm -rf TAGS

  if [ ! -d "$dir" ]
    mkdir "$dir"
  end

  #
  #  get sources from github
  #
  for dep in \
    "zeromq/libzmq"           \
    "zeromq/czmq v3.0.0"      \
    "alanxz/rabbitmq-c v0.5.2"

    echo $dep | read location tag

    set -l name (basename "$location")
    set -l target "$dir/$name"
    echo "handling $name"

    if [ ! -d "$target" ]
      git clone "https://github.com/$location" "$target"
    end

    if [ -n $tag ]
      pushd "$target"
      git checkout "$tag"
      popd
    end

    echo
  end

  #
  #  get other sources
  #
  set -l name check-0.9.14
  set -l url http://downloads.sourceforge.net/project/check/check/0.9.14

  if [ ! -d "$dir/$name" ]
    pushd "$dir"
    wget $url/$name.tar.gz
    tar xzf $name.tar.gz
    rm $name.tar.gz
    popd
  end


  #
  #  create tags file
  #
  __tags_find "$dir"
  __tags_find .

end


__tags
