make KOS_CFLAGS="$KOS_CFLAGS -I$KOS_BASE/libc/include -I$KOS_BASE/libm/common -I$KOS_BASE/include/newlib-libm-sh4 -I$KOS_BASE/kernel/arch/$KOS_ARCH/include -I$KOS_BASE/include -D_arch_dreamcast" $@
