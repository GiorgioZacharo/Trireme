/********************************************************************/
/******* HPVM-Trireme tool to generate Parallel node list *******/
/********************************************************************/
// A tool that takes the hpvm intermediate representation file that
// includes the profile information (output of GenHPVM), and also takes
// the list of functions as a txt file. The output is a txt file
// containing a table with the parallel functions.
//
// e.g. hpvm-trireme main.hpvm.ll tasks_extracted.txt

#define DEBUG_TYPE "hpvm-trireme"
#include "hpvm-trireme.h"
#include "BuildDFG/BuildDFG.h"
#include "SupportHPVM/HPVMUtils.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/ToolOutputFile.h"
#include <cxxabi.h>
#include <fstream>
#include <iostream>
#include <string>

using namespace llvm;
using namespace builddfg;
using namespace hpvm;
using namespace dfg2llvm;
using namespace std;

// InputLLVMFile - The file to read the bitcode from. Required.
//                 Expecting file output of GenHPVM (after building with
//                 PROF=use).
static cl::opt<string>
    InputLLVMFile(cl::Positional, cl::desc("<input bitcode file>"),
                  cl::Required, cl::value_desc("input LLVM Bitcode file name"));

// InputFNList - The file to read the function list from. Required.
static cl::opt<string>
    InputFNList(cl::Positional, cl::desc("<input function list (.txt)>"),
                cl::Required, cl::value_desc("input function list file name"));

// -independent-dfgs - Declare that all DFGs in application are independent.
static cl::opt<bool>
    IndepDFGs("independent-dfgs",
              cl::desc("Specifies that the DFGs in this application are all "
                       "independent. Defaults to false."),
              cl::init(false));

// OutputFName - The name to be used for the first output file.
static cl::opt<string>
    Output1FName("o1",
                 cl::desc("Specify output file name for parallel tasks table. "
                          "Defaults to \'parallel_tasks.txt\'."),
                 cl::value_desc("ofile"), cl::init("parallel_tasks.txt"));

// OutputFName - The name to be used for the first output file.
static cl::opt<string>
    Output2FName("o2",
                 cl::desc("Specify output file name for loop parallelism "
                          "table. Defaults to \'parallel_loops.txt\'."),
                 cl::value_desc("ofile"), cl::init("parallel_loops.txt"));

// OutputFName - The name to be used for the first output file.
static cl::opt<string>
    Output3FName("o3",
                 cl::desc("Specify output file name for Earliest Start Time "
                          "table. Defaults to \'earliest_start.txt\'."),
                 cl::value_desc("ofile"), cl::init("earliest_start.txt"));
static cl::opt<string>
    Delimiter("d",
              cl::desc("Specify delimiter for output file. Escape sequences "
                       "are not allowed. Default is \'tab\'."),
              cl::value_desc("delim"), cl::init("\t"));

// helper functions
void fatalError(const string &msg) {
  cerr << "FATAL: " << msg << endl;
  exit(EXIT_FAILURE);
}

void NodeNameTraversal::process(DFLeafNode *N) {
  DEBUG(errs() << "Processing leaf node: " << N->getFuncPointer()->getName()
               << "\n");
  if (N->isDummyNode())
    return;
  std::string NName = N->getFuncPointer()->getName();
  if (find_if(FNList.begin(), FNList.end(), [NName](NodeWithTimes N) {
        return N.FName == NName;
      }) != FNList.end()) {
    DEBUG(errs() << "Found node in list, adding to std::map!\n");
    FNameToNodemap[NName] = N;
  }
  // TODO: also map the node to any functions invoked in it.
}

// Function that returns a mapping between the node's wrappers and the duration
void setNodeDurations(vector<NodeWithTimes> NList,
                      map<DFNode *, float> &SWNodeDuration,
                      map<DFNode *, float> &HWNodeDuration,
                      map<string, DFNode *> NNameMap) {
  for (auto NT : NList) {
    DFNode *N = NNameMap[NT.FName];
    SWNodeDuration[N] = NT.SWTime;
    HWNodeDuration[N] = NT.HWTime;
  }
}
// Function that calculates the EST of the child graph of a given DFInternalNode
void calculateChildGraphEST(DFInternalNode *N,
                            map<DFNode *, float> &NodeDuration,
                            map<DFNode *, float> &NodeEFT,
                            map<DFNode *, float> &NodeEST) {
  // Get the child graph of the internal node and sort the children in preorder
  DFGraph *G = N->getChildGraph();
  G->sortChildren();

  auto *Entry = G->getEntry();
  auto *Exit = G->getExit();
  vector<DFNode *> ExitingNodes;

  // Visit Entry node first. Its start time is the start time of its
  // parent, and its duration is 0. The root start time will be initialized
  // outside this function.
  NodeEST[Entry] = NodeEST[N];
  NodeDuration[Entry] = 0.0;
  NodeEFT[Entry] = NodeEST[Entry];

  // For each child, do the calculation. If internal node, invoke the function
  // recursively.
  for (auto ci = G->begin(), ce = G->end(); ci != ce; ++ci) {
    DFNode *C = *ci;
    // Entry and Esit nodes will be handled separately, so skip them.
    if (C->isEntryNode() || C->isExitNode()) {
      assert(C->getParent() == N &&
             "The Parent of the Entry/Exit Node is not N!");
      continue;
    }
    // For each node, find the largest Earliest Finish Time of all its
    // predecessors. That will be its Earliest Start Time.
    float maxEFT = 0.0;
    for (auto ei = C->indfedge_begin(), ee = C->indfedge_end(); ei != ee;
         ++ei) {
      DFNode *SN = (*ei)->getSourceDF();
      if (NodeEFT.find(SN) == NodeEFT.end())
        fatalError("Expecting all predecessors of node to already be visited!");
      if (NodeEFT[SN] > maxEFT)
        maxEFT = NodeEFT[SN];
    }
    NodeEST[C] = maxEFT;

    // For an internal node, we recursively call this function on the
    // internal node.
    if (auto *IN = dyn_cast<DFInternalNode>(C)) {
      NodeDuration[IN] = 0.0;
      calculateChildGraphEST(IN, NodeDuration, NodeEFT, NodeEST);
      // assert(NodeDuration[IN] != 0 &&
      //"The internal node duration was not calculated!\n");
    }

    // The EFT is the EST + the duration.
    NodeEFT[C] = NodeEST[C] + NodeDuration[C];

    // Any nodes that do not have any successors are exiting nodes. Keep track
    // of those for when we calculate the information for the ExitNode.
    if (C->successors_size() == 0)
      ExitingNodes.push_back(C);
  }

  // We are visiting the exit node separately in case it is not connected to the
  // rest of the graph. This way we ensure that we are traversing it at the end.
  // Calculate the Earliest Start Time of the Exit node.
  // After going through the incoming edges for the Exit Node, we need to also
  // check the ExitingNodes vector for any nodes that may not be connected to
  // the Exit Node directly.
  float maxEFT = 0.0;
  for (auto ei = Exit->indfedge_begin(), ee = Exit->indfedge_end(); ei != ee;
       ++ei) {
    DFNode *SN = (*ei)->getSourceDF();
    if (NodeEFT[SN] > maxEFT)
      maxEFT = NodeEFT[SN];
  }
  for (auto *SN : ExitingNodes) {
    if (NodeEFT[SN] > maxEFT)
      maxEFT = NodeEFT[SN];
  }
  NodeEST[Exit] = maxEFT;
  // For the exit node, we set the EFT to be the same as the EST. The Internal
  // Node EFT is the same.
  assert(Exit->getParent() == N && "The Parent of the Entry Node is not N!");
  NodeDuration[Exit] = 0.0;
  NodeEFT[Exit] = NodeEST[Exit];
  NodeEFT[N] = NodeEFT[Exit];

  // Finally, use the end time and start time to calculate the internal node's
  // duration.
  NodeDuration[N] = NodeEFT[N] - NodeEST[N];
  DEBUG(errs() << "Calculated Duration for Node: "
               << N->getFuncPointer()->getName() << ": " << NodeDuration[N]
               << "\n");
}
// Overloading << operator for NodeWithTimes struct
std::ostream &operator<<(std::ostream &os, const NodeWithTimes &N) {
  return os << "{" << N.FName << ", " << N.SWTime << ", " << N.HWTime << "}";
}

void dumpVector(vector<NodeWithTimes> V) {
  cout << "\n=============================================\n";
  cout << "Printing Vector:\n";
  for (auto N : V) {
    cout << N << ", ";
  }
  cout << "\n";
  cout << "==============================================\n";
}
void dumpVector(vector<string> V) {
  cout << "\n=============================================\n";
  cout << "Printing Vector:\n";
  for (auto N : V) {
    cout << N << ", ";
  }
  cout << "\n";
  cout << "==============================================\n";
}

void dump2DVector(vector<vector<string>> V) {
  cout << "\n=============================================\n";
  cout << "Printing Vector:\n";
  for (auto VV : V) {
    for (auto N : VV) {
      cout << N << ", ";
    }
    cout << "\n";
  }
  cout << "==============================================\n";
}

void dumpMap(map<string, DFNode *> M) {
  cout << "\n=============================================\n";
  cout << "Printing Map:\n";
  for (auto I : M) {
    cout << I.first << " ==> ";
    cout << I.second->getFuncPointer()->getName().str() << "\n";
  }
  cout << "==============================================\n";
}
void dumpMap(map<DFNode *, float> M) {
  cout << "\n=============================================\n";
  cout << "Printing Map:\n";
  for (auto I : M) {
    cout << I.first->getFuncPointer()->getName().str() << " ==> ";
    cout << I.second << "\n";
  }
  cout << "==============================================\n";
}

void dumpVectorToFile(vector<vector<string>> V, string fname) {
  ofstream outFile(fname.c_str());
  if (Delimiter.find("\\") != string::npos) {
    cerr << "Invalid delimiter selected, defaulting to \'tab\'." << endl;
    cerr << "Note that escapse sequences cannot be provided as a delimiter. "
            "Omit -d to use \'tab\' in the future."
         << endl;
    Delimiter = "\t";
  }
  for (auto VV : V) {
    for (auto N : VV) {
      outFile << N << Delimiter;
    }
    outFile << "\n";
  }
}

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv,
                              "HPVM-Trireme Tool. Input bitcode "
                              "file is output of GenHPVM.");

  // Read in specified LLVM bitcode module
  LLVMContext Context;
  SMDiagnostic Err;
  unique_ptr<Module> M = parseIRFile(InputLLVMFile, Err, Context);
  if (!M.get())
    fatalError("Failed to open module " + InputLLVMFile);

  // Read txt file to get list of functions
  ifstream inFile(InputFNList.c_str());
  string FName;
  float SWTime;
  float HWTime;
  vector<NodeWithTimes> FNList;
  if (inFile.is_open()) {
    while (true) {
      inFile >> FName;
      if (inFile.eof())
        break;
      inFile >> SWTime;
      inFile >> HWTime;
      FName = FName.substr(1);
      DEBUG(errs() << "Adding: { " << FName << ", " << SWTime << ", " << HWTime
                   << "} to function list!\n");
      FNList.push_back({FName, SWTime, HWTime});
    }
  } else {
    fatalError("Unable to open file " + InputFNList);
  }

  dumpVector(FNList);

  // 2D vector that will track for each Function in the list, what other
  // functions can run in parallel with it.
  vector<vector<string>> ParallelFuncs;
  vector<vector<string>> ParallelLoops;
  vector<vector<string>> EST;

  // Build the HPVM DFG for the input file
  legacy::PassManager Passes;
  BuildDFG *DFG = new BuildDFG();
  Passes.add(DFG);
  Passes.run(*M);

  llvm::viewDFGraph(DFG->getRoot()->getChildGraph());

  vector<DFInternalNode *> Roots = DFG->getRoots();
  vector<DFInternalNode *> SortedRoots(Roots.size(), 0);

  if (Roots.size() > 1 && Roots[0]->getFuncPointer()->getName().contains(
                              "set_calibration_undistort")) {
    for (auto *Root : Roots) {
      auto RootName = Root->getFuncPointer()->getName();
      if (RootName.contains("feed_stereo_updateDB")) {
        assert(SortedRoots[0] == nullptr &&
               "Root 0 has already been filled!\n");
        SortedRoots[0] = Root;
      } else if (RootName.contains("EKFUpdateGraph_1")) {
        assert(SortedRoots[1] == nullptr &&
               "Root 1 has already been filled!\n");
        SortedRoots[1] = Root;
      } else if (RootName.contains("EKFUpdateGraph_2")) {
        assert(SortedRoots[2] == nullptr &&
               "Root 2 has already been filled!\n");
        SortedRoots[2] = Root;
      } else if (RootName.contains("EKFUpdateGraph_3")) {
        assert(SortedRoots[3] == nullptr &&
               "Root 3 has already been filled!\n");
        SortedRoots[3] = Root;
      } else if (RootName.contains("set_calibration_undistort")) {
        assert(SortedRoots[4] == nullptr &&
               "Root 4 has already been filled!\n");
        SortedRoots[4] = Root;
      }
    }
  } else {
    SortedRoots = Roots;
  }
  int i = 0;
  for (auto *Root : SortedRoots) {
    DEBUG(errs() << "Root " << i++ << ": " << Root->getFuncPointer()->getName()
                 << "\n");
  }

  // Create a maping from the input function names
  // to their corresponding DFG wrapper nodes.
  NodeNameTraversal NNT(*M, *DFG, FNList);
  for (auto Root : SortedRoots)
    NNT.visit(Root);
  map<string, DFNode *> FNameToNodeMap = NNT.getFNameToNodeMap();
  dumpMap(FNameToNodeMap);

  // Set the node durations
  map<DFNode *, float> SWNodeDuration, HWNodeDuration;
  setNodeDurations(FNList, SWNodeDuration, HWNodeDuration, FNameToNodeMap);

  // calculate the early start time of each node
  map<DFNode *, float> SWNodeEST, SWNodeEFT, HWNodeEST, HWNodeEFT;
  if (!IndepDFGs) {
    SWNodeEST[SortedRoots[0]] = 0.0;
    HWNodeEST[SortedRoots[0]] = 0.0;
    calculateChildGraphEST(SortedRoots[0], SWNodeDuration, SWNodeEFT,
                           SWNodeEST);
    calculateChildGraphEST(SortedRoots[0], HWNodeDuration, HWNodeEFT,
                           HWNodeEST);
    for (int i = 1; i < SortedRoots.size(); ++i) {
      SWNodeEST[SortedRoots[i]] = SWNodeEFT[SortedRoots[i - 1]];
      HWNodeEST[SortedRoots[i]] = HWNodeEFT[SortedRoots[i - 1]];
      calculateChildGraphEST(SortedRoots[i], SWNodeDuration, SWNodeEFT,
                             SWNodeEST);
      calculateChildGraphEST(SortedRoots[i], HWNodeDuration, HWNodeEFT,
                             HWNodeEST);
    }
  } else {
    for (int i = 0; i < SortedRoots.size(); ++i) {
      SWNodeEST[SortedRoots[i]] = 0.0;
      HWNodeEST[SortedRoots[i]] = 0.0;
      calculateChildGraphEST(SortedRoots[i], SWNodeDuration, SWNodeEFT,
                             SWNodeEST);
      calculateChildGraphEST(SortedRoots[i], HWNodeDuration, HWNodeEFT,
                             HWNodeEST);
    }
  }
  dumpMap(SWNodeEST);
  dumpMap(SWNodeEFT);
  dumpMap(HWNodeEST);
  dumpMap(HWNodeEFT);

  // Print the SW and HW early start time for each function
  for (auto FN : FNList) {
    string FName = FN.FName;
    if (FNameToNodeMap.find(FName) != FNameToNodeMap.end()) {
      vector<string> ESTVec;
      ESTVec.push_back("@" + FName);
      ESTVec.push_back(std::to_string(SWNodeEST[FNameToNodeMap[FName]]));
      ESTVec.push_back(std::to_string(HWNodeEST[FNameToNodeMap[FName]]));
      dumpVector(ESTVec);
      EST.push_back(ESTVec);
    }
  }

  // Determine loop and task parallelism between the input nodes
  for (auto FNi = FNList.begin(); FNi != FNList.end(); ++FNi) {
    string FName = FNi->FName;
    if (FNameToNodeMap.find(FName) != FNameToNodeMap.end()) {
      DFNode *SrcNode = FNameToNodeMap[FName];
      // Check loop parallelism
      if (SrcNode->getNumOfDim() > 0) {
        vector<string> ParLoopVec;
        ParLoopVec.push_back("@" + FName);
        ParLoopVec.push_back(std::to_string(SrcNode->getNumOfDim()));
        auto DimLimits = SrcNode->getDimLimits();
        for (auto DL : DimLimits) {
          if (ConstantInt *CI = dyn_cast<ConstantInt>(DL)) {
            ParLoopVec.push_back(std::to_string(CI->getSExtValue()));
          } else {
            ParLoopVec.push_back("NC");
          }
        }
        dumpVector(ParLoopVec);
        ParallelLoops.push_back(ParLoopVec);
      }
      // Check task parallelism with using node parent.
      vector<string> ParFuncVec;
      ParFuncVec.push_back("@" + FName);
      for (auto FNii = FNList.begin(); FNii != FNList.end(); ++FNii) {
        if (FNii == FNi)
          continue;
        string FName2 = FNii->FName;
        if (FNameToNodeMap.find(FName2) != FNameToNodeMap.end()) {
          DFNode *DstNode = FNameToNodeMap[FName2];
          DEBUG(errs() << "Checking " << FName << " and " << FName2 << ":\n");
          if (!existsForwardPath(SrcNode, DstNode) &&
              !existsForwardPath(DstNode, SrcNode)) {
            DEBUG(errs() << ">> parallel nodes!\n");
            ParFuncVec.push_back("@" + FName2);
          } else {
            DEBUG(errs() << ">> NOT parallel nodes!\n");
          }
        }
      }
      dumpVector(ParFuncVec);
      ParallelFuncs.push_back(ParFuncVec);
    }
  }
  dump2DVector(ParallelFuncs);
  dump2DVector(ParallelLoops);
  dump2DVector(EST);

  dumpVectorToFile(ParallelFuncs, Output1FName);
  dumpVectorToFile(ParallelLoops, Output2FName);
  dumpVectorToFile(EST, Output3FName);
}
