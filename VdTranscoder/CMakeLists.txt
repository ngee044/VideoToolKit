cmake_minimum_required(VERSION 3.18)

set(LIBRARY_NAME VdTranscoder)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(HEADER_FILES
	Transcoder.h
)

set(SOURCE_FILES
	Transcoder.cpp
)

project(${LIBRARY_NAME} VERSION 1.0.0.0)

add_library(${LIBRARY_NAME} ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libavdevice)

target_link_libraries(${LIBRARY_NAME} PUBLIC Thread Utilities PkgConfig::FFMPEG)