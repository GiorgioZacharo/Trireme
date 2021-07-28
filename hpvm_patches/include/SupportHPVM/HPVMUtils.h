//
//===---- DFG2LLVM.h - Header file for "HPVM Dataflow Graph to Target" ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This filed defines utility functions used for target-specific code generation
// for different nodes of dataflow graphs.
//
//===----------------------------------------------------------------------===//

#ifndef HPVM_UTILS_HEADER
#define HPVM_UTILS_HEADER

#include <assert.h>

#include "HPVMHint.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

using namespace llvm;

namespace hpvmUtils {
// Helper Functions

static bool isHPVMCreateNodeIntrinsic(Instruction *I) {
  if (!isa<IntrinsicInst>(I))
    return false;
  IntrinsicInst *II = cast<IntrinsicInst>(I);
  return (II->getCalledFunction()->getName())
      .startswith("llvm.hpvm.createNode");
}

static bool isHPVMCreateNodeCall(Instruction *I) {
  if (!isa<CallInst>(I))
    return false;
  CallInst *CI = cast<CallInst>(I);
  return (CI->getCalledValue()->stripPointerCasts()->getName())
      .startswith("__hpvm__createNode");
}

static bool isHPVMLaunchCall(Instruction *I) {
  if (!isa<CallInst>(I))
    return false;
  CallInst *CI = cast<CallInst>(I);
  return (CI->getCalledValue()->stripPointerCasts()->getName())
      .startswith("__hpvm__launch");
}
// Creates a new createNode intrinsic, similar to II but with different
// associated function F instead
static IntrinsicInst *
createIdenticalCreateNodeIntrinsicWithDifferentFunction(Function *F,
                                                        IntrinsicInst *II) {
  Module *M = F->getParent();

  // Find which createNode intrinsic we need to create
  Function *CreateNodeF = Intrinsic::getDeclaration(M, II->getIntrinsicID());
  Constant *Fp =
      ConstantExpr::getPointerCast(F, Type::getInt8PtrTy(II->getContext()));

  std::vector<Value *> NodeArgs;
  switch (II->getIntrinsicID()) {
  case Intrinsic::hpvm_createNode: {
    NodeArgs.push_back(Fp);
    break;
  }
  case Intrinsic::hpvm_createNode1D: {
    NodeArgs.push_back(Fp);
    NodeArgs.push_back(II->getArgOperand(1));
    break;
  }
  case Intrinsic::hpvm_createNode2D: {
    NodeArgs.push_back(Fp);
    NodeArgs.push_back(II->getArgOperand(1));
    NodeArgs.push_back(II->getArgOperand(2));
    break;
  }
  case Intrinsic::hpvm_createNode3D: {
    NodeArgs.push_back(Fp);
    NodeArgs.push_back(II->getArgOperand(1));
    NodeArgs.push_back(II->getArgOperand(2));
    NodeArgs.push_back(II->getArgOperand(3));
    break;
  }
  default:
    assert(false && "Unknown createNode intrinsic");
    break;
  }

  ArrayRef<Value *> CreateNodeArgs(NodeArgs);

  CallInst *CI =
      CallInst::Create(CreateNodeF, CreateNodeArgs, F->getName() + ".node", II);
  IntrinsicInst *CreateNodeII = cast<IntrinsicInst>(CI);
  return CreateNodeII;
}

// Fix HPVM hints for this function
static void fixHintMetadata(Module &M, Function *F, Function *G) {
  Metadata *MD_F = ValueAsMetadata::getIfExists(F);
  MDTuple *MDT_F =
      MDTuple::getIfExists(F->getContext(), ArrayRef<Metadata *>(MD_F));
  DEBUG(errs() << "Associated Metadata: " << *MDT_F << "\n");
  MDTuple *MDT_G = MDNode::get(F->getContext(),
                               ArrayRef<Metadata *>(ValueAsMetadata::get(G)));
  DEBUG(errs() << "New Metadata: " << *MDT_G << "\n");

  auto FixHint = [&](StringRef Name) {
    NamedMDNode *HintNode = M.getOrInsertNamedMetadata(Name);
    for (unsigned i = 0; i < HintNode->getNumOperands(); i++) {
      if (HintNode->getOperand(i) == MDT_F)
        HintNode->setOperand(i, MDT_G);
    }
  };

  FixHint("hpvm_hint_gpu");
  FixHint("hpvm_hint_cpu");
  FixHint("hpvm_hint_cpu_gpu");
  FixHint("hpvm_hint_cudnn");
  FixHint("hpvm_hint_promise");
}

// Assuming that the changed function is a node function, it is only used as a
// first operand of createNode*. It is enough to iterate through all createNode*
// calls in the program.
static void replaceNodeFunctionInIR(Module &M, Function *F, Function *G) {

  for (auto &Func : M) {
    DEBUG(errs() << "Function: " << Func.getName() << "\n");

    std::vector<Instruction *> toBeErased;

    for (inst_iterator i = inst_begin(&Func), e = inst_end(&Func); i != e;
         ++i) {
      Instruction *I = &*i; // Grab pointer to Instruction

      if (isHPVMCreateNodeIntrinsic(I)) {
        IntrinsicInst *II = cast<IntrinsicInst>(I);
        // The found createNode is not associated with the changed function
        if (II->getArgOperand(0)->stripPointerCasts() != F)
          continue; // skip it

        // Otherwise, create a new createNode similar to the other one,
        // but with the changed function as first operand.
        IntrinsicInst *CreateNodeII =
            createIdenticalCreateNodeIntrinsicWithDifferentFunction(G, II);
        II->replaceAllUsesWith(CreateNodeII);
        toBeErased.push_back(II);
      } else if (isHPVMCreateNodeCall(I)) {
        CallInst *CI = cast<CallInst>(I);
        // The found createNode is not associated with the changed function
        if (CI->getArgOperand(1) != F)
          continue; // skip it

        DEBUG(errs() << "Fixing use: " << *CI << "\n");
        DEBUG(errs() << "in function: " << Func.getName() << "\n");
        // Replace use of F with use of G
        CI->setArgOperand(1, G);
        DEBUG(errs() << "Fixed use: " << *CI << "\n");
      } else if (isHPVMLaunchCall(I)) {
        CallInst *CI = cast<CallInst>(I);
        // The found launch call is not associated with the changed function
        if (CI->getArgOperand(1)->stripPointerCasts() != F)
          continue;

        // Otherwise, replace F with G
        DEBUG(errs() << "Fixing use: " << *CI << "\n");
        DEBUG(errs() << "in function: " << Func.getName() << "\n");
        CI->setArgOperand(1, G);
        DEBUG(errs() << "Fixed use: " << *CI << "\n");
      }
    }

    for (auto I : toBeErased) {
      DEBUG(errs() << "\tErasing Instruction: " << *I << "\n");
      I->eraseFromParent();
    }
  }

  // Check if the function is used by a metadata node
  if (F->isUsedByMetadata()) {
    fixHintMetadata(M, F, G);
  }
  DEBUG(errs() << "DONE: Replacing function " << F->getName() << " with "
               << G->getName() << "\n");

  // Remove replaced function from the module
  // assert(F->user_empty() && "Still some uses of older function left\n");
  F->replaceAllUsesWith(UndefValue::get(F->getType()));
  F->eraseFromParent();
}

// Create new function F' as a copy of old function F with a new signature and
// input VMAP. The following two most used cases are handled by this function.
// 1. When some extra arguments need to be added to this function
//    - Here we can map the old function arguments to
//      new ones
// 2. When each pointer argument needs an additional size argument
//    - Here, in the absence of VMap, we map the arguments in order, skipping
//      over extra pointer arguments.
// The function returns the list of return instructions to the caller to fix in
// case the return type is also changed.
static Function *cloneFunction(Function *F, FunctionType *newFT,
                               bool isAddingPtrSizeArg,
                               SmallVectorImpl<ReturnInst *> *Returns = NULL,
                               std::vector<Argument *> *Args = NULL) {

  DEBUG(errs() << "Cloning Function: " << F->getName() << "\n");
  DEBUG(errs() << "Old Function Type: " << *F->getFunctionType() << "\n");
  DEBUG(errs() << "New Function Type: " << *newFT << "\n");

  assert(F->getFunctionType()->getNumParams() <= newFT->getNumParams() &&
         "This function assumes that the new function has more arguments than "
         "the old function!");

  // Create Function of specified type
  Function *newF = Function::Create(newFT, F->getLinkage(), F->getName() + "_c",
                                    F->getParent());
  DEBUG(errs() << "Old Function name: " << F->getName() << "\n");
  DEBUG(errs() << "New Function name: " << newF->getName() << "\n");
  ValueToValueMapTy VMap;
  DEBUG(errs() << "No value map provided. Creating default value map\n");
  if (isAddingPtrSizeArg) {
    DEBUG(errs() << "Case 1: Pointer arg followed by a i64 size argument in "
                    "new function\n");
    Function::arg_iterator new_ai = newF->arg_begin();
    if (Args == NULL) {
      for (Function::arg_iterator ai = F->arg_begin(), ae = F->arg_end();
           ai != ae; ++ai) {
        DEBUG(errs() << ai->getArgNo() << ". " << *ai << " : " << *new_ai
                     << "\n");
        assert(ai->getType() == new_ai->getType() &&
               "Arguments type do not match!");
        VMap[&*ai] = &*new_ai;
        new_ai->takeName(&*ai);
        if (ai->getType()->isPointerTy()) {
          std::string oldName = new_ai->getName();
          // If the current argument is pointer type, the next argument in new
          // function would be an i64 type containing the data size of this
          // argument. Hence, skip the next arguement in new function.
          ++new_ai;
          new_ai->setName("bytes_" + oldName);
        }
        ++new_ai;
      }
    } else {
      DEBUG(errs()
            << "Arguments of original function will be read from a vector!\n");
      for (auto *ai : *(Args)) {
        DEBUG(errs() << ai->getArgNo() << ". " << *ai << " : " << *new_ai
                     << "\n");
        assert(ai->getType() == new_ai->getType() &&
               "Arguments type do not match!");
        VMap[ai] = &*new_ai;
        new_ai->takeName(ai);
        if (ai->getType()->isPointerTy()) {
          std::string oldName = new_ai->getName();
          // If the current argument is pointer type, the next argument in new
          // function would be an i64 type containing the data size of this
          // argument. Hence, skip the next arguement in new function.
          ++new_ai;
          new_ai->setName("bytes_" + oldName);
        }
        ++new_ai;
      }
    }
  } else {
    DEBUG(errs()
          << "Case 2: Extra arguments are added at the end of old function\n");
    Function::arg_iterator new_ai = newF->arg_begin();
    if (Args == NULL) {
      for (Function::arg_iterator ai = F->arg_begin(), ae = F->arg_end();
           ai != ae; ++ai, ++new_ai) {
        DEBUG(errs() << ai->getArgNo() << ". " << *ai << " : " << *new_ai
                     << "\n");
        assert(ai->getType() == new_ai->getType() &&
               "Arguments type do not match!");
        VMap[&*ai] = &*new_ai;
        new_ai->takeName(&*ai);
      }
    } else {
      DEBUG(errs()
            << "Arguments of original function will be read from a vector!\n");
      for (auto *ai : *(Args)) {
        DEBUG(errs() << ai->getArgNo() << ". " << *ai << " : " << *new_ai
                     << "\n");
        assert(ai->getType() == new_ai->getType() &&
               "Arguments type do not match!");
        VMap[ai] = &*new_ai;
        new_ai->takeName(ai);
        ++new_ai;
      }
    }
  }

  // Clone function
  if (Returns == NULL)
    Returns = new SmallVector<ReturnInst *, 8>();
  CloneFunctionInto(newF, F, VMap, false, *Returns);
  newF->setAttributes(F->getAttributes());

  return newF;
}

// Overloaded version of cloneFunction
static Function *cloneFunction(Function *F, Function *newF,
                               bool isAddingPtrSizeArg,
                               SmallVectorImpl<ReturnInst *> *Returns = NULL) {

  DEBUG(errs() << "Cloning Function: " << F->getName() << "\n");
  DEBUG(errs() << "Old Function Type: " << *F->getFunctionType() << "\n");
  DEBUG(errs() << "New Function Type: " << *newF->getFunctionType() << "\n");

  assert(F->getFunctionType()->getNumParams() <=
             newF->getFunctionType()->getNumParams() &&
         "This function assumes that the new function has more arguments than "
         "the old function!");

  // Create Function of specified type
  DEBUG(errs() << "Old Function name: " << F->getName() << "\n");
  DEBUG(errs() << "New Function name: " << newF->getName() << "\n");
  ValueToValueMapTy VMap;
  DEBUG(errs() << "No value map provided. Creating default value map\n");
  if (isAddingPtrSizeArg) {
    DEBUG(errs() << "Case 1: Pointer arg followed by a i64 size argument in "
                    "new function\n");
    Function::arg_iterator new_ai = newF->arg_begin();
    for (Function::arg_iterator ai = F->arg_begin(), ae = F->arg_end();
         ai != ae; ++ai) {
      DEBUG(errs() << ai->getArgNo() << ". " << *ai << " : " << *new_ai
                   << "\n");
      assert(ai->getType() == new_ai->getType() &&
             "Arguments type do not match!");
      VMap[&*ai] = &*new_ai;
      new_ai->takeName(&*ai);
      if (ai->getType()->isPointerTy()) {
        std::string oldName = new_ai->getName();
        // If the current argument is pointer type, the next argument in new
        // function would be an i64 type containing the data size of this
        // argument. Hence, skip the next arguement in new function.
        ++new_ai;
        new_ai->setName("bytes_" + oldName);
      }
      ++new_ai;
    }
  } else {
    DEBUG(errs()
          << "Case 2: Extra arguments are added at the end of old function\n");
    Function::arg_iterator new_ai = newF->arg_begin();
    for (Function::arg_iterator ai = F->arg_begin(), ae = F->arg_end();
         ai != ae; ++ai, ++new_ai) {
      DEBUG(errs() << ai->getArgNo() << ". " << *ai << " : " << *new_ai
                   << "\n");
      assert(ai->getType() == new_ai->getType() &&
             "Arguments type do not match!");
      VMap[&*ai] = &*new_ai;
      new_ai->takeName(&*ai);
    }
  }

  // Clone function
  if (Returns == NULL)
    Returns = new SmallVector<ReturnInst *, 8>();
  CloneFunctionInto(newF, F, VMap, false, *Returns);

  newF->setAttributes(F->getAttributes());

  return newF;
}

//------------------- Helper Functions For Handling Hints -------------------//

// Return true if 1st arg (tag) contains 2nd (target)
static bool tagIncludesTarget(hpvm::Target Tag, hpvm::Target T) {
  switch (Tag) {
  case hpvm::None:
    return false;
  case hpvm::CPU_TARGET:
    if (T == hpvm::CPU_TARGET)
      return true;
    return false;
  case hpvm::GPU_TARGET:
    if (T == hpvm::GPU_TARGET)
      return true;
    return false;
  case hpvm::CPU_OR_GPU_TARGET:
    if ((T == hpvm::CPU_TARGET) || (T == hpvm::GPU_TARGET) ||
        (T == hpvm::CPU_OR_GPU_TARGET))
      return true;
    return false;
  case hpvm::CUDNN_TARGET:
      if (T == hpvm::CUDNN_TARGET)
        return true;
      return false;
  case hpvm::TENSOR_TARGET:
      if (T == hpvm::TENSOR_TARGET)
        return true;
      return false;
  default:
    assert(false && "Unknown Target\n");
    return false;  // What kind of compiler doesn't know this is unreachable?!
  }
}

static bool isSingleTargetTag(hpvm::Target T) {
  return ((T == hpvm::CPU_TARGET) || (T == hpvm::GPU_TARGET) 
       || (T == hpvm::CUDNN_TARGET) || (T == hpvm::TENSOR_TARGET));
}

// Add the specified target to the given tag
static hpvm::Target getUpdatedTag(hpvm::Target Tag, hpvm::Target T) {
  assert(((T == hpvm::CPU_TARGET) || (T == hpvm::GPU_TARGET) 
       || (T == hpvm::CUDNN_TARGET) || (T == hpvm::TENSOR_TARGET)) &&
         "The target is only allowed to be a single target: CPU, GPU, SPIR, "
          "CUDNN, PROMISE\n");

  switch (Tag) {
  case hpvm::None:
    return T;
  case hpvm::CPU_TARGET:
    assert((T != hpvm::CUDNN_TARGET) && (T != hpvm::TENSOR_TARGET) && "Unsupported target combination\n");
    if (T == hpvm::CPU_TARGET)
      return hpvm::CPU_TARGET;
    if (T == hpvm::GPU_TARGET)
      return hpvm::CPU_OR_GPU_TARGET;
    return T;
  case hpvm::GPU_TARGET:
    assert((T != hpvm::CUDNN_TARGET) && (T != hpvm::TENSOR_TARGET) && "Unsupported target combination\n");
    if (T == hpvm::CPU_TARGET)
      return hpvm::CPU_OR_GPU_TARGET;
    if (T == hpvm::GPU_TARGET)
      return hpvm::GPU_TARGET;
    return T;
  case hpvm::CPU_OR_GPU_TARGET:
    assert((T != hpvm::CUDNN_TARGET) && (T != hpvm::TENSOR_TARGET) && "Unsupported target combination\n");
    return hpvm::CPU_OR_GPU_TARGET; 
  default:
    assert(false && "Unknown Target\n");
  }
  return T;
}

// This functions add the hint as metadata in hpvm code
static void addHint(Function *F, hpvm::Target T) {
  // Get Module
  Module *M = F->getParent();
  DEBUG(errs() << "Set preferred target for " << F->getName() << ": ");

  // Based on the hint, get the hint metadata
  NamedMDNode *HintNode;
  switch (T) {
  case hpvm::GPU_TARGET:
    DEBUG(errs() << "GPU Target\n");
    HintNode = M->getOrInsertNamedMetadata("hpvm_hint_gpu");
    break;
  case hpvm::CPU_TARGET:
    DEBUG(errs() << "CPU Target\n");
    HintNode = M->getOrInsertNamedMetadata("hpvm_hint_cpu");
    break;
  case hpvm::CPU_OR_GPU_TARGET:
    DEBUG(errs() << "CPU or GPU Target\n");
    HintNode = M->getOrInsertNamedMetadata("hpvm_hint_cpu_gpu");
    break;
  case hpvm::CUDNN_TARGET:
      DEBUG(errs() << "CUDNN Target\n");
      HintNode = M->getOrInsertNamedMetadata("hpvm_hint_cudnn");
      break;
  case hpvm::TENSOR_TARGET:
      DEBUG(errs() << "PROMISE Target\n");
      DEBUG(errs() << "PROMISE\n");
      HintNode = M->getOrInsertNamedMetadata("hpvm_hint_promise");
      break;
  default:
    llvm_unreachable("Unsupported Target Hint!");
    break;
  }

  // Create a node for the function and add it to the hint node
  MDTuple *N = MDNode::get(M->getContext(),
                           ArrayRef<Metadata *>(ValueAsMetadata::get(F)));
  HintNode->addOperand(N);
}

// This function removes the hint as metadata in hpvm code
static void removeHint(Function *F, hpvm::Target T) {
  // Get Module
  Module *M = F->getParent();
  DEBUG(errs() << "Remove preferred target for " << F->getName() << ": " << T
               << "\n");

  // Based on the hint, get the hint metadata
  NamedMDNode *HintNode;
  switch (T) {
  case hpvm::GPU_TARGET:
    HintNode = M->getOrInsertNamedMetadata("hpvm_hint_gpu");
    break;
  case hpvm::CPU_OR_GPU_TARGET:
    HintNode = M->getOrInsertNamedMetadata("hpvm_hint_cpu_gpu");
    break;
  case hpvm::CPU_TARGET:
    HintNode = M->getOrInsertNamedMetadata("hpvm_hint_cpu");
    break;
  case hpvm::CUDNN_TARGET:
      HintNode = M->getOrInsertNamedMetadata("hpvm_hint_cudnn");
      break;
  case hpvm::TENSOR_TARGET:
      HintNode = M->getOrInsertNamedMetadata("hpvm_hint_promise");
      break;
  default:
    llvm_unreachable("Unsupported Target Hint!");
    break;
  }

  // Gather metadata nodes, and keep those not associated with this function
  MDNode *N = MDNode::get(M->getContext(),
                          ArrayRef<Metadata *>(ValueAsMetadata::get(F)));
  std::vector<MDNode *> MDNodes;

  for (unsigned i = 0; i < HintNode->getNumOperands(); i++) {
    MDNode *MDN = HintNode->getOperand(i);
    if (MDN == N) {
      continue;
    }
    MDNodes.push_back(MDN);
  }

  HintNode->dropAllReferences();

  for (unsigned i = 0; i < MDNodes.size(); i++) {
    HintNode->addOperand(MDNodes[i]);
  }
}

static hpvm::Target getPreferredTarget(Function *F) {
  DEBUG(errs() << "Finding preferred target for " << F->getName() << "\n");
  Module *M = F->getParent();

  auto FoundPrefTarget = [=](StringRef Name) {
    NamedMDNode *HintNode = M->getOrInsertNamedMetadata(Name);
    for (unsigned i = 0; i < HintNode->getNumOperands(); i++) {
      MDNode *N = HintNode->getOperand(i);
      Value *FHint =
          dyn_cast<ValueAsMetadata>(N->getOperand(0).get())->getValue();
      if (F == FHint)
        return true;
    }
    return false;
  };

  if (FoundPrefTarget("hpvm_hint_cpu"))
    return hpvm::CPU_TARGET;
  if (FoundPrefTarget("hpvm_hint_gpu"))
    return hpvm::GPU_TARGET;
  if (FoundPrefTarget("hpvm_hint_cpu_gpu"))
    return hpvm::CPU_OR_GPU_TARGET;
  if (FoundPrefTarget("hpvm_hint_cudnn"))
    return hpvm::CUDNN_TARGET;
  if (FoundPrefTarget("hpvm_hint_promise"))
    return hpvm::TENSOR_TARGET;
  return hpvm::None;
}

} // namespace hpvmUtils

#endif // HPVM_UTILS_HEADER
