#! /bin/sh
#
# $Id: ruli_srv_leaking.sh,v 1.3 2003/01/11 06:12:35 evertonm Exp $

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
savannah.gnu.org
sf.net
google.com
microsoft.com
uol.com.br
msn.com
cisco.com
freshmeat.net
slashdot.org
localhost
kensingtonlabs.com
vanrein.org
xxx
bogus
__EOF__

}

while :; do show_domains |
    while read d 
    do
	echo _http._tcp.$d _smtp._tcp.$d
    done
done | ./srvsolver2 0 10 $ns_list

