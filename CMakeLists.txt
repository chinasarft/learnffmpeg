cmake_minimum_required(VERSION 3.9.0)

project(learnffmpeg)


if(NOT FFINCDIR)
        message(FATAL_ERROR "shoud specify FFINCDIR")
endif()

if(NOT FFLIBDIR)
        message(FATAL_ERROR "shoud specify FFLIBDIR")
endif()



add_subdirectory(src/av2ts)
add_subdirectory(src/av2rtpts)
add_subdirectory(src/av2rtpartpv)
add_subdirectory(src/opensdp)
add_subdirectory(src/openavio)
add_subdirectory(src/av2ts2mem)
add_subdirectory(src/readfile)
add_subdirectory(src/mpegts)
add_subdirectory(src/fmp4)
