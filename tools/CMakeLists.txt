add_library(RemoteCompiler INTERFACE)
target_link_libraries(RemoteCompiler INTERFACE GRPCCompilerServer CompilerServer)
target_include_directories(RemoteCompiler INTERFACE ${PROJECT_SOURCE_DIR}/lib)


add_executable(remote-compiler remote-compiler.cpp)
target_link_libraries(remote-compiler PRIVATE fmt::fmt RemoteCompiler)
target_include_directories(remote-compiler PRIVATE ${PROJECT_SOURCE_DIR}/lib)

add_executable(vm-without-compiler vm-without-compiler.cpp)
target_link_libraries(vm-without-compiler PRIVATE VirtualMachineWithoutCompiler)
target_include_directories(vm-without-compiler
        PRIVATE
        ${PROJECT_SOURCE_DIR}/lib
)

add_executable(vm-with-compiler vm-with-compiler.cpp)
target_link_libraries(vm-with-compiler PUBLIC VirtualMachineWithCompiler)
target_include_directories(vm-with-compiler
        PRIVATE
        ${PROJECT_SOURCE_DIR}/lib
)

add_executable(worker worker.cpp)
target_link_libraries(worker PRIVATE GRPCProtocol VirtualMachine VirtualMachineWithoutCompiler VirtualMachineWithCompiler)
target_include_directories(worker
        PUBLIC
        ${PROJECT_SOURCE_DIR}/lib
)

add_executable(orchestrator orchestrator.cpp)
target_link_libraries(orchestrator PRIVATE fmt::fmt Orchestrator GRPCProtocol VirtualMachineWithCompiler)
target_include_directories(orchestrator
        PRIVATE
        ${PROJECT_SOURCE_DIR}/lib
)

# install targets
install(TARGETS
        remote-compiler
        vm-without-compiler
        vm-with-compiler
        worker
        orchestrator
)


# create a target called install-worker that installs the worker binary


#
#add_executable(static-planner static-planner.cpp)
#target_link_libraries(static-planner PRIVATE fmt::fmt WasmService)
#target_include_directories(static-planner
#        PRIVATE
#        ${PROJECT_SOURCE_DIR}/lib
#)
#
#
#add_executable(callgraph callgraph.cpp)
#target_link_libraries(callgraph PRIVATE fmt::fmt WasmService)
#target_include_directories(callgraph
#        PRIVATE
#        ${PROJECT_SOURCE_DIR}/lib
#)
#
#
add_executable(static-compiler static-compiler.cpp)
target_link_libraries(static-compiler PRIVATE fmt::fmt CompilerServer DigitalSignatureService)
target_include_directories(static-compiler
        PRIVATE
        ${PROJECT_SOURCE_DIR}/lib
)
#
#
#add_subdirectory(instrument-source)