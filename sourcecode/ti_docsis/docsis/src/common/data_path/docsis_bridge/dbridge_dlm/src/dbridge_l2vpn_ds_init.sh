#!/bin/sh

L2VPN_DS_CDEV=docsis_dbridge_l2vpn_ds

L2VPN_DS_MAJOR=`grep ${L2VPN_DS_CDEV} /proc/devices | cut -f 1 -d ' '`
mknod /dev/${L2VPN_DS_CDEV}0 c $L2VPN_DS_MAJOR 0

