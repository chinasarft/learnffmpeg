#include "uploader.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <pthread.h>
#include "servertime.h"
#include <time.h>
#include <curl/curl.h>
#ifdef __ARM
#include "socket_logging.h"
#endif

size_t getDataCallback(void* buffer, size_t size, size_t n, void* rptr);

#define TS_DIVIDE_LEN 4096
//#define UPLOAD_SEG_INFO

static char gDeviceId[64];
static int64_t nSegmentId;
static int64_t nLastUploadTsTime;
static int64_t nNewSegmentInterval = 30;

enum WaitFirstFlag {
        WF_INIT,
        WF_LOCKED,
        WF_FIRST,
        WF_QUIT,
};

static int gnCount = 1;
typedef struct _KodoUploader{
        TsUploader uploader;
#ifdef TK_STREAM_UPLOAD
        CircleQueue * pQueue_;
#else
        char *pTsData;
        int nTsDataCap;
        int nTsDataLen;
#endif
        pthread_t workerId_;
        int isThreadStarted_;
        char *pToken_;
        char ak_[64];
        char sk_[64];
        char bucketName_[256];
        int deleteAfterDays_;
        char callback_[512];
        int64_t nSegmentId;
        int64_t nFirstFrameTimestamp;
        int64_t nLastFrameTimestamp;
        UploadState state;
        
        int64_t getDataBytes;
        curl_off_t nLastUlnow;
        int64_t nUlnowRecTime;
        int nLowSpeedCnt;
        int nIsFinished;
	int64_t nCount;
        
        pthread_mutex_t waitFirstMutex_;
        enum WaitFirstFlag nWaitFirstMutexLocked_;
}KodoUploader;

static struct timespec tmResolution;
int timeoutCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
        if (ulnow == 0) {
                return 0;
        }
        
        if (tmResolution.tv_nsec == 0) {
                clock_getres(CLOCK_MONOTONIC, &tmResolution);
        }
        struct timespec tp;
        clock_gettime(CLOCK_MONOTONIC, &tp);
        int64_t nNow = (int64_t)(tp.tv_sec * 1000000000ll + tp.tv_nsec / tmResolution.tv_nsec);
        
        KodoUploader * pUploader = (KodoUploader *)clientp;
        if (pUploader->nUlnowRecTime == 0) {
                pUploader->nLastUlnow = ulnow;
                pUploader->nUlnowRecTime = nNow;
                return 0;
        }
        
        int nDiff = (int)((nNow - pUploader->nUlnowRecTime) / 1000000000);
        if (nDiff > 0) {
                //printf("(%d)%d,==========dltotal:%lld dlnow:%lld ultotal:%lld ulnow-reculnow=%lld, now - lastrectime=%lld\n",
                //       pUploader->nCount, pUploader->nLowSpeedCnt, dltotal, dlnow, ultotal, ulnow - pUploader->nLastUlnow, (nNow - pUploader->nUlnowRecTime)/1000000);
                if ((ulnow - pUploader->nLastUlnow) / nDiff < 1024) { //} && !pUploader->nIsFinished) {
                        pUploader->nLowSpeedCnt += nDiff;
                        if (pUploader->nLowSpeedCnt >= 3) {
                                logerror("accumulate upload timeout:%d", pUploader->nLowSpeedCnt); 
                                return -1;
                        }
                }
                if (nDiff >= 10) {
                        logerror("upload timeout directly:%d", nDiff); 
                        return -1;
                } else if (nDiff >= 5) {
                        if (pUploader->nLowSpeedCnt >= 1) {
                                logerror("half accumulate upload timeout:%d", pUploader->nLowSpeedCnt); 
                                return -1;
                        }
                        pUploader->nLowSpeedCnt = 2;

                }
                pUploader->nLastUlnow = ulnow;
                pUploader->nUlnowRecTime = nNow;
        }
        return 0;
}

static void setSegmentId(TsUploader* _pUploader, int64_t _nId)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pUploader;
        pKodoUploader->nSegmentId = _nId;
}

static void setAccessKey(TsUploader* _pUploader, char *_pAk, int _nAkLen)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pUploader;
        assert(sizeof(pKodoUploader->ak_) - 1 > _nAkLen);
        memcpy(pKodoUploader->ak_, _pAk, _nAkLen);
}

static void setSecretKey(TsUploader* _pUploader, char *_pSk, int _nSkLen)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pUploader;
        assert(sizeof(pKodoUploader->sk_) - 1 > _nSkLen);
        memcpy(pKodoUploader->sk_, _pSk, _nSkLen);
}

static void setBucket(TsUploader* _pUploader, char *_pBucketName, int _nBucketNameLen)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pUploader;
        assert(sizeof(pKodoUploader->bucketName_) - 1 > _nBucketNameLen);
        memcpy(pKodoUploader->bucketName_, _pBucketName, _nBucketNameLen);
}

static void setCallbackUrl(TsUploader* _pUploader, char *_pCallbackUrl, int _nCallbackUrlLen)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pUploader;
        assert(sizeof(pKodoUploader->callback_) - 1 > _nCallbackUrlLen);
        memcpy(pKodoUploader->callback_, _pCallbackUrl, _nCallbackUrlLen);
}

static void setDeleteAfterDays(TsUploader* _pUploader, int nDays)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pUploader;
        pKodoUploader->deleteAfterDays_ = nDays;
}

static void setToken(TsUploader* _pUploader, char *_pToken)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pUploader;
        pKodoUploader->pToken_ = _pToken;
}

static char * getErrorMsg(const char *_pJson, char *_pBuf, int _nBufLen)
{
        const char * pStart = _pJson;
        pStart = strstr(pStart, "\\\"error\\\"");
        printf("\\\"error\\\"");
        if (pStart == NULL)
                return NULL;
        pStart += strlen("\\\"error\\\"");

        while(*pStart != '"') {
                pStart++;
        }
        pStart++;

        const char * pEnd = strchr(pStart+1, '\\');
        if (pEnd == NULL)
                return NULL;
        int nLen = pEnd - pStart;
        if(nLen > _nBufLen - 1) {
                nLen = _nBufLen - 1;
        }
        memcpy(_pBuf, pStart, nLen);
        _pBuf[nLen] = 0;
        return _pBuf;
}

#ifdef MULTI_SEG_TEST
static int newSegCount = 0;
#endif
static void * streamUpload(void *_pOpaque)
{
        KodoUploader * pUploader = (KodoUploader *)_pOpaque;
        
        char *uptoken = NULL;
        Qiniu_Client client;
        int canFreeToken = 0;
#ifndef DISABLE_OPENSSL
        if (pUploader->pToken_ == NULL || pUploader->pToken_[0] == 0) {
                Qiniu_Mac mac;
                mac.accessKey = pUploader->ak_;
                mac.secretKey = pUploader->sk_;
                
                Qiniu_RS_PutPolicy putPolicy;
                Qiniu_Zero(putPolicy);
                putPolicy.scope = pUploader->bucketName_;
                putPolicy.deleteAfterDays = pUploader->deleteAfterDays_;
                putPolicy.callbackUrl = pUploader->callback_;
                uptoken = Qiniu_RS_PutPolicy_Token(&putPolicy, &mac);
                canFreeToken = 1;
                //init
                Qiniu_Client_InitMacAuth(&client, 1024, &mac);
        } else {
#else
                uptoken = pUploader->pToken_;
                Qiniu_Client_InitNoAuth(&client, 1024);
#endif
        
#ifndef DISABLE_OPENSSL
        }
#endif
        
        Qiniu_Io_PutRet putRet;
        Qiniu_Io_PutExtra putExtra;
        Qiniu_Zero(putExtra);
        //设置机房域名
        //Qiniu_Use_Zone_Beimei(Qiniu_False);
        //Qiniu_Use_Zone_Huabei(Qiniu_True);
        //Qiniu_Use_Zone_Huadong(Qiniu_True);
#ifdef DISABLE_OPENSSL
        Qiniu_Use_Zone_Huadong(Qiniu_False);
#else
        Qiniu_Use_Zone_Huadong(Qiniu_True);
#endif
        //Qiniu_Use_Zone_Huanan(Qiniu_True);
        
        //put extra
        //putExtra.upHost="http://nbxs-gate-up.qiniu.com";
        
        char key[128] = {0};
        
        //todo wait for first packet
        if (pUploader->nWaitFirstMutexLocked_ == WF_LOCKED) {
                pthread_mutex_lock(&pUploader->waitFirstMutex_);
                pthread_mutex_unlock(&pUploader->waitFirstMutex_);
        }
        if (pUploader->nWaitFirstMutexLocked_ != WF_FIRST) {
                goto END;
        }
        logdebug("upload start");
        
        //segmentid(time)
        int64_t curTime = GetCurrentNanosecond();
        // ts/uid/ua_id/yyyy/mm/dd/hh/mm/ss/mmm/fragment_start_ts/expiry.ts
        time_t secs = curTime / 1000000000;
#ifndef MULTI_SEG_TEST
        if ((curTime - nLastUploadTsTime) > nNewSegmentInterval * 1000000000ll) {
#else
        if (newSegCount % 10 == 0) {
                if (newSegCount == 0)
                        newSegCount++;
                else
                        newSegCount = 0;
#endif
                nSegmentId = curTime;
#ifdef UPLOAD_SEG_INFO
                // seg/segid 目前在服务端生成seg文件
                sprintf(key, "seg/%lld", gDeviceId, nSegmentId / 1000000);
                Qiniu_Error segErr = Qiniu_Io_PutBuffer(&client, &putRet, uptoken, key, "", 0, NULL);
                if (segErr.code != 200) {
                        pUploader->state = TK_UPLOAD_FAIL;
                        logerror("upload seg file %s:%s code:%d curl_error:%s kodo_error:%s", pUploader->bucketName_, key,
                                 segErr.code, segErr.message,Qiniu_Buffer_CStr(&client.b));
                        //debug_log(&client, error);
                } else {
                        pUploader->state = TK_UPLOAD_OK;
                        logdebug("upload seg file %s: key:%s success", pUploader->bucketName_, key);
                }
#endif
        }
#ifdef MULTI_SEG_TEST
        else {
                newSegCount++;
        }
#endif
        nLastUploadTsTime = curTime;
        
        memset(key, 0, sizeof(key));
        //ts/uaid/startts/fragment_start_ts/expiry.ts
        sprintf(key, "ts/%s/%lld/%lld/%d.ts", gDeviceId,
                curTime / 1000000, nSegmentId / 1000000, pUploader->deleteAfterDays_);
#ifdef TK_STREAM_UPLOAD
        client.xferinfoData = _pOpaque;
        client.xferinfoCb = timeoutCallback;
        Qiniu_Error error = Qiniu_Io_PutStream(&client, &putRet, uptoken, key, pUploader, -1, getDataCallback, &putExtra);
#else
        Qiniu_Error error = Qiniu_Io_PutBuffer(&client, &putRet, uptoken, key, (const char*)pUploader->pTsData,
                                               pUploader->nTsDataLen, &putExtra);
#endif
#ifdef __ARM
        report_status( error.code );// add by liyq to record ts upload status
#endif
        if (error.code != 200) {
                pUploader->state = TK_UPLOAD_FAIL;
                if (error.code == 401) {
                        logerror("upload file :%s expsize:%d httpcode=%d errmsg=%s", key, pUploader->getDataBytes, error.code, Qiniu_Buffer_CStr(&client.b));
                } else if (error.code >= 500) {
                        const char * pFullErrMsg = Qiniu_Buffer_CStr(&client.b);
                        char errMsg[256];
                        char *pMsg = getErrorMsg(pFullErrMsg, errMsg, sizeof(errMsg));
                        if (pMsg) {
                                logerror("upload file :%s httpcode=%d errmsg={\"error\":\"%s\"}", key, error.code, pMsg);
                        }else {
                                logerror("upload file :%s httpcode=%d errmsg=%s", key, error.code,
                                         pFullErrMsg);
                        }
                } else {
			const char *pCurlErrMsg = curl_easy_strerror(error.code);
			if (pCurlErrMsg != NULL) {
                                logerror("upload file :%s expsize:%d errorcode=%d errmsg={\"error\":\"%s\"}", key, pUploader->getDataBytes, error.code, pCurlErrMsg);
			} else {
                                logerror("upload file :%s expsize:%d errorcode=%d errmsg={\"error\":\"unknown error\"}", key, pUploader->getDataBytes, error.code);
			}
                }
                //debug_log(&client, error);
        } else {
                pUploader->state = TK_UPLOAD_OK;
                logdebug("upload file %s: size:(exp:%lld real:%lld) key:%s success", pUploader->bucketName_,
                          pUploader->getDataBytes, pUploader->nLastUlnow, key);
        }
END:
        if (canFreeToken) {
                Qiniu_Free(uptoken);
        }
        Qiniu_Client_Cleanup(&client);

        return 0;
}

#ifdef TK_STREAM_UPLOAD
size_t getDataCallback(void* buffer, size_t size, size_t n, void* rptr)
{
        KodoUploader * pUploader = (KodoUploader *) rptr;
        int nPopLen = 0;
        nPopLen = pUploader->pQueue_->Pop(pUploader->pQueue_, buffer, size * n);
        if (nPopLen < 0) {
		if (nPopLen == TK_TIMEOUT) {
                        if (pUploader->nLastFrameTimestamp >= 0 &&  pUploader->nFirstFrameTimestamp >= 0) {
                                return 0;
                        }
                        logerror("first pop from queue timeout:%d %lld %lld", nPopLen, pUploader->nLastFrameTimestamp, pUploader->nFirstFrameTimestamp);
		}
                return CURL_READFUNC_ABORT;
        }
        if (nPopLen == 0) {
                if (IsProcStatusQuit()) {
                        return CURL_READFUNC_ABORT;
                }
                return 0;
        }

        int nTmp = 0;
        char *pBuf = (char *)buffer;
        while (size * n - nPopLen > 0) {
                nTmp = pUploader->pQueue_->Pop(pUploader->pQueue_, pBuf + nPopLen, size * n - nPopLen);
                if (nTmp == 0)
                        break;
                if (nTmp < 0) {
		        if (nTmp == TK_TIMEOUT) {
                                if (pUploader->nLastFrameTimestamp >= 0 &&  pUploader->nFirstFrameTimestamp >= 0) {
                                        goto RET;
                                }
                                logerror("next pop from queue timeout:%d %lld %lld", nTmp, pUploader->nLastFrameTimestamp, pUploader->nFirstFrameTimestamp);
                        }
                        return CURL_READFUNC_ABORT;
                }
                nPopLen += nTmp;
        }
        UploaderStatInfo info;
        pUploader->pQueue_->GetStatInfo(rptr, &info);
        //if (!info.nIsReadOnly) {
        //        pUploader->nIsFinished = 1;
        //}
RET:
        pUploader->getDataBytes += nPopLen;
        return nPopLen;
}

static int streamUploadStart(TsUploader * _pUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pUploader;
        int ret = pthread_create(&pKodoUploader->workerId_, NULL, streamUpload, _pUploader);
        if (ret == 0) {
                pKodoUploader->isThreadStarted_ = 1;
                return 0;
        } else {
                logerror("start upload thread fail:%d", ret);
                return TK_THREAD_ERROR;
        }
}

static void streamUploadStop(TsUploader * _pUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pUploader;
        if(pKodoUploader->nWaitFirstMutexLocked_ == WF_LOCKED) {
                pKodoUploader->nWaitFirstMutexLocked_ = WF_QUIT;
                pthread_mutex_unlock(&pKodoUploader->waitFirstMutex_);
        }
        pthread_mutex_lock(&pKodoUploader->waitFirstMutex_);
        pKodoUploader->nWaitFirstMutexLocked_ = WF_QUIT;
        pthread_mutex_unlock(&pKodoUploader->waitFirstMutex_);
        
        if (pKodoUploader->isThreadStarted_) {
                pKodoUploader->pQueue_->StopPush(pKodoUploader->pQueue_);
                pthread_join(pKodoUploader->workerId_, NULL);
                pKodoUploader->isThreadStarted_ = 0;
        }
        return;
}

static int streamPushData(TsUploader *pTsUploader, char * pData, int nDataLen)
{
        KodoUploader * pKodoUploader = (KodoUploader *)pTsUploader;
        
        int ret = pKodoUploader->pQueue_->Push(pKodoUploader->pQueue_, (char *)pData, nDataLen);
        if (pKodoUploader->nWaitFirstMutexLocked_ == WF_LOCKED) {
                pKodoUploader->nWaitFirstMutexLocked_ = WF_FIRST;
                pthread_mutex_unlock(&pKodoUploader->waitFirstMutex_);
        }
        return ret;
}

#else

static int memUploadStart(TsUploader * _pUploader)
{
        return 0;
}

static void memUploadStop(TsUploader * _pUploader)
{
        return;
}

static int memPushData(TsUploader *pTsUploader, char * pData, int nDataLen)
{
        KodoUploader * pKodoUploader = (KodoUploader *)pTsUploader;
        if (pKodoUploader->pTsData == NULL) {
                pKodoUploader->pTsData = malloc(pKodoUploader->nTsDataCap);
                pKodoUploader->nTsDataLen = 0;
        }
        if (pKodoUploader->nTsDataLen + nDataLen > pKodoUploader->nTsDataCap){
                char * tmp = malloc(pKodoUploader->nTsDataCap * 2);
                memcpy(tmp, pKodoUploader->pTsData, pKodoUploader->nTsDataLen);
                free(pKodoUploader->pTsData);
                pKodoUploader->pTsData = tmp;
                pKodoUploader->nTsDataCap *= 2;
                memcpy(tmp + pKodoUploader->nTsDataLen, pData, nDataLen);
                pKodoUploader->nTsDataLen += nDataLen;
                return nDataLen;
        }
        memcpy(pKodoUploader->pTsData + pKodoUploader->nTsDataLen, pData, nDataLen);
        pKodoUploader->nTsDataLen += nDataLen;
        return nDataLen;
}
#endif

static void getStatInfo(TsUploader *pTsUploader, UploaderStatInfo *_pStatInfo)
{
        KodoUploader * pKodoUploader = (KodoUploader *)pTsUploader;
#ifdef TK_STREAM_UPLOAD
        pKodoUploader->pQueue_->GetStatInfo(pKodoUploader->pQueue_, _pStatInfo);
#else
        _pStatInfo->nLen_ = 0;
        _pStatInfo->nPopDataBytes_ = pKodoUploader->nTsDataLen;
        _pStatInfo->nPopDataBytes_ = pKodoUploader->nTsDataLen;
#endif
        return;
}

void recordTimestamp(TsUploader *_pTsUploader, int64_t _nTimestamp)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pTsUploader;
        if (pKodoUploader->nFirstFrameTimestamp == -1) {
                pKodoUploader->nFirstFrameTimestamp = _nTimestamp;
                pKodoUploader->nLastFrameTimestamp = _nTimestamp;
        }
        pKodoUploader->nLastFrameTimestamp = _nTimestamp;
        return;
}

UploadState getUploaderState(TsUploader *_pTsUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pTsUploader;
        return pKodoUploader->state;
}

int NewUploader(TsUploader ** _pUploader, enum CircleQueuePolicy _policy, int _nMaxItemLen, int _nInitItemCount)
{
        KodoUploader * pKodoUploader = (KodoUploader *) malloc(sizeof(KodoUploader));
        if (pKodoUploader == NULL) {
                return TK_NO_MEMORY;
        }

        memset(pKodoUploader, 0, sizeof(KodoUploader));
        int ret = pthread_mutex_init(&pKodoUploader->waitFirstMutex_, NULL);
        if (ret != 0){
                free(pKodoUploader);
                return TK_MUTEX_ERROR;
        }
        pthread_mutex_lock(&pKodoUploader->waitFirstMutex_);
        pKodoUploader->nWaitFirstMutexLocked_ = WF_LOCKED;
#ifdef TK_STREAM_UPLOAD
        ret = NewCircleQueue(&pKodoUploader->pQueue_, 0, _policy, _nMaxItemLen, _nInitItemCount);
        if (ret != 0) {
                free(pKodoUploader);
                return ret;
        }
#else
        pKodoUploader->nTsDataCap = 1024 * 1024;
#endif
        pKodoUploader->nFirstFrameTimestamp = -1;
        pKodoUploader->nLastFrameTimestamp = -1;
        pKodoUploader->uploader.SetToken = setToken;
        pKodoUploader->uploader.SetAccessKey = setAccessKey;
        pKodoUploader->uploader.SetSecretKey = setSecretKey;
        pKodoUploader->uploader.SetBucket = setBucket;
        pKodoUploader->uploader.SetCallbackUrl = setCallbackUrl;
        pKodoUploader->uploader.SetDeleteAfterDays = setDeleteAfterDays;
#ifdef TK_STREAM_UPLOAD
        pKodoUploader->uploader.UploadStart = streamUploadStart;
        pKodoUploader->uploader.UploadStop = streamUploadStop;
        pKodoUploader->uploader.Push = streamPushData;
#else
        pKodoUploader->uploader.UploadStart = memUploadStart;
        pKodoUploader->uploader.UploadStop = memUploadStop;
        pKodoUploader->uploader.Push = memPushData;
#endif
        pKodoUploader->uploader.GetStatInfo = getStatInfo;
        pKodoUploader->uploader.SetSegmentId = setSegmentId;
        pKodoUploader->uploader.RecordTimestamp = recordTimestamp;
        pKodoUploader->uploader.GetUploaderState = getUploaderState;
	pKodoUploader->nCount = gnCount++;
        
        *_pUploader = (TsUploader*)pKodoUploader;
        
        return 0;
}

void DestroyUploader(TsUploader ** _pUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)(*_pUploader);
        
        pthread_mutex_destroy(&pKodoUploader->waitFirstMutex_);
#ifdef TK_STREAM_UPLOAD
        if (pKodoUploader->isThreadStarted_) {
                pthread_join(pKodoUploader->workerId_, NULL);
        }
        DestroyQueue(&pKodoUploader->pQueue_);
#else
        free(pKodoUploader->pTsData);
#endif
        
        free(pKodoUploader);
        * _pUploader = NULL;
        return;
}

int SetDeviceId(char *_pDeviceId)
{
        int ret = 0;
        ret = snprintf(gDeviceId, sizeof(gDeviceId), "%s", _pDeviceId);
        assert(ret > 0);
        if (ret == sizeof(gDeviceId)) {
                logerror("deviceid:%s is too long", _pDeviceId);
                return TK_ARG_ERROR;
        }
        
        return 0;
}

void SetNewSegmentInterval(int nInterval)
{
        if (nInterval > 0) {
                nNewSegmentInterval = nInterval;
        }
}
