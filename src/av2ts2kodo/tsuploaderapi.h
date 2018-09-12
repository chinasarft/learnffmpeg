#ifndef __TS_UPLOADER_API__
#define __TS_UPLOADER_API__

#include "tsmuxuploader.h"
#include "log.h"
#include "base.h"

int InitUploader(char *pDeviceId, char * pToken, AvArg *pAvArg);
int UpdateToken(char * pToken);
void SetUploadBufferSize(int nSize);
void SetNewSegmentInterval(int nIntervalSecond);
int PushVideo(char * pData, int nDataLen, int64_t nTimestamp, int nIsKeyFrame, int nIsSegStart);
int PushAudio(char * pData, int nDataLen, int64_t nTimestamp);
void UninitUploader();


//for test
int GetUploadToken(char *pBuf, int nBufLen);
void SetAk(char *pAk);
void SetSk(char *pSk);
void SetBucketName(char *_pName);
void SetCallbackUrl(char *pUrl);
void SetDeleteAfterDays(int days);


#endif
