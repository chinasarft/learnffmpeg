cmake_minimum_required(VERSION 3.9.0)

project(av2ts2rtp)


if(NOT FFINCDIR)
        message(FATAL_ERROR "shoud specify FFINCDIR")
endif()

if(NOT FFLIBDIR)
        message(FATAL_ERROR "shoud specify FFLIBDIR")
endif()

include_directories(${FFINCDIR})
link_directories(${FFLIBDIR})

add_executable(av2ts2rtp
        av2ts2rtp.c)

if(WIN32)
        target_link_libraries(av2ts2rtp avformat avcodec avfilter swresample swscale avutil)
elseif(APPLE)
	set(CMAKE_EXE_LINKER_FLAGS
    		"-framework AudioToolbox -framework VideoToolbox -framework CoreGraphics -framework QuartzCore -framework CoreFoundation -framework CoreMedia -framework Security")
        target_link_libraries(av2ts2rtp avformat avcodec avfilter swresample swscale avutil bz2 lzma iconv z)
endif()

