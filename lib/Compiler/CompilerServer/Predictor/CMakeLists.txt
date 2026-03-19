find_package(Torch REQUIRED)
message(STATUS "Torch: ${TORCH_LIBRARIES}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

add_library(PredictionService PredictionService.cpp LSTMPredictor.cpp)
target_link_libraries(PredictionService PUBLIC ${TORCH_LIBRARIES})
set_property(TARGET PredictionService PROPERTY CXX_STANDARD 20)
target_include_directories(PredictionService PUBLIC ${PROJECT_SOURCE_DIR}/lib)