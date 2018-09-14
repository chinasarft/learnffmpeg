#ifndef __BASE_H__
#define __BASE_H__

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <errno.h>
#include "log.h"
#ifndef __APPLE__
#include <stdint.h>
#endif

typedef enum {
        TK_VIDEO_H264 = 1,
        TK_VIDEO_H265 = 2
}TkVideoFormat;
typedef enum {
        TK_AUDIO_PCMU = 1,
        TK_AUDIO_PCMA = 2,
        TK_AUDIO_AAC = 3
}TkAudioFormat;

typedef enum {
        TK_UPLOAD_INIT,
        TK_UPLOAD_FAIL,
        TK_UPLOAD_OK
}UploadState;

typedef struct _AvArg{
        TkAudioFormat nAudioFormat;
        int nChannels;
        int nSamplerate;
        TkVideoFormat nVideoFormat;
} AvArg;

#define TK_STREAM_UPLOAD 1

#define TK_NO_MEMORY       -1000
#define TK_MUTEX_ERROR     -1100
#define TK_COND_ERROR      -1101
#define TK_THREAD_ERROR    -1102
#define TK_TIMEOUT         -2000
#define TK_NO_PUSH         -2001
#define TK_BUFFER_IS_SMALL -2003
#define TK_ARG_TOO_LONG    -2004
#define TK_ARG_ERROR       -2100
#define TK_JSON_FORMAT     -2200
#define TK_HTTP_TIME       -2300
#define TK_OPEN_TS_ERR     -2400
#define TK_WRITE_TS_ERR    -2401
#define TK_Q_OVERWRIT      -5001

int IsProcStatusQuit();

#endif
