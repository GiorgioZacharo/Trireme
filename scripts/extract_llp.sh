#!/bin/bash


cp LA.txt LA_HPVM_LLP.txt
LLP_MAX=8

while read FUNC_NAME LOOP_NESTING ITERATIONS ; do # parallel_loops.txt
	
     while read ACCEL_NAME SW_LATENCY HW_LATENCY AREA INVOCATIONS; do     # LA.txt

	if [ $FUNC_NAME = $ACCEL_NAME ]; then

		for (( LLP_FACTOR=2; $LLP_FACTOR<=$LLP_MAX;  )); do 
			NEW_HW_LATENCY=$(echo "scale=0;$HW_LATENCY / $LLP_FACTOR" | bc -l)
			NEW_AREA=$(echo "scale=0;$AREA * $LLP_FACTOR" | bc -l)
		
			printf "$ACCEL_NAME-$LLP_FACTOR\t$SW_LATENCY\t$NEW_HW_LATENCY\t$NEW_AREA\t$INVOCATIONS\n" >> LA_HPVM_LLP.txt
		
		LLP_FACTOR=$(( LLP_FACTOR * 2 ))
		done

	fi

    done<LA.txt
	
  
done<parallel_loops.txt

echo
sort -k3 -n -r LA_HPVM_LLP.txt

