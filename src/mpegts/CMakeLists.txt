cmake_minimum_required(VERSION 3.9.0)

project(mpegts)

if(APPLE)
	include_directories("/usr/local/Cellar/ffmpeg/3.4.2/include")
	link_directories("/usr/local/Cellar/ffmpeg/3.4.2/lib")
endif()

add_executable(mpegts
	tsmux.c
	tsmux.h
	mpegts.c
	mpegts.h
	adts.h
	adts.c
        test.c)


if(APPLE)
	include_directories("/usr/local/Cellar/ffmpeg/3.4.2/include")
	link_directories("/usr/local/Cellar/ffmpeg/3.4.2/lib")
	set(CMAKE_EXE_LINKER_FLAGS
    		"-framework AudioToolbox -framework VideoToolbox -framework CoreGraphics -framework QuartzCore -framework CoreFoundation -framework CoreMedia -framework Security")
        target_link_libraries(mpegts avformat avcodec avutil bz2 lzma iconv z)
endif()
