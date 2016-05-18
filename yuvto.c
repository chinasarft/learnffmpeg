#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavutil/parseutils.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>

FILE * vfile;
void inityuv(){
    vfile = fopen("v.yuv", "rb");
    if(vfile == NULL){
        fprintf(stderr, "fopen fail\n");
        exit(1);
    }
}


//http://blog.csdn.net/leixiaohua1020/article/details/39770947
int main(int argc, char **argv){
    
    AVFormatContext *fmt_ctx = NULL;
    AVCodec * c;
    AVOutputFormat* fmt;
    AVStream * vstream;
    AVFrame *frame = NULL;
    AVCodecContext *video_enc_ctx = NULL;
    AVPacket pkt;
    unsigned char * frame_buf; 
    int ret, size, y_size;
    int got_frame=0;

    if(argc != 2){
        fprintf(stderr, "usage as:%s filename\n", argv[0]);
        exit(1);
    }
    av_register_all();

inityuv();
    //Method1 方法1.组合使用几个函数
    fmt_ctx = avformat_alloc_context();
    //Guess Format 猜格式
    fmt = av_guess_format(NULL, "argv[1]", NULL);
    fmt_ctx->oformat = fmt;
    
    //Method 2 方法2.更加自动化一些
    //avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, "argv[1]");
    //fmt = fmt_ctx->oformat;
    
    
    c = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!c){
        printf("Can not find encoder! \n");
        exit(4);
    }
    vstream = avformat_new_stream(fmt_ctx, c);
    if(vstream == NULL){
        printf("avformat_new_stream fail\n");
        exit(3);
    }
    vstream->time_base = (AVRational){1,25};
    vstream->codec->codec_id = AV_CODEC_ID_H264;
    vstream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    vstream->codec->pix_fmt = AV_PIX_FMT_YUV420P;
    vstream->codec->width = 128;
    vstream->codec->height = 128;
    vstream->codec->time_base = (AVRational){1,25};
    vstream->codec->gop_size = 10;
    vstream->codec->max_b_frames = 3;
    vstream->codec->qmin = 15;
    vstream->codec->qmax = 35;
    if(vstream->codec->codec_id == AV_CODEC_ID_H264){
        printf("set priv_data\n");
       av_opt_set(vstream->codec->priv_data, "preset", "slow", 0);
    }
    av_dump_format(fmt_ctx, 0, "argv[1]", 1);

    if (avcodec_open2(vstream->codec, c, NULL) < 0){
        printf("Failed to open encoder! \n");
        exit(5);
     }  
    video_enc_ctx = vstream->codec;

    if(avio_open(&fmt_ctx->pb, "argv[1]", AVIO_FLAG_READ_WRITE) < 0){
        printf("avio_open my.pm4 fail\n");
        exit(3);
    }
    if(avformat_write_header(fmt_ctx, NULL)<0){
        printf("avformat_write_header fail\n");
        exit(3);
    }

    frame = av_frame_alloc();
    //avframe_get_size 这个函数已经不存在了
    size = av_image_get_buffer_size(video_enc_ctx->pix_fmt, video_enc_ctx->width, video_enc_ctx->height, 1);
    frame_buf = (uint8_t *)av_malloc(size);
    //avframe_fill avpicture_fill 这两个函数都废弃了
    av_image_fill_arrays(frame->data, frame->linesize, frame_buf, video_enc_ctx->pix_fmt, video_enc_ctx->width, video_enc_ctx->height, 1);
    
    y_size = video_enc_ctx->width * video_enc_ctx->height;

    //av_new_packet(&pkt,y_size*3);
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    int i = 0;
    //yuv file还是要自己去读取，没有什么av_read_frame之类的函数
    while(fread(frame_buf, 1, y_size * 3 / 2, vfile)>0){
        frame->data[0] = frame_buf;  // 亮度Y
        frame->data[1] = frame_buf+ y_size;  // U 
        frame->data[2] = frame_buf+ y_size*5/4; // V
        //PTS
        frame->pts=i++;
        got_frame=0;
        //Encode 编码
        ret = avcodec_encode_video2(video_enc_ctx, &pkt,frame, &got_frame);
        if(ret < 0){
            printf("Failed to encode! 编码错误！\n");
            return -1;
        }
        if (got_frame==1){
            printf("Succeed to encode 1 frame! 编码成功1帧！\n");
            pkt.stream_index = vstream->index;
            ret = av_write_frame(fmt_ctx, &pkt);
            // av_free_packet(&pkt);
            av_packet_unref(&pkt);
        }

    }
    if(!feof(vfile)){
        printf("fread error:%s\n", strerror(errno));
        exit(1);
    }

    ret = avcodec_encode_video2(video_enc_ctx, &pkt, NULL, &got_frame); //frame is null to flush
    av_write_trailer(fmt_ctx);


    avcodec_close(video_enc_ctx);
    avformat_close_input(&fmt_ctx);
    if (vfile)
        fclose(vfile);
    av_frame_free(&frame);
    av_packet_unref(&pkt); //h264toyuv.c  也需要，还没加上去
}
