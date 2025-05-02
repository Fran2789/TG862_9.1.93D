/*
 * Copyright (C) 2008 Texas Instruments, Incorporated
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2.
 *
 * This program is distributed as is WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

1) LICENSE:

    This software package (package includes all files including the directory 
    .../masdk/gpl and below) is licensed subject to the GNU General Public
    License (GPL). Version 2, June 1991, available at 
    <http://www.gnu.org/licenses/gpl-2.0.html>. This is valid irrespective
    of any other or no copyright header might be present on these files. 

2) CONTENTS:

    This package contents three modules, the details were given below.

        I) MXP kernel modules [mxpmem.ko, mxpmod.ko]. This is a part of the MXP 
        module which provides OS abstraction for TI voice (Multimedia Application
        Services Development Kit) software. The source code path is .../gpl/src/mxp.

        II) DSP hardware module [hw_dspmod.ko]. The source code path is 
        .../gpl/src/dspmod.

        III) Telephony HAL module [telehal.ko]. The source code path is .../gpl/src/telehal.

    The common header files are in .../gpl/inc directory.

3) BUILD PROCEDURE:

    Following command in .../gpl directory builds all the above kernel modules 
    in respective directories. Assuming arm_v6_be_uclibc tool chain is in $PATH.

    make KERNEL_DIR={path to kernel source, e.g.: .../linux-2.6.18/src} \
                    CROSS_TOOLS=arm_v6_be_uclibc- ENDIAN=meb \
                    MASDK_PFORM={platform name, for e.g. pumaV} \
                    TOOL=sfgnu CPU=ARM11 

    Note: The kernel source needs to be patched for the platform SoC.
