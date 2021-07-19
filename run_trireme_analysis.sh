############### This is the System Aware AccelSeeker Analysis on the whole app ##############
#
#
#    Georgios Zacharopoulos <georgios@seas.harvard.edu>
#    Date: January, 2020
#    Harvard University / Universita' della Svizzera italiana (USI Lugano)
############################################################################################### 

#!/bin/bash
set -e

# Start Editing.
# LLVM build directory - Edit this line. LLVM_BUILD=path/to/llvm/build
LLVM_BUILD=~georgios/llvm-9.0.0/build

# BENCH NAME - Edit this line to use it to another benchmark/application.
#BENCH=h264
#BENCH=h264-func-mg
BENCH=main.hpvm.ll
#BENCH=audioDecoding-seq-profile.ll

# Maximum Level of Bottom-Up Analysis.
TOP_LEVEL=6
# Directory that contains the .ir files.
IRDIR= #src

# Stop Editing.

if [ ! -f "$BENCH" ]; then
        echo "$BENCH  needs to be generated."
	exit 0;
else
	echo "$BENCH exists - no need to generate it again."
fi

# Collects IO information, Indexes info and generates .gv call graph files for every function.
$LLVM_BUILD/bin/opt -load $LLVM_BUILD/lib/AccelSeekerIO.so -AccelSeekerIO -stats    > /dev/null  $BENCH
mkdir gvFiles; mv *.gv gvFiles/.
#exit 0;
# Collects SW, HW and AREA estimation bottom up.
for ((i=0; i <= $TOP_LEVEL ; i++)) ; do
	echo "$i"
 printf "$i" > level.txt

$LLVM_BUILD/bin/opt -load $LLVM_BUILD/lib/AccelSeeker.so -AccelSeeker -stats    > /dev/null  $BENCH
done

cp LA_$TOP_LEVEL.txt LA.txt; mkdir analysis_data; mv SW_*.txt HW_*.txt AREA_*.txt LA_*.txt analysis_data/.  
rm level.txt

exit 0;

# Delete all files
rm *.txt; rm -r gvFiles
