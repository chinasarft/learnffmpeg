cmake_minimum_required(VERSION 3.9.0)

project(opensdp)


if(NOT FFINCDIR)
        message(FATAL_ERROR "shoud specify FFINCDIR")
endif()

if(NOT FFLIBDIR)
        message(FATAL_ERROR "shoud specify FFLIBDIR")
endif()

include_directories(${FFINCDIR})
link_directories(${FFLIBDIR})

add_executable(opensdp
        opensdp.c)

if(WIN32)
        target_link_libraries(opensdp avformat avcodec avfilter swresample swscale avutil)
endif()

