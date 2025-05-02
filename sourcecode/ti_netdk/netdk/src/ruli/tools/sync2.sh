#! /bin/sh
#
# debug memory leak for ruli_sync_t
#
# $Id: sync2.sh,v 1.1 2003/01/09 07:59:45 evertonm Exp $

do_cat () {

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
kensingtonlabs.com
vanrein.org
xxx
bogus
__EOF__

}

show_domains () {

    do_cat
    do_cat
    do_cat

}


show_domains | while read d
do
    echo _http._tcp.$d
done | ./syncsolver 2> x

grep malloc x | awk '{ print $4 }' | sort > x.1

grep free x | awk '{ print $4 }' | sort > x.2

diff x.1 x.2 > y

cat y

grep addr y | awk '{ print $2 }' | while read i; do
    grep $i x
done

