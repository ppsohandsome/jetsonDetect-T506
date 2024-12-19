cmake_minimum_required(VERSION 3.0.0)
project(jetson_yolo VERSION 0.1.0)

add_definitions(-std=c++11)
add_definitions(-DAPI_EXPORTS)

include(CTest)
set(CMAKE_CUDA_COMPILER /usr/local/cuda/bin/nvcc)
enable_language(CUDA)

# opencv
set(OpenCV_DIR /usr/local/lib/cmake/opencv4)
find_package(OpenCV REQUIRED)

include_directories(
	./include
	./src
	./plugin
	/usr/local/include
	/usr/local/cuda/include
	/usr/local/include/librdkafka
)

link_directories(
	./lib
	/usr/local/lib
	/usr/local/cuda/lib64
)

include_directories(/home/nvidia/TensorRT-8.2.5.1/include/)
link_directories(/home/nvidia/TensorRT-8.2.5.1/lib/)

file(GLOB_RECURSE SRCS ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/src/*.cu)
file(GLOB_RECURSE PLUGIN_SRCS ${PROJECT_SOURCE_DIR}/plugin/*.cu)

add_library(myplugins SHARED ${PLUGIN_SRCS})
target_link_libraries(myplugins nvinfer cudart)

add_executable(jetsonDetect main.cpp ${SRCS}
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

target_link_libraries(jetsonDetect
	nvinfer
	cudart
	myplugins
	jsoncpp
	pthread
	jetson-utils
	-lrdkafka
	cuda
	${OpenCV_LIBS}
)

