#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavutil/parseutils.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>

static AVFormatContext *fmt_ctx = NULL;
static int refcount = 0;
static uint8_t *video_dst_data[4] = {NULL};
static int      video_dst_linesize[4];
static int video_dst_bufsize;
static AVPacket pkt;
static AVFrame *frame = NULL;
int got_frame;

FILE * vfile;
void inityuv(){
	vfile = fopen("v.yuv", "w");
	if(vfile == NULL){
		fprintf(stderr, "fopen fail\n");
		exit(1);
	}
}

int main (int argc, char **argv)
{
	int vstream_idx;
    AVStream *vstream;
    AVCodecContext *dec_ctx = NULL;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
	int ret;
	if(argc != 2){
        fprintf(stderr, "usage as:%s filename\n", argv[0]);
		exit(1);
	}
inityuv();

    /* register all formats and codecs */
    av_register_all();

    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, argv[1], NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", argv[1]);
        exit(1);
    }


    /* retrieve stream information */
	//如果输入的H264文件，这里节能拿到h264文件的宽高等信息了
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }
	
    vstream_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if(vstream_idx == AVERROR_DECODER_NOT_FOUND){
		fprintf(stderr, "av_find_best_stream AVMEDIA_TYPE_VIDEO fail\n");
		exit(1);
	}

    vstream = fmt_ctx->streams[vstream_idx];
	dec_ctx = vstream->codec;
printf("width:%d height:%d\n", dec_ctx->width, dec_ctx->height);

    dec = avcodec_find_decoder(dec_ctx->codec_id);
    if (!dec) {
        fprintf(stderr, "Failed to find %s codec\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return AVERROR(EINVAL);
    }

    /* Init the decoders, with or without reference counting */
    av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
    if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0) {
        fprintf(stderr, "Failed to open %s codec\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return ret;
    }
    ret = av_image_alloc(video_dst_data, video_dst_linesize,
                         dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt, 1);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate raw video buffer\n");
        exit(1);
    }
    video_dst_bufsize = ret;

    av_dump_format(fmt_ctx, 0, argv[1], 0);

    frame = av_frame_alloc();
    if (!frame) {
        ret = AVERROR(ENOMEM);
        fprintf(stderr, "Could not allocate frame:%d\n", ret);
		exit(1);
    }
    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    /* read frames from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        do {
            ret = avcodec_decode_video2(dec_ctx, frame, &got_frame, &pkt);
            if (ret < 0) {
                fprintf(stderr, "Error decoding video frame (%s)\n", av_err2str(ret));
                exit(1); 
            }
            pkt.data += ret;
            pkt.size -= ret;
			if(got_frame){
                /* copy decoded frame to destination buffer:
                 * this is required since rawvideo expects non aligned data */
                av_image_copy(video_dst_data, video_dst_linesize,
                              (const uint8_t **)(frame->data), frame->linesize,
                              dec_ctx->pix_fmt, dec_ctx->width, dec_ctx->height);
                fwrite(video_dst_data[0], 1, video_dst_bufsize, vfile);
			}
        } while (pkt.size > 0);
    }

    avcodec_close(dec_ctx);
    avformat_close_input(&fmt_ctx);
    if (vfile)
        fclose(vfile);
    av_frame_free(&frame);
    av_free(video_dst_data[0]);
}
