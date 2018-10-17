#include "spdlog/spdlog.h"
#include <stdarg.h>
extern std::shared_ptr<spdlog::logger> spdlogger;
#define _s_l_(x) #x
#define _str_line_(x) _s_l_(x)
#define __STR_LINE__ _str_line_(__LINE__)

#ifdef WIN32
#define logtrace(fmt,...) \
        if(spdlogger.get() != nullptr) spdlogger->trace(THIS_FILE ":" __STR_LINE__ ": " fmt, __VA_ARGS__)
#define logdebug(fmt,...) \
    if(spdlogger.get() != nullptr) spdlogger->debug(THIS_FILE ":" __STR_LINE__ ": " fmt, __VA_ARGS__)
#define loginfo(fmt,...) \
    if(spdlogger.get() != nullptr)  spdlogger->info(THIS_FILE ":" __STR_LINE__ ": " fmt, __VA_ARGS__)
#define logwarn(fmt,...) \
    if(spdlogger.get() != nullptr)  spdlogger->warn(THIS_FILE ":" __STR_LINE__ ": " fmt, __VA_ARGS__)
#define logerror(fmt,...) \
    if(spdlogger.get() != nullptr) spdlogger->error(THIS_FILE ":" __STR_LINE__ ": " fmt, __VA_ARGS__)
#else
#define logtrace(fmt,...) \
        if(spdlogger.get() != nullptr) spdlogger->trace(THIS_FILE ":" __STR_LINE__ ": " fmt, ##__VA_ARGS__)
#define logdebug(fmt,...) \
    if(spdlogger.get() != nullptr) spdlogger->debug(THIS_FILE ":" __STR_LINE__ ": " fmt, ##__VA_ARGS__)
#define loginfo(fmt,...) \
    if(spdlogger.get() != nullptr)  spdlogger->info(THIS_FILE ":" __STR_LINE__ ": " fmt, ##__VA_ARGS__)
#define logwarn(fmt,...) \
    if(spdlogger.get() != nullptr)  spdlogger->warn(THIS_FILE ":" __STR_LINE__ ": " fmt, ##__VA_ARGS__)
#define logerror(fmt,...) \
    if(spdlogger.get() != nullptr) spdlogger->error(THIS_FILE ":" __STR_LINE__ ": " fmt, ##__VA_ARGS__)
#endif

#define BOALOG_LEN 5
#if 0
static inline int loghex(char * buffer, char * data, int data_len)
{
        int i, j = 0, buf_len, logline_len = 69 + 2 * BOALOG_LEN;
        char s[100], temp[BOALOG_LEN + 1], format_l[10] = { 0 }, format_r[10] = {
                0};
        char *buf_tmp;
        buf_tmp = buffer ;
        sprintf(format_l, "%%0%dd:", BOALOG_LEN);
        sprintf(format_r, ":%%0%dd", BOALOG_LEN);
        memset(s, 0, sizeof(s));
        memset(s, '-', logline_len);
        sprintf(buf_tmp, "%s\n", s);
        buf_len = logline_len + 1;
        for (i = 0; i < data_len; i++) {
                if (j == 0) {
                        memset(s, ' ', logline_len);
                        sprintf(s, format_l, i);
                        sprintf(&s[69 + BOALOG_LEN - 1], format_r, i + 16);
                }
                sprintf(temp, "%02X ", (unsigned char) data[i]);
                memcpy(&s[j * 3 + BOALOG_LEN + 1 + (j > 7)], temp, 3);
                if (isprint((unsigned char) data[i])) {
                        s[j + 51 + BOALOG_LEN + (j > 7)] = data[i];
                } else {
                        if (data[i] != 0)
                                s[j + 51 + BOALOG_LEN + (j > 7)] = '.';
                }
                j++;
                if (j == 16) {
                        s[logline_len] = 0;
                        sprintf(buf_tmp + buf_len, "%s\n", s);
                        buf_len += logline_len + 1;
                        j = 0;
                }
        }
        if (j) {
                s[logline_len] = 0;
                sprintf(buf_tmp + buf_len, "%s\n", s);
                buf_len += logline_len + 1;
        }
        memset(s, 0, sizeof(s));
        
        /*memset(s, '-', logline_len); */
        memset(s, '=', logline_len);        /*为了perl好分析，改为=== */
        sprintf(buf_tmp + buf_len, "%s\n", s);
        buf_len += logline_len + 1;
        //printf("return len:%d\n", buf_len);
        return buf_len;
}
#endif

extern "C" void logger_init_file_output(const char * path);
extern "C" void logger_set_level_trace();
extern "C" void logger_set_level_debug();
extern "C" void logger_set_level_info();
extern "C" void logger_set_level_warn();
extern "C" void logger_set_level_error();
extern "C" void logger_flush();
extern "C" spdlog::level::level_enum logger_get_level();

