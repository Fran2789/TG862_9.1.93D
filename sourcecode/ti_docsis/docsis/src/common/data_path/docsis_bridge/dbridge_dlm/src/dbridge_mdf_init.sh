#!/bin/sh

MDF_CDEV=docsis_dbridge_mdf

MDF_MAJOR=`grep ${MDF_CDEV} /proc/devices | cut -f 1 -d ' '`
mknod /dev/${MDF_CDEV}0 c $MDF_MAJOR 0

