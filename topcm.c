#include "libavutil/samplefmt.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"
#include "libavutil/parseutils.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
int myavcodec_decode_audio4(AVCodecContext *dec_ctx,AVFrame * frame, int *got_pkt, AVPacket * pkt){
    int rv;
    if (frame == NULL){
	    fprintf(stderr, "flush\n");
    }
    rv = avcodec_send_packet(dec_ctx, pkt);
    if (rv != 0){
	printf("avcodec_send_packet fail:%d\n", rv);
	exit(1);
    }
    rv = avcodec_receive_frame(dec_ctx, frame);
    if (rv == 0){
	*got_pkt = 1;
	return 0;
    }else if(rv == AVERROR(EAGAIN)){
        *got_pkt = 0;
        return 0;
    }
    return rv;
}
  
int main(int argc, char* argv[])  
{  
    AVFormatContext *fmt_ctx;  
    AVStream *stream;  
    AVCodecContext *codec_ctx;  
    AVCodec *codec;  
    AVPacket pkt;  
    AVFrame *frame = NULL;  
    int ret, stream_index, got_frame;  
    FILE *infile = NULL, *outfile = NULL;  
      
    const char *infilename = "aac.aac";  
    const char *outfilename = "aac2pcm.pcm";  
  
    av_register_all();  
    avformat_network_init();  
  
    fmt_ctx = avformat_alloc_context();  
    if ((ret = avformat_open_input(&fmt_ctx, infilename, NULL, NULL)) < 0){  
        printf("Couldn't open input stream.\n");  
        return ret;  
    }  
    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0){  
        printf("Couldn't find stream information.\n");  
        return ret;  
    }  
    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);  
    if (ret < 0) {  
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",  
            av_get_media_type_string(AVMEDIA_TYPE_AUDIO), infilename);  
        return ret;  
    }  

    else {  
        stream_index = ret;  
        stream = fmt_ctx->streams[stream_index];  
  
        /* find decoder for the stream */  
	/*
	 * @deprecated
        codec_ctx = stream->codec;  
       codec = avcodec_find_decoder(codec_ctx->codec_id);  
       */
       codec =  avcodec_find_decoder_by_name("libfdk_aac");
       codec_ctx =  avcodec_alloc_context3(codec);
       avcodec_parameters_to_context(codec_ctx, stream->codecpar);

        if (!codec) {  
            fprintf(stderr, "Failed to find %s codec\n",  
                av_get_media_type_string(AVMEDIA_TYPE_AUDIO));  
            goto end;  
            return AVERROR(EINVAL);  
        }  
  
        if ((ret = avcodec_open2(codec_ctx, codec, NULL)) < 0) {  
            fprintf(stderr, "Failed to open %s codec\n",  
                av_get_media_type_string(AVMEDIA_TYPE_AUDIO));  
            goto end;  
            return ret;  
        }  
    }  
      
    printf("Decode audio file %s to %s\n", infilename, outfilename);  
    infile = fopen(infilename, "rb");  
    if (!infile) {  
        fprintf(stderr, "Could not open %s\n", infilename);  
        goto end;  
        exit(1);  
    }  
    outfile = fopen(outfilename, "wb");  
    if (!outfile) {  
        goto end;  
        exit(1);  
    }  
    int size = av_samples_get_buffer_size(NULL, codec_ctx->channels, codec_ctx->frame_size,
                                          codec_ctx->sample_fmt, 1);
    printf("frame_size=%d, size=%d\n", codec_ctx->frame_size,size);
    uint8_t * buffer =(uint8_t*) malloc(size);
      
    av_init_packet(&pkt);  
    while (av_read_frame(fmt_ctx, &pkt) >= 0){  
  
        if (pkt.stream_index == stream_index){  
            if (!frame) {  
                if (!(frame = av_frame_alloc())) {  
                    fprintf(stderr, "Could not allocate audio frame\n");  
                    goto end;  
                    exit(1);  
                }  
		av_samples_fill_arrays(frame->data, frame->linesize, buffer, codec_ctx->channels, codec_ctx->frame_size, codec_ctx->sample_fmt, 1);
            }  
  
            ret = myavcodec_decode_audio4(codec_ctx, frame, &got_frame, &pkt);  
            if (ret < 0) {  
                printf("Error in decoding audio frame.\n");  
                goto end;  
                return -1;  
            }  
            if (got_frame > 0){  
                /* if a frame has been decoded, output it */  
                int data_size = av_get_bytes_per_sample(codec_ctx->sample_fmt);  
                if (data_size < 0) {  
                    /* This should not occur, checking just for paranoia */  
                    fprintf(stderr, "Failed to calculate data size\n");  
                    goto end;  
                    exit(1);  
                }  
		for ( int i = 0; i < AV_NUM_DATA_POINTERS; i++){
		    if(frame->data[i] != NULL){
                        fwrite(frame->data[i] , 1, frame->linesize[i], outfile);  
		    }
		}
            }  
        }  
        av_packet_unref(&pkt);  
    }  
  
end:  
    if (outfile)  
        fclose(outfile);  
    if (infile)  
        fclose(infile);  
    avcodec_close(codec_ctx);  
    avformat_close_input(&fmt_ctx);  
    if (frame)  
        av_frame_free(&frame);  
      
    return 0;  
} 
