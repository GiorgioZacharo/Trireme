#include "SupportHPVM/DFGraph.h"
#include "SupportHPVM/DFGTreeTraversal.h"
#include <string>
#include <map>

struct NodeWithTimes {
  std::string FName;
  long SWTime;
  long HWTime;
};
namespace dfg2llvm {
// Graph Traversal to std::map func name to node ptr
class NodeNameTraversal : public DFGTreeTraversal {
private:
  std::vector<NodeWithTimes> FNList;
  std::map<std::string, DFNode *> FNameToNodemap;

public:
  NodeNameTraversal(Module &_M, BuildDFG &_DFG, std::vector<NodeWithTimes> &_FNList)
      : DFGTreeTraversal(_M, _DFG), FNList(_FNList) {}
  // skip internal nodes since we are not interested in them
  void process(DFInternalNode *N) {}
  // check if leaf node function name exists in std::vector, if it does create a std::map
  // entry between the function name and the node's parent.
  void process(DFLeafNode *N); 
  std::map<std::string, DFNode *> getFNameToNodeMap() { return FNameToNodemap; }
};
}
