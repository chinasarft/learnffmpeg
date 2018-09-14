#ifndef __TS_UPLOADER_H__
#define __TS_UPLOADER_H__

#include <qiniu/io.h>
#include <qiniu/rs.h>
#include <pthread.h>
#include <errno.h>
#include "queue.h"
#include "base.h"

typedef void (*UploadArgUpadater)(void *pOpaque, void* pUploadArg, int64_t nNow);
typedef struct _UploadArg {
        char    *pToken_;
        char    *pDeviceId_;
        void    *pUploadArgKeeper_;
        int64_t nSegmentId_;
        int64_t nLastUploadTsTime_;
        UploadArgUpadater UploadArgUpadate;
}UploadArg;

typedef struct _TsUploader TsUploader;
typedef int (*StreamUploadStart)(TsUploader* pUploader);
typedef void (*StreamUploadStop)(TsUploader*);

typedef struct _TsUploader{
        StreamUploadStart UploadStart;
        StreamUploadStop UploadStop;
        UploadState (*GetUploaderState)(TsUploader *pTsUploader);
        int(*Push)(TsUploader *pTsUploader, char * pData, int nDataLen);
        void (*GetStatInfo)(TsUploader *pTsUploader, UploaderStatInfo *pStatInfo);
        void (*RecordTimestamp)(TsUploader *pTsUploader, int64_t nTimestamp);
}TsUploader;


int NewUploader(TsUploader ** _pUploader, UploadArg *pArg, enum CircleQueuePolicy _policy, int _nMaxItemLen, int _nInitItemCount);
void DestroyUploader(TsUploader ** _pUploader);

#endif
