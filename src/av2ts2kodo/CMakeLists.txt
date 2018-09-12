cmake_minimum_required(VERSION 3.9.0)

project(av2ts2kodo)

add_definitions(-DUSE_OWN_TSMUX)
add_definitions(-DTEST_WITH_FFMPEG)

if(DISABLE_OPENSSL)
	message("tsuploader disable openssl--------->")
	add_definitions(-DDISABLE_OPENSSL)
else()
	link_directories("/Users/liuye/source/openssl-1.0.2o")
endif()
#include_directories("/Users/liuye/source/ffmpegcut/include")
#link_directories("/Users/liuye/source/ffmpegcut/lib")
include_directories("/usr/local/Cellar/ffmpeg/3.4.2/include")
link_directories("/usr/local/Cellar/ffmpeg/3.4.2/lib")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/c-sdk")
link_directories("/Users/liuye/source/curl-7.61.0/lib/.libs")
include_directories("/Users/liuye/source/curl-7.61.0/include")

add_subdirectory(c-sdk)

add_executable(av2ts2kodo
	resource.c
	resource.h
	uploader.h
	uploader.c
	base.h
	queue.c
	queue.h
	log.h
	log.c
	adts.h
	adts.c
	tsmuxuploader.c
	tsmuxuploader.h
	tsuploaderapi.h
	tsuploaderapi.c
	servertime.c
	servertime.h
	mpegts.h
	mpegts.c
	tsmux.c
	tsmux.h
	flag.h
	flag.c
        test.c)

if(APPLE)
	set(CMAKE_EXE_LINKER_FLAGS
    		"-framework AudioToolbox -framework VideoToolbox -framework CoreGraphics -framework QuartzCore -framework CoreFoundation -framework CoreMedia -framework Security")
	if(DISABLE_OPENSSL)
        	target_link_libraries(av2ts2kodo avformat avcodec avutil qiniu curl bz2 lzma iconv z)
	else()
        	target_link_libraries(av2ts2kodo avformat avcodec avutil qiniu curl crypto bz2 lzma iconv z)
	endif()
else()
	if(DISABLE_OPENSSL)
        	target_link_libraries(av2ts2kodo avformat avcodec avutil qiniu curl crypto)
	else()
        	target_link_libraries(av2ts2kodo avformat avcodec avutil qiniu curl)
	endif()
endif()

