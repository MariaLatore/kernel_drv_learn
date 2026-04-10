#!/bin/bash
dir=$(ls -I 'linux*' -I 'clean*' -I 'lost*' -F| grep '/')
for d in ${dir[@]}; do
	echo "cleaning $d..."
	cd $d; make clean; cd ..
	echo "cleaning done"
	echo
done
