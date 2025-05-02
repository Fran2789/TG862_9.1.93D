#! /bin/sh
#
# $Id: ruli_sync_leaking.sh,v 1.3 2003/01/23 05:36:05 evertonm Exp $

me=`basename $0`

if [ $# -ne 0 ]; then
	cat >&2 <<__EOF__
usage: $me
__EOF__
	exit
fi

ns=$1

show_domains () {

	cat <<__EOF__
cname-target.ruli
foreign-target.ruli
locaweb.com.br
savannah.gnu.org
sf.net
www.google.com
www.microsoft.com
www.uol.com.br
www.msn.com
www.cisco.com
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
done | ./syncsolver

