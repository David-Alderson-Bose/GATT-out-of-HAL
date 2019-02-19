export RIVIERA_VERSION=4.7
export RIVIERA_TOOLCHAIN=/scratch/components-cache/Release/master/$RIVIERA_VERSION/Riviera-Toolchain
export SYSROOT=$RIVIERA_TOOLCHAIN/sdk/sysroots/aarch64-oe-linux

export CC=${RIVIERA_TOOLCHAIN}/sdk/sysroots/`uname -m`-oesdk-linux/usr/bin/arm-oemllib32-linux/arm-oemllib32-linux-gcc
export CXX=${RIVIERA_TOOLCHAIN}/sdk/sysroots/`uname -m`-oesdk-linux/usr/bin/arm-oemllib32-linux/arm-oemllib32-linux-g++
export LD=${RIVIERA_TOOLCHAIN}/sdk/sysroots/`uname -m`-oesdk-linux/usr/bin/arm-oemllib32-linux/arm-oemllib32-linux-ld
export AR=${RIVIERA_TOOLCHAIN}/sdk/sysroots/`uname -m`-oesdk-linux/usr/bin/arm-oemllib32-linux/arm-oemllib32-linux-ar
export RANLIB=${RIVIERA_TOOLCHAIN}/sdk/sysroots/`uname -m`-oesdk-linux/usr/bin/arm-oemllib32-linux/arm-oemllib32-linux-ranlib
export INCLUDE=${RIVIERA_TOOLCHAIN}/sdk/sysroots/aarch64-oe-linux/usr/include

export COMMON_FLAGS="--sysroot=${SYSROOT} -mtune=cortex-a53 -ftree-vectorize  -I${INCLUDE}"
export CFLAGS="-std=c99"
export CXX_FLAGS="-std=c++11"
