#include "libavutil/samplefmt.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"
#include "libavutil/parseutils.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
/*
 * 使用了新版本的接口(3.2)
 * 编解码只需要AVCodecContext AVCodec就行了
 * 需要封装的话就需要AVFormatContext AVStream了
 */

FILE * vfile;
void initpcm(){
    vfile = fopen("pcm16.pcm", "rb");
    if(vfile == NULL){
        fprintf(stderr, "fopen fail\n");
        exit(1);
    }
}

int myavcodec_encode_audio2(AVCodecContext *dec_ctx,AVFrame * frame, int *got_pkt, AVPacket * pkt){
    int rv;
    if (frame == NULL){
	    fprintf(stderr, "flush\n");
    }
    rv = avcodec_send_frame(dec_ctx, frame);
    if (rv != 0){
	printf("avcodec_send_packet fail:%d\n", rv);
	exit(1);
    }
    rv = avcodec_receive_packet(dec_ctx, pkt);
    if (rv == 0){
	*got_pkt = 1;
	return 0;
    }else if(rv == AVERROR(EAGAIN)){
        *got_pkt = 0;
        return 0;
    }
    return rv;
}

AVCodecContext * get_audio_enc_context(AVCodec *c){
    AVCodecContext *enc_ctx;
    //3.2的结构一定要这样分配codecContext
    enc_ctx =  avcodec_alloc_context3(c);
    if(enc_ctx == NULL){
        fprintf(stderr,"avcodec_alloc_context3 fail\n");
	exit(10);
    }

    enc_ctx->codec_id = AV_CODEC_ID_AAC;
    enc_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    enc_ctx->sample_fmt= AV_SAMPLE_FMT_S16;
    enc_ctx->sample_rate= 48000;
    enc_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
    enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
    enc_ctx->bit_rate = 64000;
    enc_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
 
//指定time_base 至少对音频来说没用
//    audio_enc_ctx->time_base.den = enc_ctx->sample_rate;
//    audio_enc_ctx->time_base.num = 1;
    if (avcodec_open2(enc_ctx, c, NULL) < 0){
        printf("Failed to open encoder! \n");
        exit(11);
    }  
    return enc_ctx;
}
AVFormatContext *get_fmt_ctx(AVCodecContext *enc_ctx, int * index, char * outfile){
    AVFormatContext *fmt_ctx = NULL;
    AVStream        *astream;
    AVOutputFormat  *fmt;
    int ret;

    //因为输出是aac的文件，属于封装了,所以需要AVFormatContext
    //Method1 方法1.组合使用几个函数
    fmt_ctx = avformat_alloc_context();
    if(fmt_ctx == NULL){
        fprintf(stderr,"avformat_alloc_context fail\n");
	exit(20);
    }
    fmt = av_guess_format(NULL, outfile, NULL); //Guess Format 猜格式

    //Method 2 方法2.更加自动化一些
    //avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, argv[1]);
    //fmt = fmt_ctx->oformat;
    if(fmt == NULL){
	printf("av_guess_format fail\n");
	exit(21);
    }
    fmt_ctx->oformat = fmt;

    astream = avformat_new_stream(fmt_ctx, enc_ctx->codec);
    if(astream == NULL){
        printf("avformat_new_stream fail\n");
        exit(22);
    }
    ret = avcodec_parameters_from_context(astream->codecpar, enc_ctx);
    if (ret < 0) {
        fprintf(stderr, "Could not initialize stream parameters\n");
	exit(23);
    } 
//  astream->time_base.den = 48000;
//  astream->time_base.num = 1;
    if(avio_open(&fmt_ctx->pb, outfile, AVIO_FLAG_READ_WRITE) < 0){
        printf("avio_open my.pm4 fail\n");
        exit(24);
    }

    if(avformat_write_header(fmt_ctx, NULL)<0){
        printf("avformat_write_header fail\n");
        exit(25);
    }
    *index = astream->index;
    return fmt_ctx;
}

//http://blog.csdn.net/leixiaohua1020/article/details/25430449
int main(int argc, char **argv){
    
    AVFormatContext *fmt_ctx = NULL;

    AVCodecContext  *audio_enc_ctx = NULL;
    AVCodec         *c;

    AVFrame         *frame = NULL;
    AVPacket pkt;

    int ret, size, audio_stream_index;
    int got_pkt=0;

    if(argc != 2){
        fprintf(stderr, "usage as:%s filename(v.aac)\n", argv[0]);
        exit(1);
    }
    initpcm();
    av_register_all();

    //start========================
    //编解码
    c =  avcodec_find_encoder_by_name("libfdk_aac");
    if (!c){
        printf("Can not find encoder! \n");
        exit(3);
    }
    // @TODO 测试输入大小不规则的pcm到编码起看下能否工作
    c->capabilities |= AV_CODEC_CAP_VARIABLE_FRAME_SIZE;
    audio_enc_ctx = get_audio_enc_context(c);
    //end========================
    
    fmt_ctx = get_fmt_ctx(audio_enc_ctx, &audio_stream_index, argv[1]); //输出文件

    //dump看到的信息可能是 Stream #0:0: Unknown: none  但是编码确实正确的,诡异的问题
    av_dump_format(fmt_ctx, 0, argv[1], 1);


    size = av_samples_get_buffer_size(NULL, audio_enc_ctx->channels, audio_enc_ctx->frame_size,
                                          audio_enc_ctx->sample_fmt, 1);
    //frame->size 指的单声道的sample数， 比如16bit 双声道，是1024
    //av_samples_get_buffer_size 指的是一个frame的大小，是4096
    printf("audio frame_size=%d size=%d\n", audio_enc_ctx->frame_size, size);

    char buffer[102400] = {0};
    unsigned int pts = 0;
    int doneflag = 0;

    while (1) {
        int reads = fread(buffer, 1, size, vfile);
        if (reads != size) {
                printf("Read done\n");
	        doneflag = 1;
	        frame = NULL;
	        goto flush;
        }
	//如果是大端结构这里就要转为小端
	/*
        int i = 0;
        for (i = 0; i + 1 < size; i += 2) {
                char tmp = buffer[i];
                buffer[i] =  tmp;
                buffer[i+1] = buffer[i];
        }
	*/
        got_pkt=0;

	frame = av_frame_alloc();
        frame->nb_samples = audio_enc_ctx->frame_size;
        frame->format = audio_enc_ctx->sample_fmt;
        frame->data[0] = (uint8_t *)buffer;
        frame->linesize[0] = size;
	frame->pts = pts; // 指定time_base好像没有用，一定要在frame->pts这里指定
        //Encode 编码
flush:
	//和avcodec_encode_audio2 deprecated的调用参数一样
        ret = myavcodec_encode_audio2(audio_enc_ctx, frame, &got_pkt, &pkt);
        if(ret < 0){
            printf("Failed to encode! 编码错误！\n");
            return -1;
        }
        if (got_pkt==1){
            printf("Succeed to encode 1 frame! 编码成功1帧, pts:%ld\n", pkt.pts);
	    pts+=1024*1000/48000; // frame_size / sample_rate ，但是有精度损失，这里要补救一下@TODO
	    
            //pkt.stream_index = audio_stream_index; //astream->index; //有音频和视频的时候需要指定
            ret = av_write_frame(fmt_ctx, &pkt);

            // av_free_packet(&pkt);
            av_packet_unref(&pkt);
	    if(doneflag){
		    break;
	    }
        }
	if(doneflag)
	   break;
	av_frame_free(&frame);
    }

    if(!feof(vfile)){
        printf("fread error:%s\n", strerror(errno));
        exit(1);
    }

    av_write_trailer(fmt_ctx);
    avcodec_close(audio_enc_ctx);
    avformat_close_input(&fmt_ctx);
    if (vfile)
        fclose(vfile);
    av_packet_unref(&pkt);
    //av_frame_unref;
}
