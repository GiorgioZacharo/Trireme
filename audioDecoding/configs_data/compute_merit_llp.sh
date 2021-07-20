#!/bin/bash

rm MC_HPVM_LLP.txt

BENCH=$1
#BENCH=edge
#ALPHA=0.002  	#  Based on Memory Hierarchy of the Architecture.
ALPHA=$2  	#  Based on Memory Hierarchy of the Architecture.
OVERHEAD=$3	# Overhead per invocation. #decode
#OVERHEAD=100	# Overhead per invocation. #decode
#OVERHEAD=10	# Overhead per invocation.

while read FUNC_NAME SW_LATENCY HW_LATENCY AREA INVOCATIONS ; do # LA.txt
	
     while read FUNC_NAME_IO INPUT OUTPUT; do     # IO.txt

	if [ $FUNC_NAME = $FUNC_NAME_IO ]; then

		IO_LATENCY_HW=$(echo "scale=2;$ALPHA * $INPUT * $INVOCATIONS" | bc -l)
		printf "%s HW: %d IO: %.5f \n " "$FUNC_NAME"  "$HW_LATENCY" "$IO_LATENCY_HW" 
		
		TOT_OVERHEAD=$(( OVERHEAD * INVOCATIONS ))
		HW_LATENCY_TOT=$(( HW_LATENCY * INVOCATIONS ))

		MERIT=$(echo "scale=2; $SW_LATENCY - $HW_LATENCY_TOT - $TOT_OVERHEAD - $IO_LATENCY_HW" | bc -l )

		#if [ $(echo "$IO_LATENCY_HW > $HW_LATENCY_TOT" | bc -l) ] ; then
		#	MERIT=$(echo "scale=2; $SW_LATENCY - $TOT_OVERHEAD - $IO_LATENCY_HW" | bc -l )
		#else
		#	MERIT=$(echo "scale=2; $SW_LATENCY - $TOT_OVERHEAD - $HW_LATENCY_HW" | bc -l )
		#fi
		
		MERIT=$(echo "scale=2; $SW_LATENCY - $TOT_OVERHEAD - $HW_LATENCY_TOT - $IO_LATENCY_HW" | bc -l )

		if [ $INVOCATIONS -eq 0 ]; then
			MERIT=0
		fi
		
		printf "$BENCH\t$FUNC_NAME\t$MERIT\t$INPUT\t$AREA\t$INVOCATIONS\n" >> MC_HPVM_LLP.txt
	fi

    done<IO_HPVM_LLP.txt
	
  
done<LA_HPVM_LLP.txt

echo
sort -k3 -n -r MC_HPVM_LLP.txt

