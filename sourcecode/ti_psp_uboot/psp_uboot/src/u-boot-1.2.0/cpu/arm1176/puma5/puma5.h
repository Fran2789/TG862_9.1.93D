/*
 * (C) Copyright 2006-2007
 * Texas Instruments.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef _INCLUDE_PUMA5_H
#define _INCLUDE_PUMA5_H

/* PLL settings */
#define PLL_ENABLE_MASK         0x00000001      // PLLEN=1
#define PLL_BYPASS_MASK_N       0xFFFFFFFE
#define PLL_PWRDN_MASK          0x00000002
#define PLL_PWRUP_MASK_N        0xFFFFFFFD
#define PLL_RESET_MASK_N        0xFFFFFFF7      // PLLRST=0
#define PLL_UNRESET_MASK        0x00000008

#define ARMPLL_PLLM             15              // Multiplier=15+1
#define ARMPLL_PREDIV            0              // Prediv=0+1

#define PLL_PREDIV_MASK_N       0xFFFF7FE0

#define ARMPLL_PLLDIV1          0               // ARM11 0+1
#define ARMPLL_PLLDIV2          3               // VLYNQ 3+1
#define ARMPLL_PLLDIV3          1               // Full=1+1
#define ARMPLL_PLLDIV4          3               // Slows=3+1
#define ARMPLL_PLLDIV5          0               // DDR PHY=0+1
#define ARMPLL_PLLDIV6          1               // Packet Processor 1+1
#define ARMPLL_PLLDIV7          7               // Serial Flash 7+1 (50MHz)
#define ARMPLL_PLLDIV7_BOOST    4               // Serial Flash 4+1 (80MHz) for capabale devices

#define PLL_DIV_MASK_N          0xFFFF7FE0

#define REG_ARMPLL_PLLCTL       0x08620100
#define REG_ARMPLL_PLLM         0x08620110
#define REG_ARMPLL_PREDIV       0x08620114
#define REG_ARMPLL_PLLDIV1      0x08620118
#define REG_ARMPLL_PLLDIV2      0x0862011C
#define REG_ARMPLL_PLLDIV3      0x08620120
#define REG_ARMPLL_PLLDIV4      0x08620160
#define REG_ARMPLL_PLLDIV5      0x08620164
#define REG_ARMPLL_PLLDIV6      0x08620168
#define REG_ARMPLL_PLLDIV7      0x0862016C
#define REG_ARMPLL_OSCDIV1      0x08620124
#define REG_ARMPLL_PLLCMD       0x08620138
#define REG_ARMPLL_PLLSTAT      0x0862013C


#define GMIIPLL_DISABLE_MASK    0x00000001
#define GMIIPLL_ENABLE_MASK_N   0xFFFFFFFE
#define GMIIPLL_PWRDN_MASK      0x00000002
#define GMIIPLL_BYPASS_MASK_N   0xFFFFFFFB
#define GMIIPLL_UNBYPASS_MASK   0x00000004
#define GMIIPLL_RESET_MASK_N    0xFFFFFFF7
#define GMIIPLL_UNRESET_MASK    0x00000008

/* !@@ TODO: Check if values result in 125 MHz */
#define GMIIPLL_PLLMULT         4   // 4+1
#define GMIIPLL_PLLDIV          0   // 0+1

#define REG_GMIIPLL_PLLMULT     0x08620200
#define REG_GMIIPLL_PLLDIV      0x08620204
#define REG_GMIIPLL_PLLCTL      0x08620208

#define EXTPHY_RESET_DEFAULT_INDEX                  0
#define EXTPHY_RESET_TNETC550_GPIO_NUM              14
#define EXTPHY_RESET_TNETC950_GPIO_NUM              32
#define EXTPHY_RESET_TNETC958_GPIO_NUM              23
#define EXTPHY_RESET_TNETC552_GPIO_NUM              9
#define EXTPHY_RESET_ARRIS_GATEWAY_GPIO_NUM         60

/* PSC module index */
#define LPSC_DDRPHY         4
#define LPSC_DDREMIF        5
#define LPSC_UART0          12  // Slows
#define LPSC_UART1          13  // Slows
#define LPSC_GPIO           14
#define LPSC_TIMER0         15
#define LPSC_SRCLK0         24
#define LPSC_SRCLK1         25
#define LPSC_SRCLK2         26
#define LPSC_SRCLK3         27
#define LPSC_CPPI41         28  // SR CLK 4
#define LPSC_SRCLK5         29
#define LPSC_CPGMAC         30
#define LPSC_MDIO           31
#define LPSC_EMIF_VRST      36

#define PSCMDCTL_BASE           0x08621A00
#define REG_PSCMDCTL(moduleid)  (PSCMDCTL_BASE + 4*moduleid)
#define REG_PSCMDCTL_DDRPHY     0x08621A10
#define REG_PSCMDCTL_DDREMIF    0x08621A14
#define REG_PSCMDCTL_EMIFVRST   0x08621A90
#define REG_PSCPTCMD            0x08621120
#define REG_PSCPTSTAT           0x08621128


/* Pull Up/Down Reg */
#define REG_PUDCR0              0x08611B60
#define CLK_OUT0_ENABLE_MASK_N  0xFBFFFFFF
#define CLK_OUT0_DISABLE_MASK   0x04000000


/* GPIO */
#define REG_GPIOEN              0x0861090C
#define REG_GPIODIR             0x08610908
#define REG_GPIOIN              0x08610900
#define REG_GPIOOUT             0x08610904

/* AUX GPIO */
#define REG_AUX_GPIOEN          0x08610934
#define REG_AUX_GPIODIR         0x08610930
#define REG_AUX_GPIOIN          0x08610928
#define REG_AUX_GPIOOUT         0x0861092C

/* Unlock CFG MMR region */
#define PUMA5_BOOTCFG_KICK_0    0x08611A38
#define PUMA5_BOOTCFG_KICK_1    0x08611A3C

#define PUMA5_BOOTCFG_KICK_0_VAL         0x83E70B13
#define PUMA5_BOOTCFG_KICK_1_VAL         0x95A4F1E0

/* PinMux0 Register */
#define REG_PIN_MUX0            0x08611B10

/* PinMux0 MPEG Out/In settings */
#define MPEG_OUT_EN_BIT         2
#define MPEG_IN_EN_BIT          1

#define EMIF_DDR_DUAL_MODE      0x00000002

/* DDR CR for EMIF DDR ASYNC mode */
#define REG_DDR_CR              0x08611B28
#define EMIF_DDR_DUAL_MODE      0x00000002

/* MM-SPI */
#define REG_MM_SPI_SETUP0       0x08612518
#define MM_SPI_SETUP0_FR_VAL    0x2060B
#define MM_SPI_SETUP0_DFR_VAL   0x2163B
#define REG_MM_SPI_SETUP1       0x0861251C
#define MM_SPI_SETUP1_FR_VAL    0x2060B
#define MM_SPI_SETUP1_DFR_VAL   0x2163B

/* Interface power control */
#define REG_IO_PDCR             0x08611B30
#define CLK_OUT0_PWRDN          0x00001000

#define REG_ETH_CR              0x08611B38
#define ETHCR_GIG_ENABLE        0x2
#define ETHCR_FD_ENABLE         0x1


/* Watchdog Timer */
#define REG_WT_KICK_LOCK        0x08611F00
#define REG_WT_KICK             0x08611F04
#define REG_WT_CHNG_LOCK        0x08611F08
#define REG_WT_CHNG             0x08611F0C
#define REG_WT_DISABLE_LOCK     0x08611F10
#define REG_WT_DISABLE          0x08611F14

#define WT_CHNG_UNLOCK_WORD1    0x6666
#define WT_CHNG_UNLOCK_WORD2    0xBBBB
#define WT_DISABLE_UNLOCK_WORD1 0x7777
#define WT_DISABLE_UNLOCK_WORD2 0xCCCC
#define WT_DISABLE_UNLOCK_WORD3 0xDDDD

#define WT_STATE_DISABLED       0
#define WT_STATE_ENABLED        1

#endif /* !_INCLUDE_PUMA5_H */
