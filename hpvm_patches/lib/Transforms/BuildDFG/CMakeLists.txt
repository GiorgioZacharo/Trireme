if(WIN32 OR CYGWIN)
  set(LLVM_LINK_COMPONENTS Core Support)
endif()

add_llvm_library( LLVMBuildDFG
  MODULE
  BuildDFG.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )

add_llvm_library( LLVMBuildDFGStatic
  BuildDFG.cpp

  DEPENDS
  intrinsics_gen
  )
