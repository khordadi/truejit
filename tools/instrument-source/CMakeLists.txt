add_executable(instrument instrument.cpp)
target_link_libraries(instrument PRIVATE fmt::fmt LLVM Clang)
target_include_directories(instrument
        PRIVATE
        ${PROJECT_SOURCE_DIR}/lib
        # clang
        ${CLANG_INCLUDE_DIRS}
)
