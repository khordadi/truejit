cmake_minimum_required(VERSION 3.22)
project(truejit)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_VERBOSE_MAKEFILE ON)

# add -O3 to release builds
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-w)
    add_compile_options(-O3)
endif ()

# add -g to debug builds
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif ()

configure_file(lib/Config.h.in Project/Config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# make /usr/local the default search path for libraries
set(CMAKE_PREFIX_PATH /usr/local)

# add boost (timer, thread, system)
# set policy CMP0167 to avoid "FindBoost module is removed" dev warning
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

find_package(Boost 1.84 REQUIRED COMPONENTS timer thread system program_options)
link_libraries(${Boost_LIBRARIES})
include_directories(${Boost_INCLUDE_DIRS})

# add nlohmann/json
find_package(nlohmann_json REQUIRED)
link_libraries(nlohmann_json::nlohmann_json)

find_package(fmt REQUIRED)
link_libraries(fmt::fmt)

find_package(LLVM 17 REQUIRED CONFIG)
find_package(Clang 17 REQUIRED CONFIG)

find_package(OpenSSL REQUIRED)
find_package(wabt REQUIRED)

add_subdirectory(lib/DigitalSignature)
add_subdirectory(lib/Wasm)
add_subdirectory(lib/VirtualMachine)
add_subdirectory(lib/Networking) # put this before lib/Compiler; because of GRPCProtocol dependency
add_subdirectory(lib/Compiler)
add_subdirectory(lib/Orchestrator)


# tools
add_subdirectory(tools)

