#!/bin/sh
# Downloads TestGame-2003 (https://github.com/EasyRPG/TestGame), the game the
# start-up test opens, into DIR, once (nothing to do when DIR exists):
#
#   builds/testgame.sh DIR
#
# Used by builds/macos/build.sh --tests and by `make test` on Linux.
set -eu

dir="$1"
[ -d "$dir" ] && exit 0
parent="$(dirname "$dir")"
mkdir -p "$parent"
echo "Downloading TestGame-2003 into $dir"
curl -fsSL https://github.com/EasyRPG/TestGame/archive/refs/heads/master.tar.gz \
	| tar -xz -C "$parent" --strip-components 1 TestGame-master/TestGame-2003
mv "$parent/TestGame-2003" "$dir"
