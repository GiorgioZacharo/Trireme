if(WIN32 OR CYGWIN)
  set(LLVM_LINK_COMPONENTS Core Support)
endif()

add_llvm_library( LLVMClearDFG
  MODULE
  ClearDFG.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )

add_llvm_library( LLVMClearDFGStatic
  ClearDFG.cpp

  DEPENDS
  intrinsics_gen
  opt
  )
