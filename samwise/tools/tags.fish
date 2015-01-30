
function __tags
  echo "creating tags"

  if [ "tools" = (basename (pwd)) ]
    echo "moving up to /samwise"
    cd ..
  end

  set -l dir (pwd)"/.lib"

  rm -rf TAGS "$dir"
  mkdir "$dir"

  for gh in \
    "zeromq/libzmq"     \
    "zeromq/czmq"       \
    "alanxz/rabbitmq-c"

    set -l base (basename "$gh")
    set -l target "$dir/$base"

    git clone "https://github.com/$gh" "$target"
    echo
  end

  find . -name "*.c" -print -or -name "*.h" -print \
    | xargs etags --append

end


__tags
