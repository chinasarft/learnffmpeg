#ifndef __LOG_H__
#define __LOG_H__

#include <stdarg.h>
#include <stdio.h>


#define _s_l_(x) #x
#define _str_line_(x) _s_l_(x)
#define __STR_LINE__ _str_line_(__LINE__)

#define LOG_LEVEL_TRACE 1
#define LOG_LEVEL_DEBUG 2
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_WARN 4
#define LOG_LEVEL_ERROR 5

extern int nLogLevel;
typedef void (*LogFunc)(char * pLog);

void SetLogLevelToTrace();
void SetLogLevelToDebug();
void SetLogLevelToInfo();
void SetLogLevelToWarn();
void SetLogLevelToError();
void SetLogCallback(LogFunc f);
void Log(int nLevel, char * pFmt, ...);

#define logtrace(fmt,...) \
        Log(LOG_LEVEL_TRACE, __FILE__ ":" __STR_LINE__ "[T]: " fmt "\n", ##__VA_ARGS__)
#define logdebug(fmt,...) \
        Log(LOG_LEVEL_DEBUG, __FILE__ ":" __STR_LINE__ "[D]: " fmt "\n", ##__VA_ARGS__)
#define loginfo(fmt,...) \
        Log(LOG_LEVEL_INFO,  __FILE__ ":" __STR_LINE__ "[I]: " fmt "\n", ##__VA_ARGS__)
#define logwarn(fmt,...) \
        Log(LOG_LEVEL_WARN,  __FILE__ ":" __STR_LINE__ "[W]: " fmt "\n", ##__VA_ARGS__)
#define logerror(fmt,...) \
        Log(LOG_LEVEL_ERROR, __FILE__ ":" __STR_LINE__ "[E]: " fmt "\n", ##__VA_ARGS__)


#endif
