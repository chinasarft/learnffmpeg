#include "tsmux.h"
#include "base.h"
#include <pthread.h>

typedef struct PIDCounter {
        uint16_t nPID;
        uint16_t nCounter;
}PIDCounter;

typedef struct TsMuxerContext{
        TsMuxerArg arg;
        PES pes;
        int nMillisecondPatPeriod;
        uint8_t tsPacket[188];
        
        int nPidCounterMapLen;
        PIDCounter pidCounterMap[5];
        uint64_t nLastPts;
        pthread_mutex_t tsMutex_;
        int isTableWrited;
        
        uint8_t nPcrFlag; //分析ffmpeg，pcr只在pes中出现一次在最开头
}TsMuxerContext;

static uint16_t getPidCounter(TsMuxerContext* _pMuxCtx, uint64_t _nPID)
{
        int nCount = 0;
        int i;

        for ( i = 0; i  < _pMuxCtx->nPidCounterMapLen; i++) {
                if (_pMuxCtx->pidCounterMap[i].nPID == _nPID) {
                        if (_pMuxCtx->pidCounterMap[i].nCounter == 0x0F){
                                _pMuxCtx->pidCounterMap[i].nCounter = 0;
                                return 0x0F;
                        }
                        nCount = _pMuxCtx->pidCounterMap[i].nCounter++;
                        return nCount;
                }
        }
        assert(0);
        return -1;
}

static int writeTsHeader(TsMuxerContext* _pMuxCtx, uint8_t *_pBuf, int _nUinitStartIndicator, int _nPid, int _nAdaptationField)
{
        uint16_t counter = getPidCounter(_pMuxCtx, _nPid);
        return WriteTsHeader(_pBuf, _nUinitStartIndicator, counter, _nPid, _nAdaptationField);
}

static void writeTable(TsMuxerContext* _pMuxCtx, int64_t _nPts)
{
        if (_pMuxCtx->isTableWrited) {
                return;
        }
        pthread_mutex_lock(&_pMuxCtx->tsMutex_);
        if (_pMuxCtx->isTableWrited) {
                pthread_mutex_unlock(&_pMuxCtx->tsMutex_);
                return;
        }
        int nLen = 0;
        int nCount = 0;
        if (_pMuxCtx->nLastPts == 0 || _nPts - _pMuxCtx->nLastPts > 300 * 90) { //300毫米间隔
                /*
                nCount =getPidCounter(_pMuxCtx, 0x11);
                nLen = WriteSDT(_pMuxCtx->tsPacket, 1, nCount, ADAPTATION_JUST_PAYLOAD);
                memset(&_pMuxCtx->tsPacket[nLen], 0xff, 188 - nLen);
                _pMuxCtx->arg.output(_pMuxCtx->arg.pOpaque,_pMuxCtx->tsPacket, 188);
                 */
                
                nCount =getPidCounter(_pMuxCtx, 0x00);
                nLen = WritePAT(_pMuxCtx->tsPacket, 1, nCount, ADAPTATION_JUST_PAYLOAD);
                memset(&_pMuxCtx->tsPacket[nLen], 0xff, 188 - nLen);
                _pMuxCtx->arg.output(_pMuxCtx->arg.pOpaque,_pMuxCtx->tsPacket, 188);
                
                nCount =getPidCounter(_pMuxCtx, 0x1000);
                int nAudioType = 0;
                int nVideoType = 0;
                if (_pMuxCtx->arg.nAudioFormat == TK_AUDIO_AAC) {
                        nAudioType = STREAM_TYPE_AUDIO_AAC;
                } else if (_pMuxCtx->arg.nAudioFormat == TK_AUDIO_PCMU || _pMuxCtx->arg.nAudioFormat == TK_AUDIO_PCMA) {
                        nAudioType = STREAM_TYPE_PRIVATE_DATA;
                }
                if (_pMuxCtx->arg.nVideoFormat == TK_VIDEO_H264) {
                        nVideoType = STREAM_TYPE_VIDEO_H264;
                } else if (_pMuxCtx->arg.nVideoFormat == TK_VIDEO_H265) {
                        nVideoType = STREAM_TYPE_VIDEO_HEVC;
                }
                nLen = WritePMT(_pMuxCtx->tsPacket, 1, nCount, ADAPTATION_JUST_PAYLOAD, nVideoType, nAudioType);
                memset(&_pMuxCtx->tsPacket[nLen], 0xff, 188 - nLen);
                _pMuxCtx->arg.output(_pMuxCtx->arg.pOpaque,_pMuxCtx->tsPacket, 188);
        }
        _pMuxCtx->isTableWrited = 1;
        pthread_mutex_unlock(&_pMuxCtx->tsMutex_);
}

uint16_t Pids[5] = {AUDIO_PID, VIDEO_PID, PAT_PID, PMT_PID, SDT_PID};
int NewTsMuxerContext(TsMuxerArg *pArg, TsMuxerContext **_pTsMuxerCtx)
{
        int i;
        TsMuxerContext *pTsMuxerCtx = (TsMuxerContext *)malloc(sizeof(TsMuxerContext));
        if (pTsMuxerCtx == NULL) {
                return TK_NO_MEMORY;
        }
        memset(pTsMuxerCtx, 0, sizeof(TsMuxerContext));
        pTsMuxerCtx->arg = *pArg;
        pTsMuxerCtx->nPidCounterMapLen = 5;
        for ( i = 0; i < pTsMuxerCtx->nPidCounterMapLen; i++){
                pTsMuxerCtx->pidCounterMap[i].nPID = Pids[i];
                pTsMuxerCtx->pidCounterMap[i].nCounter = 0;
        }
        int ret = pthread_mutex_init(&pTsMuxerCtx->tsMutex_, NULL);
        if (ret != 0){
                free(pTsMuxerCtx);
                return TK_MUTEX_ERROR;
        }
	*_pTsMuxerCtx = pTsMuxerCtx;
        return 0;
}

static int makeTsPacket(TsMuxerContext* _pMuxCtx, int _nPid)
{
        int nReadLen = 0;
        int nCount = 0;
        do {
                int nRet = 0;
                nReadLen = GetPESData(&_pMuxCtx->pes, 0, _nPid, _pMuxCtx->tsPacket, 188);
                if (nReadLen == 188){
                        nCount = getPidCounter(_pMuxCtx, _nPid);
                        WriteContinuityCounter(_pMuxCtx->tsPacket, nCount);
                        nRet = _pMuxCtx->arg.output(_pMuxCtx->arg.pOpaque, _pMuxCtx->tsPacket, 188);
                        if (nRet < 0) {
                                return nRet;
                        }
                }
        }while(nReadLen != 0);
        return 0;
}

int MuxerAudio(TsMuxerContext* _pMuxCtx, uint8_t *_pData, int _nDataLen, int64_t _nPts)
{
        writeTable(_pMuxCtx, 0);
        pthread_mutex_lock(&_pMuxCtx->tsMutex_);
        if (_pMuxCtx->arg.nAudioFormat == TK_AUDIO_AAC) {
                InitAudioPES(&_pMuxCtx->pes, _pData, _nDataLen, _nPts);
        } else {
                InitPrivateTypePES(&_pMuxCtx->pes, _pData, _nDataLen, _nPts);
        }

        int nRet = makeTsPacket(_pMuxCtx, AUDIO_PID);
        pthread_mutex_unlock(&_pMuxCtx->tsMutex_);
        if (nRet < 0)
                return nRet;
        return 0;
}

int MuxerVideo(TsMuxerContext* _pMuxCtx, uint8_t *_pData, int _nDataLen, int64_t _nPts)
{
        writeTable(_pMuxCtx, 0);
        pthread_mutex_lock(&_pMuxCtx->tsMutex_);
        if (_pMuxCtx->nPcrFlag == 0) {
                _pMuxCtx->nPcrFlag = 1;
                InitVideoPESWithPcr(&_pMuxCtx->pes, _pMuxCtx->arg.nVideoFormat, _pData, _nDataLen, _nPts);
        } else {
                InitVideoPES(&_pMuxCtx->pes, _pMuxCtx->arg.nVideoFormat, _pData, _nDataLen, _nPts);
        }

        int nRet = makeTsPacket(_pMuxCtx, VIDEO_PID);
        pthread_mutex_unlock(&_pMuxCtx->tsMutex_);
        if (nRet < 0)
                return nRet;
        return 0;
}

int MuxerFlush(TsMuxerContext* pMuxerCtx)
{
        return 0;
}

void DestroyTsMuxerContext(TsMuxerContext *pTsMuxerCtx)
{
        free(pTsMuxerCtx);        
}
