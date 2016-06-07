#!/bin/bash

THREADS=(1 2 4 6 8 10 12 14 16 18 20 24 28 32)
LOCK=("m" "s" "c")
#for l_index in `seq 0 2`; do
#	for i in `seq 0 13`;do
	#	echo "./lab2a --threads=${THREADS[$i]} --iterations=1000 --sync=${LOCK[$l_index]}"
	#	./lab2a --threads=${THREADS[$i]} --iterations=1000 --sync=${LOCK[$l_index]}
	#done
#done

for j in `seq 0 13`; do
	echo "./lab2a --threads=${THREADS[$j]} --iterations=1000"
	./lab2a --threads=${THREADS[$j]} --iterations=1000
done
