/*
 * (C) Copyright 2013 Linaro
 * Andre Przywara, <andre.przywara@linaro.org>
 *
 * Configuration for Versatile Express. Parts were derived from other ARM
 *   configurations.
 *
 * SPDX-License-Identifier:    GPL-2.0+
 */

#ifndef __VEXPRESS_CA15X2_TC2_h
#define __VEXPRESS_CA15X2_TC2_h

#define CONFIG_VEXPRESS_EXTENDED_MEMORY_MAP
#include "vexpress_common.h"
#define CONFIG_BOOTP_VCI_STRING     "U-boot.armv7.vexpress_ca15x2_tc2"

#define CONFIG_SYSFLAGS_ADDR    0x1c010030
#define CONFIG_SMP_PEN_ADDR CONFIG_SYSFLAGS_ADDR

#define CONFIG_ARMV7_VIRT

#define CONFIG_BOOTCOMMAND "cp 0xc000000 0xf0000000 0xcc68;cp 0xc100000 0xa0000000 0x628604;cp 0x3000000 0xd0000000 0x101a44;go 0xf000004c"

#endif