#ifndef STATISTICS_H
#define STATISTICS_H
#include <vector>
#include <thread>
#include "ThreadCleaner.h"
#include <mutex>

typedef struct MediaStatInfo{
        int64_t totalAudioByte = 0;
        int totalAudioFrameCount = 0;
        int audioFps = 0;
        int audioBitrate = 0;

        int64_t totalVideoByte = 0;
        int totalVideoFrameCount = 0;
        int videoFps = 0;
        int videoBitrate = 0;
        MediaStatInfo();
        MediaStatInfo(const MediaStatInfo & m);
        void Add(const MediaStatInfo & m);
}MediaStatInfo;

class Statistics : public StopClass
{
public:
    Statistics(int videoInterval = 10);
    ~Statistics();
    void StatVideo(int size, bool isIDR);
    void StatAudio(int size);
    const MediaStatInfo & GetStatInfo(){return statInfo_;}
    void Start();
    bool IsStarted(){return isStarted_;}
    void Stop();
    void Reset();
    const char * toString();
private:
    bool quit_ = false;
    int interval_ = 0;
    bool isStarted_ = false;

    MediaStatInfo statInfo_;

    int64_t curVideoStatByte_ = 0;
    int videoFrameCount_ = 0;
    
    std::vector<int> perSecond_;

    int64_t curAudioStatByte_ = 0;
    int audioFrameCount_ = 0;

    std::thread statThread;
    std::mutex mutex_;
    char infoStr[128];
};

#endif // STATISTICS_H
