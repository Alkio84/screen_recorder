cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(screen_recorder)
IF(WIN32)
    INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)
    LINK_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
ELSE()
    find_library(CoreFoundation CoreFoundation)
    find_library(CoreGraphics CoreGraphics)

    INCLUDE_DIRECTORIES(/usr/local/Cellar/ffmpeg/4.4_2/include)
    LINK_DIRECTORIES(/usr/local/Cellar/ffmpeg/4.4_2/lib)
ENDIF()
add_executable(${PROJECT_NAME}
        main.cpp
        ScreenRecorder.h
        ScreenRecorder.cpp
        )
target_link_libraries(screen_recorder
        avcodec
        avformat
        swscale
        avdevice
        avfilter
        avutil
        postproc
        swresample
        ${CoreFoundation}
        ${CoreGraphics}
        )