DC_ARM_BASE=/usr/local/cross-tools/arm-elf
DC_ARM_PREFIX=arm-elf

export KOS_BASE=`pwd`

# KallistiOS environment variable settings
# This is a sample script; the 'configure' program can produce a full
#   script for you, but that's assuming that 'cdialog' works on your
#   system. If not, you may want to edit this script (for Bash) or 
#   environ.tcsh (for TCSH) and use that.
# This script is for the DC configuation only!
# $Id: environ-dc.sh.sample,v 1.4 2002/03/04 06:18:08 bardtx Exp $

# Build architecture
export KOS_ARCH="dreamcast"

# Compiler base strings
export KOS_CC_BASE="/opt/sh-dc-elf-4.4.1/"
export PATH=${PATH}:${KOS_CC_BASE}/bin
export KOS_CC_PREFIX="sh-dc-elf"

# KOS base paths
export KOS_INCS="${KOS_BASE}/include"

# Make utility
export KOS_MAKE="make"

# Load utility
export KOS_LOADER="dc-tool -x"

# Genromfs utility
export KOS_GENROMFS="${KOS_BASE}/utils/genromfs/genromfs"

# SH-4 GCC paths
export KOS_CC="${KOS_CC_BASE}/bin/${KOS_CC_PREFIX}-gcc"
export KOS_CCPLUS="${KOS_CC_BASE}/bin/${KOS_CC_PREFIX}-g++"
export KOS_AS="${KOS_CC_BASE}/bin/${KOS_CC_PREFIX}-as"
export KOS_AR="${KOS_CC_BASE}/bin/${KOS_CC_PREFIX}-ar"
export KOS_OBJCOPY="${KOS_CC_BASE}/bin/${KOS_CC_PREFIX}-objcopy"
export KOS_LD="${KOS_CC_BASE}/bin/${KOS_CC_PREFIX}-ld"
export KOS_STRIP="${KOS_CC_BASE}/bin/${KOS_CC_PREFIX}-strip"
# --> for gcc 2.95
#export KOS_CFLAGS="-Wall -g -ml -m4-single-only -Os -fno-builtin -fno-strict-aliasing -fomit-frame-pointer"
# --> for gcc 3.0.3
#export KOS_CFLAGS="-Wall -g -ml -m4-single-only -O3 -fno-builtin -fno-strict-aliasing -fomit-frame-pointer -fno-optimize-sibling-calls"
# --> for gcc 4.4.1
export KOS_CFLAGS="-Wall -g -ml -m4-single-only -O3 -fno-builtin -fno-strict-aliasing -fomit-frame-pointer"
export KOS_CPPFLAGS="-fno-operator-names -fno-rtti -fno-exceptions"
export KOS_AFLAGS="-little"
export KOS_LDFLAGS="-g -ml -m4-single-only -nostartfiles -nostdlib -Wl,-Ttext=0x8c010000"


export DC_ARM_CC="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-gcc"
export DC_ARM_AS="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-as"
export DC_ARM_AR="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-ar"
export DC_ARM_OBJCOPY="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-objcopy"
export DC_ARM_LD="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-ld"
export DC_ARM_CFLAGS="-mcpu=arm7 -Wall -O2"
export DC_ARM_AFLAGS="-marm7"

make KOS_CFLAGS="$KOS_CFLAGS -I$KOS_BASE/libc/include -I$KOS_BASE/libm/common -I$KOS_BASE/include/newlib-libm-sh4 -I$KOS_BASE/kernel/arch/$KOS_ARCH/include -I$KOS_BASE/include -I$KOS_BASE/addons/include -I$KOS_BASE/addons/lwip/lwip/src/include -I$KOS_BASE/addons/lwip/kos/include -I$KOS_BASE/addons/lwip/lwip/src/include/ipv4 -I$KOS_BASE/addons/liboggvorbis/liboggvorbis/libvorbis/include -I$KOS_BASE/addons/liboggvorbis/liboggvorbis/libvorbis/lib -D_arch_dreamcast" $@
