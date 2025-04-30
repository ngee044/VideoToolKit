cmake_minimum_required(VERSION 3.18)

include(CMakeDependentOption)

set(PROGRAM_NAME "VideoToolKit")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_FRAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

project(${PROGRAM_NAME} VERSION 1.0.0.0)

option(USE_ENCRYPT_LIBS "Use encrypt library" ON)
option(BUILD_THREAD_LIB "Build thread library" ON)

add_subdirectory(Utilities)
add_subdirectory(ThreadPool)
add_subdirectory(VdCapture)
