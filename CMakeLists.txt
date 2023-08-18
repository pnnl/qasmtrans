# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.8)

# Project's name
project(QASMTrans)

# Set C++17 as the standard for the entire project
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set the -O3 optimization flag for all configurations
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

# Add executable
add_executable(QASMTrans src/qasmtrans.cpp)