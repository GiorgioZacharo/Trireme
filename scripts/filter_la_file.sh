#!/bin/bash

sort -k2 -n -r LA.txt > LA.sorted.txt
MAX_PERCENTAGE=0.02

flag=0	
     while read ACCEL_NAME SW_LATENCY HW_LATENCY AREA INVOCATIONS; do     # LA.txt
	
	if [ $flag -eq 0 ]; then
	MAX=$SW_LATENCY
	THRESHOLD=$(echo "scale=2;$MAX * $MAX_PERCENTAGE" | bc -l) 
	INT_THRESHOLD=${THRESHOLD%.*}
	echo $THRESHOLD
	fi

	flag=1
	#if [ $INVOCATIONS -gt 0 ]; then
	if [ $INVOCATIONS -gt 0 ] && [ $SW_LATENCY -gt $INT_THRESHOLD ]; then
		printf "$ACCEL_NAME\t$SW_LATENCY\t$HW_LATENCY\t$AREA\t$INVOCATIONS\n" >> LA_filtered.txt
	fi

    done<LA.sorted.txt

mv LA_filtered.txt LA.txt
	
