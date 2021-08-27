#!/bin/bash

BENCH="${1:-APP_NAME}"

rm cols.txt MCI_tlp*.txt 
awk -F'\t' '{print NF;}' parallel_tasks.txt > cols.txt

# Calculate Max Column number
temp=`awk '$0>x{x=$0};END{print x}' cols.txt`
temp=$(( temp - 1 ))
echo $temp


IFS='	'
#while IFS='\t' read -r COL1 COL2 COL3; do
#read -a headers < parallel_tasks.txt
#while read -a LINE; do
#    for i in "${!LINE[@]}"; do
#	if [ $i -gt 0 ]; then 
#        echo "${LINE[0]}: ${LINE[i]}"
#
#	fi
#    done
#done<parallel_tasks.txt

while read -a LINE; do
    for i in "${!LINE[@]}"; do
        if [ $i -gt 0 ]; then # Exlude itself
          echo "${LINE[0]}: ${LINE[i]}"
	
	SW_FUN1=0
	SW_FUN2=0		
	HW_FUN1=0
	HW_FUN2=0
	AREA_FUN1=0
	AREA_FUN2=0

	SW_EST_FUN1=0
	SW_EST_FUN2=0
	HW_EST_FUN1=0
	HW_EST_FUN2=0

	IND_FUN1=
	IND_FUN2=

	# Reads the SW and HW Latencies from the AccelSeeker Analysis/Estimation
	  while read FUNC_NAME SW HW AREA; do
	    if [ $FUNC_NAME = ${LINE[0]} ];then	
		SW_FUN1=$SW
		HW_FUN1=$HW
		AREA_FUN1=$AREA
	     fi

	     if [ $FUNC_NAME = ${LINE[i]} ];then
                SW_FUN2=$SW
                HW_FUN2=$HW
		AREA_FUN2=$AREA
             fi
 	  done<SW_HW_AREA.txt

        # Reads the Earliest Starting Times (EST) SW and HW Latencies from the DFG traversal.
          while read FUNC_NAME SW HW; do
            if [ $FUNC_NAME = ${LINE[0]} ];then 
               SW_EST_FUN1=$SW
               HW_EST_FUN1=$HW
             fi 

            if [ $FUNC_NAME = ${LINE[i]} ];then 
               SW_EST_FUN2=$SW
               HW_EST_FUN2=$HW
             fi 
          done<earliest_start.txt

        # Reads the Indexes of the tasks.
          while IFS=' '  read BENCHMARK FUNC_NAME M C IND ; do
	 #	echo FUNC: $BENCH $FUNC
            if [ $FUNC_NAME = ${LINE[0]} ];then
               IND_FUN1=$IND
             fi

            if [ $FUNC_NAME = ${LINE[i]} ];then
               IND_FUN2=$IND
             fi
          done<MCI.txt
	
	MAX_HW=$(( HW_FUN1 > HW_FUN2 ? HW_FUN1 : HW_FUN2 ))
	SUM_SW=$(echo "scale=0;$SW_FUN1 + $SW_FUN2" | bc -l)
	OVERHEAD_SW=$(( SW_EST_FUN1 > SW_EST_FUN2 ? SW_EST_FUN1 - SW_EST_FUN2 : SW_EST_FUN2 - SW_EST_FUN1 ))
	OVERHEAD_HW=$(( HW_EST_FUN1 > HW_EST_FUN2 ? HW_EST_FUN1 - HW_EST_FUN2 : HW_EST_FUN2 - HW_EST_FUN1 ))
	echo "SW Overhead: $OVERHEAD_SW"
	echo "HW Overhead: $OVERHEAD_HW"
	MERIT=$(echo "scale=0;$SUM_SW - $MAX_HW" | bc -l)
	MERIT_CON=$(echo "scale=0;$SUM_SW - $MAX_HW - $OVERHEAD_SW" | bc -l) # Merit Conservative Approach (Assuming prion SW-only Implementations)
	MERIT_OPT=$(echo "scale=0;$SUM_SW - $MAX_HW - $OVERHEAD_HW" | bc -l) # Merit Optimistic Approach (Assuming prion HW-only Implementations)
	AREA_TOTAL=$(echo "scale=0;$AREA_FUN1 + $AREA_FUN2" | bc -l) # Area total calculation
	

	printf "$MERIT \n$MERIT_CON \n$MERIT_OPT\n\n"
	printf "$BENCH ${LINE[0]}.${LINE[i]} $MERIT_CON $AREA_TOTAL $IND_FUN1,$IND_FUN2\n" >> MCI_tlp_con.txt
	printf "$BENCH ${LINE[0]}.${LINE[i]} $MERIT_OPT $AREA_TOTAL $IND_FUN1,$IND_FUN2\n" >> MCI_tlp_opt.txt
        fi
    done
done<parallel_tasks.txt
