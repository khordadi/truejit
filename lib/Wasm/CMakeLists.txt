add_library(WasmService WasmService.cpp)
target_link_libraries(WasmService PUBLIC wabt::wabt)
target_include_directories(WasmService PRIVATE ${PROJECT_SOURCE_DIR}/lib)
