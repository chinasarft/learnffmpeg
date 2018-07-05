cmake_minimum_required(VERSION 3.9.0)

project(openavio)

if(NOT FFINCDIR)
        message(FATAL_ERROR "shoud specify FFINCDIR")
endif()

if(NOT FFLIBDIR)
        message(FATAL_ERROR "shoud specify FFLIBDIR")
endif()

set(CMAKE_CXX_FLAGS "-std=c++14")

include_directories(${FFINCDIR} "${CMAKE_CURRENT_SOURCE_DIR}/spdlog")
link_directories(${FFLIBDIR})

add_executable(openavio
	main.cpp)

add_library(avinput SHARED 
        input.cpp
        input.hpp
	packet.cpp
	packet.hpp
	common.hpp
        media.hpp
        media.cpp
	logger.h
	logger.cpp
        ThreadCleaner.h
        ThreadCleaner.cpp
        Statistics.h
        Statistics.cpp
	)

if(WIN32)
	add_definitions(-DWIN32)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /source-charset:utf-8")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8")
        target_link_libraries(openavio avinput avformat avcodec avfilter swresample swscale avutil)

        target_link_libraries(avinput avformat avcodec avfilter swresample swscale avutil)
elseif(APPLE)
	set(CMAKE_EXE_LINKER_FLAGS
    		"-framework AudioToolbox -framework VideoToolbox -framework CoreGraphics -framework QuartzCore -framework CoreFoundation -framework CoreMedia -framework Security")
	#find_package(OpenSSL)
	#message("->${OPENSSL_INCLUDE_DIR}")
	#message("->${OPENSSL_LIBRARIES}")
        #target_link_libraries(openavio avformat avcodec avfilter swresample swscale avutil bz2 lzma iconv z ${OPENSSL_LIBRARIES})
        target_link_libraries(openavio avinput) # avformat avcodec avfilter swresample swscale avutil bz2 lzma iconv z)

	set(CMAKE_SHARED_LINKER_FLAGS
    		"-framework AudioToolbox -framework VideoToolbox -framework CoreGraphics -framework QuartzCore -framework CoreFoundation -framework CoreMedia -framework Security")
        target_link_libraries(avinput avformat avcodec avfilter swresample swscale avutil bz2 lzma iconv z)
endif()

