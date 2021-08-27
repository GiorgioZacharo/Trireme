#!/bin/bash


cp FCI.txt FCI_HPVM_LLP.txt
cp FCI.txt FCI.txt.orig
LLP_MAX=8

while read FUNC_NAME LOOP_NESTING ITERATIONS ; do # parallel_loops.txt
	
     while read ACCEL_NAME INDEXES; do     # FCI.txt

	if [ $FUNC_NAME = $ACCEL_NAME ]; then

		for (( LLP_FACTOR=2; $LLP_FACTOR<=$LLP_MAX;  )); do 
		
			printf "$ACCEL_NAME-$LLP_FACTOR\t$INDEXES\n" >> FCI_HPVM_LLP.txt
		
		LLP_FACTOR=$(( LLP_FACTOR * 2 ))
		done

	fi

    done<FCI.txt
	
  
done<parallel_loops.txt


