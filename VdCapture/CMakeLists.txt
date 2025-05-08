cmake_minimum_required(VERSION 3.18)

set(LIBRARY_NAME VdCapture)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(HEADER_FILES
    VideoCapture.h
    Transcoder.h
)

set(SOURCE_FILES
    VideoCapture.cpp
    Transcoder.cpp
)

project(${LIBRARY_NAME} VERSION 1.0.0.0 LANGUAGES CXX CUDA)

set(NVENC_SDK_PATH "../../Video_Codec_SDK_13.0.19")

find_path(NVENC_INCLUDE_DIR
    NAMES nvEncodeAPI.h
    PATHS
        ${NVENC_SDK_PATH}/Interface
)

message("NVENC_INCLUDE_DIR: ${NVENC_INCLUDE_DIR}")

add_library(${LIBRARY_NAME} ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(CUDAToolkit REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libavdevice)

if (WIN32)
    find_library(NVENC_LIB NAMES nvencodeapi PATHS ${CMAKE_SOURCE_DIR}/libs ${NVENC_SDK_PATH})
    if(NOT NVENC_LIB)
        message(WARNING "nvencodeapi.lib not found in custom paths. Will try default link name.")
        set(NVENC_LIB nvencodeapi)
    endif()
else()
    find_library(NVENC_LIB NAMES nvidia-encode libnvidia-encode
        PATHS /usr/lib /usr/local/lib /usr/lib64 NO_DEFAULT_PATH)
    if(NOT NVENC_LIB)
        message(WARNING "libnvidia-encode.so not found explicitly. Using 'nvidia-encode'")
        set(NVENC_LIB nvidia-encode)
    endif()
endif()

target_link_libraries(${LIBRARY_NAME} PUBLIC Thread Utilities PkgConfig::FFMPEG CUDA::cudart ${NVENC_LIB})

target_include_directories(${LIBRARY_NAME} PUBLIC ${CUDAToolkit_INCLUDE_DIRS} ${NVENC_INCLUDE_DIR})