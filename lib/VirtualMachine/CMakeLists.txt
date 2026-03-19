add_library(LLVMRuntime INTERFACE)
target_link_libraries(LLVMRuntime INTERFACE LLVM)
target_include_directories(LLVMRuntime INTERFACE ${LLVM_INCLUDE_DIRS})

add_subdirectory(Instrumentation)

add_library(VirtualMachine VMCalls.cpp VirtualMachine.Wasi.cpp VirtualMachine.cpp VirtualMachine.Interp.cpp)
target_link_libraries(VirtualMachine
        PUBLIC LLVMRuntime
        PRIVATE wabt::wabt fmt::fmt Instrumentation CompilerClient WasmService DigitalSignatureService
        /usr/local/lib/libuvwasi.a uv_a
)
target_include_directories(VirtualMachine
        PRIVATE ${PROJECT_SOURCE_DIR}/lib
)


add_library(VirtualMachineWithoutCompiler INTERFACE)
target_link_libraries(VirtualMachineWithoutCompiler
        INTERFACE
        RemoteCompilerServerProxy
        fmt::fmt
        WasmService
        CompilerServer
        VirtualMachine
)
target_include_directories(VirtualMachineWithoutCompiler
        INTERFACE ${PROJECT_SOURCE_DIR}/lib
)

add_library(VirtualMachineWithCompiler INTERFACE)
target_link_libraries(VirtualMachineWithCompiler
        INTERFACE
        PredictionService
        fmt::fmt
        WasmService
        LLVMCompiler
        CompilerServer
        VirtualMachine
)
target_include_directories(VirtualMachineWithCompiler
        INTERFACE ${PROJECT_SOURCE_DIR}/lib
)