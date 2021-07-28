#!/bin/sh
   
SH="$(readlink -f /proc/$$/exe)"
if [[ "$SH" == "/bin/zsh" ]]; then
  DIR="${0:A:h}"
else
  DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
fi

cd $DIR
shopt -s globstar
for f in ./**/*
do
  if [ -f "$f" ]; then
    if [[ ( $f == *.cpp ) || ( $f == *.h ) || ( $f == *.td )  || ( $f == *.txt ) || ( $f == *.def ) ]]; then
      diff -Nu $HPVM_SRC_ROOT/$f $f > $f.patch
      patch $HPVM_SRC_ROOT/$f < $f.patch
    fi
  fi
done
