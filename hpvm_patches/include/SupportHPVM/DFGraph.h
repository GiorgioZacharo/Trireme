//===----- llvm/IR/DFGraph.h - Classes to represent a Dataflow Graph ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the definition of the following classes:
// 1. DFNode
// 2. DFGraph
// 3. DFInternalNode
// 4. DFLeafNode
// 5. DFEdge.
//
// FIXME : We still need to figure out whether these functions are independent
//===----------------------------------------------------------------------===//

#ifndef LLVM_IR_DFGRAPH_H
#define LLVM_IR_DFGRAPH_H

#include "HPVMHint.h"
#include "HPVMUtils.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

class DFNode;
class DFInternalNode;
class DFLeafNode;
class DFEdge;
class DFNodeVisitor;
class DFTreeTraversal;
class DFEdgeVisitor;
class DFGraph;

struct TargetGenFunctions {
  Function *CPUGenFunc;
  Function *GPUGenFunc;
  Function *CUDNNGenFunc;
  Function *PROMISEGenFunc;
};

struct TargetGenFuncInfo {
  bool cpu_hasCPUFunc;
  bool gpu_hasCPUFunc;
  bool cudnn_hasCPUFunc;
  bool promise_hasCPUFunc;
  bool fft_hasCPUFunc;
  bool viterbi_hasCPUFunc;
  bool epochs_hasCPUFunc;
};

class DFGraph {

private:
  typedef std::vector<DFNode *> DFNodeListType;
  typedef std::vector<DFEdge *> DFEdgeListType;

  // Important things that make up a Dataflow graph
  DFNode *Entry; ///< Dummy node to act as source for edges
                 ///< from parent to nodes in the graph
  DFNode *Exit;  ///< Dummy node to act as destination for edges
                 ///< from nodes in the graph to parent
  DFInternalNode *Parent;
  DFNodeListType ChildrenList; ///< List of children Dataflow Nodes
  DFEdgeListType DFEdgeList;   ///< List of Dataflow edges among children

public:
  inline DFGraph(DFInternalNode *P);

  virtual ~DFGraph() {}

  void addChildDFNode(DFNode *child) {
    if (std::find(ChildrenList.begin(), ChildrenList.end(), child) ==
        ChildrenList.end())
      ChildrenList.push_back(child);
  }

  void removeChildDFNode(DFNode *child) {
    children_iterator position = std::find(begin(), end(), child);
    if (position != end()) // the child was found
      ChildrenList.erase(position);
  }

  // Dataflow edge connecting child dataflow nodes
  void addDFEdge(DFEdge *E) {
    if (std::find(DFEdgeList.begin(), DFEdgeList.end(), E) == DFEdgeList.end())
      DFEdgeList.push_back(E);
  }

  DFNode *getEntry() const { return Entry; }

  DFNode *getExit() const { return Exit; }

  bool isEntry(const DFNode *N) const { return N == Entry; }

  bool isExit(const DFNode *N) const { return N == Exit; }

  inline void sortChildren();
  static inline bool compareRank(DFNode *A, DFNode *B);

  // Iterators
  typedef DFNodeListType::iterator children_iterator;
  typedef DFNodeListType::const_iterator const_children_iterator;

  typedef DFEdgeListType::iterator dfedge_iterator;
  typedef DFEdgeListType::const_iterator const_dfedge_iterator;

  //===--------------------------------------------------------------------===//
  // DFNodeList iterator forwarding functions
  //
  children_iterator begin() { return ChildrenList.begin(); }
  const_children_iterator begin() const { return ChildrenList.begin(); }
  children_iterator end() { return ChildrenList.end(); }
  const_children_iterator end() const { return ChildrenList.end(); }

  size_t size() const { return ChildrenList.size(); }
  bool empty() const { return ChildrenList.empty(); }
  const DFNode *front() const { return ChildrenList.front(); }
  DFNode *front() { return ChildrenList.front(); }
  const DFNode *back() const { return ChildrenList.back(); }
  DFNode *back() { return ChildrenList.back(); }

  //===--------------------------------------------------------------------===//

  //===--------------------------------------------------------------------===//
  // DFEdgeList iterator forwarding functions
  //
  dfedge_iterator dfedge_begin() { return DFEdgeList.begin(); }
  const_dfedge_iterator dfedge_begin() const { return DFEdgeList.begin(); }
  dfedge_iterator dfedge_end() { return DFEdgeList.end(); }
  const_dfedge_iterator dfedge_end() const { return DFEdgeList.end(); }

  size_t dfedge_size() const { return DFEdgeList.size(); }
  bool dfedge_empty() const { return DFEdgeList.empty(); }
  const DFEdge *dfedge_front() const { return DFEdgeList.front(); }
  DFEdge *dfedge_front() { return DFEdgeList.front(); }
  const DFEdge *dfedge_back() const { return DFEdgeList.back(); }
  DFEdge *dfedge_back() { return DFEdgeList.back(); }

  //===--------------------------------------------------------------------===//

  DFInternalNode *getParent() const { return Parent; }
  void setParent(DFInternalNode *_Parent) { Parent = _Parent; }

  // Child graph is streaming if any of the edges in the edge list is streaming
  inline bool isStreaming();

  //**************************************************************************//
  //*                  Functions to modify a dataflow graph                  *//
  //**************************************************************************//

  // Delete an edge of the child graph
  void deleteEdge(DFEdge *E) {
    dfedge_iterator position = std::find(dfedge_begin(), dfedge_end(), E);
    if (position != dfedge_end()) // the edge was found
      DFEdgeList.erase(position);
  }
  // Returns whether an edge was removed (might not happen if edge not found).
  inline bool removeEdge(const DFEdge *E);
};

// DFNode represents a single HPVM Dataflow Node in LLVM.
//
// A Dataflow Node basically consists of
// 1. Pointer to a function describing this dataflow node
// 2. Number of dimensions in which the node is replicated
// 3. Number of instances in each dimension
// 4. Pointer to parent Dataflow Node
// 5. List of children Dataflow Nodes (empty if it is a leaf node)
// 6. List of Dataflow Edges among children

class DFNode {

public:
  // Discriminator for LLVM-style RTTI (dyn_cast et al.)
  enum DFNodeKind { InternalNode, LeafNode };

  enum PropertyKind { Allocation, NumProperties };

private:
  typedef std::vector<DFNode *> DFNodeListType;
  typedef std::vector<DFEdge *> DFEdgeListType;
  typedef void *PropertyType;
  typedef std::map<PropertyKind, PropertyType> PropertyListType;

  // Important things that make up a Dataflow Node
  IntrinsicInst *II;        ///< Associated IntrinsicInst/Value
  Function *FuncPointer;    ///< Associated Function
  Function *GenFunc = NULL; ///< Associated Function generated by backend
  struct TargetGenFunctions GenFuncs;
  ///< Associated Functions generated by backends
  ///< (if multiple are available)
  struct TargetGenFuncInfo GenFuncInfo;
  ///< True for each target generated function
  ///< if the associated genFunc is an cpu function
  DFInternalNode *Parent;         ///< Pointer to parent dataflow Node
  unsigned NumOfDim;              ///< Number of dimensions
  std::vector<Value *> DimLimits; ///< Number of instances in each dimension
  DFNodeListType Successors;      ///< List of successors i.e.,
                                  ///< destination DFNodes to DFEdges
                                  ///< originating from this DFNode
  DFEdgeListType InDFEdges;       ///< List of incoming edges i.e.,
                                  ///< DFEdges originating from predecessor
                                  ///< DFNodes and ending at this DFNode
  DFEdgeListType OutDFEdges;      ///< List of outgoing edges i.e.,
                                  ///< DFEdges originating from this DFNode to
                                  ///< successor DFNodes
  PropertyListType PropertyList;  ///< List of Properties
  StructType *OutputType;         ///< Output Type
  unsigned Level;                 ///< Distance to the top-level DFNode in the
                                  ///< hierarchy
  unsigned Rank;                  ///< Ordering based on toplogical sort
  const DFNodeKind Kind;          ///< Kind of Node Internal/Leaf
  hpvm::Target Tag;               ///< Code Generated for which backend
  hpvm::Target Hint;              ///< To store preferred backend
  bool Root;

public:
  virtual ~DFNode() {
    // TODO: Check if fields DimLimits and OutputType need to freed here.
  }

  StringRef getName() { return FuncPointer->getName(); }

  // Iterators
  typedef DFNodeListType::iterator successor_iterator;
  typedef DFNodeListType::const_iterator const_successor_iterator;

  typedef DFEdgeListType::iterator indfedge_iterator;
  typedef DFEdgeListType::const_iterator const_indfedge_iterator;

  typedef DFEdgeListType::iterator outdfedge_iterator;
  typedef DFEdgeListType::const_iterator const_outdfedge_iterator;

  //===--------------------------------------------------------------------===//
  // Successors iterator forwarding functions
  //
  successor_iterator successors_begin() { return Successors.begin(); }
  const_successor_iterator successors_begin() const {
    return Successors.begin();
  }
  successor_iterator successors_end() { return Successors.end(); }
  const_successor_iterator successors_end() const { return Successors.end(); }

  size_t successors_size() const { return Successors.size(); }
  bool successors_empty() const { return Successors.empty(); }
  const DFNode *successors_front() const { return Successors.front(); }
  DFNode *successors_front() { return Successors.front(); }
  const DFNode *successors_back() const { return Successors.back(); }
  DFNode *successors_back() { return Successors.back(); }

  //===--------------------------------------------------------------------===//

  //===--------------------------------------------------------------------===//
  // InDFEdges iterator forwarding functions
  //
  indfedge_iterator indfedge_begin() { return InDFEdges.begin(); }
  const_indfedge_iterator indfedge_begin() const { return InDFEdges.begin(); }
  indfedge_iterator indfedge_end() { return InDFEdges.end(); }
  const_indfedge_iterator indfedge_end() const { return InDFEdges.end(); }

  size_t indfedge_size() const { return InDFEdges.size(); }
  bool indfedge_empty() const { return InDFEdges.empty(); }
  const DFEdge *indfedge_front() const { return InDFEdges.front(); }
  DFEdge *indfedge_front() { return InDFEdges.front(); }
  const DFEdge *indfedge_back() const { return InDFEdges.back(); }
  DFEdge *indfedge_back() { return InDFEdges.back(); }

  //===--------------------------------------------------------------------===//

  //===--------------------------------------------------------------------===//
  // OutDFEdges iterator forwarding functions
  //
  outdfedge_iterator outdfedge_begin() { return OutDFEdges.begin(); }
  const_outdfedge_iterator outdfedge_begin() const {
    return OutDFEdges.begin();
  }
  outdfedge_iterator outdfedge_end() { return OutDFEdges.end(); }
  const_outdfedge_iterator outdfedge_end() const { return OutDFEdges.end(); }

  size_t outdfedge_size() const { return OutDFEdges.size(); }
  bool outdfedge_empty() const { return OutDFEdges.empty(); }
  const DFEdge *outdfedge_front() const { return OutDFEdges.front(); }
  DFEdge *outdfedge_front() { return OutDFEdges.front(); }
  const DFEdge *outdfedge_back() const { return OutDFEdges.back(); }
  DFEdge *outdfedge_back() { return OutDFEdges.back(); }

  //===--------------------------------------------------------------------===//

  // Functions

  DFNodeKind getKind() const { return Kind; }

  inline DFNode(IntrinsicInst *_II, Function *_FuncPointer, hpvm::Target _Hint,
         DFInternalNode *_Parent, unsigned _NumOfDim,
         std::vector<Value *> _DimLimits, DFNodeKind _K);

  void setRoot() { Root = true; }
  bool isRoot() const {
    if (Root)
      return true;
    else
      return false;
    // It is a root node is it was created from a launch intrinsic
    //    if (II->getCalledFunction()->getName().equals("llvm.hpvm.launch")) {
    //      assert(Level == 0 && "Root node's level is zero.");
    //      return true;
    //    }
    //    return false;
  }

  StructType *getOutputType() const { return OutputType; }

  void addSuccessor(DFNode *N) { Successors.push_back(N); }

  void removeSuccessor(DFNode *N) {
    Successors.erase(std::remove(Successors.begin(), Successors.end(), N),
                     Successors.end());
  }

  // Add incoming dataflow edge
  void addInDFEdge(DFEdge *E) {
    if (std::find(InDFEdges.begin(), InDFEdges.end(), E) == InDFEdges.end())
      InDFEdges.push_back(E);
  }

  // Add outgoing dataflow edge
  void addOutDFEdge(DFEdge *E) {
    if (std::find(OutDFEdges.begin(), OutDFEdges.end(), E) == OutDFEdges.end())
      OutDFEdges.push_back(E);
  }

  // For removing dataflow edges
  // Note (applies to add*Edge too): does not update edge data in other places
  // (nodes and the graph). Does not update successors either.
  void removeInDFEdge(const DFEdge *E) {
    InDFEdges.erase(std::remove(InDFEdges.begin(), InDFEdges.end(), E),
                    InDFEdges.end());
  }

  void removeOutDFEdge(const DFEdge *E) {
    OutDFEdges.erase(std::remove(OutDFEdges.begin(), OutDFEdges.end(), E),
                     OutDFEdges.end());
  }

  Function *getFuncPointer() const { return FuncPointer; }

  void setFuncPointer(Function *_FuncPointer) { FuncPointer = _FuncPointer; }

  IntrinsicInst *getInstruction() const { return II; }
  void setInstruction(IntrinsicInst *_II) { II = _II; }

  DFInternalNode *getParent() const { return Parent; }
  void setParent(DFInternalNode *_Parent) { Parent = _Parent; }

  // Adds the node to the graph of P and updates this node's parent
  inline void addToNodeGraph(DFInternalNode *P);

  unsigned getNumOfDim() const { return NumOfDim; }
  void setNumOfDim(unsigned numDims) { NumOfDim = numDims; }

  const std::vector<Value *> &getDimLimits() const { return DimLimits; }
  std::vector<Value *> &getDimLimits() { return DimLimits; }
  void setDimLimits(std::vector<Value *> _DimLimits) { DimLimits = _DimLimits; }

  //std::vector<Value *> getDimLimits() const { return DimLimits; }

  unsigned getLevel() const { return Level; }

  unsigned getRank() const { return Rank; }

  void setTag(hpvm::Target T) { Tag = T; }

  hpvm::Target getTag() const { return Tag; }

  void *getProperty(PropertyKind PType) {
    assert(PropertyList.count(PType) == 1 &&
           "Requesting a property not defined!");
    return PropertyList[PType];
  }

  void setProperty(PropertyKind PType, void *PValue) {
    assert(PropertyList.count(PType) == 0 &&
           "Inserting a property already defined!");
    PropertyList[PType] = PValue;
  }

  void setGenFunc(Function *F, hpvm::Target T) {
    GenFunc = F;
    Tag = T;
  }

  Function *getGenFunc() const { return GenFunc; }

  void setHasCPUFuncForTarget(hpvm::Target T, bool isCPUFunc) {
    switch (T) {
    case hpvm::None:
      return; // Do nothing.
    case hpvm::CPU_TARGET:
      GenFuncInfo.cpu_hasCPUFunc = isCPUFunc;
      break;
    case hpvm::GPU_TARGET:
      GenFuncInfo.gpu_hasCPUFunc = isCPUFunc;
      break;
    case hpvm::TENSOR_TARGET:
      GenFuncInfo.promise_hasCPUFunc = isCPUFunc;
      break;
    case hpvm::CUDNN_TARGET:
      GenFuncInfo.cudnn_hasCPUFunc = isCPUFunc;
      break;
    case hpvm::CPU_OR_GPU_TARGET:
      break;
    default:
      assert(false && "Unknown target\n");
      break;
    }
    return;
  }

  bool hasCPUGenFuncForTarget(hpvm::Target T) const {
    switch (T) {
    case hpvm::None:
      return false;
    case hpvm::CPU_TARGET:
      return GenFuncInfo.cpu_hasCPUFunc;
    case hpvm::GPU_TARGET:
      return GenFuncInfo.gpu_hasCPUFunc;
    case hpvm::CUDNN_TARGET:
      return GenFuncInfo.cudnn_hasCPUFunc;
    case hpvm::TENSOR_TARGET:
      return GenFuncInfo.promise_hasCPUFunc;
    case hpvm::CPU_OR_GPU_TARGET:
      assert(false && "Single target expected (CPU/GPU/SPIR/CUDNN/PROMISE)\n");
    default:
      assert(false && "Unknown target\n");
    }
    return false;
  }

  void addGenFunc(Function *F, hpvm::Target T, bool isCPUFunc) {

    switch (T) {
    case hpvm::CPU_TARGET:
      if (GenFuncs.CPUGenFunc != NULL) {
        DEBUG(errs() << "Warning: Second generated CPU function for node "
                     << FuncPointer->getName() << "\n");
      }
      GenFuncs.CPUGenFunc = F;
      GenFuncInfo.cpu_hasCPUFunc = isCPUFunc;
      break;
    case hpvm::GPU_TARGET:
      if (GenFuncs.GPUGenFunc != NULL) {
        DEBUG(errs() << "Warning: Second generated GPU function for node "
                     << FuncPointer->getName() << "\n");
      }
      GenFuncs.GPUGenFunc = F;
      GenFuncInfo.gpu_hasCPUFunc = isCPUFunc;
      break;
    case hpvm::CUDNN_TARGET:
      if (GenFuncs.CUDNNGenFunc != NULL) {
        DEBUG(errs() << "Warning: Second generated GPU function for node "
                     << FuncPointer->getName() << "\n");
      }
      GenFuncs.CUDNNGenFunc = F;
      GenFuncInfo.cudnn_hasCPUFunc = isCPUFunc;
      break;
    case hpvm::TENSOR_TARGET:
      if (GenFuncs.PROMISEGenFunc != NULL) {
        DEBUG(errs() << "Warning: Second generated PROMISE function for node "
                     << FuncPointer->getName() << "\n");
      }
      GenFuncs.PROMISEGenFunc = F;
      GenFuncInfo.promise_hasCPUFunc = isCPUFunc;
      break;
    case hpvm::CPU_OR_GPU_TARGET:
      assert(false && "A node function should be set with a tag specifying its \
                type, not the node hint itself\n");
    default:
      assert(false && "Unknown target for generated function\n");
    }

    Tag = hpvmUtils::getUpdatedTag(Tag, T);
  }

  Function *getGenFuncForTarget(hpvm::Target T) const {
    switch (T) {
    case hpvm::None:
      return NULL;
    case hpvm::CPU_TARGET:
      return GenFuncs.CPUGenFunc;
    case hpvm::GPU_TARGET:
      return GenFuncs.GPUGenFunc;
    case hpvm::CUDNN_TARGET:
      return GenFuncs.CUDNNGenFunc;
    case hpvm::TENSOR_TARGET:
      return GenFuncs.PROMISEGenFunc;
    case hpvm::CPU_OR_GPU_TARGET:
      assert(false &&
             "Requesting genarated node function with dual tag instead of \
                CPU/GPU/SPIR/CUDNN/PROMISE\n");
    default:
      assert(false && "Unknown target for generated function\n");
    }
    return NULL;
  }

  void removeGenFuncForTarget(hpvm::Target T) {
    switch (T) {
    case hpvm::None:
      return;
    case hpvm::CPU_TARGET:
      GenFuncs.CPUGenFunc = NULL;
      GenFuncInfo.cpu_hasCPUFunc = false;
      break;
    case hpvm::GPU_TARGET:
      GenFuncs.GPUGenFunc = NULL;
      GenFuncInfo.gpu_hasCPUFunc = false;
      break;
    case hpvm::CUDNN_TARGET:
      GenFuncs.CUDNNGenFunc = NULL;
      GenFuncInfo.cudnn_hasCPUFunc = false;
      break;
    case hpvm::TENSOR_TARGET:
      GenFuncs.PROMISEGenFunc = NULL;
      GenFuncInfo.promise_hasCPUFunc = false;
      break;
    case hpvm::CPU_OR_GPU_TARGET:
      assert(false &&
             "Removing genarated node function with dual tag instead of \
                CPU/GPU/SPIR/CUDNN/PROMISE\n");
    default:
      assert(false && "Unknown target for generated function\n");
    }
    return;
  }

  void setTargetHint(hpvm::Target T) { Hint = T; }

  hpvm::Target getTargetHint() const { return Hint; }

  bool isDummyNode() const { return isEntryNode() || isExitNode(); }

  bool isAllocationNode() {
    // If Allocation Property is defined then it is not an allocation node
    return PropertyList.count(Allocation) != 0;
  }
  inline void setRank(unsigned r);
  inline bool isEntryNode() const;
  inline bool isExitNode() const;
  inline DFEdge *getInDFEdgeAt(unsigned inPort);
  inline DFEdge *getExtendedInDFEdgeAt(unsigned inPort);
  inline DFEdge *getOutDFEdgeAt(unsigned outPort);
  inline DFEdge *getExtendedOutDFEdgeAt(unsigned outPort);
  inline std::map<unsigned, unsigned> getInArgMap();
  inline std::map<unsigned, std::pair<Value *, unsigned>> getSharedInArgMap();
  inline std::vector<unsigned> getOutArgMap();
  inline int getAncestorHops(DFNode *N);
  inline bool hasSideEffects();

  virtual void applyDFNodeVisitor(DFNodeVisitor &V) = 0;
  //  virtual void applyDFEdgeVisitor(DFEdgeVisitor &V) = 0;

  void clearGraphElements() {
    Successors.clear();
    InDFEdges.clear();
    OutDFEdges.clear();
    Parent = NULL;
  }
  
  bool checkForwardPath(DFNode *N) {
    assert(!isRoot() && !N->isRoot() &&
           "This function cannot be called on Root nodes!\n");
    assert(getParent() == N->getParent() &&
           "This function can only be called on two nodes that have the same "
           "parent!\n");
    DEBUG(errs() << "\t\tcheckForwardPath() ? " << getFuncPointer()->getName()
                 << " -> " << N->getFuncPointer()->getName() << "\n");
    for (auto si = successors_begin(); si != successors_end(); ++si) {
      DFNode *NI = *si;
      DEBUG(errs() << "\t\t\t" << NI->getFuncPointer()->getName() << "\n");
      if (NI == N) {
        return true;
      } else if (NI->isDummyNode()) {
        continue;
      } else {
        if (NI->checkForwardPath(N))
          return true;
      }
    }
    return false;
  }
};

/*****************************************************
 * DFInternalNode class implementation
 *****************************************************/
class DFInternalNode : public DFNode {

private:
  DFGraph *childGraph; ///< Pointer to dataflow graph

  // Constructor
  DFInternalNode(IntrinsicInst *II, Function *FuncPointer, hpvm::Target Hint,
                 DFInternalNode *Parent, int NumOfDim,
                 std::vector<Value *> DimLimits)
      : DFNode(II, FuncPointer, Hint, Parent, NumOfDim, DimLimits, InternalNode) {

    childGraph = new DFGraph(this);
  }

public:
  static DFInternalNode *
  Create(IntrinsicInst *II, Function *FuncPointer,
         hpvm::Target Hint = hpvm::CPU_TARGET, DFInternalNode *Parent = NULL,
         int NumOfDim = 0,
         std::vector<Value *> DimLimits = std::vector<Value *>()) {

    return new DFInternalNode(II, FuncPointer, Hint, Parent, NumOfDim,
                              DimLimits);
  }

  static bool classof(const DFNode *N) { return N->getKind() == InternalNode; }

  void addChildToDFGraph(DFNode *N) { childGraph->addChildDFNode(N); }

  void removeChildFromDFGraph(DFNode *N) { childGraph->removeChildDFNode(N); }

  inline void addEdgeToDFGraph(DFEdge *E);

  void removeEdgeFromDFGraph(DFEdge *E) { childGraph->removeEdge(E); }

  DFGraph *getChildGraph() const { return childGraph; }

  bool isChildGraphStreaming() { return childGraph->isStreaming(); }

  inline void applyDFNodeVisitor(DFNodeVisitor &V); /*virtual*/
  //  inline void applyDFEdgeVisitor(DFEdgeVisitor &V); /*virtual*/
};

/*****************************************************
 * DFLeafNode class implementation
 *****************************************************/
class DFLeafNode : public DFNode {

private:
  // Constructor
  DFLeafNode(IntrinsicInst *II, Function *FuncPointer, hpvm::Target Hint,
             DFInternalNode *Parent, int NumOfDim = 0,
             std::vector<Value *> DimLimits = std::vector<Value *>())
      : DFNode(II, FuncPointer, Hint, Parent, NumOfDim, DimLimits, LeafNode) {}

public:
  static DFLeafNode *
  Create(IntrinsicInst *II, Function *FuncPointer, hpvm::Target Hint,
         DFInternalNode *Parent, int NumOfDim = 0,
         std::vector<Value *> DimLimits = std::vector<Value *>()) {
    return new DFLeafNode(II, FuncPointer, Hint, Parent, NumOfDim, DimLimits);
  }

  static bool classof(const DFNode *N) { return N->getKind() == LeafNode; }

  inline void applyDFNodeVisitor(DFNodeVisitor &V); /*virtual*/
  //  inline void applyDFEdgeVisitor(DFEdgeVisitor &V); /*virtual*/
};

// DFEdge represents a single HPVM Dataflow Edge in LLVM.
//
// A Dataflow Edge basically consists of
// 1. Pointer to the dataflow node that is the source of this edge
// 2. Pointer to the dataflow node that is the destination of this edge
// 3. Type of the edge. The type of the edge is one of the following
//    - one to one : the edge connects only the "same" (numbered) instances
//                   between the source and destination nodes
//    - all to all : the edge connects all the onstances of the source with all
//                   the instances of the destination dataflow node (one to all
//                   is a special case of this type, sith the source or the
//                   destination dataflow node having multiplicity 1)
// 4. Location of the transfered data in the output of the source dataflow node
// 5. Location of the transfered data in the input of the destination dataflow
//    node

class DFEdge {
private:
  // Important things that make up a Dataflow Edge
  DFNode *SrcDF;           ///< Pointer to source dataflow Node
  DFNode *DestDF;          ///< Pointer to destination dataflow Node
  bool EdgeType;           ///< ONE_TO_ONE or ALL_TO_ALL
  unsigned SourcePosition; ///< Position of data in the output of source
                           ///< DFnode
  unsigned DestPosition;   ///< Position of data in the input of
                           ///< destination DFnode
  Type *ArgType;           ///< Type of the argument
  bool isStreaming;        ///< Is this an streaming edge

  // Functions
  DFEdge(DFNode *_SrcDF, DFNode *_DestDF, bool _EdgeType,
         unsigned _SourcePosition, unsigned _DestPosition, Type *_ArgType,
         bool _isStreaming)
      : SrcDF(_SrcDF), DestDF(_DestDF), EdgeType(_EdgeType),
        SourcePosition(_SourcePosition), DestPosition(_DestPosition),
        ArgType(_ArgType), isStreaming(_isStreaming) {}

public:
  // TODO: Decide whether we need this type
  //  typedef enum {ONE_TO_ONE = false, ALL_TO_ALL} DFEdgeType;

  static DFEdge *Create(DFNode *SrcDF, DFNode *DestDF, bool EdgeType,
                        unsigned SourcePosition, unsigned DestPosition,
                        Type *ArgType, bool isStreaming = false) {
    return new DFEdge(SrcDF, DestDF, EdgeType, SourcePosition, DestPosition,
                      ArgType, isStreaming);
  }

  DFNode *getSourceDF() const { return SrcDF; }

  void setSourceDF(DFNode *N) { SrcDF = N; }

  DFNode *getDestDF() const { return DestDF; }

  void setDestDF(DFNode *N) { DestDF = N; }

  bool getEdgeType() const { return EdgeType; }

  void setEdgeType(bool _EdgeType) { EdgeType = _EdgeType; }

  unsigned getSourcePosition() const { return SourcePosition; }

  void setSourcePosition(unsigned i) { SourcePosition = i; }

  unsigned getDestPosition() const { return DestPosition; }

  void setDestPosition(unsigned i) { DestPosition = i; }

  Type *getType() const { return ArgType; }

  bool isStreamingEdge() const { return isStreaming; }

  void setStreamingEdge(bool _isStreaming) { isStreaming = _isStreaming; }
};

//===--------------------- DFGraph Outlined Functions --------------===//
DFGraph::DFGraph(DFInternalNode *P) {
  Parent = P;
  // Create dummy entry and exit nodes and add them to the graph
  Entry =
      DFLeafNode::Create(NULL, Parent->getFuncPointer(), hpvm::None, Parent);
  Exit = DFLeafNode::Create(NULL, Parent->getFuncPointer(), hpvm::None, Parent);
  addChildDFNode(Entry);
  addChildDFNode(Exit);
}

void DFGraph::sortChildren() { std::sort(begin(), end(), compareRank); }

bool DFGraph::compareRank(DFNode *A, DFNode *B) {
  return A->getRank() < B->getRank();
}

bool DFGraph::isStreaming() {
  for (auto E : DFEdgeList) {
    if (E->isStreamingEdge())
      return true;
  }
  return false;
}

bool DFGraph::removeEdge(const DFEdge *E) {
  dfedge_iterator position = std::find(dfedge_begin(), dfedge_end(), E);
  if (position != dfedge_end()) // the edge was found
    DFEdgeList.erase(position);

  DFNode *S = E->getSourceDF();
  DFNode *D = E->getDestDF();
  S->removeSuccessor(D);
  S->removeOutDFEdge(E);
  D->removeInDFEdge(E);

  if (position == dfedge_end())
    DEBUG_WITH_TYPE(
        "dfgraph",
        errs() << "DFGraph::removeEdge called with non-existent edge.\n");
  return position != dfedge_end();
}

//===--------------------- DFNode Outlined Functions --------------===//
DFNode::DFNode(IntrinsicInst *_II, Function *_FuncPointer, hpvm::Target _Hint,
               DFInternalNode *_Parent, unsigned _NumOfDim,
               std::vector<Value *> _DimLimits, DFNodeKind _K)
    : II(_II), FuncPointer(_FuncPointer), Parent(_Parent), NumOfDim(_NumOfDim),
      DimLimits(_DimLimits), Kind(_K) {

  Type *Ty = FuncPointer->getFunctionType()->getReturnType();

  // Allow the return type to be void too, in the hHPVM IR. If return type is
  // void, create an empty struct type and keep that as the return type of the
  // node.
  if (Ty->isVoidTy())
    Ty = StructType::get(Ty->getContext(), true);

  // All nodes output type must always be a struct type.
  assert(isa<StructType>(Ty) && "Invalid return type of a dataflow node");

  // Check that the number of dimensions is correct
  assert(NumOfDim <= 3 && "Invalid num of dimensions for dataflow node!");

  // Check that the number of dimensions is correct
  assert(DimLimits.size() == NumOfDim &&
         "Incompatible num of dimensions and dimension limits for DFNode!");

  OutputType = cast<StructType>(Ty);
  Level = (_Parent) ? _Parent->getLevel() + 1 : 0;
  Rank = 0;

  Tag = hpvm::None;
  GenFuncs.CPUGenFunc = NULL;
  GenFuncs.GPUGenFunc = NULL;
  GenFuncs.CUDNNGenFunc = NULL;
  GenFuncs.PROMISEGenFunc = NULL;

  GenFuncInfo.cpu_hasCPUFunc = false;
  GenFuncInfo.gpu_hasCPUFunc = false;
  GenFuncInfo.cudnn_hasCPUFunc = false;
  GenFuncInfo.promise_hasCPUFunc = false;
  Root = false;
  Hint = _Hint;
}

void DFNode::addToNodeGraph(DFInternalNode *P) {
  Parent = P;
  P->addChildToDFGraph(this);
}

void DFNode::setRank(unsigned r) {
  Rank = r;
  // Update rank of successors
  for (outdfedge_iterator i = outdfedge_begin(), e = outdfedge_end(); i != e;
       ++i) {
    DFEdge *E = *i;
    DFNode *D = E->getDestDF();
    if (D->getRank() <= r)
      D->setRank(r + 1);
  }
}

bool DFNode::isEntryNode() const {
  if (Parent == NULL)
    return false;
  return Parent->getChildGraph()->isEntry(this);
}

bool DFNode::isExitNode() const {
  if (Parent == NULL)
    return false;
  return Parent->getChildGraph()->isExit(this);
}

DFEdge *DFNode::getInDFEdgeAt(unsigned inPort) {

  // If it is not a dummy node, then check if inPort should be less than the
  // number of arguments in the associated function.
  assert((inPort < FuncPointer->getFunctionType()->getNumParams() ||
          isDummyNode()) &&
         "Invalid input port request!");

  for (indfedge_iterator i = indfedge_begin(), e = indfedge_end(); i != e;
       ++i) {
    DFEdge *E = *i;
    if (inPort == E->getDestPosition())
      return E;
  }
  return NULL;
}

DFEdge *DFNode::getExtendedInDFEdgeAt(unsigned inPort) {
  DFEdge *Ein = getInDFEdgeAt(inPort);
  DFNode *sn = Ein->getSourceDF();
  if (!sn->isEntryNode())
    return Ein;

  DFNode *pn = getParent();
  if (pn->isRoot())
    return Ein;

  DFEdge *PEin = pn->getInDFEdgeAt(inPort);
  DFInternalNode *SPN = dyn_cast<DFInternalNode>(PEin->getSourceDF());
  if (!SPN)
    return PEin;

  unsigned outPort = PEin->getSourcePosition();
  return SPN->getChildGraph()->getExit()->getInDFEdgeAt(outPort);
}

DFEdge *DFNode::getOutDFEdgeAt(unsigned outPort) {

  // Cannot perform check for the number of outputs here,
  // it depends on the node's return type

  // ADEL: Note - kept this same as non-release branch because wasn't sure how
  // that change would affect Scheduler backend. The release-branch-change was
  // made by Akash for Movidius stuff.
  for (outdfedge_iterator i = outdfedge_begin(), e = outdfedge_end(); i != e;
       ++i) {
    DFEdge *E = *i;
    if (outPort == E->getSourcePosition())
      return E;
  }
  return NULL;
}

DFEdge *DFNode::getExtendedOutDFEdgeAt(unsigned outPort) {
  DFEdge *Eout = getOutDFEdgeAt(outPort);
  if (!Eout->getDestDF()->isExitNode())
    return Eout;

  DFNode *pn = getParent();
  if (pn->isRoot())
    return Eout;

  DFEdge *PEout = pn->getOutDFEdgeAt(outPort);
  DFInternalNode *DPN = dyn_cast<DFInternalNode>(PEout->getDestDF());
  if (!DPN)
    return PEout;

  unsigned inPort = PEout->getDestPosition();
  return DPN->getChildGraph()->getEntry()->getOutDFEdgeAt(inPort);
}

// Ignore Allocation Nodes
std::map<unsigned, unsigned> DFNode::getInArgMap() {
  std::map<unsigned, unsigned> map;
  for (unsigned i = 0; i < InDFEdges.size(); i++) {
    DFEdge *E = getInDFEdgeAt(i);
    if (E->getSourceDF()->isAllocationNode())
      continue;
    unsigned pos = E->getSourcePosition();
    map[i] = pos;
  }
  return map;
}

// Only Allocation Nodes - only detect relevant indices
std::map<unsigned, std::pair<Value *, unsigned>> DFNode::getSharedInArgMap() {
  std::map<unsigned, std::pair<Value *, unsigned>> map;
  for (unsigned i = 0; i < InDFEdges.size(); i++) {
    DFEdge *E = getInDFEdgeAt(i);
    if (!E->getSourceDF()->isAllocationNode())
      continue;
    map[i] = std::pair<Value *, unsigned>(NULL, 0);
  }
  return map;
}

std::vector<unsigned> DFNode::getOutArgMap() {
  std::vector<unsigned> map(OutDFEdges.size());
  for (unsigned i = 0; i < OutDFEdges.size(); i++) {
    DFEdge *E = getOutDFEdgeAt(i);
    unsigned pos = E->getDestPosition();
    map[pos] = i;
  }
  return map;
}

int DFNode::getAncestorHops(DFNode *N) {
  DFNode *temp = this;
  int hops = 0;
  while (temp != NULL) {
    if (temp == N)
      return hops;
    temp = temp->getParent();
    hops++;
  }
  // N not found among the ancestors
  // Return -1 to indicate that N is not an ancestor.
  return -1;
}

// Returns true if the node does not have any side effects
// This shall mostly be a work in progress as the conditions for determining "no
// side effects" property precisely can be large. List the checks here
// Check #1: No incoming pointer argument
// TODO: More precidse can be that no incoming edge is a pointer with out
// attribute
bool DFNode::hasSideEffects() {
  bool hasSideEffects = false;
  // Check #1: No incoming pointer argument
  for (DFEdge *E : this->InDFEdges) {
    hasSideEffects |= E->getType()->isPointerTy();
  }
  return hasSideEffects;
}

//===--------------------- DFInternalNode Outlined Functions --------------===//
void DFInternalNode::addEdgeToDFGraph(DFEdge *E) {
  // errs() << "----- ADD EDGE TO DFGRAPH\n";
  DFNode *S = E->getSourceDF();
  DFNode *D = E->getDestDF();
  // errs() << "INIT SOURCE NODE: " << S << "\n";
  // errs() << "INIT DEST NODE: " << D << "\n";

  assert(std::find(childGraph->begin(), childGraph->end(), S) !=
             childGraph->end() &&
         "Source node not found in child dataflow graph!");

  assert(std::find(childGraph->begin(), childGraph->end(), D) !=
             childGraph->end() &&
         "Destination node not found in child dataflow graph!");

  // Update Graph
  childGraph->addDFEdge(E);

  // Update source and destination nodes
  S->addSuccessor(D);
  S->addOutDFEdge(E);
  D->addInDFEdge(E);

  // errs() << "SET SOURCE NODE: " << E->getSourceDF() << "\n";
  // errs() << "SET DEST NODE: " << E->getDestDF() << "\n";

  // Update Rank
  if (D->getRank() <= S->getRank())
    D->setRank(S->getRank() + 1);
}

//===------------------------ Property Objects ---------------------------====//
class AllocationNodeProperty {
public:
  typedef std::pair<DFEdge *, Value *> AllocationType;
  typedef std::vector<AllocationType> AllocationListType;

private:
  AllocationListType AllocationList;

public:
  AllocationNodeProperty() {}

  unsigned getNumAllocations() { return AllocationList.size(); }

  AllocationListType getAllocationList() { return AllocationList; }

  void insertAllocation(DFEdge *E, Value *V) {
    AllocationList.push_back(AllocationType(E, V));
  }
};

//===-------------------------- Visitor Classes ---------------------------===//
// Visitor for DFNode objects
class DFNodeVisitor {
public:
  virtual ~DFNodeVisitor() {}
  virtual void visit(DFInternalNode *N) = 0;
  virtual void visit(DFLeafNode *N) = 0;
};

void DFInternalNode::applyDFNodeVisitor(DFNodeVisitor &V) { V.visit(this); }

void DFLeafNode::applyDFNodeVisitor(DFNodeVisitor &V) { V.visit(this); }

class DFTreeTraversal : public DFNodeVisitor {

public:
  virtual ~DFTreeTraversal() {}

  virtual void visit(DFInternalNode *N) {
    DEBUG(errs() << "Visited Node (I) - " << N->getFuncPointer()->getName()
                 << "\n");
    for (DFGraph::children_iterator i = N->getChildGraph()->begin(),
                                    e = N->getChildGraph()->end();
         i != e; ++i) {
      DFNode *child = *i;
      child->applyDFNodeVisitor(*this);
    }
  }

  virtual void visit(DFLeafNode *N) {
    DEBUG(errs() << "Visited Node (L) - " << N->getFuncPointer()->getName()
                 << "\n");
  }
};

class FollowSuccessors : public DFNodeVisitor {

public:
  virtual void visit(DFInternalNode *N) {
    /*DFNodeListType L; // Empty List that will contain the sorted elements
    DFNodeListType S; // Set of all nodes with no incoming edges

    // Add Entry node to S.
    S.push_back(N->getChildGraph()->getEntry());

    while(!S.empty()) {
      DFNode* X = S.pop_back();
      for (DFEdge* E: in X->getOutDFEdges()) {
        DFNode* dest = E->getDestDF();
        if
      }
    }*/
    DEBUG(errs() << "Visited Node (I) - " << N->getFuncPointer()->getName()
                 << "\n");
    for (DFInternalNode::successor_iterator i = N->successors_begin(),
                                            e = N->successors_end();
         i != e; ++i) {
      /* Traverse the graph.
       * Choose the kind of traversal we want
       * Do we do a DAG kind of traversal?
       */
    }
  }

  virtual void visit(DFLeafNode *N) {
    DEBUG(errs() << "Visited Node (L) - " << N->getFuncPointer()->getName()
                 << "\n");
  }
};

class ReplaceNodeFunction : public DFNodeVisitor {

protected:
  // Member variables
  Module &M;
  Function *F = NULL; // Function to replace
  Function *G = NULL; // Function to be replaced by

  // Functions
  void replaceNodeFunction(DFInternalNode *N) {
    if (N->getFuncPointer() == F)
      N->setFuncPointer(G);
  }

  void replaceNodeFunction(DFLeafNode *N) {
    if (N->getFuncPointer() == F)
      N->setFuncPointer(G);
  }

  ~ReplaceNodeFunction(){};

public:
  // Constructor
  ReplaceNodeFunction(Module &_M, Function *_F, Function *_G)
      : M(_M), F(_F), G(_G) {}

  ReplaceNodeFunction(Module &_M) : M(_M), F(NULL), G(NULL) {}

  void setF(Function *_F) { F = _F; }

  void setG(Function *_G) { G = _G; }

  virtual void visit(DFInternalNode *N) {
    DEBUG(errs() << "Start: Replace Node Function for Node (I) - "
                 << N->getFuncPointer()->getName() << "\n");

    // Follows a bottom-up approach.
    for (DFGraph::children_iterator i = N->getChildGraph()->begin(),
                                    e = N->getChildGraph()->end();
         i != e; ++i) {
      DFNode *child = *i;
      child->applyDFNodeVisitor(*this);
    }
    // Generate code for this internal node now. This way all the cloned
    // functions for children exist.
    replaceNodeFunction(N);
    DEBUG(errs() << "DONE: Replace Node Function for Node (I) - "
                 << N->getFuncPointer()->getName() << "\n");
  }

  virtual void visit(DFLeafNode *N) {
    DEBUG(errs() << "Start: Replace Node Function for Node (L) - "
                 << N->getFuncPointer()->getName() << "\n");
    replaceNodeFunction(N);
    DEBUG(errs() << "DONE: Replace Node Function for Node (L) - "
                 << N->getFuncPointer()->getName() << "\n");
  }
};

/*
// Visitor for DFEdge objects
class DFEdgeVisitor {
public:
  virtual void visit(DFEdge* E) = 0;
};
*/

//===--------------------------------------------------------------------===//
// GraphTraits specializations for DFNode graph (DFG)
//===--------------------------------------------------------------------===//

template <> struct GraphTraits<DFNode *> {
  typedef DFNode *NodeRef;
  typedef typename DFNode::successor_iterator ChildIteratorType;

  static inline ChildIteratorType child_begin(NodeRef N) {
    return N->successors_begin();
  }
  static inline ChildIteratorType child_end(NodeRef N) {
    return N->successors_end();
  }
};

template <> struct GraphTraits<DFGraph *> : public GraphTraits<DFNode *> {
  typedef typename DFGraph::children_iterator nodes_iterator;

  static NodeRef getEntryNode(DFGraph *G) { return G->front(); }

  static nodes_iterator nodes_begin(DFGraph *G) { return G->begin(); }

  static inline nodes_iterator nodes_end(DFGraph *G) { return G->end(); }
};

template <> struct DOTGraphTraits<DFGraph *> : public DefaultDOTGraphTraits {

  DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

  static std::string getGraphName(DFGraph *G) {
    DFInternalNode *Parent = G->getParent();
    if (Parent != NULL)
      return Parent->getFuncPointer()->getName();
    else
      return "Dataflow Graph";
  }

  static std::string getGraphProperties(DFGraph *G) {
    return "\tcompound=true;";
  }

  std::string getNodeLabel(DFNode *N, DFGraph *G) {
    if (N->isEntryNode())
      return "Entry";
    if (N->isExitNode())
      return "Exit";
    return N->getFuncPointer()->getName();
  }

  static bool isCompoundNode(DFNode *N) {
    bool ret = isa<DFInternalNode>(N);
    return ret;
  }

  static DFGraph *getSubGraph(DFNode *N, DFGraph *G) {
    DFInternalNode *IN = dyn_cast<DFInternalNode>(N);
    assert(IN && "No subgraph for leaf dataflow node!");
    return IN->getChildGraph();
  }

  static DFNode *getAnySimpleNodeForSrc(DFNode *N) {
    DFInternalNode *IN = dyn_cast<DFInternalNode>(N);
    assert(IN && "No subgraph for leaf dataflow node!");
    return IN->getChildGraph()->getExit();
  }

  static DFNode *getAnySimpleNodeForDest(DFNode *N) {
    DFInternalNode *IN = dyn_cast<DFInternalNode>(N);
    assert(IN && "No subgraph for leaf dataflow node!");
    return IN->getChildGraph()->getEntry();
  }

  static std::string getNodeAttributes(DFNode *N, DFGraph *G) {
    std::string Attr = "";
    raw_string_ostream OS(Attr);
    OS << "shape=oval";
    return OS.str();
  }

  static std::string getEdgeAttributes(DFNode *N, DFNode::successor_iterator SI,
                                       DFGraph *G) {
    std::string Attr = "";
    raw_string_ostream OS(Attr);
    bool comma = false;
    if (DFInternalNode *SrcNode = dyn_cast<DFInternalNode>(N)) {
      comma = true;
      OS << "ltail=cluster";
      OS << static_cast<const void *>(SrcNode);
    }
    DFNode *DN = *SI;
    if (DFInternalNode *DestNode = dyn_cast<DFInternalNode>(DN)) {
      if (comma)
        OS << ", ";
      OS << "lhead=cluster";
      OS << static_cast<const void *>(DestNode);
    }
    return OS.str();
  }

  static void addCustomGraphFeatures(DFGraph *G, GraphWriter<DFGraph *> &GW) {}
};

inline void viewDFGraph(DFGraph *G) {
  DFInternalNode *Parent = G->getParent();
  assert(Parent && "Child Graph with no parent\n");

  llvm::WriteGraph(G,
                   (Parent->isRoot() ? "Root_Dataflow_Graph"
                                     : (Parent->getFuncPointer()->getName())));
}

// Check if there exists a forward path in the graph between this node
// and node N
// TODO: Add a visitedNodes set to avoid revisiting the same node twice
static bool existsForwardPath(llvm::DFNode *SN, llvm::DFNode *DN) {
  // If Src and Dst are not at the same level in the graph, we first
  // need to find the one with the smaller level (higher in the graph heirarchy)
  // and then traverse the parents of the other node until we reach the
  // one which has the same level. Once we have a Src/Dst that are at the same
  // level if they have the same parent, we call checkForwardPath() to see if
  // there exists a path between them, otherwise we recursively call this
  // function on the direct parents of the two nodes. If we get to the points
  // where the two nodes are Roots, we assume that they have a path between.
  // TODO: Add ability to check if two DFGs can be run in parallel.
  auto SrcLvl = SN->getLevel();
  auto DstLvl = DN->getLevel();
  llvm::DFNode *Src = SN;
  llvm::DFNode *Dst = DN;
  DEBUG(errs() << "\texistForwardPath() ? " << Src->getFuncPointer()->getName()
               << " -> " << Dst->getFuncPointer()->getName() << "\n");
  if (SrcLvl < DstLvl) {
    do {
      Dst = cast<llvm::DFNode>(Dst->getParent());
    } while (Dst->getLevel() > SrcLvl);
  } else if (DstLvl < SrcLvl) {
    do {
      Src = cast<llvm::DFNode>(Src->getParent());
    } while (Src->getLevel() > DstLvl);
  }
  assert(Src->getLevel() == Dst->getLevel() &&
         "Src and Dst levels should be equal here. Something isn't right!");
  if (Src->isRoot() || Dst->isRoot()) {
    // TODO: if this is the case, we should check if the DFGs can be parallel or
    // not.
    DEBUG(errs() << "The two nodes belong to different DFGs, assume that they "
                    "are sequential and return true!\n");
    return true;
  }
  if (Src->getParent() == Dst->getParent()) {
    return Src->checkForwardPath(Dst);
  }
  return existsForwardPath(Src->getParent(), Dst->getParent());
}


} // namespace llvm

#endif
