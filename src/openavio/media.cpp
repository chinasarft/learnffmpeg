#include "media.hpp"
#define THIS_FILE "media.cpp"

//
// AudioResampler
//

AudioResampler::AudioResampler(int _channels, int _nSampleRate, AVSampleFormat _sampleFormat) :
    nChannel(_channels),
    nSampleRate(_nSampleRate),
    sampleFormat(_sampleFormat)
{
}

AudioResampler::~AudioResampler()
{
    if (pSwr_ != nullptr) {
        swr_free(&pSwr_);
    }
}

int AudioResampler::InitAudioResampling(IN const std::shared_ptr<MediaFrame>& _pFrame)
{
    // for fdkaac encoder, input samples should be PCM signed16le, otherwise do resampling
    if (_pFrame->AvFrame()->format != sampleFormat) {
#ifdef LOG_TRADITIONAL
        loginfo("input sample_format=%d, need sample_format=%d, initiate resampling",
#else
        loginfo("input sample_format={} need sample_format={}, initiate resampling",
#endif
            _pFrame->AvFrame()->format, sampleFormat);
        pSwr_ = swr_alloc();
        av_opt_set_int(pSwr_, "in_channel_layout", av_get_default_channel_layout(_pFrame->AvFrame()->channels), 0);
        av_opt_set_int(pSwr_, "out_channel_layout", av_get_default_channel_layout(nChannel), 0);
        av_opt_set_int(pSwr_, "in_sample_rate", _pFrame->AvFrame()->sample_rate, 0);
        av_opt_set_int(pSwr_, "out_sample_rate", nSampleRate, 0);
        av_opt_set_sample_fmt(pSwr_, "in_sample_fmt", static_cast<AVSampleFormat>(_pFrame->AvFrame()->format), 0);
        av_opt_set_sample_fmt(pSwr_, "out_sample_fmt", sampleFormat, 0);
        if (swr_init(pSwr_) != 0) {
            logerror("could not initiate resampling");
            return -1;
        }
    }

    return 0;
}


int AudioResampler::Resample(IN const std::shared_ptr<MediaFrame>& _pInFrame, OUT std::vector<uint8_t>& _buffer,
                             IN int _nBufOffset)
{
    AVFrame * f = _pInFrame->AvFrame();
    if (f->format == sampleFormat && f->channels == nChannel  && f->sample_rate == nSampleRate) {
        _buffer.resize(f->nb_samples * av_get_bytes_per_sample(sampleFormat) + _nBufOffset);
        std::copy(f->data[0], f->data[0] + f->linesize[0], _buffer.begin() + _nBufOffset);
        return 0;
    }

    if (pSwr_ == nullptr) {
        if (InitAudioResampling(_pInFrame) != 0) {
            logerror("could not init resampling");
            return -1;
        }
    }

    int nRetVal;
    uint8_t **pDstData = nullptr;
    int nDstLinesize;
    int nDstBufSize;
    int64_t nDstNbSamples = av_rescale_rnd(_pInFrame->AvFrame()->nb_samples, nSampleRate,
        _pInFrame->AvFrame()->sample_rate, AV_ROUND_UP);
    int64_t nMaxDstNbSamples = nDstNbSamples;

    // get output buffer
    nRetVal = av_samples_alloc_array_and_samples(&pDstData, &nDstLinesize, nChannel,
        nDstNbSamples, sampleFormat, 0);
    if (nRetVal < 0) {
        logerror("resampler: could not allocate destination samples");
        return -1;
    }

    // get output samples
    nDstNbSamples = av_rescale_rnd(swr_get_delay(pSwr_, _pInFrame->AvFrame()->sample_rate) + _pInFrame->AvFrame()->nb_samples,
        nSampleRate, _pInFrame->AvFrame()->sample_rate, AV_ROUND_UP);
    if (nDstNbSamples > nMaxDstNbSamples) {
        av_freep(&pDstData[0]);
        nRetVal = av_samples_alloc(pDstData, &nDstLinesize, nChannel,
            nDstNbSamples, sampleFormat, 1);
        if (nRetVal < 0) {
            logerror("resampler: could not allocate sample buffer");
            return -1;
        }
        nMaxDstNbSamples = nDstNbSamples;
    }

    // convert !!
    nRetVal = swr_convert(pSwr_, pDstData, nDstNbSamples, (const uint8_t **)_pInFrame->AvFrame()->extended_data,
        _pInFrame->AvFrame()->nb_samples);
    if (nRetVal < 0) {
        logerror("resampler: converting failed");
        return -1;
    }

    // get output buffer size
    nDstBufSize = av_samples_get_buffer_size(&nDstLinesize, nChannel, nRetVal, sampleFormat, 1);
    if (nDstBufSize < 0) {
        logerror("resampler: could not get sample buffer size");
        return -1;
    }

    _buffer.resize(nDstBufSize + _nBufOffset);
    std::copy(pDstData[0], pDstData[0] + nDstBufSize, _buffer.begin() + _nBufOffset);

    // cleanup
    if (pDstData)
        av_freep(&pDstData[0]);
    av_freep(&pDstData);

    return 0;
}

void AudioResampler::Gain(INOUT const uint8_t* _pData, IN int _nSize, IN int _nPercent)
{
    if (_nSize <= 0 || _nSize % 2 != 0) {
        logwarn("gain: size not positive or size is not even");
        return;
    }
    if (_nPercent < 0) {
        _nPercent = 0;
        memset((char *)_pData, 0, _nSize);
        return;
    }
    if (_nPercent > 300) {
        _nPercent = 300;
    }

    int16_t* p16 = (int16_t*)_pData;
    for (int i = 0; i < _nSize; i += 2) {
        int32_t nGained = static_cast<int32_t>(*p16) * _nPercent / 100;
        if (nGained < -0x80000) {
            nGained = -0x80000;
        }
        else if (nGained > 0x7fff) {
            nGained = 0x7fff;
        }
        *p16++ = nGained;
    }
}
