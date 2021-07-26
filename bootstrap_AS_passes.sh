#! /bin/sh

export LLVM_SRC_TREE="hpvm/hpvm/llvm/"

# Copy the folders containing the Analysis passes to LLVM source tree of HPVM.
cp -r AccelSeeker  AccelSeekerIO  $LLVM_SRC_TREE/lib/Transforms/.
echo "add_subdirectory(AccelSeeker)" >> $LLVM_SRC_TREE/lib/Transforms/CMakeLists.txt 
echo "add_subdirectory(AccelSeekerIO)" >> $LLVM_SRC_TREE/lib/Transforms/CMakeLists.txt 
