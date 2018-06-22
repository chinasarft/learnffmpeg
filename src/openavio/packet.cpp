#include "packet.hpp"
#define THIS_FILE "packet.cpp"

MediaPacket::MediaPacket(IN const AVStream& _avStream, IN const AVPacket* _pAvPacket)
{
    // save codec pointer
    pAvCodecPar_ = _avStream.codecpar;
    StreamType stream = static_cast<StreamType>(pAvCodecPar_->codec_type);
    if (stream != STREAM_AUDIO && stream != STREAM_VIDEO) {
        stream = STREAM_DATA;
    }
    SetStreamType(stream);
    SetCodec(static_cast<CodecType>(pAvCodecPar_->codec_id));
    Width(pAvCodecPar_->width);
    Height(pAvCodecPar_->height);
    SampleRate(pAvCodecPar_->sample_rate);
    Channels(pAvCodecPar_->channels);

    // copy packet
    pAvPacket_ = const_cast<AVPacket*>(_pAvPacket);
}

MediaPacket::MediaPacket()
{
    pAvPacket_ = av_packet_alloc();
    av_init_packet(pAvPacket_);
}

MediaPacket::~MediaPacket()
{
    av_packet_free(&pAvPacket_);
}

AVPacket* MediaPacket::AvPacket() const
{
    return const_cast<AVPacket*>(pAvPacket_);
}

AVCodecParameters* MediaPacket::AvCodecParameters() const
{
    return pAvCodecPar_;
}

uint64_t MediaPacket::Pts() const
{
    return pAvPacket_->pts;
}

void MediaPacket::Pts(uint64_t _pts)
{
    pAvPacket_->pts = _pts;
}

uint64_t MediaPacket::Dts() const
{
    return pAvPacket_->dts;
}

void MediaPacket::Dts(uint64_t _dts)
{
    pAvPacket_->dts = _dts;
}

StreamType MediaPacket::GetStreamType() const
{
    return stream_;
}

void MediaPacket::SetStreamType(StreamType _type)
{
    stream_ = _type;
}

CodecType MediaPacket::GetCodec() const
{
    return codec_;
}

void MediaPacket::SetCodec(CodecType _type)
{
    codec_ = _type;
}

char* MediaPacket::Data()const
{
    return reinterpret_cast<char*>(pAvPacket_->data);
}

int MediaPacket::Size() const
{
    return static_cast<int>(pAvPacket_->size);
}

void MediaPacket::Print() const
{
#if LOG_TRADITIONAL
    loginfo("packet: pts=%lu, dts=%lu, stream=%d, codec=%d, size=%lu",
#else
    loginfo("packet: pts={}, dts={}, stream={}, codec={}, size={}",
#endif
        static_cast<unsigned long>(pAvPacket_->pts), static_cast<unsigned long>(pAvPacket_->dts),
        GetStreamType(), GetCodec(), static_cast<unsigned long>(pAvPacket_->size));
        
}

void MediaPacket::Dump(const std::string& _title) const
{
#if LOG_TRADITIONAL
    logdebug("%spts=%lu, dts=%lu, stream=%d, codec=%d, size=%lu", _title.c_str(),
#else
    logdebug("{}pts={}, dts={}, stream={}, codec={}, size={}", _title.c_str(),
#endif
        static_cast<unsigned long>(pAvPacket_->pts), static_cast<unsigned long>(pAvPacket_->dts),
        GetStreamType(), GetCodec(), static_cast<unsigned long>(pAvPacket_->size));
    //PrintMem(Data(), Size());
}

int MediaPacket::Width() const
{
    return nWidth_;
}

int MediaPacket::Height() const
{
    return nHeight_;
}

void MediaPacket::Width(int _nValue)
{
    nWidth_ = _nValue;
}

void MediaPacket::Height(int _nValue)
{
    nHeight_ = _nValue;
}

int MediaPacket::SampleRate() const
{
    return nSampleRate_;
}

int MediaPacket::Channels() const
{
    return nChannels_;
}

void MediaPacket::SampleRate(int _nValue)
{
    nSampleRate_ = _nValue;
}

void MediaPacket::Channels(int _nValue)
{
    nChannels_ = _nValue;
}

bool MediaPacket::IsKey() const
{
    return ((pAvPacket_->flags & AV_PKT_FLAG_KEY) != 0);
}

void MediaPacket::SetKey()
{
    pAvPacket_->flags |= AV_PKT_FLAG_KEY;
}

//
// MediaFrame
//

MediaFrame::MediaFrame(IN const AVFrame* _pAvFrame)
{
    pAvFrame_ = const_cast<AVFrame*>(_pAvFrame);
}

MediaFrame::MediaFrame()
{
    pAvFrame_ = av_frame_alloc();
}

MediaFrame::~MediaFrame()
{
    av_frame_free(&pAvFrame_);

    if (pExtraBuf_ != nullptr) {
        av_free(pExtraBuf_);
        pExtraBuf_ = nullptr;
    }
}

MediaFrame::MediaFrame(const MediaFrame& _frame)
    :MediaFrame()
{
    if (_frame.AvFrame() == nullptr) {
        return;
    }

    switch (_frame.GetStreamType()) {
    case STREAM_VIDEO:
        pAvFrame_->format = _frame.AvFrame()->format;
        pAvFrame_->width = _frame.AvFrame()->width;
        pAvFrame_->height = _frame.AvFrame()->height;
        av_frame_get_buffer(pAvFrame_, 32);
        break;
    case STREAM_AUDIO:
        pAvFrame_->nb_samples = _frame.AvFrame()->nb_samples;
        pAvFrame_->format = _frame.AvFrame()->format;
        pAvFrame_->channels = _frame.AvFrame()->channels;
        pAvFrame_->channel_layout = _frame.AvFrame()->channel_layout;
        pAvFrame_->sample_rate = _frame.AvFrame()->sample_rate;
        av_frame_get_buffer(pAvFrame_, 0);
        break;
    default:
        return;
    }

    av_frame_copy(pAvFrame_, _frame.AvFrame());
    av_frame_copy_props(pAvFrame_, _frame.AvFrame());

    // attr held by instance
    stream_ = _frame.stream_;
    codec_ = _frame.codec_;
    pts = _frame.pts;
}

StreamType MediaFrame::GetStreamType() const
{
    return stream_;
}

void MediaFrame::SetStreamType(StreamType _type)
{
    stream_ = _type;
}

CodecType MediaFrame::GetCodec() const
{
    return codec_;
}

void MediaFrame::SetCodec(CodecType _type)
{
    codec_ = _type;
}

void MediaFrame::ExtraBuffer(unsigned char* _pBuf)
{
    pExtraBuf_ = _pBuf;
}

void MediaFrame::Print() const
{
    loginfo("frame: pts=%lu, stream=%d, codec=%d, linesize=%lu",
        static_cast<unsigned long>(pAvFrame_->pts), GetStreamType(), GetCodec(), static_cast<unsigned long>(pAvFrame_->linesize[0]));
}
