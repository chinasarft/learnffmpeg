#include "tsmux.h"

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
        
        uint8_t nPcrFlag; //分析ffmpeg，pcr只在pes中出现一次在最开头
}TsMuxerContext;

static uint16_t getPidCounter(TsMuxerContext* _pMuxCtx, uint64_t _nPID)
{
        int nCount = 0;
        for (int i = 0; i  < _pMuxCtx->nPidCounterMapLen; i++) {
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
        int nLen = 0;
        int nCount = 0;
        if (_pMuxCtx->nLastPts == 0 || _nPts - _pMuxCtx->nLastPts > 300) {
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
                nLen = WritePMT(_pMuxCtx->tsPacket, 1, nCount, ADAPTATION_JUST_PAYLOAD, STREAM_TYPE_VIDEO_H264, STREAM_TYPE_AUDIO_AAC);
                memset(&_pMuxCtx->tsPacket[nLen], 0xff, 188 - nLen);
                _pMuxCtx->arg.output(_pMuxCtx->arg.pOpaque,_pMuxCtx->tsPacket, 188);
        }
}

uint16_t Pids[5] = {AUDIO_PID, VIDEO_PID, PAT_PID, PMT_PID, SDT_PID};
TsMuxerContext * NewTsMuxerContext(TsMuxerArg *pArg)
{
        TsMuxerContext *pTsMuxerCtx = (TsMuxerContext *)malloc(sizeof(TsMuxerContext));
        memset(pTsMuxerCtx, 0, sizeof(TsMuxerContext));
        pTsMuxerCtx->arg = *pArg;
        pTsMuxerCtx->nPidCounterMapLen = 5;
        for (int i = 0; i < pTsMuxerCtx->nPidCounterMapLen; i++){
                pTsMuxerCtx->pidCounterMap[i].nPID = Pids[i];
                pTsMuxerCtx->pidCounterMap[i].nCounter = 0;
        }
        return pTsMuxerCtx;
}

static void makeTsPacket(TsMuxerContext* _pMuxCtx, int _nPid)
{
        int nReadLen = 0;
        int nCount = 0;
        do {
                
                nReadLen = GetPESData(&_pMuxCtx->pes, 0, _nPid, _pMuxCtx->tsPacket, 188);
                if (nReadLen == 188){
                        nCount = getPidCounter(_pMuxCtx, _nPid);
                        WriteContinuityCounter(_pMuxCtx->tsPacket, nCount);
                        _pMuxCtx->arg.output(_pMuxCtx->arg.pOpaque, _pMuxCtx->tsPacket, 188);
                }
        }while(nReadLen != 0);
}

int MuxerAudio(TsMuxerContext* _pMuxCtx, uint8_t *_pData, int _nDataLen, int64_t _nPts)
{
        writeTable(_pMuxCtx, _nPts);
       
        InitAudioPES(&_pMuxCtx->pes, _pData, _nDataLen, _nPts);
        makeTsPacket(_pMuxCtx, AUDIO_PID);
        
        return 0;
}

int MuxerVideo(TsMuxerContext* _pMuxCtx, uint8_t *_pData, int _nDataLen, int64_t _nPts)
{
        writeTable(_pMuxCtx, _nPts);
        
        if (_pMuxCtx->nPcrFlag == 0) {
                _pMuxCtx->nPcrFlag = 1;
                InitVideoPESWithPcr(&_pMuxCtx->pes, _pData, _nDataLen, _nPts);
        } else {
                InitVideoPES(&_pMuxCtx->pes, _pData, _nDataLen, _nPts);
        }
        makeTsPacket(_pMuxCtx, VIDEO_PID);
        
        return 0;
}

int MuxerFlush(TsMuxerContext* pMuxerCtx)
{
        return 0;
}

void DestroyTsMuxerContext(TsMuxerContext *pTsMuxerCtx)
{
        
}
