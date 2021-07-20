
#!/bin/bash

# e.g. ./run_config.sh audiodecoder 0.1 50

# 1 - 100 MGB/s
# 0.1 - 1  GB/s
# 0.02 - 5 GB/s
# 0.01 - 10  GB/s
# 0.003125 - 32 GB/s 

BENCH=$1	# Name of the Benchmark
ALPHA=$2	# Parameter that affects the Bandwidth for IO latency
OVHD=$3 	# Invocation Overhead


cp LA.txt LA.LLVM.txt
./compute_merit.sh $BENCH $ALPHA $OVHD
./remove_fractional_point.sh MC.txt
./generate_accelcands_list.sh 
./filter_mci.sh MCI.txt
cp MCI.txt MCI.llvm.txt

#exit 0;

# Task Level Parallelism Estimation.
./compute_sw_hw.sh $ALPHA $OVHD
./remove_fractional_point.sh SW_HW_AREA.txt
./extract_tlp.sh $BENCH
./filter_mci.sh MCI_tlp_opt.txt 
cp MCI.txt.orig MCI.tlp.txt
./filter_mci.sh MCI.tlp.txt
cat MCI_tlp_opt.txt >> MCI.tlp.txt

#exit 0;
#Loop Level Parallelism Estimation.
./update_io.sh; ./update_fci.sh 
./extract_llp.sh 
./compute_merit_llp.sh $BENCH $ALPHA $OVHD
./remove_fractional_point.sh MC_HPVM_LLP.txt
cp MC_HPVM_LLP.txt MC.txt; cp FCI_HPVM_LLP.txt FCI.txt;
./generate_accelcands_list.sh
./filter_mci.sh MCI.txt
cp MCI.txt MCI.llp.txt; cp FCI.txt.orig FCI.txt;

# Pipeline Estimation
./extract_pipe.sh $BENCH
./filter_mci.sh MCI_pipe.txt
cp MCI.llvm.txt MCI.pipe.txt
./filter_mci.sh MCI.pipe.txt
cat MCI_pipe.txt >> MCI.pipe.txt

./filter_mci.sh MCI_pipe_tlp.txt; cp MCI.llvm.txt MCI.pipe.tlp.txt; cat MCI_pipe_tlp.txt >> MCI.pipe.tlp.txt

mkdir conf.$ALPHA.$OVHD
mv MC* conf.$ALPHA.$OVHD/.
