#set(Rust_CARGO /usr/bin/cargo)
#set(Rust_RUSTC /usr/bin/rustc)

include(FetchContent)
FetchContent_Declare(
        Corrosion
        GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
        GIT_TAG v0.5
)
FetchContent_MakeAvailable(Corrosion)
set(RUST_LIB_PATH Cargo.toml)
corrosion_import_crate(MANIFEST_PATH ${RUST_LIB_PATH})


add_library(CraneliftCompiler CraneliftCompiler.cpp)
target_link_libraries(CraneliftCompiler PRIVATE cranelift_compiler)
target_include_directories(CraneliftCompiler PRIVATE ${PROJECT_SOURCE_DIR}/lib)


#add_executable(test_cranelift_compiler test.cpp)
#target_link_libraries(test_cranelift_compiler CraneliftCompiler WasmService)
#target_include_directories(test_cranelift_compiler PRIVATE ${PROJECT_SOURCE_DIR}/lib)