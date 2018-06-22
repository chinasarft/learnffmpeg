#include "common.hpp"

#define THIS_FILE "main.cpp"
#define RTP_TEST
void getFrameCallback(void * opaque, std::shared_ptr<MediaFrame>& _pFrame)
{
    const char * name = nullptr;
    if (_pFrame->GetStreamType() == AVMEDIA_TYPE_AUDIO) {
        name = "audio";
    }
    else {
        name = "video";
    }
    loginfo("{} framepts:{}", name, _pFrame->pts);
}

int main(int argc, char **argv) {

    logger_init_file_output("log.log");
    logger_set_level_debug();
    //logger_set_level_info();
    fprintf(stderr, "level:%d\n", logger_get_level());

#ifndef RTP_TEST
    InputParam param;
    param.name_ = "test";
    //param.url_ = "E:\\Videos\\lzc67hd.mp4";
    //param.url_ = "rtmp://live.hkstv.hk.lxdns.com/live/hks";
    //param.url_ = "/Users/liuye/Documents/qml/iceplayer/b.mp4";
    param.url_ = "/Users/liuye/Documents/qml/iceplayer/v.h264";
        param.formatHint_ = "h264";
    //param.url_ = "/Users/liuye/Documents/p2p/build/src/mysiprtp/Debug/hks.h264";
#if 0
    param.url_ = "/Users/liuye/Documents/qml/iceplayer/a.mulaw";
    param.audioOpts.push_back("ar");
    param.audioOpts.push_back("8000");
    param.formatHint_ = "mulaw";
#endif
        param.userData_ = nullptr;
        param.getFrameCb_ = getFrameCallback;
        
        Input input(param);
        //input.just_for_test_heap();
        input.Start();
        input.WaitStop();
        
#else
        InputParam param1;
        param1.name_ = "video";
        param1.url_ = "/Users/liuye/Documents/qml/iceplayer/v.h264";
        param1.formatHint_ = "h264";
        param1.getFrameCb_ = getFrameCallback;
        //param1.feedDataCb_ = videoFeed;
        //param1.feedCbOpaqueArg_ = this;
        //param1.userData_ = this;
        
        InputParam param2;
        param2.name_ = "audio";
        param2.url_ = "/Users/liuye/Documents/qml/iceplayer/a.mulaw";
        param2.formatHint_ = "mulaw";
        param2.getFrameCb_ = getFrameCallback;
        param2.audioOpts.push_back("ar");
        param2.audioOpts.push_back("8000");
        //param2.feedDataCb_ = audioFeed;
        //param2.userData_ = this;
        //param2.feedCbOpaqueArg_ = this;

        
        Input input1(param1);
        Input input2(param1);
        
        input1.Start();
        input2.Start();
        
        input1.WaitStop();
        input2.WaitStop();
#endif

    logger_flush();
    fprintf(stderr, "level:%d\n", logger_get_level());
}
