cmake_minimum_required(VERSION 3.9.0)

project(learnffmpeg)


if(NOT FFINCDIR)
        message(FATAL_ERROR "shoud specify FFINCDIR")
endif()

if(NOT FFLIBDIR)
        message(FATAL_ERROR "shoud specify FFLIBDIR")
endif()


add_subdirectory(src/av2ts)
