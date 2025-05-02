#! /bin/sh

int_handler () {
    echo INTERRUPTED
    repeat=0
}

#trap int_handler SIGINT

repeat=1

while [ $repeat -eq 1 ]
do
	i=0
	while [ $i -lt 100 ]; do
		echo br.
		i=$(($i + 1))
	done 
	sleep 3
done | ./hostsolver 0 2 127.0.0.1 2>/dev/null
