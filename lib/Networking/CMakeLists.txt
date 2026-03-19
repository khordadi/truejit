# add grpc
find_package(Threads REQUIRED)
find_package(absl CONFIG REQUIRED)
message(STATUS "absl path: ${absl_DIR}")
option(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "protobuf path: ${Protobuf_DIR}")
set(PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(REFLECTION gRPC::grpc++_reflection)
set(PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
find_package(gRPC CONFIG REQUIRED)
set(GRPC_GRPCPP gRPC::grpc++)
set(GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
get_filename_component(protocol "protocol.proto" ABSOLUTE)
get_filename_component(protocol_path "${protocol}" PATH)
set(protocol_srcs "${CMAKE_CURRENT_BINARY_DIR}/protocol.pb.cc")
set(protocol_hdrs "${CMAKE_CURRENT_BINARY_DIR}/protocol.pb.h")
set(grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/protocol.grpc.pb.cc")
set(grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/protocol.grpc.pb.h")
add_custom_command(
        OUTPUT "${protocol_srcs}" "${protocol_hdrs}" "${grpc_srcs}" "${grpc_hdrs}"
        COMMAND ${PROTOBUF_PROTOC}
        ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}" --cpp_out "${CMAKE_CURRENT_BINARY_DIR}" -I "${protocol_path}" --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN_EXECUTABLE}" "${protocol}"
        DEPENDS "${protocol}"
)

# GRPCProtocol
add_library(GRPCProtocol
        ${grpc_srcs}
        ${grpc_hdrs}
        ${protocol_srcs}
        ${protocol_hdrs}
        Orchestration.cpp
)
target_link_libraries(GRPCProtocol
        absl::flags absl::flags_parse
        ${REFLECTION}
        ${GRPC_GRPCPP}
        ${PROTOBUF_LIBPROTOBUF}
)
# Include generated *.pb.h files
target_include_directories(GRPCProtocol PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/.. PRIVATE ${PROJECT_SOURCE_DIR}/lib)
