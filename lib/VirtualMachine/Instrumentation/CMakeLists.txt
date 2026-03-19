add_library(Instrumentation SpecializationProfiler/SpecializationProfiler.cpp BranchProfiler/BranchProfiler.cpp WebAssemblyPrinter/WebAssemblyPrinter.cpp
        CallProfiler/CallProfiler.cpp)
target_link_libraries(Instrumentation PRIVATE wabt::wabt fmt::fmt)
target_include_directories(Instrumentation PRIVATE ${PROJECT_SOURCE_DIR}/lib)