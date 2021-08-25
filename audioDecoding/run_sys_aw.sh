############### This is the System Aware AccelSeeker Analysis on the whole app ##############
#
#
#    Georgios Zacharopoulos <georgios@seas.harvard.edu>
#    Date: August, 2021
#    Harvard University 
############################################################################################### 

#!/bin/bash
set -e

# Start Editing.
# LLVM build directory - Edit this line. LLVM_BUILD=path/to/llvm/build
LLVM_BUILD=../hpvm/hpvm/build

# BENCH NAME - Edit this line to use it to another benchmark/application.
BENCH=main.hpvm.ll

# Maximum Level of Bottom-Up Analysis.
TOP_LEVEL=6

# Directory that contains the IR .ll files. (if needed to be specified - default=empty)
IRDIR= 

# Stop Editing.

if [ ! -f "$BENCH" ]; then
        echo "$BENCH  needs to be generated."
	exit 0;
else
	echo "$BENCH exists - no need to generate it again."
fi

# Collects IO information, Indexes info and generates .gv call graph files for every function.
$LLVM_BUILD/bin/opt -load $LLVM_BUILD/lib/AccelSeekerIO.so -AccelSeekerIO -stats    > /dev/null  $BENCH

# Collects SW, HW and AREA estimation bottom up.
for ((i=0; i <= $TOP_LEVEL ; i++)) ; do
	echo "$i"
 printf "$i" > level.txt

$LLVM_BUILD/bin/opt -load $LLVM_BUILD/lib/AccelSeeker.so -AccelSeeker -stats    > /dev/null  $BENCH
done

cp LA_$TOP_LEVEL.txt LA.txt; mkdir analysis_data; mv SW_*.txt HW_*.txt AREA_*.txt LA_*.txt analysis_data/.  
rm level.txt

# Generate the SW-HW tasks file used as input for the parallelism extraction tool.
$SCRIPTS_DIR/compute_sw_hw_tasks.sh
# Parallelism extraction tool. Input: IR (.ll) file of application and the SW-HW tasks file.
$LLVM_BUILD/bin/hpvm-accelseeker $BENCH SW_HW.txt

exit 0;

# Delete all data files produced.
rm *.txt; rm -r gvFiles
