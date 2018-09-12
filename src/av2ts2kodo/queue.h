#ifndef __CIRCLE_QUEUE_H__
#define __CIRCLE_QUEUE_H__

#include <pthread.h>
#ifndef __APPLE__
#include <stdint.h>
#endif

enum CircleQueuePolicy{
        TSQ_FIX_LENGTH,
        TSQ_VAR_LENGTH
};

typedef struct _CircleQueue CircleQueue;


typedef int(*CircleQueuePush)(CircleQueue *pQueue, char * pData, int nDataLen);
typedef int(*CircleQueuePop)(CircleQueue *pQueue, char * pBuf, int nBufLen);
typedef int(*CircleQueuePopWithTimeout)(CircleQueue *pQueue, char * pBuf, int nBufLen, int64_t nTimeoutAfterUsec);
typedef void(*CircleQueueStopPush)(CircleQueue *pQueue);

typedef struct _UploaderStatInfo {
        int nPushDataBytes_;
        int nPopDataBytes_;
        int nLen_;
        int nOverwriteCnt;
        int nIsReadOnly;
	int nDropped;
}UploaderStatInfo;

typedef struct _CircleQueue{
        CircleQueuePush Push;
        CircleQueuePop Pop;
        CircleQueuePopWithTimeout PopWithTimeout;
        CircleQueueStopPush StopPush;
        void (*GetStatInfo)(CircleQueue *pQueue, UploaderStatInfo *pStatInfo);
}CircleQueue;

int NewCircleQueue(CircleQueue **pQueue, int nIsAvailableAfterTimeout,  enum CircleQueuePolicy policy, int nMaxItemLen, int nInitItemCount);
void DestroyQueue(CircleQueue **_pQueue);

#endif
