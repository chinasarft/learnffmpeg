#ifndef __TS_UPLOADER_API__
#define __TS_UPLOADER_API__

#include "tsmuxuploader.h"
#include "log.h"
#include "base.h"

#define DEBUG_MODE

typedef struct {
        char  *pToken_;
        int   nTokenLen_;
        char  *pDeviceId_;
        int   nDeviceIdLen_;
        int   nUploaderBufferSize;
        int   nNewSegmentInterval;
}UserUploadArg;

int InitUploader();

int CreateAndStartAVUploader(TsMuxUploader **pTsMuxUploader, AvArg *pAvArg, UserUploadArg *pUserUploadArg);
int UpdateToken(TsMuxUploader *pTsMuxUploader, char * pToken, int nTokenLen);
void SetUploadBufferSize(TsMuxUploader *pTsMuxUploader, int nSize);
void SetNewSegmentInterval(TsMuxUploader *pTsMuxUploader, int nIntervalSecond);
int PushVideo(TsMuxUploader *pTsMuxUploader, char * pData, int nDataLen, int64_t nTimestamp, int nIsKeyFrame, int nIsSegStart);
int PushAudio(TsMuxUploader *pTsMuxUploader, char * pData, int nDataLen, int64_t nTimestamp);
void DestroyAVUploader(TsMuxUploader **pTsMuxUploader);
void UninitUploader();


//for test
int GetUploadToken(char *pBuf, int nBufLen);
void SetAk(char *pAk);
void SetSk(char *pSk);
void SetBucketName(char *_pName);
void SetCallbackUrl(char *pUrl);
void SetDeleteAfterDays(int days);


#endif
