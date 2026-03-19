add_subdirectory(CompilationService)
add_subdirectory(Predictor)

# TODO: make CodeCache libraries
add_library(CompilerServer CompilerServer.cpp
        SyncCompilation/SyncCompilation.cpp
        AsyncCompilation/AsyncCompilation.cpp
)
target_link_libraries(CompilerServer PUBLIC wabt::wabt PRIVATE WasmService PredictionService CompilationService)
target_include_directories(CompilerServer PRIVATE ${PROJECT_SOURCE_DIR}/lib)

add_library(GRPCCompilerServer RemoteCompilerServer/RemoteCompilerServer.cpp)
target_link_libraries(GRPCCompilerServer PUBLIC GRPCProtocol PRIVATE fmt::fmt CompilerServer)
target_include_directories(GRPCCompilerServer PRIVATE ${PROJECT_SOURCE_DIR}/lib)
