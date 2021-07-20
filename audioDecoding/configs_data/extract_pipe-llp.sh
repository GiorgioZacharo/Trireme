#!/bin/bash

BENCH="${1:-APP_NAME}"

rm cols.txt MCI_tlp*.txt 
awk -F'\t' '{print NF;}' parallel_tasks.txt > cols.txt

# Calculate Max Column number
temp=`awk '$0>x{x=$0};END{print x}' cols.txt`
temp=$(( temp - 1 ))
echo $temp

SW_PIPES_SUM=0
HW_PIPES_MAX=0
AREA_PIPES_TOT=0
IND_PIPES=
NAME_PIPES=

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
        if [ $i -gt 1 ]; then # Exlude itself ! For sets of three !
          echo "${LINE[0]}: ${LINE[1]} ${LINE[i]}"
	
        SW_FUN1=0
        SW_FUN2=0
        SW_FUN3=0
        HW_FUN1=0
        HW_FUN2=0
        HW_FUN3=0
        AREA_FUN1=0
        AREA_FUN2=0
        AREA_FUN3=0

        IND_FUN1=
        IND_FUN2=
        IND_FUN3=

	# Reads the SW and HW Latencies from the AccelSeeker Analysis/Estimation
	  while read FUNC_NAME SW HW AREA INV; do
	    if [ $FUNC_NAME = ${LINE[0]} ];then	
		SW_FUN1=$SW
		HW_FUN1=$HW
		AREA_FUN1=$AREA
	     fi

	     if [ $FUNC_NAME = ${LINE[1]} ];then
                SW_FUN2=$SW
                HW_FUN2=$HW
		AREA_FUN2=$AREA
             fi

	     if [ $FUNC_NAME = ${LINE[i]} ];then
                SW_FUN3=$SW
                HW_FUN3=$HW
                AREA_FUN3=$AREA
             fi

 	  #done<SW_HW_AREA.txt
 	  done<LA_HPVM_LLP.txt

        # Reads the Indexes of the tasks.
          while IFS=' '  read BENCHMARK FUNC_NAME M C IND ; do
	 #	echo FUNC: $BENCH $FUNC
            if [ $FUNC_NAME = ${LINE[0]} ];then
               IND_FUN1=$IND
             fi

             if [ $FUNC_NAME = ${LINE[1]} ];then
               IND_FUN2=$IND
             fi


            if [ $FUNC_NAME = ${LINE[i]} ];then
               IND_FUN3=$IND
             fi
          done<MCI.llp.txt

        MAX_HW=$(( HW_FUN1 > HW_FUN2 ? HW_FUN1 : HW_FUN2 ))
        printf "$MAX_HW\n"
        MAX_HW=$(( MAX_HW > HW_FUN3 ? MAX_HW : HW_FUN3 ))
        printf "$MAX_HW\n"
        SUM_SW=$(echo "scale=0;$SW_FUN1 + $SW_FUN2 + $SW_FUN3" | bc -l)

        MERIT=$(echo "scale=0;$SUM_SW - $MAX_HW" | bc -l)
        AREA_TOTAL=$(echo "scale=0;$AREA_FUN1 + $AREA_FUN2 + $AREA_FUN3" | bc -l) # Area total calculation

        # For Parallel Pipelines Estimation.
        SW_PIPES_SUM=$(( SW_PIPES_SUM + SUM_SW ))
        HW_PIPES_MAX=$(( MAX_HW > HW_PIPES_MAX ? MAX_HW : HW_PIPES_MAX ))
        AREA_PIPES_TOT=$(( AREA_PIPES_TOT + $AREA_TOTAL ))
        IND_PIPES="$IND_FUN1,$IND_FUN2,$IND_FUN3,$IND_PIPES"
        NAME_PIPES="${LINE[0]}.${LINE[1]}.${LINE[i]}.$NAME_PIPES"	
	
        printf "$BENCH ${LINE[0]}.${LINE[1]}.${LINE[i]} $MERIT $AREA_TOTAL $IND_FUN1,$IND_FUN2,$IND_FUN3\n" >> MCI_pipe-llp.txt
        fi
    done
done<pipelined_tasks-llp.txt
#done<parallel_tasks.txt
 MERIT_PARALLEL_PIPE=$(echo "scale=0;$SW_PIPES_SUM - $HW_PIPES_MAX" | bc -l)
 printf "$BENCH $NAME_PIPES $MERIT_PARALLEL_PIPE $AREA_PIPES_TOT $IND_PIPES\n" >> MCI_pipe_tlp-llp.txt

