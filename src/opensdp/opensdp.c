//extern "C" {#include <libavformat/avformat.h>}
#include <libavformat/avformat.h>
#include <assert.h>

static const char* SDP_DATA = "SDP:\r"
"v=0\r"
"o=- 0 0 IN IP4 127.0.0.1\r"
"s=No Name\r"
"c=IN IP4 127.0.0.1\r"
"t=0 0\r"
"a=tool:libavformat 57.83.100\r"
"m=video 4320 RTP/AVP 33\r"
"b=AS:356\r"
"a=rtpmap:33 MP2T/90000";


struct SdpOpaque
{
    int cap;
    int len;
    int cur;
    uint8_t * data;
};

int sdp_read(void *opaque, uint8_t *buf, int size)
{
    assert(opaque);
    assert(buf);
    struct SdpOpaque * octx = (struct SdpOpaque*)(opaque);

    if (octx->cur == octx->len) {
        return 0;
    }

    int readN = size <= octx->len - octx->cur ? size : octx->len - octx->cur;

    if (readN > 0)
        memcpy(buf, octx->data + octx->cur, readN);

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


    opaque->data = ((char *)opaque) + sizeof(struct SdpOpaque);
    opaque->cap = reqLen - sizeof(struct SdpOpaque);
    opaque->len = strlen(data);
    opaque->cur = 0;
    memcpy(opaque->data, data, opaque->len);

    AVIOContext * pbctx = avio_alloc_context(avioBuffer, avioBufferSize, 0, opaque, sdp_read, NULL, NULL);
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

    avio_close(ctx->pb);
    avformat_close_input(fctx);
}

int main()
{
    int ret;
    avformat_network_init();

    AVFormatContext *sdpctx = NULL;
    ret = sdp_open(&sdpctx, SDP_DATA, NULL);
    if (ret != 0) {
        char err[512] = { 0 };
        av_strerror(ret, err, sizeof(err));
        printf("sdpopen fail:%s\n", err);
        return ret;
    }

    av_dump_format(sdpctx, 0, "memory.sdp", 0);

    if ((ret = avformat_find_stream_info(sdpctx, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        goto end;
    }
    

    for (size_t i = 0; i < sdpctx->nb_streams; ++i) {
        if (sdpctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            printf("find audio\n");
        }
        if (sdpctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            printf("find video\n");
        }
    }

end:
    sdp_close(&sdpctx);
    getchar();
    return 0;
}
