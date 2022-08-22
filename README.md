# Trireme

# Overview

The Trireme© framework is a tool for automatically exploring hierarchical multi-level parallelism for domain specific hardware acceleration. Trireme identifies and selects HW accelerators directly from the application source files, while suggesting parallelism-related optimizations (Task Level, Loop Level and Pipeline Parallelism). It is built within LLVM9 compiler infrastructure and consists of Analysis Passes that estimate Software (SW) latency, Hardware (HW) latency, Area and I/O requirements. Subsequently, an exact 
selection algorithm selects the subset of HW accelerators that maximizes performance (speedup) under a user
defined area (HW resources) budget.

If you use Trireme in your research, we would appreciate a citation to:
*TBD*

# Installation

First we need to install all necessary tools. (HPVM/LLVM9 and AccelSeeker Analysis passes)

## Getting source code and building HPVM

Download HPVM v1.0 from the [HPVM releases page](https://gitlab.engr.illinois.edu/llvm/hpvm-release/-/releases), then follow the instructions in the downloaded Readme for installing HPVM.

## AccelSeeker Analysis Passes

All necessary files containing the analysis passes need to be copied to the LLVM9 source tree of HPVM. 
 
    ./bootstrap_AS_passes.sh

LLVM9 can then be recompiled using make and a new Shared Object (SO) should be created in order to load the AccelSeeker passes.

    cd hpvm/hpvm/build && make

# Usage

For testing, audio decoder https://github.com/ILLIXR/audio_pipeline from the [ILLIXR](https://github.com/ILLIXR/ILLIXR), the Illinois Extended Reality testbed, of University of Illinois at Urbana-Champaign is used. The audio pipeline is responsible for both recording and playing back spatialized audio for XR.

    cd audioDecoding

### 1) Collect dynamic profiling information and generate the annotated  Intermediate Representation (IR) files.

    cd sim
    ./prof_gen.sh hpvm-gemm-blocked

We make sure that the LLVM lines in "Makefile_AccelSeeker" point to the path of the LLVM9 build and lib directory:    

    BIN_DIR_LLVM=path/to/llvm/build/bin
    LIB_DIR_LLVM=path/to/llvm/build/lib

Then we run the instrumented binary with the appropriate input parameters and generate the annotated IR files using
the profiling information.    

    make profile

The script generates the IR .ll file required by the next step of the Trireme analysis.

### 2) Identification of candidates for acceleration and estimation of Latency, Area and I/O requirements.   

We make sure that the LLVM_BUILD line in "run_sys_aw.sh" points to the path of the LLVM8 build directory:

    LLVM_BUILD=path/to/llvm/build

The following script invokes the AccelSeeker Analysis passes and generates the files needed to construct the final Merit/Cost estimation.
The files generated are: FCI.txt  IO.txt  LA.txt 
    
    ./run_sys_aw.sh


### 3) Merit, Cost Estimation of candidates for acceleration and application of the Overlapping Rule.

The following script generates the Merit/Cost (MC) file along with the implementation of the Overlapping rule in the final Merit/Cost/Indexes (MCI) file.
The files generated are: MCI.txt  MC.txt

    ./generate_accelcands_list.sh

The MCI.txt format is as follows:

BENCHMARK-NAME ACCELERATOR-NAME MERIT(CYCLES SAVED) COST(LUTS) FUNCTION_INDEXES

** Modifications are needed to comply for every benchmark. **

# Author

Georgios Zacharopoulos georgios@seas.harvard.edu Date: July, 2021
