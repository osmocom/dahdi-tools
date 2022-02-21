#!/bin/sh
# jenkins build helper script for dahdi-tools.  This is how we build on jenkins.osmocom.org

if ! [ -x "$(command -v osmo-build-dep.sh)" ]; then
	echo "Error: We need to have scripts/osmo-deps.sh from http://git.osmocom.org/osmo-ci/ in PATH !"
	exit 2
fi

set -ex

base="$PWD"
deps="$base/deps"
inst="$deps/install"
export deps inst

osmo-clean-workspace.sh

mkdir "$deps" || true

cd "$deps"
if [ -d dahdi-linux ]; then
	(cd dahdi-linux && git fetch && git checkout -f -B master origin/master)
else
	git clone https://git.osmocom.org/dahdi-linux
fi

cd $base

autoreconf -fi
./configure --with-dahdi="$deps/dahdi-linux"
$MAKE $PARALLEL_MAKE

osmo-clean-workspace.sh
