//extern "C" {#include <libavformat/avformat.h>}
#include <libavformat/avformat.h>
#include <assert.h>
#include <stdio.h>

static const char* SDP_DATA = "SDP:\n"
"v=0\n"
"o=- 0 0 IN IP4 127.0.0.1\n"
"s=No Name\n"
"c=IN IP4 127.0.0.1\n"
"t=0 0\n"
"a=tool:libavformat 57.83.100\n"
"m=video 4320 RTP/AVP 33\n"
"b=AS:356\n"
"a=rtpmap:33 MP2T/90000";


struct SdpOpaque
{
    int cap;
    int len;
    int cur;
    uint8_t * data;
};
char errbuf[512] = { 0 };

int sdp_read(void *opaque, uint8_t *buf, int size)
{
    assert(opaque);
    assert(buf);
    struct SdpOpaque * octx = (struct SdpOpaque*)(opaque);
printf("read ...%d %d\n", octx->cur, size);
    if (octx->cur == octx->len) {
        return 0;
    }

    int readN = size <= octx->len - octx->cur ? size : octx->len - octx->cur;

    if (readN > 0)
        memcpy(buf, octx->data + octx->cur, readN);
    octx->cur += readN;
    return readN;
}

int sdp_open(AVFormatContext **pctx, const char *data, AVDictionary **options)
{
    assert(pctx);
    *pctx = avformat_alloc_context();
    assert(*pctx);

    int avioBufferSize = 4096;
    void * avioBuffer = av_malloc(avioBufferSize);
    int reqLen = sizeof(struct SdpOpaque) + strlen(data) + 1;
    struct SdpOpaque * opaque = malloc(reqLen);
    memset(opaque, 0, reqLen);


    opaque->data = ((uint8_t *)opaque) + sizeof(struct SdpOpaque);
    opaque->cap = reqLen - sizeof(struct SdpOpaque);
    opaque->len = strlen(data);
    opaque->cur = 0;
    memcpy(opaque->data, data, opaque->len);
    
    int sdpLen = sdp_read(opaque, avioBuffer, avioBufferSize);
    AVIOContext * pbctx = avio_alloc_context(avioBuffer, sdpLen, 0, opaque, NULL, NULL, NULL);

    //AVIOContext * pbctx = avio_alloc_context(avioBuffer, avioBufferSize, 0, opaque, sdp_read, NULL, NULL);
    assert(pbctx);

    (*pctx)->pb = pbctx;
    AVInputFormat * infmt = av_find_input_format("sdp");

    return avformat_open_input(pctx, "memory.sdp", infmt, options);
}

void sdp_close(AVFormatContext **fctx)
{
    assert(fctx);
    AVFormatContext *ctx = *fctx;

    // Opaque can be non-POD type, free it before and assign to null
    struct SdpOpaque* opaque = (struct SdpOpaque*)(ctx->pb->opaque);
    free(opaque);
    ctx->pb->opaque = NULL;

    //avio_close(ctx->pb); //avio_open了才需要这个？
    avformat_close_input(fctx);
}

int main()
{
    int ret;
    av_register_all(); //ffmpeg4.0 就注释掉
    avformat_network_init();

    AVFormatContext *sdpctx = NULL;
    AVDictionary * options = NULL;
    av_dict_set(&options, "protocol_whitelist", "file,rtp,udp,tcp,tls", 0);
    //ret = sdp_open(&sdpctx, SDP_DATA, &options);
    ret = avformat_open_input(&sdpctx, "/Users/liuye/Downloads/ffmpeg-3.4.2-macos64-shared/bin/test.sdp", NULL, &options);
    if (ret != 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        printf("sdpopen fail:%s\n", errbuf);
        return ret;
    }

    av_dump_format(sdpctx, 0, "memory.sdp", 0);

#if 1
    if ((ret = avformat_find_stream_info(sdpctx, 0)) < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        printf("Failed to retrieve input stream information：%s\n", errbuf);
        goto end;
    }
#endif
    

    for (size_t i = 0; i < sdpctx->nb_streams; ++i) {
        if (sdpctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            printf("find audio\n");
            continue;
        }
        if (sdpctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            printf("find video\n");
            continue;
        }
        printf("other type:%d\n", sdpctx->streams[i]->codecpar->codec_type);
    }
    AVPacket pkt;
    av_init_packet(&pkt);
    while(av_read_frame(sdpctx, &pkt) >= 0){
        
        fprintf(stderr, "receive one packet: %d\n", pkt.stream_index);
        av_packet_unref(&pkt);
    }

end:
    sdp_close(&sdpctx);
    //getchar();
    return 0;
}
