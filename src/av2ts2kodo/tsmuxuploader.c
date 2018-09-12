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
}FFTsMuxContext;

typedef struct _FFTsMuxUploader{
        TsMuxUploader tsMuxUploader_;
        pthread_mutex_t muxUploaderMutex_;
        char *pToken_;
        unsigned char *pAACBuf;
        int nAACBufLen;
        char ak_[64];
        char sk_[64];
        char bucketName_[256];
        int deleteAfterDays_;
        char callback_[512];
        FFTsMuxContext *pTsMuxCtx;
        
        int64_t nLastVideoTimestamp;
        int64_t nLastUploadVideoTimestamp; //initial to -1
        int nKeyFrameCount;
        int nFrameCount;
        int nSegmentId;
        AvArg avArg;
        UploadState ffMuxSatte;
}FFTsMuxUploader;

static int aAacfreqs[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050 ,16000 ,12000, 11025, 8000, 7350};
static int gnQBufsize = 0;

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
                                pFFTsMuxUploader->nSegmentId = (int64_t)time(NULL);
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
                free(pTsMuxCtx);
        }
        
        return 0;
}

#define getFFmpegErrorMsg(errcode) char msg[128];\
                av_strerror(errcode, msg, sizeof(msg))

static void inline setQBufferSize(char *desc, int s)
{
        gnQBufsize = s;
        loginfo("desc:(%s) buffer Q size is:%d", desc, gnQBufsize);
        return;
}

static int getBufferSize() {
        if (gnQBufsize != 0) {
                return gnQBufsize;
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
                setQBufferSize("default", nSize);
                return gnQBufsize;
        }
        loginfo("toto memory size:%lld\n", nTotalMemSize);
        
        int64_t M = 1024 * 1024;
        if (nTotalMemSize <= 32 * M) {
                setQBufferSize("le 32M", nSize);
                return gnQBufsize;
        } else if (nTotalMemSize <= 64 * M) {
                setQBufferSize("le 64M", nSize * 2);
                return gnQBufsize;
        } else if (nTotalMemSize <= 128 * M) {
                setQBufferSize("le 128M", nSize * 3);
                return gnQBufsize;
        } else if (nTotalMemSize <= 256 * M) {
                setQBufferSize("le 256M", nSize * 4);
                return gnQBufsize;
        } else if (nTotalMemSize <= 512 * M) {
                setQBufferSize("le 512M", nSize * 6);
                return gnQBufsize;
        } else if (nTotalMemSize <= 1024 * M) {
                setQBufferSize("le 1G", nSize * 8);
                return gnQBufsize;
        } else if (nTotalMemSize <= 2 * 1024 * M) {
                setQBufferSize("le 2G", nSize * 10);
                return gnQBufsize;
        } else if (nTotalMemSize <= 4 * 1024 * M) {
                setQBufferSize("le 4G", nSize * 12);
                return gnQBufsize;
        } else {
                setQBufferSize("gt 4G", nSize * 16);
                return gnQBufsize;
        }
        return gnQBufsize;
}

static int newTsMuxContext(FFTsMuxContext ** _pTsMuxCtx, AvArg *_pAvArg)
#ifdef USE_OWN_TSMUX
{
        FFTsMuxContext * pTsMuxCtx = (FFTsMuxContext *)malloc(sizeof(FFTsMuxContext));
        if (pTsMuxCtx == NULL) {
                return TK_NO_MEMORY;
        }
        memset(pTsMuxCtx, 0, sizeof(FFTsMuxContext));
        
        int nBufsize = getBufferSize();
        int ret = NewUploader(&pTsMuxCtx->pTsUploader_, TSQ_FIX_LENGTH, 188, nBufsize / 188);
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
        int ret = NewUploader(&pTsMuxCtx->pTsUploader_, TSQ_FIX_LENGTH, FF_OUT_LEN, nBufsize / FF_OUT_LEN);
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

static const unsigned char pr2six[256] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
        64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 63,
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
};

static void Base64Decode(char *bufplain, const char *bufcoded) {
        register const unsigned char *bufin;
        register unsigned char *bufout;
        register int nprbytes;
        
        bufin = (const unsigned char *) bufcoded;
        while (pr2six[*(bufin++)] <= 63);
        nprbytes = (bufin - (const unsigned char *) bufcoded) - 1;
        
        bufout = (unsigned char *) bufplain;
        bufin = (const unsigned char *) bufcoded;
        
        while (nprbytes > 4) {
                *(bufout++) = (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
                *(bufout++) = (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
                *(bufout++) = (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
                bufin += 4;
                nprbytes -= 4;
        }
        
        if (nprbytes > 1)
                *(bufout++) = (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
        if (nprbytes > 2)
                *(bufout++) = (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
        if (nprbytes > 3)
                *(bufout++) = (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
        
        *(bufout++) = '\0';
}

static int getExpireDays(char * pToken)
{
        char * pPolicy = strchr(pToken, ':');
        if (pPolicy == NULL) {
                return TK_ARG_ERROR;
        }
        pPolicy++;
        pPolicy = strchr(pPolicy, ':');
        if (pPolicy == NULL) {
                return TK_ARG_ERROR;
        }
        
        pPolicy++; //jump :
        int len = (strlen(pPolicy) + 2) * 3 / 4 + 1;
        char *pPlain = malloc(len);
        Base64Decode(pPlain, pPolicy);
        pPlain[len - 1] = 0;
        
        char *pExpireStart = strstr(pPlain, "\"deleteAfterDays\"");
        if (pExpireStart == NULL) {
                free(pPlain);
                return 0;
        }
        pExpireStart += strlen("\"deleteAfterDays\"");
        
        char days[10] = {0};
        int nStartFlag = 0;
        int nDaysLen = 0;
        char *pDaysStrat = NULL;
        while(1) {
                if (*pExpireStart >= 0x30 && *pExpireStart <= 0x39) {
                        if (nStartFlag == 0) {
                                pDaysStrat = pExpireStart;
                                nStartFlag = 1;
                        }
                        nDaysLen++;
                }else {
                        if (nStartFlag)
                                break;
                }
                pExpireStart++;
        }
        memcpy(days, pDaysStrat, nDaysLen);
        free(pPlain);
        return atoi(days);
}

static void setToken(TsMuxUploader* _PTsMuxUploader, char *_pToken)
{
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_PTsMuxUploader;
        pFFTsMuxUploader->deleteAfterDays_ = getExpireDays(_pToken);
        pFFTsMuxUploader->pToken_ = _pToken;
}

static void setAccessKey(TsMuxUploader* _PTsMuxUploader, char *_pAk, int _nAkLen)
{
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_PTsMuxUploader;
        assert(sizeof(pFFTsMuxUploader->ak_) - 1 > _nAkLen);
        memcpy(pFFTsMuxUploader->ak_, _pAk, _nAkLen);
}

static void setSecretKey(TsMuxUploader* _PTsMuxUploader, char *_pSk, int _nSkLen)
{
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_PTsMuxUploader;
        assert(sizeof(pFFTsMuxUploader->sk_) - 1 > _nSkLen);
        memcpy(pFFTsMuxUploader->sk_, _pSk, _nSkLen);
}

static void setBucket(TsMuxUploader* _PTsMuxUploader, char *_pBucketName, int _nBucketNameLen)
{
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_PTsMuxUploader;
        assert(sizeof(pFFTsMuxUploader->bucketName_) - 1 > _nBucketNameLen);
        memcpy(pFFTsMuxUploader->bucketName_, _pBucketName, _nBucketNameLen);
}

static void setCallbackUrl(TsMuxUploader* _PTsMuxUploader, char *_pCallbackUrl, int _nCallbackUrlLen)
{
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_PTsMuxUploader;
        assert(sizeof(pFFTsMuxUploader->callback_) - 1 > _nCallbackUrlLen);
        memcpy(pFFTsMuxUploader->callback_, _pCallbackUrl, _nCallbackUrlLen);
}

static void setDeleteAfterDays(TsMuxUploader* _PTsMuxUploader, int nDays)
{
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_PTsMuxUploader;
        pFFTsMuxUploader->deleteAfterDays_ = nDays;
}

int NewTsMuxUploader(TsMuxUploader **_pTsMuxUploader, AvArg *_pAvArg)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)malloc(sizeof(FFTsMuxUploader));
        if (pFFTsMuxUploader == NULL) {
                return TK_NO_MEMORY;
        }
        memset(pFFTsMuxUploader, 0, sizeof(FFTsMuxUploader));
        pFFTsMuxUploader->nLastUploadVideoTimestamp = -1;
        
        int ret = 0;
        ret = pthread_mutex_init(&pFFTsMuxUploader->muxUploaderMutex_, NULL);
        if (ret != 0){
                free(pFFTsMuxUploader);
                return TK_MUTEX_ERROR;
        }
        
        pFFTsMuxUploader->tsMuxUploader_.SetToken = setToken;
        pFFTsMuxUploader->tsMuxUploader_.SetSecretKey = setSecretKey;
        pFFTsMuxUploader->tsMuxUploader_.SetAccessKey = setAccessKey;
        pFFTsMuxUploader->tsMuxUploader_.SetBucket = setBucket;
        pFFTsMuxUploader->tsMuxUploader_.SetCallbackUrl = setCallbackUrl;
        pFFTsMuxUploader->tsMuxUploader_.SetDeleteAfterDays = setDeleteAfterDays;
        pFFTsMuxUploader->tsMuxUploader_.PushAudio = PushAudio;
        pFFTsMuxUploader->tsMuxUploader_.PushVideo = PushVideo;
        
        pFFTsMuxUploader->avArg.nAudioFormat = _pAvArg->nAudioFormat;
        pFFTsMuxUploader->avArg.nChannels = _pAvArg->nChannels;
        pFFTsMuxUploader->avArg.nSamplerate = _pAvArg->nSamplerate;
        pFFTsMuxUploader->avArg.nVideoFormat = _pAvArg->nVideoFormat;
        
        *_pTsMuxUploader = (TsMuxUploader *)pFFTsMuxUploader;
        
        return 0;
}

int TsMuxUploaderStart(TsMuxUploader *_pTsMuxUploader)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        
        assert(pFFTsMuxUploader->pTsMuxCtx == NULL);
        
        int ret = newTsMuxContext(&pFFTsMuxUploader->pTsMuxCtx, &pFFTsMuxUploader->avArg);
        if (ret != 0) {
                free(pFFTsMuxUploader);
                return ret;
        }
        if (pFFTsMuxUploader->pToken_ == NULL || pFFTsMuxUploader->pToken_[0] == 0) {
                pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->SetAccessKey(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_,
                                                                        pFFTsMuxUploader->ak_, strlen(pFFTsMuxUploader->ak_));
                pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->SetSecretKey(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_,
                                                                        pFFTsMuxUploader->sk_, strlen(pFFTsMuxUploader->sk_));
                pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->SetBucket(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_,
                                                                     pFFTsMuxUploader->bucketName_, strlen(pFFTsMuxUploader->bucketName_));
                pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->SetCallbackUrl(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_,
                                                                          pFFTsMuxUploader->callback_, strlen(pFFTsMuxUploader->callback_));
        } else {
                pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->SetToken(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_, pFFTsMuxUploader->pToken_);
        }
        pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->SetDeleteAfterDays(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_,
                                                                      pFFTsMuxUploader->deleteAfterDays_);

        if (pFFTsMuxUploader->nSegmentId == 0) {
                pFFTsMuxUploader->nSegmentId = (int64_t)time(NULL);
        }
        pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->SetSegmentId(
                                                                pFFTsMuxUploader->pTsMuxCtx->pTsUploader_, pFFTsMuxUploader->nSegmentId );
        pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->UploadStart(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_);
        return 0;
}

void DestroyTsMuxUploader(TsMuxUploader **_pTsMuxUploader)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)(*_pTsMuxUploader);
        
        pushRecycle(pFFTsMuxUploader);
        if (pFFTsMuxUploader) {
                if (pFFTsMuxUploader->pAACBuf) {
                        free(pFFTsMuxUploader->pAACBuf);
                }
                free(pFFTsMuxUploader);
        }
        return;
}

void SetUploadBufferSize(int nSize)
{
        if (nSize < 256) {
                logwarn("set queue buffer is too small(%d). need great equal than 256k", nSize);
                return;
        }
        loginfo("set queue buffer to:%dk", nSize);
        gnQBufsize = nSize * 1024;
}
