#include "input.hpp"

#define THIS_FILE "input.cpp"
#define INPUT_RESAMPLE
//
// AvReceiver
//

AvReceiver::AvReceiver(IN InputParam * _param) :
    param_(_param)
{
}

AvReceiver::~AvReceiver()
{
    release();
}

void AvReceiver::release()
{
    if (pAvContext_ != nullptr) {
        avformat_close_input(&pAvContext_);
        if (pAvioCtx_ != nullptr) {
            if (pAvioCtx_->buffer != pAvioCtxBuffer) {
                logwarn("ffmpeg internal buffer have changed");
            }
            av_freep(&pAvioCtx_->buffer);
            av_freep(&pAvioCtx_);
            pAvioCtxBuffer = nullptr;
            pAvioCtx_ = nullptr;
        }
        pAvContext_ = nullptr;
    }
}

int AvReceiver::AvInterruptCallback(void* _pContext)
{
    using namespace std::chrono;
    AvReceiver* pReceiver = reinterpret_cast<AvReceiver*>(_pContext);
    high_resolution_clock::time_point now = high_resolution_clock::now();
    auto diff = duration_cast<milliseconds>(now - pReceiver->start_).count();
    if (diff > pReceiver->nTimeout_) {
        logerror("receiver timeout, %ld milliseconds, %lld-%lld=%lld", pReceiver->nTimeout_,
            now.time_since_epoch().count(), pReceiver->start_.time_since_epoch().count(), diff);
        return -1;
    }
    if(pReceiver->param_->__innerQuitFlag->load() == true) {
        return -1;
    }

    return 0;
}

int AvReceiver::initContext()
{
    if (pAvContext_ != nullptr) {
        logwarn("AvReceiver already inited");
        return -1;
    }

    if (param_->url_.empty() && param_->feedDataCb_ == nullptr) {
        logerror("AvReceiver wrong param_");
        return -2;
    }

    // allocate AV context
    pAvContext_ = avformat_alloc_context();
    if (pAvContext_ == nullptr) {
        logerror("av context could not be created");
        return -3;
    }
    pAvContext_->interrupt_callback.callback = AvReceiver::AvInterruptCallback;
    pAvContext_->interrupt_callback.opaque = this;

    // for timeout timer
    if (nTimeout_ != 0)
        nTimeout_ = 10 * 1000; // 10 seconds
    start_ = std::chrono::high_resolution_clock::now();
#if LOG_TRADITIONAL
    loginfo("receiver timeout=%lu milliseconds start st:%lld", nTimeout_, start_.time_since_epoch().count());
#else
    loginfo("receiver timeout={} milliseconds start st:{}", nTimeout_, start_.time_since_epoch().count());
#endif


    AVDictionary * options = nullptr;
    if (!param_->formatHint_.empty()) {
        // fix delay of av_read_frame
        av_dict_set(&options,"probesize","512",0);
        av_dict_set(&options,"max_analyze_duration", "200",0);
    }
        
    AVInputFormat * infmt = nullptr;
    if (!param_->formatHint_.empty()) {
        infmt = av_find_input_format(param_->formatHint_.c_str());
        fprintf(stderr, "find %s AVInputFormat", param_->formatHint_.c_str());
    }
        
    int nStatus = 0;
    if (!param_->url_.empty()) {
        // open input stream
#if LOG_TRADITIONAL
        loginfo("input URL: %s", param_->url_.c_str());
#else
        loginfo("input URL: {}", param_->url_.c_str());
#endif
        nStatus = avformat_open_input(&pAvContext_, param_->url_.c_str(), infmt, &options);
    }
    else if (param_->feedDataCb_) {
        pAvContext_ = avformat_alloc_context();
        pAvioCtxBuffer = (uint8_t *)av_malloc(4096);
        pAvioCtx_ = avio_alloc_context(pAvioCtxBuffer, 4096, 0,
            param_->feedCbOpaqueArg_, param_->feedDataCb_, nullptr, nullptr);
        pAvContext_->pb = pAvioCtx_;

        nStatus = avformat_open_input(&pAvContext_, nullptr, infmt, &options);
    }
    av_dict_free(&options);
#if LOG_TRADITIONAL
        STAUS_CHECK(nStatus, -3, "could not open input stream: %s %s", param_->url_.c_str(), param_->formatHint_.c_str());
#else
        STAUS_CHECK(nStatus, -3, "could not open input stream: {} {}", param_->url_.c_str(), param_->formatHint_.c_str());
#endif

        AVDictionary *opts[10] = {0};
        for (int i = 0; i < pAvContext_->nb_streams; i++) {
                opts[i] = nullptr;
                if (pAvContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                        AVDictionary *aOpts = NULL;
                        for (int j = 0; j < param_->audioOpts.size(); j+=2) {
                                av_dict_set(&aOpts, param_->audioOpts[j].c_str(), param_->audioOpts[j+1].c_str(), 0);
                        }
                        opts[i] = aOpts;
                } else if (pAvContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                        AVDictionary *vOpts = NULL;
                        for (int j = 0; j < param_->audioOpts.size(); j+=2) {
                                av_dict_set(&vOpts, param_->videoOpts[j].c_str(), param_->videoOpts[j+1].c_str(), 0);
                        }
                        opts[i] = vOpts;
                }
        }
        
    // get stream info
    nStatus = avformat_find_stream_info(pAvContext_, opts);
    if (nStatus < 0) {
        logerror("could not get stream info");
        return -1;
    }
    for (int i = 0; i < pAvContext_->nb_streams; i++) {
        av_dict_free(&opts[i]);
    }

    for (unsigned int i = 0; i < pAvContext_->nb_streams; i++) {
        struct AVStream * pAvStream = pAvContext_->streams[i];
        streams_.push_back(pAvStream);
#if LOG_TRADITIONAL
        loginfo("stream is found: avstream=%d, avcodec=%d",
                
#else
        loginfo("stream is found: avstream={}, avcodec={}",
#endif
            pAvStream->codecpar->codec_type, pAvStream->codecpar->codec_id);
        if (pAvContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioAvgFps_ = av_q2d(pAvContext_->streams[i]->avg_frame_rate);
            hasAudio_ = true;
        }
        if (pAvContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoAvgFps_ = av_q2d(pAvContext_->streams[i]->avg_frame_rate);
            hasVideo_ = true;
        }
    }

    mediaDuration_ = pAvContext_->duration / 1000000;
    
#if LOG_TRADITIONAL
    loginfo("hasVideo=%s hasAudio=%s vFps=%d aFps=%d", hasVideo_ ? "yes" : "no",
#else
    loginfo("hasVideo={} hasAudio={} vFps={} aFps={}", hasVideo_ ? "yes" : "no",
#endif
        hasAudio_ ? "yes" : "no", videoAvgFps_, audioAvgFps_);

    return 0;
}

int AvReceiver::Receive(IN PacketHandlerType& _callback)
{
    int ret = initContext();
    if (ret != 0) {
        return ret;
    }

    while (true) {
        AVPacket* pAvPacket = av_packet_alloc();
        av_init_packet(pAvPacket);
        int ret = 0;
        if ((ret = av_read_frame(pAvContext_, pAvPacket)) == 0) {
            if (pAvPacket->stream_index < 0 ||
                static_cast<unsigned int>(pAvPacket->stream_index) >= pAvContext_->nb_streams) {
                logwarn("invalid stream index in packet");
                av_packet_free(&pAvPacket);
                continue;
            }

            // we need all PTS/DTS use milliseconds, sometimes they are macroseconds such as TS streams
            AVRational tb = AVRational{ 1, 1000 };
            AVRounding r = static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
            pAvPacket->dts = av_rescale_q_rnd(pAvPacket->dts, streams_[pAvPacket->stream_index]->time_base, tb, r);
            pAvPacket->pts = av_rescale_q_rnd(pAvPacket->pts, streams_[pAvPacket->stream_index]->time_base, tb, r);

            int nStatus = _callback(std::make_unique<MediaPacket>(*streams_[pAvPacket->stream_index], pAvPacket));
            if (nStatus != 0) {
                return nStatus;
            }
            start_ = std::chrono::high_resolution_clock::now();
        }
        else {
#if LOG_TRADITIONAL
            logerror("av_frame_frame error:%d", ret);
#else
            logerror("av_frame_frame error:{}", ret);
#endif
            break;
        }
    }

    return 0;
}


//
// AvDecoder
//

AvDecoder::AvDecoder()
{
}

AvDecoder::~AvDecoder()
{
    if (bIsDecoderAvailable_) {
        avcodec_close(pAvDecoderContext_);
    }
    if (pAvDecoderContext_ != nullptr) {
        avcodec_free_context(&pAvDecoderContext_);
    }
}

int AvDecoder::Init(IN const std::unique_ptr<MediaPacket>& _pPacket)
{
    // create decoder
    if (pAvDecoderContext_ == nullptr) {
        // find decoder
        AVCodec *pAvCodec = avcodec_find_decoder(static_cast<AVCodecID>(_pPacket->GetCodec()));
        if (pAvCodec == nullptr) {
#if LOG_TRADITIONAL
            logerror("could not find AV decoder for codec_id=%d", _pPacket->GetCodec());
#else
            logerror("could not find AV decoder for codec_id={}", _pPacket->GetCodec());
#endif
            return -1;
        }

        // initiate AVCodecContext
        pAvDecoderContext_ = avcodec_alloc_context3(pAvCodec);
        if (pAvDecoderContext_ == nullptr) {
            logerror("could not allocate AV codec context");
            return -1;
        }

        // if the packet is from libavformat
        // just use context parameters in AVStream to get one directly otherwise fake one
        if (_pPacket->AvCodecParameters() != nullptr) {
            if (avcodec_parameters_to_context(pAvDecoderContext_, _pPacket->AvCodecParameters()) < 0) {
                logerror("could not copy decoder context");
                return -1;
            }
        }

        // open it
        if (avcodec_open2(pAvDecoderContext_, pAvCodec, nullptr) < 0) {
            logerror("could not open decoder");
            return -1;
        }
        else {
#if LOG_TRADITIONAL
            loginfo("open decoder: stream=%d, codec=%d", _pPacket->GetStreamType(), _pPacket->GetCodec());
#else
            loginfo("open decoder: stream={}, codec={}", _pPacket->GetStreamType(), _pPacket->GetCodec());
#endif
            bIsDecoderAvailable_ = true;
        }
    }

    return 0;
}

int AvDecoder::Decode(IN const std::unique_ptr<MediaPacket>& _pPacket, IN FrameHandlerType& _callback)
{
    if (Init(_pPacket) < 0) {
        return -1;
    }

    //
    // decode ! and get one frame to encode
    //
    do {
        bool bNeedSendAgain = false;
        int nStatus = avcodec_send_packet(pAvDecoderContext_, _pPacket->AvPacket());
        if (nStatus != 0) {
            if (nStatus == AVERROR(EAGAIN)) {
                logwarn("decoder internal: assert failed, we should not get EAGAIN");
                bNeedSendAgain = true;
            }
            else {
#if LOG_TRADITIONAL
                logerror("decoder: could not send frame, status=%d", nStatus);
#else
                logerror("decoder: could not send frame, status={}", nStatus);
#endif
                _pPacket->Print();
                return -1;
            }
        }

        while (1) {
            // allocate a frame for outputs
            auto pFrame = std::make_shared<MediaFrame>();
            pFrame->SetStreamType(_pPacket->GetStreamType());
            pFrame->SetCodec(_pPacket->GetCodec());

            nStatus = avcodec_receive_frame(pAvDecoderContext_, pFrame->AvFrame());
            if (nStatus == 0) {
                pFrame->pts = av_frame_get_best_effort_timestamp(pFrame->AvFrame());
                //pFrame->pts = pFrame->AvFrame()->best_effort_timestamp;
                int nStatus = _callback(pFrame);
                if (nStatus < 0) {
                    return nStatus;
                }
                if (bNeedSendAgain) {
                    break;
                }
            }
            else if (nStatus == AVERROR(EAGAIN)) {
                return 0;
            }
            else {
#if LOG_TRADITIONAL
                logerror("decoder: could not receive frame, status=%d", nStatus);
#else
                logerror("decoder: could not receive frame, status={}", nStatus);
#endif
                _pPacket->Print();
                return -1;
            }
        }
    } while (1);

    return 0;
}

//
// Input
//
Input::Input(IN InputParam _param) :
    param_(_param),
    startAudioPts(-1),
    startVideoPts(-1),
    prevAudioPts(-1),
    prevVideoPts(-1),
    audioComposationTime(-1),
    videoComposationTime(-1),
    startSysTime(-1),
    audioFrameCount_(0),
    audioResampleFrameCount_(0),
    videoFrameCount_(0)
{
    bReceiverExit_.store(false);
    bRestart_.store(true);
    param_.__innerQuitFlag = &bReceiverExit_;
    stat_ = std::make_shared<Statistics>(10);
}

// start thread => receiver loop => decoder loop
void Input::Start()
{
    auto recv = [this] {
        while (bReceiverExit_.load() == false) {
            avReceiver_ = std::make_unique<AvReceiver>(&param_);
            vDecoder_ = std::make_unique<AvDecoder>();
            aDecoder_ = std::make_unique<AvDecoder>();

            auto receiverHook = [&](IN const std::unique_ptr<MediaPacket> _pPacket) -> int {
                auto type = _pPacket->GetStreamType();
                if (!stat_->IsStarted()) {
                    stat_->Start();
                }
                if (type == AVMEDIA_TYPE_AUDIO) {
                    stat_->StatAudio(_pPacket->AvPacket()->size);
                } else if (type == AVMEDIA_TYPE_VIDEO) {
                    stat_->StatVideo(_pPacket->AvPacket()->size, false);
                }

                if (bReceiverExit_.load() == true) {
                    return -1;
                }

                auto decoderHook = [&](const std::shared_ptr<MediaFrame>& _pFrame) -> int {
                    if (bReceiverExit_.load() == true) {
                        return -1;
                    }

                    outputFrame(std::make_shared<MediaFrame>(*_pFrame));

                    return 0;
                };

                // start decoder loop
                if (_pPacket->GetStreamType() == AVMEDIA_TYPE_VIDEO) {
                    vDecoder_->Decode(_pPacket, decoderHook);
                }
                else if (_pPacket->GetStreamType() == AVMEDIA_TYPE_AUDIO) {
                    aDecoder_->Decode(_pPacket, decoderHook);
                }

                return 0;
            };

            // start receiver loop
            // 退出条件是receiverHook返回非0
            avReceiver_->Receive(receiverHook);
                
            while(audioFrameQueue_.size() > 0) {
                std::shared_ptr<MediaFrame> frame = audioFrameQueue_.front();
                int64_t nNowAudio = os_gettime_ns();
                int nComposation = (audioComposationTime == -1 ? 0 : audioComposationTime);
                int diff = outputFrame(frame, nNowAudio, prevAudioPts, startAudioPts, nComposation);
                if (diff == 0) {
                    audioFrameQueue_.pop();
                } else {
                    os_sleep_ms(5);
                }
            }
            while(videoFrameQueue_.size() > 0) {
                std::shared_ptr<MediaFrame> frame = videoFrameQueue_.front();;
                int64_t nNowAudio = os_gettime_ns();
                int nComposation = (videoComposationTime == -1 ? 0 : videoComposationTime);
                int diff = outputFrame(frame, nNowAudio, prevVideoPts, startVideoPts, nComposation);
                if (diff == 0) {
                    videoFrameQueue_.pop();
                } else {
                    os_sleep_ms(5);
                }
            }

            stat_->Reset();
            if (bRestart_.load()) {
                // prevent receiver reconnecting too fast
                loginfo("restart input");
                reset();
                os_sleep_ms(2000);
            } else {
                bReceiverExit_.store(true);
                break;
            }
        }
    };

    receiver_ = std::thread(recv);
}


void Input::reset() {
    startAudioPts = -1;
    startVideoPts = -1;
    prevAudioPts = -1;
    prevVideoPts = -1;
    audioComposationTime = -1;
    videoComposationTime = -1;
    startSysTime = -1;
    audioFrameCount_ = 0;
    audioResampleFrameCount_ = 0;
    videoFrameCount_ = 0;
}

void Input::Stop()
{
    if (stat_.get() != nullptr){
        stat_->Stop();
    }
    bReceiverExit_.store(true);
    reset();
    if (receiver_.joinable()) {
        receiver_.join();
    }
    reset();
}

void Input::WaitStop()
{
        bRestart_.store(false);
        if (receiver_.joinable()) {
                receiver_.join();
        }
        reset();
}

int Input::outputFrame(std::shared_ptr<MediaFrame>& _pFrame, int64_t nNow, int64_t &_nPrevTime,
                       int64_t &_nStartFrameTime, int64_t _nComposation)
{
    if (_nStartFrameTime == -1) {
        _nStartFrameTime = _pFrame->pts;
        if (_nStartFrameTime < 0) {
#if LOG_TRADITIONAL
            logwarn("%s first video pts abnormal: %lld",
                param_.name_.c_str(), _nStartFrameTime);
#else
            logwarn("%s first video pts abnormal: {}",
                param_.name_.c_str(), _nStartFrameTime);
#endif
            _nStartFrameTime = 0;
        }
    }
        
    if (startSysTime == -1) {
        startSysTime = nNow;
    }

    if(_nPrevTime == -1){ //如果是第一帧直接输出
        _nPrevTime = _pFrame->pts;
        param_.getFrameCb_(param_.userData_, _pFrame);
        return 0;
    }
    else {
#ifdef ACCURACY_NS
        int64_t nWallElapse = nNow - startSysTime;
        int64_t nFrameElapse = (_pFrame->pts  - _nStartFrameTime + _nComposation) * 1000000;
        int64_t nDiff = nFrameElapse - nWallElapse - 1000000;
        if (nDiff > 1000000) {
            return nDiff;
        }
#else
        int64_t nWallElapse = (nNow - startSysTime) / 1000000;
        int64_t nFrameElapse = _pFrame->pts  - _nStartFrameTime + _nComposation;
        int64_t nDiff = nFrameElapse - nWallElapse - 1;
        if (nDiff > 0) {
            return nDiff;
        }
#endif
        
        //logdebug("sleep, nDiff=%lld, frmaeElapse:%lld", nDiff/1000000, nFrameElapse);

        
        else {
            _nPrevTime = _pFrame->pts;
            param_.getFrameCb_(param_.userData_, _pFrame);
            return 0;
        }
    }
}

void Input::setPts(const std::shared_ptr<MediaFrame>& _pFrame, int64_t nFrameCount_, int64_t _nPrevTime, int _nFps)
{
        if (_pFrame->pts == AV_NOPTS_VALUE) { // pts拿不到，比如裸的h264文件
                //assert(_nFps != 0);
                if (_nFps != 0) {
                        _pFrame->pts = (nFrameCount_ * 1000 / _nFps);
                } else {
                        if (_nPrevTime == -1) {
                                _pFrame->pts = 0;
                        } else {
                                _pFrame->pts = _nPrevTime + 1;
                        }
                }
                return;
        }
        if (_pFrame->pts < _nPrevTime) {
                int64_t nOldPts = _pFrame->pts;
                if (_nFps != 0) {
                        _pFrame->pts = (_nPrevTime + 1000 / _nFps);
#if LOG_TRADITIONAL
                    logwarn("%s pts not monotone. prevPts:%lld -> curPts:%lld, setto:%lld",
#else
                    logwarn("{} pts not monotone. prevPts:{} -> curPts:{}, setto:{}",
#endif
                        param_.name_.c_str(), _nPrevTime, nOldPts, _pFrame->pts);
                } else {
                        _pFrame->pts += 1;
#if LOG_TRADITIONAL
                    logwarn("%s pts not monotone with no fps. prevPts:%lld -> curPts:%lld, setto:%lld",
#else
                    logwarn("{} pts not monotone with no fps. prevPts:{} -> curPts:{}, setto:{}",
#endif
                        param_.name_.c_str(), _nPrevTime, nOldPts, _pFrame->pts);
                }
        }
        return;
}

void Input::outputFrame(const std::shared_ptr<MediaFrame>& _pFrame)
{
    int64_t originPts = _pFrame->pts;
    // format video/audio data
    if (_pFrame->GetStreamType() == AVMEDIA_TYPE_AUDIO) {
        audioFrameCount_++;
        setPts(_pFrame, audioFrameCount_, prevAudioPts, avReceiver_->GetAudioCtxFps());
        int64_t adjustPts = _pFrame->pts;
#ifdef INPUT_RESAMPLE
        if (startAudioPts == -1) {
            startAudioPts = _pFrame->pts;
        }
        resampleAndSetAudio(_pFrame);
#else
        audioFrameQueue_.push(_pFrame);
#endif
#if LOG_TRADITIONAL
            logdebug("origin audiopts:%lld  adjustpts:%lld resamplePts:%lld", originPts, adjustPts, _pFrame->pts);
#else
            logdebug("origin audiopts:{} adjustpts:{} resamplePts:{}", originPts, adjustPts, _pFrame->pts);
#endif
    }
    else if (_pFrame->GetStreamType() == AVMEDIA_TYPE_VIDEO) {
        videoFrameCount_++;
        setPts(_pFrame, videoFrameCount_, prevVideoPts, avReceiver_->GetVideoCtxFps());
#if LOG_TRADITIONAL
            logdebug("origin videopts:%lld adjustpts:%lld", originPts,  _pFrame->pts);
#else
            logdebug("origin videopts:{} adjustpts:{}", originPts, _pFrame->pts);
#endif
        videoFrameQueue_.push(_pFrame);
    }
       
    std::shared_ptr<MediaFrame> frame;
    while(true) {
        int64_t nNowAudio = os_gettime_ns();

        int64_t audioDiff = -1, nextAudioTime = -1;
        int64_t videoDiff = -1, nextVideoTime = -1;

        if (audioFrameQueue_.size() > 0) {
            frame = audioFrameQueue_.front();
            if (audioComposationTime == -1) {
                if (!avReceiver_->HasVideo())
                    audioComposationTime = 0;
                else if (startAudioPts > -1 && startVideoPts > -1) {
                    if (startAudioPts <= startVideoPts)
                        audioComposationTime = 0;
                    else {
                        audioComposationTime = startAudioPts - startVideoPts;
                    }
                }
            }
            int nComposation = (audioComposationTime == -1 ? 0 : audioComposationTime);
            audioDiff = outputFrame(frame, nNowAudio, prevAudioPts, startAudioPts, nComposation);
            if (audioDiff == 0) {
                audioFrameQueue_.pop();
            }
            else {
                nextAudioTime = frame->pts;
            }
        }

        int64_t nNowVideo = 0;
        
        if (videoFrameQueue_.size() > 0) {
            nNowVideo = os_gettime_ns();
            frame = videoFrameQueue_.front();
            if (videoComposationTime == -1) {
                if (!avReceiver_->HasAudio())
                    videoComposationTime = 0;
                else if (startAudioPts > -1 && startVideoPts > -1) {
                    if (startVideoPts <= startAudioPts)
                        videoComposationTime = 0;
                    else {
                        videoComposationTime =  startVideoPts - startAudioPts;
                    }
                }
            }
            int nComposation = (videoComposationTime == -1 ? 0 : videoComposationTime);
            videoDiff = outputFrame(frame, nNowVideo, prevVideoPts, startVideoPts, nComposation);
            if (videoDiff == 0) {
                videoFrameQueue_.pop();
            }
            else {
                nextVideoTime = frame->pts;
            }
        }
        
        auto audioSleep = [videoDiff, audioDiff, this] () {
#if LOG_TRADITIONAL
            logdebug("audio sleep:%lld video:%lld", audioDiff, videoDiff);
#else
                logdebug("audio sleep:{} video:{} {} {}", audioDiff, videoDiff, audioFrameCount_, audioResampleFrameCount_);
#endif
#ifdef ACCURACY_NS
            os_sleep_ns(audioDiff);
#else
            os_sleep_ms(audioDiff);
#endif
                logdebug("after audio sleep");
        };
            
        auto videoSleep = [videoDiff, audioDiff, this] () {
#if LOG_TRADITIONAL
            logdebug("video sleep:%lld audio:%lld", videoDiff, audioDiff);
#else
                logdebug("video sleep:{} audio:{} {} {}", videoDiff, audioDiff, videoFrameCount_, videoFrameQueue_.size());
#endif
#ifdef ACCURACY_NS
            os_sleep_ns(videoDiff);
#else
            os_sleep_ms(videoDiff);
#endif
                logdebug("after video sleep");
        };
        
        if (!avReceiver_->HasAudio()) {
            if(videoDiff > 0) {
                videoSleep();
                continue;
            }
            break; //只有视频直接break，来一帧消耗一个帧的
        }

        if(!avReceiver_->HasVideo()) {
            if(audioDiff > 0 ) {
                audioSleep();
                continue;
            }
            break; //只有音频直接break，来一帧消耗一个帧的
        }

        if (videoDiff == -1){ //视频队列空的情况
            //TODO 检查音频队列最大值，音频带封面的可能 
            break; //不断读取
        }
            
        if (audioDiff == -1) { //音频队列空的情况
            //没有见过视频里面只有一瞬间音频的，所以这里不考虑这种情况
            break; //不断读取
        }
            
        //if (audioDiff == 0 && videoDiff > 0) { //音频输出了，但是视频没有输出
        //}
        

        if (videoDiff > 0 && audioDiff > 0) {
            if (videoDiff < audioDiff) {
                videoSleep();
            }
            else {
                audioSleep();
            }
        }
    }
}

Input::~Input()
{
        fprintf(stderr, "~Input");
    Stop();
}

std::string Input::GetName()
{
    return param_.name_;
}

std::string Input::String()
{
    std::string out = "<" + GetName() + ">\n";
    return out;
}
                            
void Input::resampleAndSetAudio(const std::shared_ptr<MediaFrame>& _pFrame)
{
        if (resampler_.get() == nullptr){
                resampler_ = std::make_shared<AudioResampler>(_pFrame->AvFrame()->channels,
                                                              _pFrame->AvFrame()->sample_rate);
                //sampleBuffer_.reserve();
        }
        // if the buffer size meets the min requirement of encoding one frame, build a frame and push upon audio queue
        size_t nSizeEachFrame = resampler_->GetChannels() * av_get_bytes_per_sample(resampler_->GetSampleFormat())
                                                        *AudioResampler::FRAME_SIZE;
        
        //当前帧resample后的大小
        size_t nCurFrameResampleSize = resampler_->GetChannels() * av_get_bytes_per_sample(resampler_->GetSampleFormat())
                                * _pFrame->AvFrame()->nb_samples;
        
        size_t nBufSize = sampleBuffer_.size();
        if (sampleBuffer_.capacity() < nCurFrameResampleSize + nBufSize) {
                sampleBuffer_.reserve(nCurFrameResampleSize + nBufSize + 128);
        }

        // resample to the same audio format
        if (resampler_->Resample(_pFrame, sampleBuffer_, nBufSize) != 0) {
                return;
        }
        
        while (sampleBuffer_.size() >= nSizeEachFrame) {
                std::shared_ptr<MediaFrame> pNewFrame = std::make_shared<MediaFrame>();
                pNewFrame->SetStreamType(_pFrame->GetStreamType());
                pNewFrame->SetCodec(_pFrame->GetCodec());
                av_frame_copy_props(pNewFrame->AvFrame(), _pFrame->AvFrame());
                pNewFrame->AvFrame()->nb_samples = AudioResampler::FRAME_SIZE;
                pNewFrame->AvFrame()->format = resampler_->GetSampleFormat();
                pNewFrame->AvFrame()->channels = resampler_->GetChannels();
                pNewFrame->AvFrame()->channel_layout = av_get_default_channel_layout(resampler_->GetChannels());
                pNewFrame->AvFrame()->sample_rate = resampler_->GetSampleRate();
                
                av_frame_get_buffer(pNewFrame->AvFrame(), 0);
                
                std::copy(sampleBuffer_.begin(), sampleBuffer_.begin() + nSizeEachFrame,
                          pNewFrame->AvFrame()->data[0]);
                
                // move rest samples to beginning of the buffer
                std::copy(sampleBuffer_.begin() + nSizeEachFrame, sampleBuffer_.end(), sampleBuffer_.begin());
                sampleBuffer_.resize(sampleBuffer_.size() - nSizeEachFrame);
                if (startAudioPts == -1)
                    pNewFrame->pts = int64_t(audioResampleFrameCount_ * resampler_->DurationPerFrame());
                else
                    pNewFrame->pts = int64_t(audioResampleFrameCount_ * resampler_->DurationPerFrame()) + startAudioPts;
                audioResampleFrameCount_++;
                audioFrameQueue_.push(pNewFrame);
        }
}
                            
