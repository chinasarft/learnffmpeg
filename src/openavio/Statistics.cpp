#include "Statistics.h"
#include "common.hpp"
#include <sstream>

MediaStatInfo::MediaStatInfo()
{
        totalAudioByte = 0;
        totalAudioFrameCount = 0;
        audioFps = 0;
        audioBitrate = 0;
        
        totalVideoByte = 0;
        totalVideoFrameCount = 0;
        videoFps = 0;
        videoBitrate = 0;
}

MediaStatInfo::MediaStatInfo(const MediaStatInfo & m)
{
        totalAudioByte = m.totalAudioByte;
        totalAudioFrameCount = m.totalVideoFrameCount;
        audioFps = m.audioFps;
        audioBitrate = m.audioBitrate;
        
        totalVideoByte = m.totalVideoByte;
        totalVideoFrameCount = m.totalVideoFrameCount;
        videoFps = m.videoFps;
        videoBitrate = m.videoBitrate;
}

void MediaStatInfo::Add(const MediaStatInfo & m)
{
        totalAudioByte += m.totalAudioByte;
        totalAudioFrameCount += m.totalVideoFrameCount;
        audioFps += m.audioFps;
        audioBitrate += m.audioBitrate;
        
        totalVideoByte += m.totalVideoByte;
        totalVideoFrameCount += m.totalVideoFrameCount;
        videoFps += m.videoFps;
        videoBitrate += m.videoBitrate;
}

Statistics::Statistics(int interval) :
    interval_(interval)
{
    perSecond_.resize(interval_);
    for (int i = 0; i < interval_; i++) {
        perSecond_[i] = 0;
    }
}

Statistics::~Statistics()
{
    fprintf(stderr, "~Statistics\n");
}

void Statistics::Start()
{
    if (isStarted_)
        return;
    auto w = [this]() {
        int vCount = 0; //每一次循环加1
        while(!quit_) {
            os_sleep_ms(1000);
            std::lock_guard<std::mutex> lock(mutex_);

            perSecond_[vCount % interval_] = curVideoStatByte_;
            vCount++;

            int base = vCount < interval_ ? vCount : interval_;
            int tmp = 0;
            for(int i = 0; i < interval_; i++) {
                tmp += perSecond_[i];
            }
            statInfo_.videoBitrate = tmp / base;

            statInfo_.videoFps = videoFrameCount_;
            statInfo_.audioFps = audioFrameCount_;
            statInfo_.audioBitrate = curAudioStatByte_;

            curVideoStatByte_ = 0;
            curAudioStatByte_ = 0;
            videoFrameCount_ = 0;
            audioFrameCount_ = 0;
        }
    };
    statThread = std::thread(w);
    isStarted_ = true;
}

void Statistics::Reset()
{
        std::lock_guard<std::mutex> lock(mutex_);
        MediaStatInfo newInfo;
        statInfo_ = newInfo;
        curVideoStatByte_ = 0;
        videoFrameCount_ = 0;
        curAudioStatByte_ = 0;
        audioFrameCount_ = 0;

        for (int i = 0; i < interval_; i++) {
                perSecond_[i] = 0;
        }
}

void Statistics::Stop()
{
    quit_ = true;
    if (statThread.joinable()) {
        statThread.join();
    }
}

void Statistics::StatVideo(int size, bool isIDR)
{
    std::lock_guard<std::mutex> lock(mutex_);
    statInfo_.totalVideoByte += size;
    statInfo_.totalVideoFrameCount++;
    curVideoStatByte_ += size;
    videoFrameCount_++;
}

void Statistics::StatAudio(int size)
{
    std::lock_guard<std::mutex> lock(mutex_);
    statInfo_.totalAudioByte += size;
    statInfo_.totalAudioFrameCount++;
    curAudioStatByte_ += size;
    audioFrameCount_++;
}

const char * Statistics::toString()
{
        memset(infoStr, 0, sizeof(infoStr));
        sprintf(infoStr, "vFps:%d vBr:%d kbps vCount:%d | aFps:%d aBr:%d aCount:%d",
                statInfo_.videoFps, statInfo_.videoBitrate * 8 / 1000, statInfo_.totalVideoFrameCount,
                statInfo_.audioFps, statInfo_.audioBitrate * 8 / 1000, statInfo_.totalAudioFrameCount);

    return (const char *)infoStr;
}
