#! /bin/sh
#
# $Id: ruli_res_leaking.sh,v 1.2 2003/01/09 04:34:20 evertonm Exp $

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
www.google.com
www.microsoft.com
www.uol.com.br
www.msn.com
www.cisco.com
freshmeat.net
slashdot.org
localhost
xxx
bogus
__EOF__

}

while :; do show_domains; done | ./addrsolver 0 10 $ns_list
