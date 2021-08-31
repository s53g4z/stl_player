#!/bin/sh

clear;
rm -f stlplayer;
gcc -Wall -Wextra -std=c11 -g -O0 -D USE_GLES2=1 \
	initgl.c levelreader.c stlplayer.c util.c -o stlplayer \
	-lEGL -lX11 -lGLESv2 -lm -lpthread "$@";
exit $?;
