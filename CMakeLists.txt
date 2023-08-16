# Specify a minimum CMake version
cmake_minimum_required(VERSION 3.20) 

# Specify the project details
project(MyPackage 
        VERSION 0.0.1 
        DESCRIPTION "QASMTrans" 
        LANGUAGES C CXX)


# Set C++ and CUDA standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# adding debug symbols
set(CMAKE_BUILD_TYPE Debug) 
# set(CMAKE_CXX_FLAGS_DEBUG "-g -pg"
# set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-pg")



if(APPLE)
    message("Running on Apple platform")
    # Additional configuration for Apple platforms
else()
    message("Not running on Apple platform")
    # Configuration for non-Apple platforms
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_CXX_COMPILER g++)
    set(CMAKE_CUDA_HOST_COMPILER mpicxx)

   
    # Set the nvshmem home directory
    list(APPEND CMAKE_PREFIX_PATH $ENV{HOME}/qasmtrans)
    # set(CMAKE_CUDA_FLAGS_RELEASE "-O3")
endif()


# Enable highest optimization for release build
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/header)
include_directories(${PROJECT_SOURCE_DIR}/parser)
include_directories(${PROJECT_SOURCE_DIR}/dump)
include_directories(${PROJECT_SOURCE_DIR}/passes)

# # # Subdirectories
# add_subdirectory(qasm)

set(SOURCE_FILE "qasmtrans.cpp")

add_executable(qasmtrans src/qasmtrans.cpp)
target_compile_features(qasmtrans PRIVATE cxx_std_17)
