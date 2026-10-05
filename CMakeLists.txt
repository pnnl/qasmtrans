# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.20)

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

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(QASMTRANS_ENABLE_NATIVE "Enable -march=native for local benchmark builds." OFF)
option(QASMTRANS_ENABLE_IPO "Enable interprocedural optimization when supported." OFF)
option(QASMTRANS_BUILD_PYTHON "Build the pybind11 Python module." ON)

# Keep an optimized default build even without an explicit build type.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")

if(QASMTRANS_ENABLE_NATIVE)
    add_compile_options(-march=native)
endif()

if(QASMTRANS_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT qasmtrans_ipo_supported OUTPUT qasmtrans_ipo_error)
    if(qasmtrans_ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "IPO/LTO requested but not supported: ${qasmtrans_ipo_error}")
    endif()
endif()

if(QASMTRANS_BUILD_PYTHON)
    set(Python3_FIND_VIRTUALENV FIRST)

    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -m pybind11 --cmakedir
        OUTPUT_VARIABLE _pybind11_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _pybind11_status
    )

    if (NOT _pybind11_status EQUAL 0)
        message(FATAL_ERROR "Failed to find pybind11 via Python. Did you run 'pip install pybind11'?")
    endif()

    list(APPEND CMAKE_PREFIX_PATH "${_pybind11_dir}")

    find_package(pybind11 CONFIG REQUIRED)
endif()

# Add executable
add_executable(QASMTrans
    src/qasmtrans.cpp
    src/cli_pipeline.cpp
    src/cli_support.cpp
)

# Make the project's headers and the vendored Lemon headers visible to the target
target_include_directories(QASMTrans
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
set(QASMTRANS_RUNTIME_TARGETS QASMTrans)

if(QASMTRANS_BUILD_PYTHON)
    # Python binding skeleton (pybind11 module)
    pybind11_add_module(qasmtrans_core MODULE
        src/python_binding.cpp
        src/cli_support.cpp
    )
    target_include_directories(qasmtrans_core
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    )
    list(APPEND QASMTRANS_RUNTIME_TARGETS qasmtrans_core)
    # Place the module next to the python/ helpers for easy import during development
    set_target_properties(qasmtrans_core PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python
    )
endif()

# Apple-provided Clang (and the Homebrew LLVM toolchain that scikit-build often
# selects when building wheels) does not always add the matching libc++/c++abi
# search paths when linking executables under pip. That leads to link failures
# such as "std::__1::__hash_memory" being undefined when QASMTrans is built
# during `pip install .`. Locate the runtime libraries next to the active
# compiler and link against them explicitly so we always pull in the matching
# C++ runtime regardless of the surrounding environment.
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    get_filename_component(_cxx_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_toolchain_root "${_cxx_dir}/.." ABSOLUTE)
    set(_cxx_runtime_hint "${_toolchain_root}/lib/c++")
    set(_cxx_lib_hints "${_cxx_runtime_hint};${_toolchain_root}/lib")
    find_library(_libcxx c++ HINTS ${_cxx_lib_hints})
    find_library(_libcxxabi c++abi HINTS ${_cxx_lib_hints})
    foreach(target_name IN LISTS QASMTRANS_RUNTIME_TARGETS)
        if(TARGET ${target_name})
            if(_libcxx)
                target_link_libraries(${target_name} PRIVATE ${_libcxx})
            endif()
            if(_libcxxabi)
                target_link_libraries(${target_name} PRIVATE ${_libcxxabi})
            endif()
        endif()
    endforeach()
endif()

if(QASMTRANS_BUILD_PYTHON)
    install(TARGETS qasmtrans_core
        LIBRARY DESTINATION qasmtrans
        RUNTIME DESTINATION qasmtrans
        ARCHIVE DESTINATION qasmtrans
    )
    install(FILES
        python/qasmtrans/__init__.py
        DESTINATION qasmtrans
    )
endif()
