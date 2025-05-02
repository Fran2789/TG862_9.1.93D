#! /bin/sh
#
# debug memory leak for ruli_srv_t
#
# $Id: ss.sh,v 1.1 2003/01/09 07:49:51 evertonm Exp $

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
kensingtonlabs.com
vanrein.org
xxx
bogus
__EOF__

}

show_domains | while read d; do

    echo _http._tcp.$d | ./srvsolver2 0 10 127.0.0.1 127.0.0.1 2> x

    grep malloc x | awk '{ print $4 }' | sort > x.1

    grep free x | awk '{ print $4 }' | sort > x.2

    diff x.1 x.2 > y

    cat y

    grep addr y | awk '{ print $2 }' | while read i; do
	grep $i x
    done

done


