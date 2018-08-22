#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include "adts.h"
#include "tsmux.h"

TsMuxerArg avArg;

typedef int (*DataCallback)(void *opaque, void *pData, int nDataLen, int nFlag, int64_t timestamp, int nIsKeyFrame);
#define THIS_IS_AUDIO 1
#define THIS_IS_VIDEO 2

#define TEST_AAC 1
//#define TEST_AAC_NO_ADTS 1
#define USE_LINK_ACC 1

//#define INPUT_FROM_FFMPEG

#ifdef INPUT_FROM_FFMPEG
#include <libavformat/avformat.h>
#ifndef TEST_AAC
#define TEST_AAC 1
#endif
#endif

#ifdef TEST_AAC
static int aacfreq[13] = {96000, 88200,64000,48000,44100,32000,24000, 22050 , 16000 ,12000,11025,8000,7350};
typedef struct ADTS{
        ADTSFixheader fix;
        ADTSVariableHeader var;
}ADTS;
#endif
#ifndef TEST_AAC_NO_ADTS

#endif


typedef enum {
        TK_VIDEO_H264,
        TK_VIDEO_H265
}TkVideoFormat;
typedef enum {
        TK_AUDIO_PCMU,
        TK_AUDIO_PCMA,
        TK_AUDIO_AAC
}TkAudioFormat;

FILE *outTs;
int gTotalLen = 0;
char gtestToken[1024] = {0};

enum HEVCNALUnitType {
        HEVC_NAL_TRAIL_N    = 0,
        HEVC_NAL_TRAIL_R    = 1,
        HEVC_NAL_TSA_N      = 2,
        HEVC_NAL_TSA_R      = 3,
        HEVC_NAL_STSA_N     = 4,
        HEVC_NAL_STSA_R     = 5,
        HEVC_NAL_RADL_N     = 6,
        HEVC_NAL_RADL_R     = 7,
        HEVC_NAL_RASL_N     = 8,
        HEVC_NAL_RASL_R     = 9,
        HEVC_NAL_VCL_N10    = 10,
        HEVC_NAL_VCL_R11    = 11,
        HEVC_NAL_VCL_N12    = 12,
        HEVC_NAL_VCL_R13    = 13,
        HEVC_NAL_VCL_N14    = 14,
        HEVC_NAL_VCL_R15    = 15,
        HEVC_NAL_BLA_W_LP   = 16,
        HEVC_NAL_BLA_W_RADL = 17,
        HEVC_NAL_BLA_N_LP   = 18,
        HEVC_NAL_IDR_W_RADL = 19,
        HEVC_NAL_IDR_N_LP   = 20,
        HEVC_NAL_CRA_NUT    = 21,
        HEVC_NAL_IRAP_VCL22 = 22,
        HEVC_NAL_IRAP_VCL23 = 23,
        HEVC_NAL_RSV_VCL24  = 24,
        HEVC_NAL_RSV_VCL25  = 25,
        HEVC_NAL_RSV_VCL26  = 26,
        HEVC_NAL_RSV_VCL27  = 27,
        HEVC_NAL_RSV_VCL28  = 28,
        HEVC_NAL_RSV_VCL29  = 29,
        HEVC_NAL_RSV_VCL30  = 30,
        HEVC_NAL_RSV_VCL31  = 31,
        HEVC_NAL_VPS        = 32,
        HEVC_NAL_SPS        = 33,
        HEVC_NAL_PPS        = 34,
        HEVC_NAL_AUD        = 35,
        HEVC_NAL_EOS_NUT    = 36,
        HEVC_NAL_EOB_NUT    = 37,
        HEVC_NAL_FD_NUT     = 38,
        HEVC_NAL_SEI_PREFIX = 39,
        HEVC_NAL_SEI_SUFFIX = 40,
};
enum HevcType {
        HEVC_META = 0,
        HEVC_I = 1,
        HEVC_B =2
};

static const uint8_t *ff_avc_find_startcode_internal(const uint8_t *p, const uint8_t *end)
{
        const uint8_t *a = p + 4 - ((intptr_t)p & 3);
        
        for (end -= 3; p < a && p < end; p++) {
                if (p[0] == 0 && p[1] == 0 && p[2] == 1)
                        return p;
        }
        
        for (end -= 3; p < end; p += 4) {
                uint32_t x = *(const uint32_t*)p;
                //      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
                //      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
                if ((x - 0x01010101) & (~x) & 0x80808080) { // generic
                        if (p[1] == 0) {
                                if (p[0] == 0 && p[2] == 1)
                                        return p;
                                if (p[2] == 0 && p[3] == 1)
                                        return p+1;
                        }
                        if (p[3] == 0) {
                                if (p[2] == 0 && p[4] == 1)
                                        return p+2;
                                if (p[4] == 0 && p[5] == 1)
                                        return p+3;
                        }
                }
        }
        
        for (end += 3; p < end; p++) {
                if (p[0] == 0 && p[1] == 0 && p[2] == 1)
                        return p;
        }
        
        return end + 3;
}

const uint8_t *ff_avc_find_startcode(const uint8_t *p, const uint8_t *end){
        const uint8_t *out= ff_avc_find_startcode_internal(p, end);
        if(p<out && out<end && !out[-1]) out--;
        return out;
}

static inline int64_t getCurrentMilliSecond(){
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (tv.tv_sec*1000 + tv.tv_usec/1000);
}

static int getFileAndLength(char *_pFname, FILE **_pFile, int *_pLen)
{
        FILE * f = fopen(_pFname, "r");
        if ( f == NULL ) {
                return -1;
        }
        *_pFile = f;
        fseek(f, 0, SEEK_END);
        long nLen = ftell(f);
        fseek(f, 0, SEEK_SET);
        *_pLen = (int)nLen;
        return 0;
}

static int readFileToBuf(char * _pFilename, char ** _pBuf, int *_pLen)
{
        int ret;
        FILE * pFile;
        int nLen = 0;
        ret = getFileAndLength(_pFilename, &pFile, &nLen);
        if (ret != 0) {
                fprintf(stderr, "open file %s fail\n", _pFilename);
                return -1;
        }
        char *pData = malloc(nLen);
        assert(pData != NULL);
        ret = fread(pData, 1, nLen, pFile);
        if (ret <= 0) {
                fprintf(stderr, "open file %s fail\n", _pFilename);
                fclose(pFile);
                free(pData);
                return -2;
        }
        *_pBuf = pData;
        *_pLen = nLen;
        return 0;
}

static int is_h265_picture(int t)
{
        switch (t) {
                case HEVC_NAL_VPS:
                case HEVC_NAL_SPS:
                case HEVC_NAL_PPS:
                case HEVC_NAL_SEI_PREFIX:
                        return HEVC_META;
                case HEVC_NAL_IDR_W_RADL:
                case HEVC_NAL_CRA_NUT:
                        return HEVC_I;
                case HEVC_NAL_TRAIL_N:
                case HEVC_NAL_TRAIL_R:
                case HEVC_NAL_RASL_N:
                case HEVC_NAL_RASL_R:
                        return HEVC_B;
                default:
                        return -1;
        }
}

int start_file_test(char * _pAudioFile, char * _pVideoFile, DataCallback callback, void *opaque)
{
        assert(!(_pAudioFile == NULL && _pVideoFile == NULL));
        
        int ret;
        
        char * pAudioData = NULL;
        int nAudioDataLen = 0;
        if(_pAudioFile != NULL){
                ret = readFileToBuf(_pAudioFile, &pAudioData, &nAudioDataLen);
                if (ret != 0) {
                        printf("map data to buffer fail:%s", _pAudioFile);
                        return -1;
                }
        }
        
        char * pVideoData = NULL;
        int nVideoDataLen = 0;
        if(_pVideoFile != NULL){
                ret = readFileToBuf(_pVideoFile, &pVideoData, &nVideoDataLen);
                if (ret != 0) {
                        free(pAudioData);
                        printf( "map data to buffer fail:%s", _pVideoFile);
                        return -2;
                }
        }
        
        int bAudioOk = 1;
        int bVideoOk = 1;
        if (_pVideoFile == NULL) {
                bVideoOk = 0;
        }
        if (_pAudioFile == NULL) {
                bAudioOk = 0;
        }
        int64_t nSysTimeBase = getCurrentMilliSecond();
        int64_t nNextAudioTime = nSysTimeBase;
        int64_t nNextVideoTime = nSysTimeBase;
        int64_t nNow = nSysTimeBase;
        int audioOffset = 0;
        
        uint8_t * nextstart = (uint8_t *)pVideoData;
        uint8_t * endptr = nextstart + nVideoDataLen;
        int cbRet = 0;
        int nIDR = 0;
        int nNonIDR = 0;
        int isAAC = 0;
        int64_t aacFrameCount = 0;
        if (memcmp(_pAudioFile + strlen(_pAudioFile) - 3, "aac", 3) == 0)
                isAAC = 1;
        while (bAudioOk || bVideoOk) {
                if (bVideoOk && nNow+1 > nNextVideoTime) {
                        
                        uint8_t * start = NULL;
                        uint8_t * end = NULL;
                        uint8_t * sendp = NULL;
                        int eof = 0;
                        int type = -1;
                        do{
                                start = (uint8_t *)ff_avc_find_startcode((const uint8_t *)nextstart, (const uint8_t *)endptr);
                                end = (uint8_t *)ff_avc_find_startcode(start+4, endptr);
                                
                                nextstart = end;
                                if(sendp == NULL)
                                        sendp = start;
                                
                                if(start == end || end > endptr){
                                        eof = 1;
                                        bVideoOk = 0;
                                        break;
                                }
                                
                                if (avArg.nVideoFormat == TK_VIDEO_H264) {
                                        if(start[2] == 0x01){//0x 00 00 01
                                                type = start[3] & 0x1F;
                                        }else{ // 0x 00 00 00 01
                                                type = start[4] & 0x1F;
                                        }
                                        if(type == 1 || type == 5 ){
                                                if (type == 1) {
                                                        nNonIDR++;
                                                } else {
                                                        nIDR++;
                                                }
                                                //printf("send one video(%d) frame packet:%ld", type, end - sendp);
                                                cbRet = callback(opaque, sendp, end - sendp, THIS_IS_VIDEO, nNextVideoTime-nSysTimeBase, type == 5);
                                                if (cbRet != 0) {
                                                        bVideoOk = 0;
                                                }
                                                nNextVideoTime += 40;
                                                break;
                                        }
                                }else{
                                        int dlen = 3;
                                        if(start[2] == 0x01){//0x 00 00 01
                                                type = start[3] & 0x7E;
                                        }else{ // 0x 00 00 00 01
                                                dlen = 4;
                                                type = start[4] & 0x7E;
                                        }
                                        type = (type >> 1);
                                        int hevctype = is_h265_picture(type);
                                        if (hevctype == -1) {
                                                printf("unknown type:%d\n", type);
                                                continue;
                                        }
                                        //printf("%d------------->%d\n",dlen, type);
                                        if(hevctype == HEVC_I || hevctype == HEVC_B ){
                                                if (type == 20) {
                                                        nNonIDR++;
                                                } else {
                                                        nIDR++;
                                                }
                                                //printf("send one video(%d) frame packet:%ld", type, end - sendp);
                                                cbRet = callback(opaque, sendp, end - sendp, THIS_IS_VIDEO, nNextVideoTime-nSysTimeBase, hevctype == HEVC_I);
                                                if (cbRet != 0) {
                                                        bVideoOk = 0;
                                                }
                                                nNextVideoTime += 40;
                                                break;
                                        }
                                }
                        }while(1);
                }
                if (bAudioOk && nNow+1 > nNextAudioTime) {
                        if (isAAC) {
#ifdef TEST_AAC
                                ADTS adts;
                                if(audioOffset+7 <= nAudioDataLen) {
                                        ParseAdtsfixedHeader((unsigned char *)(pAudioData + audioOffset), &adts.fix);
                                        int hlen = adts.fix.protection_absent == 1 ? 7 : 9;
                                        ParseAdtsVariableHeader((unsigned char *)(pAudioData + audioOffset), &adts.var);
                                        if (audioOffset+hlen+adts.var.aac_frame_length <= nAudioDataLen) {
#ifdef TEST_AAC_NO_ADTS
                                                cbRet = callback(opaque, pAudioData + audioOffset + hlen, adts.var.aac_frame_length - hlen,
                                                                 THIS_IS_AUDIO, nNextAudioTime-nSysTimeBase, 0);
#else
                                                cbRet = callback(opaque, pAudioData + audioOffset, adts.var.aac_frame_length,
                                                                 THIS_IS_AUDIO, nNextAudioTime-nSysTimeBase, 0);
#endif
                                                if (cbRet != 0) {
                                                        bAudioOk = 0;
                                                        continue;
                                                }
                                                audioOffset += adts.var.aac_frame_length;
                                                aacFrameCount++;
                                                int nFreq = aacfreq[adts.fix.sampling_frequency_index];
                                                int64_t d = ((1024*1000.0)/nFreq/adts.fix.channel_configuration) * aacFrameCount;
                                                nNextAudioTime = nSysTimeBase + d;
                                        } else {
                                                bAudioOk = 0;
                                        }
                                } else {
                                        bAudioOk = 0;
                                }
#endif
                        } else {
                                if(audioOffset+160 <= nAudioDataLen) {
                                        cbRet = callback(opaque, pAudioData + audioOffset, 160, THIS_IS_AUDIO, nNextAudioTime-nSysTimeBase, 0);
                                        if (cbRet != 0) {
                                                bAudioOk = 0;
                                                continue;
                                        }
                                        audioOffset += 160;
                                        nNextAudioTime += 20;
                                } else {
                                        bAudioOk = 0;
                                }
                        }
                }
                
                int64_t nSleepTime = 0;
                if (nNextAudioTime > nNextVideoTime) {
                        if (nNextVideoTime - nNow >  1)
                                nSleepTime = (nNextVideoTime - nNow - 1) * 1000;
                } else {
                        if (nNextAudioTime - nNow > 1)
                                nSleepTime = (nNextAudioTime - nNow - 1) * 1000;
                }
                if (nSleepTime != 0) {
                        //printf("sleeptime:%lld\n", nSleepTime);
                        usleep(nSleepTime);
                }
                nNow = getCurrentMilliSecond();
        }
        
        if (pAudioData) {
                free(pAudioData);
        }
        if (pVideoData) {
                free(pVideoData);
                printf("IDR:%d nonIDR:%d\n", nIDR, nNonIDR);
        }
        return 0;
}

#ifdef INPUT_FROM_FFMPEG

static int getAacFreqIndex(int _nFreq)
{
        switch(_nFreq){
                case 96000:
                        return 0;
                case 88200:
                        return 1;
                case 64000:
                        return 2;
                case 48000:
                        return 3;
                case 44100:
                        return 4;
                case 32000:
                        return 5;
                case 24000:
                        return 6;
                case 22050:
                        return 7;
                case 16000:
                        return 8;
                case 12000:
                        return 9;
                case 11025:
                        return 10;
                case 8000:
                        return 11;
                case 7350:
                        return 12;
                default:
                        return -1;
        }
}
char gAACBuf[1024];
int start_ffmpeg_test(char * _pUrl, DataCallback callback, void *opaque)
{
        AVFormatContext *pFmtCtx = NULL;
        int ret = avformat_open_input(&pFmtCtx, _pUrl, NULL, NULL);
        if (ret != 0) {
                char msg[128] = {0};
                av_strerror(ret, msg, sizeof(msg)) ;
                return ret;
        }
        
        AVBSFContext *pBsfCtx = NULL;
        if ((ret = avformat_find_stream_info(pFmtCtx, 0)) < 0) {
                printf("Failed to retrieve input stream information");
                goto end;
        }
        
        printf("===========Input Information==========\n");
        av_dump_format(pFmtCtx, 0, _pUrl, 0);
        printf("======================================\n");
        
        int nAudioIndex = 0;
        int nVideoIndex = 0;
        for (size_t i = 0; i < pFmtCtx->nb_streams; ++i) {
                if (pFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                        printf("find audio\n");
                        nAudioIndex = i;
                        continue;
                }
                if (pFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                        printf("find video\n");
                        nVideoIndex = i;
                        continue;
                }
                printf("other type:%d\n", pFmtCtx->streams[i]->codecpar->codec_type);
        }
        
        const AVBitStreamFilter *filter = av_bsf_get_by_name("h264_mp4toannexb");
        if(!filter){
                av_log(NULL,AV_LOG_ERROR,"Unkonw bitstream filter");
                goto end;
        }
        
        ret = av_bsf_alloc(filter, &pBsfCtx);
        if (ret != 0) {
                goto end;
        }
        avcodec_parameters_copy(pBsfCtx->par_in, pFmtCtx->streams[nVideoIndex]->codecpar);
        av_bsf_init(pBsfCtx);
        
        AVPacket pkt;
        av_init_packet(&pkt);
        while((ret = av_read_frame(pFmtCtx, &pkt)) == 0){
                if(nVideoIndex == pkt.stream_index) {
                        ret = av_bsf_send_packet(pBsfCtx, &pkt);
                        if(ret < 0) {
                                fprintf(stderr, "av_bsf_send_packet fail: %d\n", ret);
                                goto end;
                        }
                        ret = av_bsf_receive_packet(pBsfCtx, &pkt);
                        if (AVERROR(EAGAIN) == ret){
                                av_packet_unref(&pkt);
                                continue;
                        }
                        if(ret < 0){
                                fprintf(stderr, "av_bsf_receive_packet: %d\n", ret);
                                goto end;
                        }
                        ret = callback(opaque, pkt.data, pkt.size, THIS_IS_VIDEO, pkt.pts, pkt.flags == 1);
                } else {
                        //ffmpeg 没有带adts头
                        ADTSFixheader fixHeader;
                        ADTSVariableHeader varHeader;
                        InitAdtsFixedHeader(&fixHeader);
                        InitAdtsVariableHeader(&varHeader, pkt.size);
                        fixHeader.channel_configuration = pFmtCtx->streams[pkt.stream_index]->codecpar->channels;
                        int nFreqIdx = getAacFreqIndex(pFmtCtx->streams[pkt.stream_index]->codecpar->sample_rate);
                        fixHeader.sampling_frequency_index = nFreqIdx;
                        
                        ConvertAdtsHeader2Char(&fixHeader, &varHeader, (unsigned char *)gAACBuf);
                        int nHeaderLen = varHeader.aac_frame_length - pkt.size;
                        memcpy(gAACBuf + nHeaderLen, pkt.data, pkt.size);
                        pkt.size = varHeader.aac_frame_length;
                        
                        ret = callback(opaque, gAACBuf, varHeader.aac_frame_length, THIS_IS_AUDIO, pkt.pts, 0);
                }
                
                av_packet_unref(&pkt);
        }
        
end:
        if (pBsfCtx)
                av_bsf_free(&pBsfCtx);
        if (pFmtCtx)
                avformat_close_input(&pFmtCtx);
        /* close output */
        if (ret < 0 && ret != AVERROR_EOF) {
                printf("Error occurred.\n");
                return -1;
        }
        
        return 0;
}
#endif

TsMuxerContext *pTs;
static int dataCallback(void *opaque, void *pData, int nDataLen, int nFlag, int64_t timestamp, int nIsKeyFrame)
{
        int ret = 0;
        if (nFlag == THIS_IS_AUDIO){
                printf("audio pts:%lld len:%d\n", timestamp, nDataLen);
                MuxerAudio(pTs, pData, nDataLen, timestamp);
        } else {
                printf("video pts:%lld len:%d\n", timestamp, nDataLen);
                MuxerVideo(pTs, pData, nDataLen, timestamp);
        }
        return ret;
}

void signalHandler(int s){
        DestroyTsMuxerContext(pTs);
        exit(0);
}

FILE *pTsFile;
int writeTs(void *pOpaque, void* pTsData, int nTsDataLen)
{
        if(pTsFile == NULL) {
                pTsFile = fopen("my.ts", "w");
        }
        if(pTsFile == NULL) {
                return -1;
        }
        return fwrite(pTsData, 1, nTsDataLen, pTsFile);
}


int main(int argc, char* argv[])
{
#ifdef INPUT_FROM_FFMPEG
  #if LIBAVFORMAT_VERSION_MAJOR < 58
        //int nFfmpegVersion = avcodec_version();
        av_register_all();
  #endif
  #ifdef INPUT_FROM_FFMPEG
        avformat_network_init();
  #endif
#endif
        signal(SIGINT, signalHandler);
        
#ifdef TEST_AAC
        avArg.nAudioFormat = TK_AUDIO_AAC;
        avArg.nAudioChannels = 1;
        avArg.nAudioSampleRate = 16000;
#else
        avArg.nAudioFormat = TK_AUDIO_PCMU;
        avArg.nAudioChannels = 1;
        avArg.nAudioSampleRate = 8000;
#endif
        avArg.output = writeTs;
        avArg.pOpaque = pTs;
        avArg.nVideoFormat = TK_VIDEO_H264;
        
        pTs = NewTsMuxerContext(&avArg);
#ifdef __APPLE__
        char * pVFile = "/Users/liuye/Documents/material/h265_aac_1_16000_h264.h264";
  #ifdef TEST_AAC
        char * pAFile = "/Users/liuye/Documents/material/h265_aac_1_16000_a.aac";
  #else
        char * pAFile = "/Users/liuye/Documents/material/h265_aac_1_16000_pcmu_8000.mulaw";
  #endif
        if (avArg.nVideoFormat == TK_VIDEO_H265) {
                pVFile = "/Users/liuye/Documents/material/h265_aac_1_16000_v.h265";
        }
#else
        
        char * pVFile = "/liuye/Documents/material/h265_aac_1_16000_h264.h264";
  #ifdef TEST_AAC
        char * pAFile = "/liuye/Documents/material/h265_aac_1_16000_a.aac";
  #else
        char * pAFile = "/liuye/Documents/material/h265_aac_1_16000_pcmu_8000.mulaw";
  #endif
        if (avArg.nVideoFormat == TK_VIDEO_H265) {
                pVFile = "/liuye/Documents/material/h265_aac_1_16000_v.h265";
        }
#endif
        
#ifdef INPUT_FROM_FFMPEG
        printf("rtmp://localhost:1935/live/movie\n");
        start_ffmpeg_test("rtmp://localhost:1935/live/movie", dataCallback, NULL);
        //start_ffmpeg_test("rtmp://live.hkstv.hk.lxdns.com/live/hks", dataCallback, NULL);
#else
        printf("audio:%s\n", pAFile);
        printf("video:%s\n", pVFile);
        start_file_test(pAFile, pVFile, dataCallback, NULL);
#endif

        DestroyTsMuxerContext(pTs);
        return 0;
}

