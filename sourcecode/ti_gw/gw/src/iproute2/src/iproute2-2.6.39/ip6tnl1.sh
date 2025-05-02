#!/bin/sh
if [ -z "$1" ]; then 
    echo usage: $0 remoteIpv6Address
    exit
fi
Local=`ifconfig erouter0 | grep "inet6 addr:" | grep -v 'fe80' | awk '{ print $3}'`;
Via=`ip -6 route show | grep default | grep erouter0 | awk '{ print $3}'`
ip6tnl1Address=`echo $Local | sed -e 's/./4/'`
#Enable IPv6 forwarding
echo "1" > /proc/sys/net/ipv6/conf/erouter0/forwarding
#add ipv6 default routing rule for device erouter0 through CMTS
temp=`ip -6 route add default via $Via dev erouter0`
#create tunnel between br0/eth0 and erouter0
temp=`ip -6 tunnel add ip6tnl1 mode ip4ip6 remote $1 local $Local dev erouter0`
#tunnel up
temp=`ip link set dev ip6tnl1 txqueuelen 1000 up`
temp=`ip -6 addr add $ip6tnl1Address dev ip6tnl1`
temp=`ifconfig ip6tnl1 0.0.0.0`
echo "1" > /proc/sys/net/ipv6/conf/ip6tnl1/forwarding
#delete ipv4 default rule from routing table
temp=`ip route del table 3 | ip route show table 3 | grep default`
#add ipv4 default rule to route all to ip6tnl1 device
temp=`ip route add table 3 default dev ip6tnl1`


