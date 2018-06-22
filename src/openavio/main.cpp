#include "common.hpp"

#define THIS_FILE "main.cpp"

void getFrameCallback(void * opaque, std::shared_ptr<MediaFrame>& _pFrame)
{
    const char * name = nullptr;
    if (_pFrame->GetStreamType() == STREAM_AUDIO) {
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


    InputParam param;
    param.name_ = "test";
    //param.url_ = "E:\\Videos\\lzc67hd.mp4";
    //param.url_ = "rtmp://live.hkstv.hk.lxdns.com/live/hks";
    //param.url_ = "/Users/liuye/Documents/qml/iceplayer/b.mp4";
    param.url_ = "/Users/liuye/Documents/qml/iceplayer/v.h264";
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
    logger_flush();
        fprintf(stderr, "level:%d\n", logger_get_level());
}
