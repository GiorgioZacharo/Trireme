#!/bin/bash

LLP_MAX=8
LLP_FACTOR=2
PARALLEL_LOOP=0

     while read TASK[1] TASK[2] TASK[3] TASK[4] TASK[5]; do     # parallel_tasks.txt
     #while read TASK[1]; do # TASK2 TASK3 TASK4 TASK5; do     # parallel_tasks.txt
   for (( LLP_FACTOR=2; $LLP_FACTOR<=$LLP_MAX;  )); do
     for  (( i=1; i<=5; i++ )); do
	PARALELL_LOOP=0
	while read FUNC_NAME LOOP_NESTING ITERATIONS ; do # parallel_loops.txt

	#for  (( i=1; i<=5; i++ )); do 
        	if [[ $FUNC_NAME == ${TASK[$i]} ]]; then

                  # for (( LLP_FACTOR=2; $LLP_FACTOR<=$LLP_MAX;  )); do

                        printf "${TASK[$i]}-$LLP_FACTOR\t" >> pipelined_tasks-llp.txt
			PARALLEL_LOOP=1
		##else

			##printf "${TASK[$i]} " >> pipelined_tasks-llp.txt
                	#LLP_FACTOR=$(( LLP_FACTOR * 2 ))
                   #done

        	fi
	#done

 #printf "\n" >> pipelined_tasks-llp.txt

    done<parallel_loops.txt
	
	if [ $PARALLEL_LOOP -eq 0 ]; then
	printf "${TASK[$i]} " >> pipelined_tasks-llp.txt
	fi
	echo "${TASK[1]} PL:  $PARALLEL_LOOP" 
	PARALELL_LOOP=0
	

      done # for loop end
      printf "\n" >> pipelined_tasks-llp.txt
       LLP_FACTOR=$(( LLP_FACTOR * 2 ))
	done
   done <pipelined_tasks.txt 
    #done <parallel_tasks.txt 

cat  pipelined_tasks-llp.txt
