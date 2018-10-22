#ifndef __PACKET_HPP__
#define __PACKET_HPP__

#include "common.hpp"

class FeedFrame;
class MediaPacket
{
public:
    MediaPacket(IN const AVStream& pAvStream, IN const AVPacket* pAvPacket);
    ~MediaPacket();
    MediaPacket();
    MediaPacket(IN FeedFrame & feedFrame);
    MediaPacket(const MediaPacket&) = delete; // no copy for risk concern

    // get raw AV structs
    AVPacket* AvPacket() const;
    AVCodecParameters* AvCodecParameters() const;

    // pts and dts
    uint64_t Pts() const;
    void Pts(IN uint64_t);
    uint64_t Dts() const;
    void Dts(IN uint64_t);

    // stream and codec
    enum AVMediaType GetStreamType() const;
    void SetStreamType(IN enum AVMediaType);
    enum AVCodecID GetCodec() const;
    void SetCodec(IN enum AVCodecID);

    // data fields
    char* Data()const;
    int Size() const;

    // util
    void Print() const;
    void Dump(const std::string& title = "") const;

    // video
    int Width() const;
    int Height() const;
    void Width(int);
    void Height(int);
    bool IsKey() const;
    void SetKey();

    // audio
    int SampleRate() const;
    int Channels() const;
    void SampleRate(int);
    void Channels(int);

private:
    AVPacket* pAvPacket_ = nullptr;
    AVCodecParameters* pAvCodecPar_ = nullptr;

    // save following fields seperately
    enum AVMediaType stream_;
    enum AVCodecID codec_;

    // video specific
    int nWidth_ = -1, nHeight_ = -1;
    int nSampleRate_ = -1, nChannels_ = -1;
};

class MediaFrame
{
public:
    MediaFrame(IN const AVFrame* pFrame);
    MediaFrame();
    MediaFrame(const MediaFrame&);
    ~MediaFrame();
    inline AVFrame* AvFrame() const { return pAvFrame_; }

    enum AVMediaType GetStreamType() const;
    void SetStreamType(IN enum AVMediaType);
    enum AVCodecID GetCodec() const;
    void SetCodec(IN enum AVCodecID);

    void ExtraBuffer(unsigned char* pBuf); // TODO: delete, will use AudioResampler instead

    void Print() const;

    int64_t pts; //MediaFrame(const MediaFrame&);去这个构造函数添加pts的赋值
    AVFrame* pAvFrame_;

private:


    enum AVMediaType stream_;
    enum AVCodecID codec_;

    unsigned char* pExtraBuf_ = nullptr;
};

#endif
