#!/bin/bash
dir=$(ls)
for d in ${dir[@]}; do
	if [ $d == 'linux-6.12.75.tar' -o $d == 'linux-6.12.75' ]; then
		echo "skip $d"
		echo
		continue
	fi
        if [ $d == 'clean.sh' -o $d == 'lost+found' ]; then 
		echo "skip $d"
		echo
		continue
	fi
	echo "cleaning $d..."
	cd $d; make clean; cd ..
	echo "cleaning done"
	echo
done
