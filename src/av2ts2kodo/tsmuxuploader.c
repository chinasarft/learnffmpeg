#include "tsmuxuploader.h"
#include "base.h"
#include <unistd.h>
#include "adts.h"
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#else
#include <sys/sysinfo.h>
#endif
#include "servertime.h"

#ifdef USE_OWN_TSMUX
#include "tsmux.h"
#else
#include <libavformat/avformat.h>
#endif

#define FF_OUT_LEN 4096
#define QUEUE_INIT_LEN 150

#define TK_STREAM_TYPE_AUDIO 1
#define TK_STREAM_TYPE_VIDEO 2

typedef struct _FFTsMuxContext{
        AsyncInterface asyncWait;
        TsUploader *pTsUploader_;
#ifdef USE_OWN_TSMUX
        TsMuxerContext *pFmtCtx_;
#else
        AVFormatContext *pFmtCtx_;
#endif
        int nOutVideoindex_;
        int nOutAudioindex_;
        int64_t nPrevAudioTimestamp;
        int64_t nPrevVideoTimestamp;
        TsMuxUploader * pTsMuxUploader;
}FFTsMuxContext;

typedef struct _Token {
        int nQuit;
        char * pPrevToken_;
        int nPrevTokenLen_;
        char * pToken_;
        int nTokenLen_;
        pthread_mutex_t tokenMutex_;
}Token;

typedef struct _FFTsMuxUploader{
        TsMuxUploader tsMuxUploader_;
        pthread_mutex_t muxUploaderMutex_;
        unsigned char *pAACBuf;
        int nAACBufLen;
        FFTsMuxContext *pTsMuxCtx;
        
        int64_t nLastVideoTimestamp;
        int64_t nLastUploadVideoTimestamp; //initial to -1
        int nKeyFrameCount;
        int nFrameCount;
        AvArg avArg;
        UploadState ffMuxSatte;
        
        int nUploadBufferSize;
        int nNewSegmentInterval;
        
        char deviceId_[65];
        Token token_;
        UploadArg uploadArg;
}FFTsMuxUploader;

static int aAacfreqs[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050 ,16000 ,12000, 11025, 8000, 7350};

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

static void pushRecycle(FFTsMuxUploader *_pFFTsMuxUploader)
{
        if (_pFFTsMuxUploader) {
                
                pthread_mutex_lock(&_pFFTsMuxUploader->muxUploaderMutex_);
                if (_pFFTsMuxUploader->pTsMuxCtx) {
#ifndef USE_OWN_TSMUX
                        av_write_trailer(_pFFTsMuxUploader->pTsMuxCtx->pFmtCtx_);
#endif
                        logerror("push to mgr:%p", _pFFTsMuxUploader->pTsMuxCtx);
                        PushFunction(_pFFTsMuxUploader->pTsMuxCtx);
                        _pFFTsMuxUploader->pTsMuxCtx = NULL;
                }
                
                pthread_mutex_unlock(&_pFFTsMuxUploader->muxUploaderMutex_);
        }
        return;
}

static int writeTsPacketToMem(void *opaque, uint8_t *buf, int buf_size)
{
        FFTsMuxContext *pTsMuxCtx = (FFTsMuxContext *)opaque;
        
        int ret = pTsMuxCtx->pTsUploader_->Push(pTsMuxCtx->pTsUploader_, (char *)buf, buf_size);
        if (ret < 0){
                if (ret == TK_Q_OVERWRIT) {
                        logdebug("write ts to queue overwrite:%d", ret);
                } else {
                        logdebug("write ts to queue fail:%d", ret);
                }
                return ret;
        } else {
                logtrace("write_packet: should write:len:%d  actual:%d\n", buf_size, ret);
        }
        return ret;
}

static int push(FFTsMuxUploader *pFFTsMuxUploader, char * _pData, int _nDataLen, int64_t _nTimestamp, int _nFlag){
#ifndef USE_OWN_TSMUX
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = (uint8_t *)_pData;
        pkt.size = _nDataLen;
#endif
        
        //logtrace("push thread id:%d\n", (int)pthread_self());
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
        
        FFTsMuxContext *pTsMuxCtx = NULL;
        int count = 0;

        count = 1;
        pTsMuxCtx = pFFTsMuxUploader->pTsMuxCtx;
        while(pTsMuxCtx == NULL && count) {
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                usleep(3*1000);
                pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
                pTsMuxCtx = pFFTsMuxUploader->pTsMuxCtx;
                count--;
        }
        if (pTsMuxCtx == NULL) {
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                logwarn("upload context is NULL");
                return 0;
        }
        if (pTsMuxCtx->pTsUploader_->GetUploaderState(pTsMuxCtx->pTsUploader_) == TK_UPLOAD_FAIL) {
                if (pFFTsMuxUploader->ffMuxSatte != TK_UPLOAD_FAIL) {
                        logdebug("upload fail. drop the data");
                }
                pFFTsMuxUploader->ffMuxSatte = TK_UPLOAD_FAIL;
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                return 0;
        }

        int ret = 0;
        int isAdtsAdded = 0;
        
        if (_nFlag == TK_STREAM_TYPE_AUDIO){
                //fprintf(stderr, "audio frame: len:%d pts:%lld\n", _nDataLen, _nTimestamp);
                if (pTsMuxCtx->nPrevAudioTimestamp != 0 && _nTimestamp - pTsMuxCtx->nPrevAudioTimestamp <= 0) {
                        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                        logwarn("audio pts not monotonically: prev:%lld now:%lld", pTsMuxCtx->nPrevAudioTimestamp, _nTimestamp);
                        return 0;
                }
#ifndef USE_OWN_TSMUX
                pkt.pts = _nTimestamp * 90;
                pkt.stream_index = pTsMuxCtx->nOutAudioindex_;
                pkt.dts = pkt.pts;
                pTsMuxCtx->nPrevAudioTimestamp = _nTimestamp;
#endif
                
                unsigned char * pAData = (unsigned char * )_pData;
                if (pFFTsMuxUploader->avArg.nAudioFormat ==  TK_AUDIO_AAC && (pAData[0] != 0xff || (pAData[1] & 0xf0) != 0xf0)) {
                        ADTSFixheader fixHeader;
                        ADTSVariableHeader varHeader;
                        InitAdtsFixedHeader(&fixHeader);
                        InitAdtsVariableHeader(&varHeader, _nDataLen);
                        fixHeader.channel_configuration = pFFTsMuxUploader->avArg.nChannels;
                        int nFreqIdx = getAacFreqIndex(pFFTsMuxUploader->avArg.nSamplerate);
                        fixHeader.sampling_frequency_index = nFreqIdx;
                        if (pFFTsMuxUploader->pAACBuf == NULL || pFFTsMuxUploader->nAACBufLen < varHeader.aac_frame_length) {
                                if (pFFTsMuxUploader->pAACBuf) {
                                        free(pFFTsMuxUploader->pAACBuf);
                                        pFFTsMuxUploader->pAACBuf = NULL;
                                }
                                pFFTsMuxUploader->pAACBuf = (unsigned char *)malloc(varHeader.aac_frame_length);
                                pFFTsMuxUploader->nAACBufLen = (int)varHeader.aac_frame_length;
                        }
                        if(pFFTsMuxUploader->pAACBuf == NULL || pFFTsMuxUploader->avArg.nChannels < 1 || pFFTsMuxUploader->avArg.nChannels > 2
                           || nFreqIdx < 0) {
                                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                                if (pFFTsMuxUploader->pAACBuf == NULL) {
                                        logwarn("malloc %d size memory fail", varHeader.aac_frame_length);
                                        return TK_NO_MEMORY;
                                } else {
                                        logwarn("wrong audio arg:channel:%d sameplerate%d", pFFTsMuxUploader->avArg.nChannels,
                                                pFFTsMuxUploader->avArg.nSamplerate);
                                        return TK_ARG_ERROR;
                                }
                        }
                        ConvertAdtsHeader2Char(&fixHeader, &varHeader, pFFTsMuxUploader->pAACBuf);
                        int nHeaderLen = varHeader.aac_frame_length - _nDataLen;
                        memcpy(pFFTsMuxUploader->pAACBuf + nHeaderLen, _pData, _nDataLen);
                        isAdtsAdded = 1;
#ifdef USE_OWN_TSMUX
                        MuxerAudio(pTsMuxCtx->pFmtCtx_, (uint8_t *)pFFTsMuxUploader->pAACBuf, varHeader.aac_frame_length, _nTimestamp);
#else
                        pkt.data = (uint8_t *)pFFTsMuxUploader->pAACBuf;
                        pkt.size = varHeader.aac_frame_length;
#endif
                } 
#ifdef USE_OWN_TSMUX
                else {
                        ret = MuxerAudio(pTsMuxCtx->pFmtCtx_, (uint8_t*)_pData, _nDataLen, _nTimestamp);
                }
#endif
        }else{
                //fprintf(stderr, "video frame: len:%d pts:%lld\n", _nDataLen, _nTimestamp);
                if (pTsMuxCtx->nPrevVideoTimestamp != 0 && _nTimestamp - pTsMuxCtx->nPrevVideoTimestamp <= 0) {
                        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                        logwarn("video pts not monotonically: prev:%lld now:%lld", pTsMuxCtx->nPrevVideoTimestamp, _nTimestamp);
                        return 0;
                }
#ifdef USE_OWN_TSMUX
                ret = MuxerVideo(pTsMuxCtx->pFmtCtx_, (uint8_t*)_pData, _nDataLen, _nTimestamp);
#else
                pkt.pts = _nTimestamp * 90;
                pkt.stream_index = pTsMuxCtx->nOutVideoindex_;
                pkt.dts = pkt.pts;
                pTsMuxCtx->nPrevVideoTimestamp = _nTimestamp;
#endif
        }
        

#ifndef USE_OWN_TSMUX
        ret = av_interleaved_write_frame(pTsMuxCtx->pFmtCtx_, &pkt);
#endif
        if (ret == 0) {
                pTsMuxCtx->pTsUploader_->RecordTimestamp(pTsMuxCtx->pTsUploader_, _nTimestamp);
        } else {
                if (pFFTsMuxUploader->ffMuxSatte != TK_UPLOAD_FAIL)
                        logerror("Error muxing packet:%d", ret);
                pFFTsMuxUploader->ffMuxSatte = TK_UPLOAD_FAIL;
        }

        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
#ifndef USE_OWN_TSMUX
        if (isAdtsAdded) {
                free(pkt.data);
        }
#endif
        return ret;
}

static int PushVideo(TsMuxUploader *_pTsMuxUploader, char * _pData, int _nDataLen, int64_t _nTimestamp, int nIsKeyFrame, int _nIsSegStart)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;

        int ret = 0;
        if (pFFTsMuxUploader->nKeyFrameCount == 0 && !nIsKeyFrame) {
                logwarn("first video frame not IDR. drop this frame\n");
                return 0;
        }
        if (pFFTsMuxUploader->nLastUploadVideoTimestamp == -1) {
                pFFTsMuxUploader->nLastUploadVideoTimestamp = _nTimestamp;
        }
        // if start new uploader, start from keyframe
        if (nIsKeyFrame) {
                if( (_nTimestamp - pFFTsMuxUploader->nLastUploadVideoTimestamp) > 4980
                   //at least 2 keyframe and aoubt last 5 second
                   || (_nIsSegStart && pFFTsMuxUploader->nFrameCount != 0)// new segment is specified
                   ||  pFFTsMuxUploader->ffMuxSatte == TK_UPLOAD_FAIL){   // upload fail
                        printf("next ts:%d %lld\n", pFFTsMuxUploader->nKeyFrameCount, _nTimestamp - pFFTsMuxUploader->nLastUploadVideoTimestamp);
                        pFFTsMuxUploader->nKeyFrameCount = 0;
                        pFFTsMuxUploader->nFrameCount = 0;
                        pFFTsMuxUploader->nLastUploadVideoTimestamp = _nTimestamp;
                        pFFTsMuxUploader->ffMuxSatte = TK_UPLOAD_INIT;
                        pushRecycle(pFFTsMuxUploader);
                        if (_nIsSegStart) {
                                pFFTsMuxUploader->uploadArg.nSegmentId_ = GetCurrentNanosecond();
                        }
                        ret = TsMuxUploaderStart(_pTsMuxUploader);
                        if (ret != 0) {
                                return ret;
                        }
                }
                pFFTsMuxUploader->nKeyFrameCount++;
        }

        pFFTsMuxUploader->nLastVideoTimestamp = _nTimestamp;
        
        ret = push(pFFTsMuxUploader, _pData, _nDataLen, _nTimestamp, TK_STREAM_TYPE_VIDEO);
        if (ret == 0){
                pFFTsMuxUploader->nFrameCount++;
        }
        return ret;
}

static int PushAudio(TsMuxUploader *_pTsMuxUploader, char * _pData, int _nDataLen, int64_t _nTimestamp)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        int ret = push(pFFTsMuxUploader, _pData, _nDataLen, _nTimestamp, TK_STREAM_TYPE_AUDIO);
        if (ret == 0){
                pFFTsMuxUploader->nFrameCount++;
        }
        return ret;
}

static int waitToCompleUploadAndDestroyTsMuxContext(void *_pOpaque)
{
        FFTsMuxContext *pTsMuxCtx = (FFTsMuxContext*)_pOpaque;
        
        if (pTsMuxCtx) {
#ifndef USE_OWN_TSMUX
                if (pTsMuxCtx->pFmtCtx_) {
                        if (pTsMuxCtx->pFmtCtx_->pb) {
                                avio_flush(pTsMuxCtx->pFmtCtx_->pb);
                        }
                }
#endif
                pTsMuxCtx->pTsUploader_->UploadStop(pTsMuxCtx->pTsUploader_);

                UploaderStatInfo statInfo = {0};
                pTsMuxCtx->pTsUploader_->GetStatInfo(pTsMuxCtx->pTsUploader_, &statInfo);
                logdebug("uploader push:%d pop:%d remainItemCount:%d dropped:%d", statInfo.nPushDataBytes_,
                         statInfo.nPopDataBytes_, statInfo.nLen_, statInfo.nDropped);
                DestroyUploader(&pTsMuxCtx->pTsUploader_);
#ifdef USE_OWN_TSMUX
                DestroyTsMuxerContext(pTsMuxCtx->pFmtCtx_);
#else
                if (pTsMuxCtx->pFmtCtx_->pb && pTsMuxCtx->pFmtCtx_->pb->buffer)  {
                        av_free(pTsMuxCtx->pFmtCtx_->pb->buffer);
                }
                if (!(pTsMuxCtx->pFmtCtx_->oformat->flags & AVFMT_NOFILE))
                        avio_close(pTsMuxCtx->pFmtCtx_->pb);
                if (pTsMuxCtx->pFmtCtx_->pb) {
                        avio_context_free(&pTsMuxCtx->pFmtCtx_->pb);
                }
                avformat_free_context(pTsMuxCtx->pFmtCtx_);
#endif
                FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)(pTsMuxCtx->pTsMuxUploader);
                if (pFFTsMuxUploader) {
                        if (pFFTsMuxUploader->pAACBuf) {
                                free(pFFTsMuxUploader->pAACBuf);
                        }
                        if (pFFTsMuxUploader->token_.pToken_) {
                                free(pFFTsMuxUploader->token_.pToken_);
                                pFFTsMuxUploader->token_.pToken_ = NULL;
                        }
                        if (pFFTsMuxUploader->token_.pPrevToken_) {
                                free(pFFTsMuxUploader->token_.pPrevToken_);
                        }
                        free(pFFTsMuxUploader);
                }
                free(pTsMuxCtx);
        }
        
        return 0;
}

#define getFFmpegErrorMsg(errcode) char msg[128];\
                av_strerror(errcode, msg, sizeof(msg))

static void inline setQBufferSize(FFTsMuxUploader *pFFTsMuxUploader, char *desc, int s)
{
        pFFTsMuxUploader->nUploadBufferSize = s;
        loginfo("desc:(%s) buffer Q size is:%d", desc, pFFTsMuxUploader->nUploadBufferSize);
        return;
}

static int getBufferSize(FFTsMuxUploader *pFFTsMuxUploader) {
        if (pFFTsMuxUploader->nUploadBufferSize != 0) {
                return pFFTsMuxUploader->nUploadBufferSize;
        }
        int nSize = 256*1024;
        int64_t nTotalMemSize = 0;
        int nRet = 0;
#ifdef __APPLE__
        int mib[2];
        size_t length;
        mib[0] = CTL_HW;
        mib[1] = HW_MEMSIZE;
        length = sizeof(int64_t);
        nRet = sysctl(mib, 2, &nTotalMemSize, &length, NULL, 0);
#else
        struct sysinfo info = {0};
        nRet = sysinfo(&info);
        nTotalMemSize = info.totalram;
#endif
        if (nRet != 0) {
                setQBufferSize(pFFTsMuxUploader, "default", nSize);
                return pFFTsMuxUploader->nUploadBufferSize;
        }
        loginfo("toto memory size:%lld\n", nTotalMemSize);
        
        int64_t M = 1024 * 1024;
        if (nTotalMemSize <= 32 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 32M", nSize);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else if (nTotalMemSize <= 64 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 64M", nSize * 2);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else if (nTotalMemSize <= 128 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 128M", nSize * 3);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else if (nTotalMemSize <= 256 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 256M", nSize * 4);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else if (nTotalMemSize <= 512 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 512M", nSize * 6);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else if (nTotalMemSize <= 1024 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 1G", nSize * 8);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else if (nTotalMemSize <= 2 * 1024 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 2G", nSize * 10);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else if (nTotalMemSize <= 4 * 1024 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 4G", nSize * 12);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else {
                setQBufferSize(pFFTsMuxUploader, "gt 4G", nSize * 16);
                return pFFTsMuxUploader->nUploadBufferSize;
        }
}

static int newTsMuxContext(FFTsMuxContext ** _pTsMuxCtx, AvArg *_pAvArg, UploadArg *_pUploadArg, int nQBufSize)
#ifdef USE_OWN_TSMUX
{
        FFTsMuxContext * pTsMuxCtx = (FFTsMuxContext *)malloc(sizeof(FFTsMuxContext));
        if (pTsMuxCtx == NULL) {
                return TK_NO_MEMORY;
        }
        memset(pTsMuxCtx, 0, sizeof(FFTsMuxContext));

        int ret = NewUploader(&pTsMuxCtx->pTsUploader_, _pUploadArg, TSQ_FIX_LENGTH, 188, nQBufSize / 188);
        if (ret != 0) {
                free(pTsMuxCtx);
                return ret;
        }

        TsMuxerArg avArg;
        avArg.nAudioFormat = _pAvArg->nAudioFormat;
        avArg.nAudioChannels = _pAvArg->nChannels;
        avArg.nAudioSampleRate = _pAvArg->nSamplerate;

        avArg.output = writeTsPacketToMem;
        avArg.nVideoFormat = _pAvArg->nVideoFormat;
        avArg.pOpaque = pTsMuxCtx;

        ret = NewTsMuxerContext(&avArg, &pTsMuxCtx->pFmtCtx_);
        if (ret != 0) {
                DestroyUploader(&pTsMuxCtx->pTsUploader_);
                free(pTsMuxCtx);
                return ret;
        }


        pTsMuxCtx->asyncWait.function = waitToCompleUploadAndDestroyTsMuxContext;
        * _pTsMuxCtx = pTsMuxCtx;
        return 0;
}
#else
{
        FFTsMuxContext * pTsMuxCtx = (FFTsMuxContext *)malloc(sizeof(FFTsMuxContext));
        if (pTsMuxCtx == NULL) {
                return TK_NO_MEMORY;
        }
        memset(pTsMuxCtx, 0, sizeof(FFTsMuxContext));
        
        int nBufsize = getBufferSize();
        int ret = NewUploader(&pTsMuxCtx->pTsUploader_, _pUploadArg, TSQ_FIX_LENGTH, FF_OUT_LEN, nBufsize / FF_OUT_LEN);
        if (ret != 0) {
                free(pTsMuxCtx);
                return ret;
        }
        
        uint8_t *pOutBuffer = NULL;
        //Output
        ret = avformat_alloc_output_context2(&pTsMuxCtx->pFmtCtx_, NULL, "mpegts", NULL);
        if (ret < 0) {
                getFFmpegErrorMsg(ret);
                logerror("Could not create output context:%d(%s)", ret, msg);
                ret = TK_NO_MEMORY;
                goto end;
        }
        AVOutputFormat *pOutFmt = pTsMuxCtx->pFmtCtx_->oformat;
        pOutBuffer = (unsigned char*)av_malloc(4096);
        AVIOContext *avio_out = avio_alloc_context(pOutBuffer, 4096, 1, pTsMuxCtx, NULL, writeTsPacketToMem, NULL);
        pTsMuxCtx->pFmtCtx_->pb = avio_out;
        pTsMuxCtx->pFmtCtx_->flags = AVFMT_FLAG_CUSTOM_IO;
        pOutFmt->flags |= AVFMT_NOFILE;
        pOutFmt->flags |= AVFMT_NODIMENSIONS;
        //ofmt->video_codec //是否指定为ifmt_ctx_v的视频的codec_type.同理音频也一样
        //测试下来即使video_codec和ifmt_ctx_v的视频的codec_type不一样也是没有问题的
        
        //add video
        AVStream *pOutStream = avformat_new_stream(pTsMuxCtx->pFmtCtx_, NULL);
        if (!pOutStream) {
                getFFmpegErrorMsg(ret);
                logerror("Failed allocating output stream:%d(%s)", ret, msg);
                ret = TK_NO_MEMORY;
                goto end;
        }
        pOutStream->time_base.num = 1;
        pOutStream->time_base.den = 90000;
        pTsMuxCtx->nOutVideoindex_ = pOutStream->index;
        pOutStream->codecpar->codec_tag = 0;
        pOutStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        if (_pAvArg->nVideoFormat == TK_VIDEO_H264)
                pOutStream->codecpar->codec_id = AV_CODEC_ID_H264;
        else
                pOutStream->codecpar->codec_id = AV_CODEC_ID_H265;
        //end add video
        
        //add audio
        pOutStream = avformat_new_stream(pTsMuxCtx->pFmtCtx_, NULL);
        if (!pOutStream) {
                getFFmpegErrorMsg(ret);
                logerror("Failed allocating output stream:%d(%s)", ret, msg);
                ret = TK_NO_MEMORY;
                goto end;
        }
        pOutStream->time_base.num = 1;
        pOutStream->time_base.den = 90000;
        pTsMuxCtx->nOutAudioindex_ = pOutStream->index;
        pOutStream->codecpar->codec_tag = 0;
        pOutStream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        switch(_pAvArg->nAudioFormat){
                case TK_AUDIO_PCMU:
                        pOutStream->codecpar->codec_id = AV_CODEC_ID_PCM_MULAW;
                        break;
                case TK_AUDIO_PCMA:
                        pOutStream->codecpar->codec_id = AV_CODEC_ID_PCM_ALAW;
                        break;
                case TK_AUDIO_AAC:
                        pOutStream->codecpar->codec_id = AV_CODEC_ID_AAC;
                        break;
        }
        pOutStream->codecpar->sample_rate = _pAvArg->nSamplerate;
        pOutStream->codecpar->channels = _pAvArg->nChannels;
        pOutStream->codecpar->channel_layout = av_get_default_channel_layout(pOutStream->codecpar->channels);
        //end add audio
        
        //printf("==========Output Information==========\n");
        //av_dump_format(pTsMuxCtx->pFmtCtx_, 0, "xx.ts", 1);
        //printf("======================================\n");

        //Open output file
        if (!(pOutFmt->flags & AVFMT_NOFILE)) {
                if ((ret = avio_open(&pTsMuxCtx->pFmtCtx_->pb, "xx.ts", AVIO_FLAG_WRITE)) < 0) {
                        getFFmpegErrorMsg(ret);
                        logerror("Could not open output:%d(%s)", ret, msg);
                        ret = TK_OPEN_TS_ERR;
                        goto end;
                }
        }
        //Write file header
        int erno = 0;
        if ((erno = avformat_write_header(pTsMuxCtx->pFmtCtx_, NULL)) < 0) {
                getFFmpegErrorMsg(erno);
                logerror("fail to write ts header:%d(%s)", erno, msg);
                ret = TK_WRITE_TS_ERR;
                goto end;
        }
        
        pTsMuxCtx->asyncWait.function = waitToCompleUploadAndDestroyTsMuxContext;
        *_pTsMuxCtx = pTsMuxCtx;
        return 0;
end:
        if (pOutBuffer) {
                av_free(pOutBuffer);
        }
        if (pTsMuxCtx->pFmtCtx_->pb)
                avio_context_free(&pTsMuxCtx->pFmtCtx_->pb);
        if (pTsMuxCtx->pFmtCtx_) {
                if (pTsMuxCtx->pFmtCtx_ && !(pOutFmt->flags & AVFMT_NOFILE))
                        avio_close(pTsMuxCtx->pFmtCtx_->pb);
                avformat_free_context(pTsMuxCtx->pFmtCtx_);
        }
        if (pTsMuxCtx->pTsUploader_)
                DestroyUploader(&pTsMuxCtx->pTsUploader_);
        
        return ret;
}
#endif

static int setToken(TsMuxUploader* _PTsMuxUploader, char *_pToken, int _nTokenLen)
{
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_PTsMuxUploader;
        if (pFFTsMuxUploader->token_.pToken_ == NULL) {
                pFFTsMuxUploader->token_.pToken_ = malloc(_nTokenLen + 1);
                if (pFFTsMuxUploader->token_.pToken_  == NULL) {
                        return TK_NO_MEMORY;
                }
        }else {
                if (pFFTsMuxUploader->token_.pPrevToken_ != NULL) {
                        free(pFFTsMuxUploader->token_.pPrevToken_);
                }
                pFFTsMuxUploader->token_.pPrevToken_ = pFFTsMuxUploader->token_.pToken_;
                pFFTsMuxUploader->token_.nPrevTokenLen_ = pFFTsMuxUploader->token_.nTokenLen_;
                
                pFFTsMuxUploader->token_.pToken_ = malloc(_nTokenLen + 1);
                if (pFFTsMuxUploader->token_.pToken_  == NULL) {
                        return TK_NO_MEMORY;
                }
        }
        memcpy(pFFTsMuxUploader->token_.pToken_, _pToken, _nTokenLen);
        pFFTsMuxUploader->token_.nTokenLen_ = _nTokenLen;
        pFFTsMuxUploader->token_.pToken_[_nTokenLen] = 0;
        
        pFFTsMuxUploader->uploadArg.pToken_ = _pToken;
        return 0;
}

static void upadateUploadArg(void *_pOpaque, void* pArg, int64_t nNow)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)_pOpaque;
        UploadArg *_pUploadArg = (UploadArg *)pArg;
        if (pFFTsMuxUploader->uploadArg.nSegmentId_ == 0) {
                pFFTsMuxUploader->uploadArg.nLastUploadTsTime_ = _pUploadArg->nLastUploadTsTime_;
                pFFTsMuxUploader->uploadArg.nSegmentId_ = _pUploadArg->nSegmentId_;
                return;
        }
        int64_t nDiff = pFFTsMuxUploader->nNewSegmentInterval * 1000000000LL;
        if (nNow - pFFTsMuxUploader->uploadArg.nLastUploadTsTime_ >= nDiff) {
                pFFTsMuxUploader->uploadArg.nSegmentId_ = nNow;
                _pUploadArg->nSegmentId_ = nNow;
        }
        pFFTsMuxUploader->uploadArg.nLastUploadTsTime_ = _pUploadArg->nLastUploadTsTime_;
        return;
}

static void setUploaderBufferSize(TsMuxUploader* _pTsMuxUploader, int nBufferSize)
{
        if (nBufferSize < 256) {
                logwarn("setUploaderBufferSize is to small:%d. ge 256 required", nBufferSize);
                return;
        }
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)_pTsMuxUploader;
        pFFTsMuxUploader->nUploadBufferSize = nBufferSize * 1024;
}

static void setNewSegmentInterval(TsMuxUploader* _pTsMuxUploader, int nInterval)
{
        if (nInterval < 15) {
                logwarn("setNewSegmentInterval is to small:%d. ge 15 required", nInterval);
                return;
        }
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)_pTsMuxUploader;
        pFFTsMuxUploader->nNewSegmentInterval = nInterval;
}

int NewTsMuxUploader(TsMuxUploader **_pTsMuxUploader, AvArg *_pAvArg, char *_pDeviceId, int _nDeviceIdLen,
                     char *_pToken, int _nTokenLen)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)malloc(sizeof(FFTsMuxUploader));
        if (pFFTsMuxUploader == NULL) {
                return TK_NO_MEMORY;
        }
        memset(pFFTsMuxUploader, 0, sizeof(FFTsMuxUploader));
        
        int ret = 0;
        ret = setToken((TsMuxUploader *)pFFTsMuxUploader, _pToken, _nTokenLen);
        if (ret != 0) {
                return ret;
        }

        if (_nDeviceIdLen >= sizeof(pFFTsMuxUploader->deviceId_)) {
                free(pFFTsMuxUploader);
                logerror("device max support lenght is 64");
                return TK_ARG_TOO_LONG;
        }
        memcpy(pFFTsMuxUploader->deviceId_, _pDeviceId, _nDeviceIdLen);
        pFFTsMuxUploader->uploadArg.pDeviceId_ = pFFTsMuxUploader->deviceId_;
        
        pFFTsMuxUploader->uploadArg.pUploadArgKeeper_ = pFFTsMuxUploader;
        pFFTsMuxUploader->uploadArg.UploadArgUpadate = upadateUploadArg;
        
        pFFTsMuxUploader->nNewSegmentInterval = 30;
        
        pFFTsMuxUploader->nLastUploadVideoTimestamp = -1;
        
        ret = pthread_mutex_init(&pFFTsMuxUploader->muxUploaderMutex_, NULL);
        if (ret != 0){
                free(pFFTsMuxUploader);
                return TK_MUTEX_ERROR;
        }
        
        pFFTsMuxUploader->tsMuxUploader_.SetToken = setToken;
        pFFTsMuxUploader->tsMuxUploader_.PushAudio = PushAudio;
        pFFTsMuxUploader->tsMuxUploader_.PushVideo = PushVideo;
        pFFTsMuxUploader->tsMuxUploader_.SetUploaderBufferSize = setUploaderBufferSize;
        pFFTsMuxUploader->tsMuxUploader_.SetNewSegmentInterval = setNewSegmentInterval;
        
        pFFTsMuxUploader->avArg = *_pAvArg;
        
        *_pTsMuxUploader = (TsMuxUploader *)pFFTsMuxUploader;
        
        return 0;
}

int TsMuxUploaderStart(TsMuxUploader *_pTsMuxUploader)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        
        assert(pFFTsMuxUploader->pTsMuxCtx == NULL);
        
        int nBufsize = getBufferSize(pFFTsMuxUploader);
        int ret = newTsMuxContext(&pFFTsMuxUploader->pTsMuxCtx, &pFFTsMuxUploader->avArg, &pFFTsMuxUploader->uploadArg, nBufsize);
        if (ret != 0) {
                free(pFFTsMuxUploader);
                return ret;
        }

        pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->UploadStart(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_);
        return 0;
}

void DestroyTsMuxUploader(TsMuxUploader **_pTsMuxUploader)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)(*_pTsMuxUploader);
        
        if (pFFTsMuxUploader->pTsMuxCtx) {
                pFFTsMuxUploader->pTsMuxCtx->pTsMuxUploader = (TsMuxUploader*)pFFTsMuxUploader;
        }
        pushRecycle(pFFTsMuxUploader);
        *_pTsMuxUploader = NULL;
        return;
}
