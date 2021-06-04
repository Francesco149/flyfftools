#!/bin/sh

dir="$(dirname "$0")"
. "$dir"/cflags
for prog in flyffres flyffjobs; do
  $cc $cflags "$@" "${prog}.c" $ldflags -o "$prog"
done

