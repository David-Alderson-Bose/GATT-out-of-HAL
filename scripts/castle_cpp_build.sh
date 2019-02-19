#! /usr/bin/env bash
echo "Compiling source file $1 into program $2..."
source ./castle_c_setup.sh
$CXX $COMMON_FLAGS -v $CXX_FLAGS -I$INCLUDE/hardware  $1 -o $2
echo "Done."

