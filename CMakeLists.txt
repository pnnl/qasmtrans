# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.10)

# Setup cc/CC
if(DEFINED ENV{cc} AND DEFINED ENV{CC})
    message("Environment variables cc and CC are set")
    set(CMAKE_C_COMPILER "$ENV{cc}")
    set(CMAKE_CXX_COMPILER "$ENV{CC}")
else()
    message("Environment variables cc and/or CC are not set, using default settings")
endif()

# Project's name
project(QASMTrans)

# Set C++17 as the standard for the entire project
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set the -O3 optimization flag for all configurations
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

# Add executable
add_executable(QASMTrans src/qasmtrans.cpp)
