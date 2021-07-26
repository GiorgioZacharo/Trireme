# Trireme

# Overview

The Trireme© framework is a tool for automatically exploring hierarchical multi-level parallelism for domain specific hardware acceleration. Trireme identifies and selects HW accelerators directly from the application source files, while suggesting parallelism-related optimizations (Task Level, Loop Level and Pipeline Parallelism). It is built within LLVM9 compiler infrastructure and consists of Analysis Passes that estimate Software (SW) latency, Hardware (HW) latency, Area and I/O requirements. Subsequently, an exact 
selection algorithm selects the subset of HW accelerators that maximizes performance (speedup) under a user
defined area (HW resources) budget.

If you use Trireme in your research, we would appreciate a citation to:

# Installation

First we need to install all necessary tools. (LLVM8 and AccelSeeker Analysis passes)
## Getting source code and building HPVM

Checkout HPVM:
```shell
git clone https://gitlab.engr.illinois.edu/llvm/hpvm-release.git/
cd hpvm-release/hpvm
```

HPVM installer script can be used to download, configure and build HPVM along with LLVM and Clang. 
```shell
bash install.sh [flags]
```

