cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

project(screen_recorder)
IF(WIN32)
    INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)
    LINK_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/lib)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
ELSEIF(APPLE)
        find_library(CoreFoundation CoreFoundation)
        find_library(CoreGraphics CoreGraphics)

        INCLUDE_DIRECTORIES(/usr/local/Cellar/ffmpeg/4.4.1_5/include)
        LINK_DIRECTORIES(/usr/local/Cellar/ffmpeg/4.4.1_5/lib)
    ELSE()
    MESSAGE(STATUS "BOH")
    FIND_PACKAGE(X11 REQUIRED)
ENDIF()
add_executable(${PROJECT_NAME}
        main.cpp
        ScreenRecorder.h
        ScreenRecorder.cpp
        AudioRecorder.cpp)
IF(WIN32)
    target_link_libraries(screen_recorder
            avcodec
            avformat
            swscale
            avdevice
            avfilter
            avutil
            postproc
            swresample
            )
ELSEIF(APPLE)
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
    ELSE()
    target_link_libraries(screen_recorder
            avcodec
            avformat
            swscale
            avdevice
            avfilter
            avutil
            postproc
            swresample
            ${X11_LIBRARIES}
            )
ENDIF()