#!/bin/bash

set -xe

CFLAGS="-Wall -Wextra -ggdb"
LFLAGS=$(pkg-config --libs x11 gl glx)

cc -o xg xg.c $CFLAGS $LFLAGS
