############### This script invokes the System Aware AccelSeeker Analysis on the entire app and extracts the HPVM paralleism information. ##############
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

./run_sys_aw.sh  
$LLVM_BUILD/hpvm-accelseeker main.hpvm.ll input_task_list.txt
