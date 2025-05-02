#! /bin/sh
#
# $Id: ruli_srv_target_leaking.sh,v 1.1 2003/01/11 22:48:31 evertonm Exp $

me=`basename $0`

if [ $# -lt 1 ]; then
	cat >&2 <<__EOF__
usage: $me name-server-list

example: $me 127.0.0.1 192.168.0.1
__EOF__
	exit
fi

ns_list=$*

show_domains () {

	cat <<__EOF__
bad-target.ruli
cname-target.ruli
foreign-target.ruli
__EOF__

}

while :; do show_domains |
    while read d 
    do
	echo _http._tcp.$d
    done
done | ./srvsolver2 0 1 $ns_list

