#!/bin/bash

rm SW_HW.txt 

#SCRIPTS_DIR=scripts

ALPHA=0.1        #  Based on Memory Hierarchy of the Architecture.
OVERHEAD=100     # Overhead per invocation. #decode


while read FUNC_NAME SW_LATENCY HW_LATENCY AREA INVOCATIONS ; do # LA.txt
	
     while read FUNC_NAME_IO INPUT OUTPUT; do     # IO.txt

	if [ $FUNC_NAME = $FUNC_NAME_IO ]; then

		if [ $INVOCATIONS -gt 0 ]; then
	        	IO_LATENCY_HW=$(echo "scale=2;$ALPHA * $INPUT * $INVOCATIONS" | bc -l)
               	 	printf "%s HW: %d IO: %.5f \n " "$FUNC_NAME"  "$HW_LATENCY" "$IO_LATENCY_HW"

                	TOT_OVERHEAD=$(( OVERHEAD * INVOCATIONS ))
                	HW_LATENCY_TOT=$(( HW_LATENCY * INVOCATIONS ))
			HW_LATENCY_TOTAL=$(echo "scale=2; $TOT_OVERHEAD + $HW_LATENCY_TOT + $IO_LATENCY_HW" | bc -l )

		fi
		
		printf "$FUNC_NAME $SW_LATENCY $HW_LATENCY_TOTAL\n" >> SW_HW.txt
	fi

    done<IO.txt
	
done<LA.txt

echo
sort -k2 -n -r SW_HW.txt

$SCRIPTS_DIR/remove_fractional_point.sh SW_HW.txt
rm SW_HW.txt.orig
