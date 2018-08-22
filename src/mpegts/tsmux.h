#ifndef __TS_MUX__
#define __TS_MUX__
#include <stdint.h>
#include "mpegts.h"

typedef struct TsMuxerContext TsMuxerContext;


typedef struct {
        TkAudioFormat nAudioFormat;
        int nAudioSampleRate;
        int nAudioChannels;
        TkVideoFormat nVideoFormat;
        TsPacketCallback output;
        void *pOpaque;
}TsMuxerArg;

TsMuxerContext * NewTsMuxerContext(TsMuxerArg *pArg);
int MuxerAudio(TsMuxerContext* pMuxerCtx, uint8_t *pData, int nDataLen, int64_t nPts);
int MuxerVideo(TsMuxerContext* pMuxerCtx, uint8_t *pData, int nDataLen,  int64_t nPts);
int MuxerFlush(TsMuxerContext* pMuxerCtx);
void DestroyTsMuxerContext(TsMuxerContext *pTsMuxerCtx);

#endif
