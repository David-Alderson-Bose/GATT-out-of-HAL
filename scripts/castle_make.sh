#! /usr/bin/env bash

# Print help
usage() { 
cat <<EOF
Use this to call makefiles for a caslte target without having to 
go through components. 

Exports the following to be available to gnu make:
Gnu tools for castle: You know, CC, CXX, etc.
CASTLE_INCLUDE: location for most header files
CASTLE_LIB: location for most library files, both static and shared
COMMON_FLAGS=: sysroot, -mtune=cortex-a53 -ftree-vectorize -L{CASTLE_LIB} -I{CASTLE_INCLUDE}"
CFLAGS: -std=c99
CXX_FLAGS: -std=c++11

Usage:
$0 [<rule>] [-r <Riviera Version>] 
<rule>                  Optional. Target rule to pass into make.
-r <Riviera Version>    Optional. Riviera Version (defaults to highest available).

EOF
}

# Check for non-opt first arg, it'll be a make target rule
if [ -z ${1+x} ]; then # check for no args
    : # if there's no args, just pass
elif [[ $1 != -* ]]; then 
    RULE=$1
    shift # bump arg positions for upcoming getopts
fi

# Parse args
while getopts ":r:h" opt; do
    case "${opt}" in
        h) 
            usage
            exit 0
            ;;
        r)
            VERSION=${OPTARG}
            ;;
        *)
            usage
            exit 1
    esac
done


# Determine riviera version
if [ -z ${VERSION+x} ]; then 
    # If no version provided, go find the highest one
    RIVIERA="/scratch/components-cache/Release/master/"
    RIVIERA_VERSION=`ls $RIVIERA | sort -d | tail -n1`
else 
    RIVIERA_VERSION=$VERSION
fi

# Source version-dependent variables and see if things worked
export RIVIERA_TOOLCHAIN=/scratch/components-cache/Release/master/$RIVIERA_VERSION/Riviera-Toolchain
if [ ! -d "$RIVIERA_TOOLCHAIN" ]; then
    echo "Directory $RIVIERA_TOOLCHAIN does not exist! Please check that $RIVIERA_VERSION is correct"
    exit 1
fi

# Finish setup
echo "Basing build on tools found in $RIVIERA_TOOLCHAIN"
export SYSROOT=$RIVIERA_TOOLCHAIN/sdk/sysroots/aarch64-oe-linux
export CC=${RIVIERA_TOOLCHAIN}/sdk/sysroots/`uname -m`-oesdk-linux/usr/bin/arm-oemllib32-linux/arm-oemllib32-linux-gcc
export CXX=${RIVIERA_TOOLCHAIN}/sdk/sysroots/`uname -m`-oesdk-linux/usr/bin/arm-oemllib32-linux/arm-oemllib32-linux-g++
export LD=${RIVIERA_TOOLCHAIN}/sdk/sysroots/`uname -m`-oesdk-linux/usr/bin/arm-oemllib32-linux/arm-oemllib32-linux-ld
export AR=${RIVIERA_TOOLCHAIN}/sdk/sysroots/`uname -m`-oesdk-linux/usr/bin/arm-oemllib32-linux/arm-oemllib32-linux-ar
export RANLIB=${RIVIERA_TOOLCHAIN}/sdk/sysroots/`uname -m`-oesdk-linux/usr/bin/arm-oemllib32-linux/arm-oemllib32-linux-ranlib
export CASTLE_INCLUDE=${RIVIERA_TOOLCHAIN}/sdk/sysroots/aarch64-oe-linux/usr/include
export CASTLE_LIB=${RIVIERA_TOOLCHAIN}/sdk/sysroots/aarch64-oe-linux/usr/lib
export COMMON_FLAGS="--sysroot=${SYSROOT} -mtune=cortex-a53 -ftree-vectorize -L${CASTLE_LIB} -I${CASTLE_INCLUDE}"
export CFLAGS="-std=c99"
export CXX_FLAGS="-std=c++11"

# DO IT
make $RULE



