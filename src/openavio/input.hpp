#ifndef __INPUT_HPP__
#define __INPUT_HPP__

#include "common.hpp"
#include "Statistics.h"

class MediaPacket;
class MediaFrame;
class AudioResampler;
// AvReceiver
typedef const std::function<int(const std::unique_ptr<MediaPacket>)> PacketHandlerType;
typedef void(*GetFrameCallback)(void * ,std::shared_ptr<MediaFrame>&);
typedef int(*FeedDataCallback)(void *, uint8_t *buf, int buf_size);

typedef struct InputParam {
    std::string name_;
    std::string url_;
    std::string formatHint_; //h264 alaw mulaw #ffmpeg -formats
    void * userData_;
    GetFrameCallback getFrameCb_ = nullptr;
    FeedDataCallback feedDataCb_ = nullptr;
    void * feedCbOpaqueArg_ = nullptr;
    std::vector<std::string> audioOpts;
    std::vector<std::string> videoOpts;
    std::atomic<bool> *__innerQuitFlag;
}InputParam;

class AvReceiver
{
public:
    AvReceiver(IN InputParam * param);
    ~AvReceiver();
    int Receive(IN PacketHandlerType& callback);

    int GetVideoCtxFps(){return videoAvgFps_;}
    int GetAudioCtxFps(){return audioAvgFps_;}
    int HasAudio() { return hasAudio_; }
    int HasVideo() { return hasVideo_; }

private:
    struct AVFormatContext* pAvContext_ = nullptr;
    AVIOContext *pAvioCtx_ = nullptr;
    uint8_t *pAvioCtxBuffer = nullptr;

    InputParam *param_ = nullptr;
    bool hasAudio_ = false;
    bool hasVideo_ = false;
    int  videoAvgFps_ = -1;
    int  audioAvgFps_ = -1;
    int  mediaDuration_ = -1;

    std::chrono::high_resolution_clock::time_point start_;
    long nTimeout_ = 10000; // 10 seconds timeout by default
    std::vector<struct AVStream*> streams_;

private:
    static int AvInterruptCallback(void* pContext);
    int initContext();
    void release();
};

// AvDecoder
typedef const std::function<int(const std::shared_ptr<MediaFrame>&)> FrameHandlerType;
class AvDecoder
{
public:
    AvDecoder();
    ~AvDecoder();
    int Decode(IN const std::unique_ptr<MediaPacket>& pPacket, IN FrameHandlerType& callback);
private:
    int Init(IN const std::unique_ptr<MediaPacket>& pPakcet);
private:
    AVCodecContext* pAvDecoderContext_ = nullptr;
    bool bIsDecoderAvailable_ = false;
};

// Input
using namespace std::chrono;
class Input : public StopClass
{
public:
    Input(IN InputParam _param);
    ~Input();
    std::string GetName();
    std::string String();
    void Start();
    void Stop();
    void WaitStop();
    const MediaStatInfo & GetMediaStatInfo(){return stat_->GetStatInfo();}
    const char * GetMediaStatInfoStr(){return stat_->toString();}

private:
    // push one video/audio
    int outputFrame(std::shared_ptr<MediaFrame>& pFrame, int64_t nNow,
                    int64_t &nPrevTime, int64_t &nStartFrameTime, int64_t composation);
    void outputFrame(const std::shared_ptr<MediaFrame>& pFrame);
    void setPts(const std::shared_ptr<MediaFrame>& pFrame, int64_t nFrameCount, int64_t _nPrevTime, int _nFps);
    void reset();
    void resampleAndSetAudio(const std::shared_ptr<MediaFrame>& _pFrame);
public:
    static const size_t AUDIO_Q_LEN = 200;
    static const size_t VIDEO_Q_LEN = 30;
private:
    InputParam param_;

    std::unique_ptr<AvReceiver> avReceiver_;
    std::unique_ptr<AvDecoder>  aDecoder_;
    std::unique_ptr<AvDecoder> vDecoder_;
    std::thread receiver_;

    std::atomic<bool> bReceiverExit_;
    std::atomic<bool> bRestart_;

    // frame相关的pts一般都是毫秒
    int64_t startAudioPts;
    int64_t startVideoPts;
    int64_t prevAudioPts;
    int64_t prevVideoPts;
    int64_t audioComposationTime;
    int64_t videoComposationTime;
        
    int64_t startSysTime; //纳秒

    int64_t audioFrameCount_;
    int64_t audioResampleFrameCount_;
    int64_t videoFrameCount_;

    std::queue<std::shared_ptr<MediaFrame>> audioFrameQueue_;
    std::queue<std::shared_ptr<MediaFrame>> videoFrameQueue_;
    std::vector<uint8_t> sampleBuffer_;
    std::shared_ptr<AudioResampler> resampler_;
    std::shared_ptr<Statistics> stat_;
};

#endif
