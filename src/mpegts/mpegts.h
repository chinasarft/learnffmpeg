#ifndef __MPEG_TS__
#define __MPEG_TS__
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* pids */
#define PAT_PID                 0x0000
#define SDT_PID                 0x0011
//下面的pid不是标准，完全是因为简单化
#define PMT_PID 0x1000
#define VIDEO_PID 0x101
#define AUDIO_PID 0x100

/* table ids */
#define PAT_TID   0x00
#define PMT_TID   0x02
#define M4OD_TID  0x05
#define SDT_TID   0x42




#define STREAM_TYPE_PRIVATE_SECTION 0x05
#define STREAM_TYPE_PRIVATE_DATA    0x06
#define STREAM_TYPE_AUDIO_AAC       0x0f
#define STREAM_TYPE_VIDEO_H264      0x1b
#define STREAM_TYPE_VIDEO_HEVC      0x24

//调整字段控制,。01仅含有效负载，10仅含调整字段，11含有调整字段和有效负载。为00的话解码器不进行处理。
#define ADAPTATION_INGNORE 0x0
#define ADAPTATION_JUST_PAYLOAD 0x1
#define ADAPTATION_JUST_PADDING 0x2
#define ADAPTATION_BOTH 0x3


typedef int (*TsPacketCallback)(void *pOpaque, void* pTsData, int nTsDataLen);

typedef struct PES PES;
typedef struct PES{
        uint8_t *pESData;
        int nESDataLen;
        int nPos; //指向pESData
        int nStreamId; //Audio streams (0xC0-0xDF), Video streams (0xE0-0xEF)
        int nPid;
        int64_t nPts;
        uint8_t nWithPcr;
        //设想是传入h264(或者音频)给pESData， 在封装ts时候每次应该封装多少长度的数据是应该知道的
        //也是尽量减少内存使用
}PES;

void InitVideoPESWithPcr(PES *_pPes, uint8_t *_pData, int _nDataLen, int64_t _nPts);
void InitVideoPES(PES *pPes, uint8_t *pData, int nDataLen, int64_t nPts);
void InitAudioPES(PES *pPes, uint8_t *pData, int nDataLen, int64_t nPts);
int GetPESData(PES *pPes, int _nCounter, int _nPid, uint8_t *pData, int nLen ); //返回0则到了EOF

int WriteTsHeader(uint8_t *_pBuf, int _nUinitStartIndicator, int _nCount, int _nPid, int _nAdaptationField);
void SetAdaptationFieldFlag(uint8_t *_pBuf, int _nAdaptationField);
int WriteSDT(uint8_t *_pBuf, int _nUinitStartIndicator, int _nCount, int _nAdaptationField);
int WritePAT(uint8_t *_pBuf, int _nUinitStartIndicator, int _nCount, int _nAdaptationField);
int WritePMT(uint8_t *_pBuf, int _nUinitStartIndicator, int _nCount, int _nAdaptationField, int _nVStreamType, int _nAStreamType);

#endif
