DC_ARM_BASE=/usr/local/cross-tools/arm-elf
DC_ARM_PREFIX=arm-elf

export DC_ARM_CC="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-gcc"
export DC_ARM_AS="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-as"
export DC_ARM_AR="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-ar"
export DC_ARM_OBJCOPY="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-objcopy"
export DC_ARM_LD="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-ld"
export DC_ARM_CFLAGS="-mcpu=arm7 -Wall -O2"
export DC_ARM_AFLAGS="-marm7"

make KOS_CFLAGS="$KOS_CFLAGS -I$KOS_BASE/libc/include -I$KOS_BASE/libm/common -I$KOS_BASE/include/newlib-libm-sh4 -I$KOS_BASE/kernel/arch/$KOS_ARCH/include -I$KOS_BASE/include -I$KOS_BASE/addons/include -I$KOS_BASE/addons/lwip/lwip/src/include -I$KOS_BASE/addons/lwip/kos/include -I$KOS_BASE/addons/lwip/lwip/src/include/ipv4 -I$KOS_BASE/addons/liboggvorbis/liboggvorbis/libvorbis/include -I$KOS_BASE/addons/liboggvorbis/liboggvorbis/libvorbis/lib -D_arch_dreamcast" $@
