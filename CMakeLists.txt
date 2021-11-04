cmake_minimum_required(VERSION 3.16)

project(screen_recorder)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)
LINK_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
add_executable(${PROJECT_NAME}
        main.cpp
        ScreenRecorder.h
        ScreenRecorder.cpp
        )
target_link_libraries(screen_recorder avcodec avformat swscale avdevice avfilter avutil postproc swresample)