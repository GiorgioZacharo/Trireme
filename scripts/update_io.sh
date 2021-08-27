#!/bin/bash


cp IO.txt IO_HPVM_LLP.txt
LLP_MAX=8

while read FUNC_NAME LOOP_NESTING ITERATIONS ; do # parallel_loops.txt
	
     while read ACCEL_NAME INDEXES; do     # IO.txt

	if [ $FUNC_NAME = $ACCEL_NAME ]; then

		for (( LLP_FACTOR=2; $LLP_FACTOR<=$LLP_MAX;  )); do 
		
			printf "$ACCEL_NAME-$LLP_FACTOR\t$INDEXES\n" >> IO_HPVM_LLP.txt
		
		LLP_FACTOR=$(( LLP_FACTOR * 2 ))
		done

	fi

    done<IO.txt
	
  
done<parallel_loops.txt


