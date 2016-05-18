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
    AVStream * mp4Stream;
    AVFrame *frame = NULL;
    AVCodecContext *video_enc_ctx = NULL;
    AVPacket pkt;
    unsigned char * frame_buf; 
    int ret, size, y_size;
    int got_frame=0;

    av_register_all();

inityuv();
    //Method1 方法1.组合使用几个函数
    fmt_ctx = avformat_alloc_context();
    //Guess Format 猜格式
    fmt = av_guess_format(NULL, "b.h264", NULL);
    fmt_ctx->oformat = fmt;
    
    //Method 2 方法2.更加自动化一些
    //avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, out_file);
    //fmt = fmt_ctx->oformat;
    
    
    c = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!c){
        printf("Can not find encoder! \n");
        exit(4);
    }
    mp4Stream = avformat_new_stream(fmt_ctx, c);
    if(mp4Stream == NULL){
        printf("avformat_new_stream fail\n");
        exit(3);
    }
    mp4Stream->time_base = (AVRational){1,25};
    mp4Stream->codec->codec_id = AV_CODEC_ID_H264;
    mp4Stream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    mp4Stream->codec->pix_fmt = AV_PIX_FMT_YUV420P;
    mp4Stream->codec->width = 128;
    mp4Stream->codec->height = 128;
    mp4Stream->codec->time_base = (AVRational){1,25};
    mp4Stream->codec->gop_size = 10;
    mp4Stream->codec->max_b_frames = 3;
    mp4Stream->codec->qmin = 15;
    mp4Stream->codec->qmax = 35;
    if(mp4Stream->codec->codec_id == AV_CODEC_ID_H264){
        printf("set priv_data\n");
       av_opt_set(mp4Stream->codec->priv_data, "preset", "slow", 0);
    }
    av_dump_format(fmt_ctx, 0, "b.h264", 1);

    if (avcodec_open2(mp4Stream->codec, c, NULL) < 0){
        printf("Failed to open encoder! \n");
        exit(5);
     }  
	video_enc_ctx = mp4Stream->codec;

    if(avio_open(&fmt_ctx->pb, "b.h264", AVIO_FLAG_READ_WRITE) < 0){
        printf("avio_open my.pm4 fail\n");
        exit(3);
    }
    if(avformat_write_header(fmt_ctx, NULL)<0){
        printf("avformat_write_header fail\n");
        exit(3);
    }

    frame = av_frame_alloc();
    //size = avframe_get_size(video_enc_ctx->pix_fmt, video_enc_ctx->width, video_enc_ctx->height);
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
            pkt.stream_index = mp4Stream->index;
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
