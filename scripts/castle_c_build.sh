#! /usr/bin/env bash
echo "Compiling source file $1 into program $2..."
source ./castle_c_setup.sh
$CC $COMMON_FLAGS $CFLAGS $1 -o $2
echo "Done."

