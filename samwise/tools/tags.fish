
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

  for dep in \
    "zeromq/libzmq"           \
    "zeromq/czmq v3.0.0"      \
    "alanxz/rabbitmq-c v0.5.2"

    echo $dep | read location tag

    set -l name (basename "$location")
    set -l target "$dir/$name"
    echo "handling $name"

    if [ ! -d "$target" ]
      git clone "https://github.com/$name" "$target"
    end

    if [ -n $tag ]
      pushd "$target"
      git checkout "$tag"
      popd
    end

    echo
  end

  __tags_find "$dir"
  __tags_find .

end


__tags
