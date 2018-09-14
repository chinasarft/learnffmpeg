#ifndef __TS_MUX_UPLOADER_H__
#define __TS_MUX_UPLOADER_H__
#include "uploader.h"
#include "resource.h"
#include "base.h"

typedef struct _TsMuxUploader TsMuxUploader;

#define TK_AUDIO_FORMAT_G711A 1
#define TK_AUDIO_FORMAT_G711U 2
#define TK_VIDEO_FORMAT_H264 1
#define TK_VIDEO_FORMAT_H265 2


typedef struct _TsMuxUploader{
        int(*PushVideo)(TsMuxUploader *pTsMuxUploader, char * pData, int nDataLen, int64_t nTimestamp, int nIsKeyFrame, int nIsSegStart);
        int(*PushAudio)(TsMuxUploader *pTsMuxUploader, char * pData, int nDataLen, int64_t nTimestamp);
        int (*SetToken)(TsMuxUploader*, char *, int);
        void (*SetUploaderBufferSize)(TsMuxUploader*, int);
        void (*SetNewSegmentInterval)(TsMuxUploader*, int);
}TsMuxUploader;

int NewTsMuxUploader(TsMuxUploader **pTsMuxUploader, AvArg *pAvArg, char *pDeviceId, int nDeviceIdLen,
                     char *pToken, int nTokenLen);
int TsMuxUploaderStart(TsMuxUploader *pTsMuxUploader);
void DestroyTsMuxUploader(TsMuxUploader **pTsMuxUploader);
#endif
