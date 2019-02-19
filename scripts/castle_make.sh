#! /usr/bin/env bash

# Print help
usage() { 
cat <<EOF
Usage $0 [-r <Riviera Version>] [-v]
-d <install_dir>    Optional. Riviera Version (defaults to highest available).
EOF
}

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
    export RIVIERA_VERSION=`ls $RIVIERA | sort -d | tail -n1`
else 
    export RIVIERA_VERSION=$VERSION
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
export INCLUDE=${RIVIERA_TOOLCHAIN}/sdk/sysroots/aarch64-oe-linux/usr/include
export LIB=${RIVIERA_TOOLCHAIN}/sdk/sysroots`uname -m`-oesdk-linux/usr/lib
export COMMON_FLAGS="--sysroot=${SYSROOT} -mtune=cortex-a53 -ftree-vectorize  -I${INCLUDE}"
export CFLAGS="-std=c99"
export CXX_FLAGS="-std=c++11"

# DO IT
make



