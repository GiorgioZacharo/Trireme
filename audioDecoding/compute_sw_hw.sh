#!/bin/bash

rm SW_HW.txt SW_HW_AREA.txt INV_IO_OVHD.txt

BENCH=H264

#ALPHA=0.002  	#  Based on Memory Hierarchy of the Architecture.
#ALPHA=300  	#  Based on Memory Hierarchy of the Architecture. #decode_tlp 4k
#OVERHEAD=8000	# Overhead per invocation. #decodei_tlp
#OVERHEAD=100	# Overhead per invocation. #decode
ALPHA=$1        #  Based on Memory Hierarchy of the Architecture.
OVERHEAD=$2     # Overhead per invocation. #decode


while read FUNC_NAME SW_LATENCY HW_LATENCY AREA INVOCATIONS ; do # LA.txt
	
     while read FUNC_NAME_IO INPUT OUTPUT; do     # IO.txt

	if [ $FUNC_NAME = $FUNC_NAME_IO ]; then

		if [ $INVOCATIONS -gt 0 ]; then
	        	IO_LATENCY_HW=$(echo "scale=2;$ALPHA * $INPUT * $INVOCATIONS" | bc -l)
               	 	printf "%s HW: %d IO: %.5f \n " "$FUNC_NAME"  "$HW_LATENCY" "$IO_LATENCY_HW"

                	TOT_OVERHEAD=$(( OVERHEAD * INVOCATIONS ))
                	HW_LATENCY_TOT=$(( HW_LATENCY * INVOCATIONS ))
			HW_LATENCY_TOTAL=$(echo "scale=2; $TOT_OVERHEAD + $HW_LATENCY_TOT + $IO_LATENCY_HW" | bc -l )
			MERIT=$(echo "scale=2; $SW_LATENCY - $HW_LATENCY_TOTAL" | bc -l )
			NEW_TIME=$(echo "scale=2; $SW_LATENCY - $MERIT" | bc -l )
			SPEEDUP=$(echo "scale=2; $SW_LATENCY / $NEW_TIME" | bc -l )

		fi
		
		#printf "$FUNC_NAME\t$SW_LATENCY\t$HW_LATENCY_TOTAL\t$MERIT\t$SPEEDUP\n" >> SW_HW.txt
		printf "$FUNC_NAME\t$SW_LATENCY\t$HW_LATENCY_TOTAL\n" >> SW_HW.txt
		printf "$FUNC_NAME\t$SW_LATENCY\t$HW_LATENCY_TOTAL\t$AREA\n" >> SW_HW_AREA.txt
		printf "$FUNC_NAME\t$IO_LATENCY_HW\t$TOT_OVERHEAD\n" >> INV_IO_OVHD.txt
	fi

    done<IO.txt
	
done<LA.LLVM.txt

echo
sort -k2 -n -r SW_HW.txt

