cmake_minimum_required(VERSION 3.9.0)

project(av2ts)


if(NOT FFINCDIR)
        message(FATAL_ERROR "shoud specify FFINCDIR")
endif()

if(NOT FFLIBDIR)
        message(FATAL_ERROR "shoud specify FFLIBDIR")
endif()

include_directories(${FFINCDIR})
link_directories(${FFLIBDIR})

add_executable(av2ts
        av2ts.c)

if(WIN32)
        target_link_libraries(av2ts avformat avcodec avfilter swresample swscale avutil)
endif()

