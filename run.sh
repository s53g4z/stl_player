#!/bin/sh

if ! [ -f ./stl_player ]; then
	./build.sh;
fi
vblank_mode=3 ./stl_player;
exit $?;
