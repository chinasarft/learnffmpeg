#ifndef __COMMON_HPP__
#define __COMMON_HPP__

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <cstring>
#include <functional>
#include <thread>
#include <stdexcept>
#include <ctime>
#include <chrono>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <random>
#include <shared_mutex>
#include <queue>
#include "ThreadCleaner.h"

extern "C"
{
#include "libavutil/opt.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
}

#ifdef WIN32
#include <windows.h>
typedef int ssize_t;
#else
#include <sys/time.h>
#include <unistd.h>
#endif


#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

#include "logger.h"

#define STAUS_CHECK(status, retv, msg, ...) if (status < 0) {\
    release();\
    logerror(msg,  __VA_ARGS__);\
    return retv;\
}

#define NON_NULLPTR_CHECK(ptr, retv, msg, ...) if (ptr == nullptr) {\
    release();\
    logerror(msg,  __VA_ARGS__);\
    return retv;\
}

//#define ACCURACY_NS

#define os_gettime_ns() std::chrono::high_resolution_clock::now().time_since_epoch().count()
#define os_gettime_ms() std::chrono::high_resolution_clock::now().time_since_epoch().count()/1000000
#define os_sleep_ns(ns) std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
#define os_sleep_ms(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms));

#define os_getmonotonictime_ns() std::chrono::high_resolution_clock::steady_clock::now().time_since_epoch().count()
#define os_getmonotonictime_ms() std::chrono::high_resolution_clock::steady_clock::now().time_since_epoch().count()/1000000

#include "packet.hpp"
#include "input.hpp"
#include "media.hpp"
#endif
