if(WIN32 OR CYGWIN)
  set(LLVM_LINK_COMPONENTS Core Support)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLLVM_BUILD_DIR=${CMAKE_BINARY_DIR}")

add_llvm_library( LLVMClearProf
  MODULE
  ClearProf.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )
