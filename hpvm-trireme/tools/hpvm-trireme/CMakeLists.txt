set(LLVM_LINK_COMPONENTS
  Analysis
  AsmParser
  AsmPrinter
  BitReader
  Core
  IRReader

  MC
  ScalarOpts
  TransformUtils
  SelectionDAG
  Support
  )

add_llvm_tool (hpvm-trireme
  hpvm-trireme.cpp

  DEPENDS
  intrinsics_gen
  LLVMBuildDFG
  LLVMClearDFG
  LLVMClearProf
  )

target_link_libraries(hpvm-trireme PRIVATE LLVMBuildDFGStatic LLVMClearDFGStatic)

export_executable_symbols(hpvm-trireme)
