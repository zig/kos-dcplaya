# KallistiOS ##version##
#
# kernel/fs/Makefile
# (c)2000-2001 Dan Potter
#   
# $Id: Makefile,v 1.3 2002/08/13 04:54:22 bardtx Exp $

OBJS = fs.o fs_romdisk.o fs_ramdisk.o fs_pty.o
OBJS += fs_utils.o elf.o
SUBDIRS = 

# fix bizarre gcc 4.4.1 bug (?)
fs.o: FIX_GCC_BUG = -fno-inline-functions

include $(KOS_BASE)/Makefile.prefab

