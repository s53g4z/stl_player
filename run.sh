#!/bin/sh

if ! [ -x ./stl_player ]; then
	./build.sh;
fi
vblank_mode=3 ./stl_player;
exit $?;
