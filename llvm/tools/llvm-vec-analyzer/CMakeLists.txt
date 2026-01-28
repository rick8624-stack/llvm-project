set(LLVM_LINK_COMPONENTS
  Analysis
  Core
  IRReader
  Support
  )

add_llvm_tool(llvm-vec-analyzer
  llvm-vec-analyzer.cpp
  VectorizationAnalyzer.cpp
  )
