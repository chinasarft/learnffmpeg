#ifndef __MEDIA_HPP__
#define __MEDIA_HPP__

#include "common.hpp"

class MediaFrame;
class AudioResampler
{
public:
    static const int FRAME_SIZE = 1024;
public:
    AudioResampler(int channels, int sampleRate, AVSampleFormat sampleFormat = AV_SAMPLE_FMT_S16);
    ~AudioResampler();
    int Resample(IN const std::shared_ptr<MediaFrame>& _pInFrame, OUT std::vector<uint8_t>& buffer, IN int nBufOffset);
    inline double DurationPerFrame() {return (AudioResampler::FRAME_SIZE * 1000.0 / nSampleRate);}
    inline int64_t DurationPerFrameInt() { return (AudioResampler::FRAME_SIZE * 1000 / nSampleRate);}
    inline int GetChannels(){return nChannel;}
    inline int GetSampleRate() {return nSampleRate;}
    inline AVSampleFormat GetSampleFormat(){return sampleFormat;}
    static void Gain(INOUT const uint8_t* _pData, IN int _nSize, IN int _nPercent);
private:
    int InitAudioResampling(IN const std::shared_ptr<MediaFrame>& pFrame);
private:
    SwrContext* pSwr_ = nullptr; // for resampling
    int nChannel;
    int nSampleRate;
    AVSampleFormat sampleFormat;
};

#endif
