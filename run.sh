#!/bin/sh

if ! [ -f ./stl_player ]; then
	./build.sh;
fi
./stl_player;
exit $?;
