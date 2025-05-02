#!/bin/sh

FC_CDEV=docsis_fltr_class
FC_MAJOR=`grep ${FC_CDEV} /proc/devices | cut -f 1 -d ' '`
mknod /dev/${FC_CDEV}0 c $FC_MAJOR 0

