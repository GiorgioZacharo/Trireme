//=== GenHPVM.cpp - Implements "Hierarchical Dataflow Graph Builder Pass" ===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass takes LLVM IR with HPVM-C functions to generate textual representa-
// -tion for HPVM IR consisting of HPVM intrinsics. Memory-to-register
// optimization pass is expected to execute prior to execution of this pass.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "clearprof"

#include "SupportHPVM/HPVMHint.h"
#include "SupportHPVM/HPVMUtils.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#define TIMER(X)                                                               \
  do {                                                                         \
    if (HPVMTimer) {                                                           \
      X;                                                                       \
    }                                                                          \
  } while (0)

#ifndef LLVM_BUILD_DIR
#error LLVM_BUILD_DIR is not defined
#endif

#define STR_VALUE(X) #X
#define STRINGIFY(X) STR_VALUE(X)
#define LLVM_BUILD_DIR_STR STRINGIFY(LLVM_BUILD_DIR)

using namespace llvm;
using namespace hpvmUtils;


namespace {

// Helper Functions

// Check if the dummy function call is a __hpvm__node call
#define IS_HPVM_CALL(callName)                                                 \
  static bool isHPVMCall_##callName(Instruction *I) {                          \
    if (!isa<CallInst>(I))                                                     \
      return false;                                                            \
    CallInst *CI = cast<CallInst>(I);                                          \
    return (CI->getCalledValue()->stripPointerCasts()->getName())              \
        .equals("__hpvm__" #callName);                                         \
  }

IS_HPVM_CALL(edge)
IS_HPVM_CALL(createNodeND)
IS_HPVM_CALL(bindIn)
IS_HPVM_CALL(bindOut)

IS_HPVM_CALL(attributes)
IS_HPVM_CALL(hint)

static bool isHPVMInternalNodeCall(CallInst *CI) {
  // Ignore the attributes call
  // It belongs to both leaf and internal nodes, so it can't be used to
  // identify between them.
  // Also, no valid HPVM node can contaian only this call, so we can safely
  // assume that we can differentiate between HPVM and non HPVM functions based
  // on other calls
  //  if (isHPVMCall_attributes(CI))
  //    return true;
  if (isHPVMCall_createNodeND(CI))
    return true;
  if (isHPVMCall_edge(CI))
    return true;
  if (isHPVMCall_bindIn(CI))
    return true;
  if (isHPVMCall_bindOut(CI))
    return true;
  return false;
}

struct ClearProf : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  ClearProf() : ModulePass(ID) {}

private:
  // Member variables
  Module *M;

public:
  // Functions
  bool runOnModule(Module &M);
};


// Public Functions of GlearProf pass
bool ClearProf::runOnModule(Module &M) {
  DEBUG(errs() << "\nClearProf PASS\n");
  this->M = &M;

  std::vector<Function *> functions;

  for (auto &F : M)
    functions.push_back(&F);

  std::vector<Instruction *> ProfileInstructions;
  for (Function *f : functions) {
    DEBUG(errs() << "Function: " << f->getName() << "\n");
    // Identify Internal HPVM node functions
    bool skip = true;
    for (inst_iterator i = inst_begin(f), e = inst_end(f); i != e; ++i) {
      Instruction *I = &*i;

      CallInst *CI = dyn_cast<CallInst>(I);
      if (CI) {
        if (isHPVMInternalNodeCall(CI)) {
          // A call consistent with Internal Node HPVM function found
          skip = false;
          break;
        }
      }
    }
  
    // If function is to be skipped, move on to next loop iteration
    // to process next function
    if (skip) {
      DEBUG(errs() << "Skipping function " << f->getName() << "\n");
      continue;
    }
  
    DEBUG(errs() << "Processing function " << f->getName() << "\n");
    for (inst_iterator i = inst_begin(f), e = inst_end(f); i != e; ++i) {
      Instruction *I = &*i; // Grab pointer to Instruction
      DEBUG(errs() << *I << "\n");
      if (isa<ReturnInst>(I) || isa<CastInst>(I) || isa<IntrinsicInst>(I))
        continue;
      if (CallInst *CI = dyn_cast<CallInst>(I)) {
        if (isHPVMInternalNodeCall(CI) ||
            isHPVMCall_attributes(CI) ||
            isHPVMCall_hint(CI))
          continue;
      }
      DEBUG(errs() << "Marking it for removal.\n");
      ProfileInstructions.push_back(I);
    }
  
  }

  DEBUG(errs() << "Verify that instructions follow profiling pattern.\n");
  for (unsigned i = 0; i < ProfileInstructions.size(); i++) {
    Instruction *I = ProfileInstructions[i];
    DEBUG(errs() << *I << "\n");
    if (i%3 == 0) {
      if (isa<LoadInst>(I)) {
        DEBUG(errs() << "Detected Load instruction.\n"
                     << "Within pattern for profiling.\n");
      } else {
        DEBUG(errs() << "WARNING: Detected instruction is not Load.\n");
        DEBUG(errs() << "Instruction sequence not compatible with profiling.\n");
      }
    } else if (i%3 == 1) {
      BinaryOperator *BinOpI = dyn_cast<BinaryOperator>(I);
      if (BinOpI &&
          BinOpI->getOpcode() == Instruction::BinaryOps::Add &&
          BinOpI->getOperand(0) == ProfileInstructions[i-1]) {
        DEBUG(errs() << "Detected Add instruction with 0 operand: "
                     << *BinOpI->getOperand(0) << "\n"
                     << "Within pattern for profiling.\n");
      } else if (!BinOpI || BinOpI->getOpcode() != Instruction::BinaryOps::Add) {
        DEBUG(errs() << "WARNING: Detected instruction is not Add. ");
        DEBUG(errs() << "Instruction sequence not compatible with profiling.\n");
      } else {
        DEBUG(errs() << "WARNING: Detected Add instruction does not have "
                     << "appropriate 0 operand for profiling sequence.\n");
        DEBUG(errs() << "Instruction sequence not compatible with profiling.\n");
      }
    } else if (i%3 == 2) {
      StoreInst *SI = dyn_cast<StoreInst>(I);
      if (SI && SI->getValueOperand() == ProfileInstructions[i-1]) {
        DEBUG(errs() << "Detected Store instruction with value operand: "
                     << *SI->getValueOperand() << "\n"
                     << "Within pattern for profiling.\n");
      } else if (!SI) {
        DEBUG(errs() << "WARNING: Detected instruction is not Store. ");
        DEBUG(errs() << "Instruction sequence not compatible with profiling.\n");
      } else {
        DEBUG(errs() << "WARNING: Detected Store instruction does not have "
                     << "appropriate value operand for profiling sequence.\n");
        DEBUG(errs() << "Instruction sequence not compatible with profiling.\n");
      }
    }
  }

  DEBUG(errs() << "Erase " << ProfileInstructions.size() << " profiling statements:\n");
  for (auto I : ProfileInstructions) {
    DEBUG(errs() << *I << "\n");
  }
  while (!ProfileInstructions.empty()) {
    Instruction *I = ProfileInstructions.back();
    DEBUG(errs() << "\tErasing " << *I << "\n");
    I->eraseFromParent();
    ProfileInstructions.pop_back();
  }


  return true;
}

} // End of namespace 

char ClearProf::ID = 0;
static RegisterPass<ClearProf>
    X("clearprof",
      "Pass to remove injected profiling code from HPVM internal node functions from LLVM IR with HPVM-C function calls",
      false, true);
