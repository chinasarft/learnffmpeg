#include "libavutil/imgutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"
#include "libavutil/parseutils.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"

#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"



const char* filename = "in.flv";
const char* filename1 = "in1.flv";
//const char* filters_descr = "[in]scale=1280:720[scale];movie=1.jpg[wm];[scale][wm]overlay=0:0[out]";
//const char* filters_descr = "[in]scale=1280:720[scale];movie=iperf.png[wm];[scale][wm]overlay=0:0[out]";
const char* filters_descr = "[in1]scale=160:90[small];[in][small]overlay=0:0[out]";
int main()
{
    FILE *fp_yuv=fopen("test.yuv","wb+");
    if(!fp_yuv)
        return 1;
    int ret = 0;
    av_register_all();
    avfilter_register_all();

    //第一个视频
    AVFormatContext *pFormat = NULL;
    avformat_open_input(&pFormat,filename,NULL,NULL);
    if(!pFormat)
        return 2;
    if (avformat_find_stream_info(pFormat, NULL) < 0) {
        printf("Could not find stream information\n");
    }
    av_dump_format(pFormat, 0, filename, 0);
    AVCodecContext *video_dec_ctx = pFormat->streams[0]->codec;
    AVStream *video_st = pFormat->streams[0];
    AVCodec *video_dec = avcodec_find_decoder(video_dec_ctx->codec_id);
    avcodec_open2(video_dec_ctx, video_dec,NULL);
    if(!video_dec_ctx || !video_dec || !video_st)
        return 3;


    //第二个视频文件
    AVFormatContext *pFormat1 = NULL;
    avformat_open_input(&pFormat1,filename1,NULL,NULL);
    if(!pFormat1)
        return 2;
    if (avformat_find_stream_info(pFormat1, NULL) < 0) {
        printf("Could not find stream information\n");
    }
    av_dump_format(pFormat1, 0, filename1, 0);
    AVCodecContext *video_dec_ctx1 = pFormat1->streams[0]->codec;
    AVStream *video_st1 = pFormat1->streams[0];
    AVCodec *video_dec1 = avcodec_find_decoder(video_dec_ctx1->codec_id);
    avcodec_open2(video_dec_ctx1, video_dec1,NULL);
    if(!video_dec_ctx1 || !video_dec1 || !video_st1)
        return 3;




    //大视频
    uint8_t *video_dst_data[4] = {NULL};
    int video_dst_linesize[4];
    int video_dst_bufsize;
    ret = av_image_alloc(video_dst_data, video_dst_linesize,
        640, 360,
        video_dec_ctx->pix_fmt, 1);
    video_dst_bufsize = ret;


    AVPacket *pkt=(AVPacket *)malloc(sizeof(AVPacket)); 
    av_init_packet(pkt);
    AVPacket *pkt1=(AVPacket *)malloc(sizeof(AVPacket)); 
    av_init_packet(pkt1);
    
    //filter
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
    AVFilterGraph *filter_graph;
    AVFilter *buffersrc = avfilter_get_by_name("buffer");
    AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();

    AVFilterContext *buffersrc_ctx1;
    AVFilter *buffersrc1 = avfilter_get_by_name("buffer");
    AVFilterInOut *outputs1 = avfilter_inout_alloc();


    //alloc filter graph
    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        return 4;
    }
    char args[512] = {0};
    snprintf(args, sizeof(args),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
        video_dec_ctx->width, video_dec_ctx->height, video_dec_ctx->pix_fmt,
        video_dec_ctx->time_base.num, video_dec_ctx->time_base.den,
        video_dec_ctx->sample_aspect_ratio.num, video_dec_ctx->sample_aspect_ratio.den);
    //用buffersrc初始化buffersrc_ctx,并加入filter_graph
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
        args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        return 5;
    }

    memset(args, 0, sizeof(args));
    snprintf(args, sizeof(args),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
        video_dec_ctx1->width, video_dec_ctx1->height, video_dec_ctx1->pix_fmt,
        video_dec_ctx1->time_base.num, video_dec_ctx1->time_base.den,
        video_dec_ctx1->sample_aspect_ratio.num, video_dec_ctx1->sample_aspect_ratio.den);
    ret = avfilter_graph_create_filter(&buffersrc_ctx1, buffersrc1, "in1",
        args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        return 5;
    }

    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
        NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        return 6;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
        AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        return 7;
    }
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = outputs1;
    outputs1->name = av_strdup("in1");
    outputs1->filter_ctx = buffersrc_ctx1;
    outputs1->pad_idx = 0;
    outputs1->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
        &inputs, &outputs, NULL)) < 0)
        return 8;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        return 10;

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    fprintf(stderr, "--------------------\n");
    char * strgraph = avfilter_graph_dump(filter_graph, NULL);
    if (strgraph != NULL){
        fprintf(stderr, "%s\n", strgraph);
    }
    fprintf(stderr, "--------------------\n");


    int gotpicture = 0, inend = 0;
    int gotpicture1 = 0, in1end = 0;

    while(1)
    {
        if(inend && in1end){
            break;
        }
        ret = av_read_frame(pFormat,pkt);
        if (ret == 0){
            if(pkt->stream_index == 0)
            {
                AVFrame* pFrame = av_frame_alloc();
                ret = avcodec_decode_video2(video_dec_ctx,pFrame,&gotpicture,pkt);
                if(ret<0)
                    return 11;
                if(gotpicture)
                {    
                    if (av_buffersrc_add_frame_flags(buffersrc_ctx, pFrame, AV_BUFFERSRC_FLAG_PUSH) < 0) {
                        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                        break;
                    }
                    printf("add to buffersrc_ctx\n");
                }
                av_frame_free(&pFrame);
            }
        }else{
                inend = 1;
        }
        av_packet_unref(pkt);

        ret = av_read_frame(pFormat1,pkt1);
        if(ret == 0){
            if(pkt1->stream_index == 0)
            {
                AVFrame* pFrame1 = av_frame_alloc();
                ret = avcodec_decode_video2(video_dec_ctx1,pFrame1,&gotpicture1,pkt1);
                if(ret<0)
                    return 11;
                if(gotpicture1)
                {    
                    if (av_buffersrc_add_frame_flags(buffersrc_ctx1, pFrame1, AV_BUFFERSRC_FLAG_PUSH) < 0) {
                        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                        break;
                    }
                    printf("add to buffersrc_ctx1\n");
                }
                av_frame_free(&pFrame1);
            }
        }else{
                in1end=1;
        }
        av_packet_unref(pkt1);

        /* pull filtered frames from the filtergraph */
        while (1) {
            AVFrame* filt_frame = av_frame_alloc();
            ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
            if (ret == AVERROR(EAGAIN)){
                printf("buffersink_ctx again\n");
                break;
            } else if ( ret == AVERROR_EOF || ret == AVERROR(EIO)){
                    return 33;
            }
            if (ret < 0)
                return 12;
        printf("write video\n");
            //int y_size=filt_frame->width*filt_frame->height;
            av_image_copy(video_dst_data, video_dst_linesize,
                (const uint8_t **)(filt_frame->data), filt_frame->linesize,
                AV_PIX_FMT_YUV420P, filt_frame->width, filt_frame->height);
            fwrite(video_dst_data[0], 1, video_dst_bufsize, fp_yuv);
            av_frame_free(&filt_frame);
        }
    }
    return 0;
    
}
