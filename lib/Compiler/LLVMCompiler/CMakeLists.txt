add_library(LLVMCompiler LLVMCompiler.cpp)
target_link_libraries(LLVMCompiler PUBLIC wabt::wabt PRIVATE LLVM)
target_include_directories(LLVMCompiler
        PRIVATE
        ${PROJECT_SOURCE_DIR}/lib
        ${LLVM_INCLUDE_DIRS}
)
