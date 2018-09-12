#include "queue.h"
#include "base.h"


#define QUEUE_READ_ONLY_STATE 1
#define QUEUE_TIMEOUT_STATE 2

typedef struct _CircleQueueImp{
        CircleQueue circleQueue;
        char *pData_;
        int nCap_;
        int nLen_;
        int nStart_;
        int nEnd_;
        volatile int nQState_;
        int nItemLen_;
        pthread_mutex_t mutex_;
        pthread_cond_t condition_;
        enum CircleQueuePolicy policy;
        UploaderStatInfo statInfo;
	int nIsAvailableAfterTimeout;
}CircleQueueImp;

static int PushQueue(CircleQueue *_pQueue, char *pData_, int nDataLen)
{
        CircleQueueImp *pQueueImp = (CircleQueueImp *)_pQueue;
        assert(pQueueImp->nItemLen_ - sizeof(int) >= nDataLen);

        pthread_mutex_lock(&pQueueImp->mutex_);
        
        if (!pQueueImp->nIsAvailableAfterTimeout && pQueueImp->nQState_ == QUEUE_TIMEOUT_STATE) {
                pQueueImp->statInfo.nDropped += nDataLen;
                pthread_mutex_unlock(&pQueueImp->mutex_);
                return 0;
        }
        if (pQueueImp->nQState_ == QUEUE_READ_ONLY_STATE) {
                pQueueImp->statInfo.nDropped += nDataLen;
                logwarn("queue is only readable now");
                pthread_mutex_unlock(&pQueueImp->mutex_);
                return TK_NO_PUSH;
        }
        
        int nPos = pQueueImp->nEnd_;
        if (pQueueImp->nLen_ < pQueueImp->nCap_) {
                if(pQueueImp->nEnd_ + 1 == pQueueImp->nCap_){
                        pQueueImp->nEnd_ = 0;
                } else {
                        pQueueImp->nEnd_++;
                }
                memcpy(pQueueImp->pData_ + nPos * pQueueImp->nItemLen_, &nDataLen, sizeof(int));
                memcpy(pQueueImp->pData_ + nPos * pQueueImp->nItemLen_ + sizeof(int), pData_, nDataLen);
                pQueueImp->nLen_++;
                pthread_mutex_unlock(&pQueueImp->mutex_);
                pthread_cond_signal(&pQueueImp->condition_);
                pQueueImp->statInfo.nPushDataBytes_ += nDataLen;
                return nDataLen;
        }
        
        if (pQueueImp->nLen_ == pQueueImp->nCap_) {
                if (pQueueImp->policy == TSQ_FIX_LENGTH) {
                        if(pQueueImp->nEnd_ + 1 == pQueueImp->nCap_){
                                pQueueImp->nEnd_ = 0;
                                pQueueImp->nStart_ = 0;
                        } else {
                                pQueueImp->nEnd_++;
                                pQueueImp->nStart_++;
                        }
                        memcpy(pQueueImp->pData_ + nPos * pQueueImp->nItemLen_, &nDataLen, sizeof(int));
                        memcpy(pQueueImp->pData_ + nPos * pQueueImp->nItemLen_  + sizeof(int), pData_, nDataLen);
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        pthread_cond_signal(&pQueueImp->condition_);

                        pQueueImp->statInfo.nPushDataBytes_ += nDataLen;
                        pQueueImp->statInfo.nOverwriteCnt++;
                        return TK_Q_OVERWRIT;
                } else{
                        char *pTmp = (char *)malloc(pQueueImp->nItemLen_ * pQueueImp->nCap_ * 2);
                        int nOriginCap = pQueueImp->nCap_;
                        if (pTmp == NULL) {
                                pthread_mutex_unlock(&pQueueImp->mutex_);
                                return -1;
                        }
                        pQueueImp->nCap_ *= 2;
                        //TODO
                        free(pQueueImp->pData_);
                        pQueueImp->pData_ = pTmp;
                        
                        
                        memcpy(pTmp,
                               pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nItemLen_,
                               (nOriginCap - pQueueImp->nStart_) * pQueueImp->nItemLen_);
                        memcpy(pTmp + (nOriginCap - pQueueImp->nStart_) * pQueueImp->nItemLen_,
                               pQueueImp->pData_ + pQueueImp->nEnd_ * pQueueImp->nItemLen_,
                               pQueueImp->nEnd_ * pQueueImp->nItemLen_);
                        pQueueImp->nStart_ = 0;
                        pQueueImp->nEnd_ = nOriginCap + 1;
                        memcpy(pQueueImp->pData_ + nPos * pQueueImp->nItemLen_, &nDataLen, sizeof(int));
                        memcpy(pTmp + nOriginCap * pQueueImp->nItemLen_ + sizeof(int), pData_, nDataLen);
                        
                        pQueueImp->nLen_++;
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        pthread_cond_signal(&pQueueImp->condition_);

                        pQueueImp->statInfo.nPushDataBytes_ += nDataLen;
                        return nDataLen;
                }
        }
        
        pthread_mutex_unlock(&pQueueImp->mutex_);
        
        return -1;
}

static int PopQueueWithTimeout(CircleQueue *_pQueue, char *pBuf_, int nBufLen, int64_t nUSec)
{
        CircleQueueImp *pQueueImp = (CircleQueueImp *)_pQueue;
        
        pthread_mutex_lock(&pQueueImp->mutex_);
        if (!pQueueImp->nIsAvailableAfterTimeout && pQueueImp->nQState_ == QUEUE_TIMEOUT_STATE) {
                pthread_mutex_unlock(&pQueueImp->mutex_);
                return TK_TIMEOUT;
        }
        if (pQueueImp->nQState_ == QUEUE_READ_ONLY_STATE && pQueueImp->nLen_ == 0) {
                pthread_mutex_unlock(&pQueueImp->mutex_);
                return 0;
        }
        
        int ret = 0;
        while (pQueueImp->nLen_ == 0) {
                struct timeval now;
                gettimeofday(&now, NULL);
                struct timespec timeout;
                timeout.tv_sec = now.tv_sec + nUSec / 1000000;
                timeout.tv_nsec = (now.tv_usec + nUSec % 1000000) * 1000;
                
                ret = pthread_cond_timedwait(&pQueueImp->condition_, &pQueueImp->mutex_, &timeout);
                if (pQueueImp->nQState_ == QUEUE_READ_ONLY_STATE && pQueueImp->nLen_ == 0) {
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        return 0;
                }
                if (ret == ETIMEDOUT) {
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        pQueueImp->nQState_ = QUEUE_TIMEOUT_STATE;
                        return TK_TIMEOUT;
                }
        }
        assert (pQueueImp->nLen_ != 0);
        int nDataLen = 0;
        memcpy(&nDataLen, pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nItemLen_, sizeof(int));
        int nRemain = nDataLen - nBufLen;
        logtrace("pop remain:%d pop:%d buflen:%d len:%d", nRemain, nDataLen, nBufLen, pQueueImp->nLen_);
        if (nRemain > 0) {
                memcpy(pBuf_, pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nItemLen_ + sizeof(int), nBufLen);
                memcpy(pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nItemLen_, &nRemain, sizeof(int));
                memcpy(pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nItemLen_ + sizeof(int),
                       pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nItemLen_ + sizeof(int) + nBufLen,
                       nRemain);
                nDataLen = nBufLen;
        } else {
                memcpy(pBuf_, pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nItemLen_ + sizeof(int), nDataLen);
                if (pQueueImp->nStart_ + 1 == pQueueImp->nCap_) {
                        pQueueImp->nStart_ = 0;
                } else {
                        pQueueImp->nStart_++;
                }
                pQueueImp->nLen_--;
        }
        
        pQueueImp->statInfo.nPopDataBytes_ += nDataLen;
        pthread_mutex_unlock(&pQueueImp->mutex_);
        return nDataLen;
}


static int PopQueue(CircleQueue *_pQueue, char *pBuf_, int nBufLen)
{
        int64_t usec = 1000000;
        return PopQueueWithTimeout(_pQueue, pBuf_, nBufLen, usec * 60 * 60 * 24 * 365);
}

static int PopQueueWithNoOverwrite(CircleQueue *_pQueue, char *pBuf_, int nBufLen)
{
        CircleQueueImp *pQueueImp = (CircleQueueImp *)_pQueue;
        if (pQueueImp->statInfo.nOverwriteCnt > 0) {
                return TK_Q_OVERWRIT;
        }
        int64_t usec = 1000000;
        if (pQueueImp->statInfo.nPushDataBytes_> 0) {
                return PopQueueWithTimeout(_pQueue, pBuf_, nBufLen, usec * 1);
        } else {
                return PopQueueWithTimeout(_pQueue, pBuf_, nBufLen, usec * 60 * 60 * 24 * 365);
        }
}

static void StopPush(CircleQueue *_pQueue)
{
        CircleQueueImp *pQueueImp = (CircleQueueImp *)_pQueue;
        
        pthread_mutex_lock(&pQueueImp->mutex_);
        pQueueImp->nQState_ = QUEUE_READ_ONLY_STATE;
        pthread_mutex_unlock(&pQueueImp->mutex_);
        
        pthread_cond_signal(&pQueueImp->condition_);
        return;
}

static void getStatInfo(CircleQueue *_pQueue, UploaderStatInfo *_pStatInfo)
{
        CircleQueueImp *pQueueImp = (CircleQueueImp *)_pQueue;
        
        _pStatInfo->nPushDataBytes_ = pQueueImp->statInfo.nPushDataBytes_;
        _pStatInfo->nPopDataBytes_ = pQueueImp->statInfo.nPopDataBytes_;
        _pStatInfo->nLen_ = pQueueImp->nLen_;
        _pStatInfo->nDropped = pQueueImp->statInfo.nDropped;
        _pStatInfo->nIsReadOnly = pQueueImp->nQState_;
        return;
}

int NewCircleQueue(CircleQueue **_pQueue, int nIsAvailableAfterTimeout, enum CircleQueuePolicy _policy, int _nMaxItemLen, int _nInitItemCount)
{
        int ret;
        CircleQueueImp *pQueueImp = (CircleQueueImp *)malloc(sizeof(CircleQueueImp) +
                                                             (_nMaxItemLen + sizeof(int)) * _nInitItemCount);
        if (pQueueImp == NULL) {
                return TK_NO_MEMORY;
        }
        memset(pQueueImp, 0, sizeof(CircleQueueImp));

        ret = pthread_mutex_init(&pQueueImp->mutex_, NULL);
        if (ret != 0){
                return TK_MUTEX_ERROR;
        }
        ret = pthread_cond_init(&pQueueImp->condition_, NULL);
        if (ret != 0){
                pthread_mutex_destroy(&pQueueImp->mutex_);
                return TK_COND_ERROR;
        }
        
        pQueueImp->policy = _policy;
        pQueueImp->pData_ = (char *)pQueueImp + sizeof(CircleQueueImp);
        pQueueImp->nCap_ = _nInitItemCount;
        pQueueImp->nItemLen_ = _nMaxItemLen + sizeof(int); //前缀int类型的一个长度
        pQueueImp->circleQueue.Pop = PopQueueWithNoOverwrite;
        pQueueImp->circleQueue.Push = PushQueue;
        pQueueImp->circleQueue.PopWithTimeout = PopQueueWithTimeout;
        pQueueImp->circleQueue.StopPush = StopPush;
        pQueueImp->circleQueue.GetStatInfo = getStatInfo;
        pQueueImp->nIsAvailableAfterTimeout = nIsAvailableAfterTimeout;
        
        *_pQueue = (CircleQueue*)pQueueImp;
        return 0;
}

void DestroyQueue(CircleQueue **_pQueue)
{
        CircleQueueImp *pQueueImp = (CircleQueueImp *)(*_pQueue);

        StopPush(*_pQueue);
        
        pthread_mutex_destroy(&pQueueImp->mutex_);
        pthread_cond_destroy(&pQueueImp->condition_);

        free(pQueueImp);
        *_pQueue = NULL;
        return;
}
