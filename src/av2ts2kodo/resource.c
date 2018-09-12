#include "resource.h"
#include "base.h"

typedef struct _ResourceMgr
{
        CircleQueue * pQueue_;
        pthread_t mgrThreadId_;
        int nQuit_;
        int nIsStarted_;
}ResourceMgr;

static ResourceMgr manager;

static void * recycle(void *_pOpaque)
{
        UploaderStatInfo info = {0};
        manager.pQueue_->GetStatInfo(manager.pQueue_, &info);
        while(!manager.nQuit_ && info.nLen_ == 0) {
                AsyncInterface *pAsync = NULL;
                int ret = manager.pQueue_->Pop(manager.pQueue_, (char *)(&pAsync), sizeof(AsyncInterface *));
                if (ret == TK_TIMEOUT) {
                        continue;
                }
                if (ret == sizeof(TsUploader *)) {
                        loginfo("pop from mgr:%p\n", pAsync);
                        if (pAsync == NULL) {
                                logwarn("NULL function");
                        } else {
                                AsynFunction func = pAsync->function;
                                func(pAsync);
                        }
                }
                manager.pQueue_->GetStatInfo(manager.pQueue_, &info);
        }
}

int PushFunction(void *_pAsyncInterface)
{
        if (!manager.nIsStarted_) {
                return -1;
        }
        return manager.pQueue_->Push(manager.pQueue_, (char *)(&_pAsyncInterface), sizeof(AsyncInterface *));
}

int StartMgr()
{
        if (manager.nIsStarted_) {
                return 0;
        }

        int ret = NewCircleQueue(&manager.pQueue_, 1, TSQ_FIX_LENGTH, sizeof(void *), 100);
        if (ret != 0){
                return ret;
        }

        ret = pthread_create(&manager.mgrThreadId_, NULL, recycle, NULL);
        if (ret != 0) {
                manager.nIsStarted_ = 0;
                return TK_THREAD_ERROR;
        }
        manager.nIsStarted_ = 1;
        
        return 0;
}

void StopMgr()
{
        manager.nQuit_ = 1;
        if (manager.nIsStarted_) {
                PushFunction(NULL);
                pthread_join(manager.mgrThreadId_, NULL);
                manager.nIsStarted_ = 0;
                if (manager.pQueue_) {
                        DestroyQueue(&manager.pQueue_);
                }
        }
        return;
}
