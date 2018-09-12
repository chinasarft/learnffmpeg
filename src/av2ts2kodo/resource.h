#ifndef __RESOURCE_MGR_H__
#define __RESOURCE_MGR_H__

#include "uploader.h"
typedef int (*AsynFunction)(void * pOpaque);
typedef struct _AsyncInterface{
        AsynFunction function;
}AsyncInterface;


int StartMgr();
void StopMgr();
int PushFunction(void *pAsyncInterface);

#endif
