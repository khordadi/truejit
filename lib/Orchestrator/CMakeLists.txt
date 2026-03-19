add_library(Orchestrator Orchestrator.cpp OrchestratorTransport.cpp)
target_link_libraries(Orchestrator PRIVATE fmt::fmt GRPCProtocol RemoteCompiler)
target_include_directories(Orchestrator PRIVATE
        ${PROJECT_SOURCE_DIR}/lib
)