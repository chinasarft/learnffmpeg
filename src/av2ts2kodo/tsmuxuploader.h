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
        void (*SetToken)(TsMuxUploader*, char *);
        void (*SetAccessKey)(TsMuxUploader* pTsMuxUploader, char *pAk, int nAkLen);
        void (*SetSecretKey)(TsMuxUploader*pTsMuxUploader, char * pSk, int nSkLe);
        void (*SetBucket)(TsMuxUploader*pTsMuxUploader, char * pBucketName, int nBucketNameLen);
        void (*SetCallbackUrl)(TsMuxUploader*pTsMuxUploader, char * pCallbackUrl, int nCallbackUrlLen);
        void (*SetDeleteAfterDays)(TsMuxUploader*pTsMuxUploader, int nAfterDays);
}TsMuxUploader;

int NewTsMuxUploader(TsMuxUploader **pTsMuxUploader, AvArg *pAvArg);
int TsMuxUploaderStart(TsMuxUploader *pTsMuxUploader);
void DestroyTsMuxUploader(TsMuxUploader **pTsMuxUploader);
#endif
