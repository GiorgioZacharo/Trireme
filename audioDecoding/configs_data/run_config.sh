
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

# Task Level Parallelism Estimation.
./compute_sw_hw.sh $ALPHA $OVHD
./remove_fractional_point.sh SW_HW_AREA.txt
./extract_tlp.sh $BENCH
./filter_mci.sh MCI_tlp_opt.txt 
cp MCI.txt.orig MCI.tlp.txt
./filter_mci.sh MCI.tlp.txt
cat MCI_tlp_opt.txt >> MCI.tlp.txt

#Loop Level Parallelism Estimation.
./update_io.sh; ./update_fci.sh 
./extract_llp.sh 
./compute_merit_llp.sh $BENCH $ALPHA $OVHD
./remove_fractional_point.sh MC_HPVM_LLP.txt
cp MC_HPVM_LLP.txt MC.txt; cp FCI_HPVM_LLP.txt FCI.txt;
./generate_accelcands_list.sh
./filter_mci.sh MCI.txt
cp MCI.txt MCI.llp.txt; cp FCI.txt.orig FCI.txt;

# Task Level-Loop AND Level Parallelism
./remove_fractional_point.sh LA_HPVM_LLP.txt
./extract_tlp-llp.sh  $BENCH
./filter_mci.sh MCI_tlp_llp_opt.txt
cp MCI.tlp.txt MCI.tlp-llp.txt
./filter_mci.sh MCI.tlp-llp.txt
cat MCI_tlp_llp_opt.txt >> MCI.tlp-llp.txt

# Pipeline Estimation
./extract_pipe.sh $BENCH
./filter_mci.sh MCI_pipe.txt
cp MCI.llvm.txt MCI.pipe.txt
./filter_mci.sh MCI.pipe.txt
cat MCI_pipe.txt >> MCI.pipe.txt

./filter_mci.sh MCI_pipe_tlp.txt; cp MCI.llvm.txt MCI.pipe.tlp.txt; cat MCI_pipe_tlp.txt >> MCI.pipe.tlp.txt

# Pipeline Estimation AND Loop Level Parallelism 
./extract_pipe-llp.sh  $BENCH
./filter_mci.sh MCI_pipe-llp.txt
cp MCI.llvm.txt MCI.pipe-llp.txt
./filter_mci.sh MCI.pipe-llp.txt
cat MCI_pipe-llp.txt >> MCI.pipe-llp.txt

./filter_mci.sh MCI_pipe_tlp-llp.txt; cp MCI.llvm.txt MCI.pipe.tlp-llp.txt; cat MCI_pipe_tlp-llp.txt >> MCI.pipe.tlp-llp.txt

mkdir conf.$ALPHA.$OVHD
mv MC* conf.$ALPHA.$OVHD/.
