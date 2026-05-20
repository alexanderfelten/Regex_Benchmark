cmake_minimum_required(VERSION 3.10)

project(benchmark LANGUAGES CXX CUDA)

set(CMAKE_CUDA_STANDARD 11.0)

set(CMAKE_CUDA_ARCHITECTURES native)

find_package(CUDAToolkit REQUIRED)

message("CMAKE_CUDA_ARCHITECTURES: ${CMAKE_CUDA_ARCHITECTURES}")
message("CUDAToolkit_LIBRARIES_ROOT: ${CUDAToolkit_LIBRARIES_ROOT}")
message("CUDAToolkit_INCLUDE_DIRS: ${CUDAToolkit_INCLUDE_DIRS}")
message("CUDAToolkit_LIBRARY_DIR: ${CUDAToolkit_LIBRARY_DIR}")

include_directories(${CUDAToolkit_INCLUDE_DIRS})

add_executable(
    benchmark
    GPU_Parallel.cu
)

set_target_properties(
    benchmark
    PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
)

link_directories(${CUDAToolkit_LIBRARY_DIR})

target_link_libraries(
    benchmark
    PRIVATE
    cuda.lib
)

#[[ 
rm -r build
cmake -B build
cmake --build build --config Release
.\build\Release\benchmark.exe
]]

#End of File