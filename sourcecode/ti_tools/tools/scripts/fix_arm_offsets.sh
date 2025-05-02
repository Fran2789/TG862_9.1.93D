#! /bin/sh

########################################################
#
#   Create utility for hexadecimal add operation
#

cat > $1_add.c <<_ACEOF
#include <stdio.h>

int main ()
{
    unsigned int   val1;
    unsigned int   val2;
    scanf("0x%x 0x%x",&val1, &val2);
    printf("0x%x", val1 + val2);

    return 0;
}
_ACEOF

gcc $1_add.c -o $1_add.bin
########################################################

########################################################
#
#   Create utility for hexadecimal substitute operation
#

cat > $1_sub.c <<_ACEOF
#include <stdio.h>

int main ()
{
    unsigned int   val1;
    unsigned int   val2;
    scanf("0x%x 0x%x",&val1, &val2);
    printf("0x%x", val1 - val2);

    return 0;
}
_ACEOF

gcc $1_sub.c -o $1_sub.bin
########################################################

########################################################
#
#   Remove all the absolute addresses that we are going to re-calculate
#
cat $2 | grep -v "CONFIG_ARM_AVALANCHE_KERNEL_.*_ADDRESS" | grep -v "CONFIG_ARM_AVALANCHE_SDRAM_ADDRESS" | grep -v "CONFIG_UBOOT_SDRAM_ADDRESS" > $2.tmp
mv  $2.tmp $2
#
########################################################

########################################################
#
#   Read DDR Base address and ARM start offset in it
#
ddr_phys_base=`cat $2 | grep CONFIG_ARM_AVALANCHE_SDRAM_PHYS_ADDRESS | sed 's/CONFIG_ARM_AVALANCHE_SDRAM_PHYS_ADDRESS=//g'`
ddr_phys_arm_start_offset=`cat $2 | grep CONFIG_ARM_AVALANCHE_SDRAM_PHYS_OFFSET  | sed 's/CONFIG_ARM_AVALANCHE_SDRAM_PHYS_OFFSET=//g'`
#
########################################################

########################################################
#
#   Read all the ARM related offsets
#
arm_offset2=`cat $2 | grep CONFIG_ARM_AVALANCHE_KERNEL_ZRELADDR_OFFSET  | sed 's/CONFIG_ARM_AVALANCHE_KERNEL_ZRELADDR_OFFSET=//g'`
arm_offset3=`cat $2 | grep CONFIG_ARM_AVALANCHE_KERNEL_INITRD_OFFSET    | sed 's/CONFIG_ARM_AVALANCHE_KERNEL_INITRD_OFFSET=//g'`
arm_offset4=`cat $2 | grep CONFIG_ARM_AVALANCHE_KERNEL_PARAMS_OFFSET    | sed 's/CONFIG_ARM_AVALANCHE_KERNEL_PARAMS_OFFSET=//g'`
arm_offset5=`cat $2 | grep CONFIG_UBOOT_SDRAM_OFFSET                    | sed 's/CONFIG_UBOOT_SDRAM_OFFSET=//g'`
#
########################################################

########################################################
#
#   Re-Calculate the offsets
#
ddr_phys_arm_start_base=`echo $ddr_phys_arm_start_offset $ddr_phys_base | $PWD/$1_add.bin`
addr2=`echo $ddr_phys_arm_start_base $arm_offset2 | $PWD/$1_add.bin`
addr3=`echo $ddr_phys_arm_start_base $arm_offset3 | $PWD/$1_add.bin`
addr4=`echo $ddr_phys_arm_start_base $arm_offset4 | $PWD/$1_add.bin`
addr5=`echo $ddr_phys_arm_start_base $arm_offset5 | $PWD/$1_add.bin`

#
########################################################


echo After relocation:
echo CONFIG_ARM_AVALANCHE_SDRAM_ADDRESS=$ddr_phys_arm_start_base
echo CONFIG_ARM_AVALANCHE_KERNEL_ZRELADDR_ADDRESS=$addr2
echo CONFIG_ARM_AVALANCHE_KERNEL_INITRD_ADDRESS=$addr3
echo CONFIG_ARM_AVALANCHE_KERNEL_PARAMS_ADDRESS=$addr4
echo CONFIG_UBOOT_SDRAM_ADDRESS=$addr5


########################################################
#
#   Add the new variables back into configuration file
#
echo CONFIG_ARM_AVALANCHE_SDRAM_ADDRESS=$ddr_phys_arm_start_base >> $2
echo CONFIG_ARM_AVALANCHE_KERNEL_ZRELADDR_ADDRESS=$addr2         >> $2
echo CONFIG_ARM_AVALANCHE_KERNEL_INITRD_ADDRESS=$addr3           >> $2
echo CONFIG_ARM_AVALANCHE_KERNEL_PARAMS_ADDRESS=$addr4           >> $2
echo CONFIG_UBOOT_SDRAM_ADDRESS=$addr5                           >> $2
#
########################################################


########################################################
#
#   Remove all the temporary garbage ...
#
rm -rf $1_*
#
########################################################

