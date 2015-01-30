
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
  set -l dir (pwd)"/.lib"
  popd
  
  rm -rf TAGS

  if [ ! -d "$dir" ]
    mkdir "$dir"
  end

  for gh in \
    "zeromq/libzmq"     \
    "zeromq/czmq"       \
    "alanxz/rabbitmq-c"

    set -l base (basename "$gh")
    set -l target "$dir/$base"
    echo "handling $base"

    if [ -d "$target" ]
      pushd "$target"
      git pull
      popd
    else
      git clone "https://github.com/$gh" "$target"
    end

    echo
  end

  __tags_find "$dir"
  __tags_find .

end


__tags
