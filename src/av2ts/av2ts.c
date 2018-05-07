#include <stdio.h>
#include <libavformat/avformat.h>

/*
FIX: H.264 in some container format (FLV, MP4, MKV etc.) need
"h264_mp4toannexb" bitstream filter (BSF)
*Add SPS,PPS in front of IDR frame
*Add start code ("0,0,0,1") in front of NALU
H.264 in some container (MPEG2TS) don't need this BSF.
*/
//'1': Use H.264 Bitstream Filter
#define USE_H264BSF 0

/*
FIX:AAC in some container format (FLV, MP4, MKV etc.) need
"aac_adtstoasc" bitstream filter (BSF)
*/
//'1': Use AAC Bitstream Filter
#define USE_AACBSF 0

void init_aacbsfc(AVBSFContext **aacbsfc, AVCodecParameters * codecpar){
    const AVBitStreamFilter * filter = av_bsf_get_by_name("aac_adtstoasc");
    av_bsf_alloc(filter, aacbsfc);
    avcodec_parameters_copy((*aacbsfc)->par_in, codecpar);
    av_bsf_init(*aacbsfc);
}
void init_mp4toannexb(AVBSFContext **mp4toannexb, AVCodecParameters * codecpar){
    const AVBitStreamFilter * filter = av_bsf_get_by_name("h264_mp4toannexb");
    av_bsf_alloc(filter, mp4toannexb);
    avcodec_parameters_copy((*mp4toannexb)->par_in, codecpar);
    av_bsf_init(*mp4toannexb);
}


int main(int argc, char* argv[])
{
    AVOutputFormat *ofmt = NULL;
    //Input AVFormatContext and Output AVFormatContext
    AVFormatContext *ifmt_ctx_v = NULL, *ifmt_ctx_a = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int videoindex_v = -1, videoindex_out = -1;
    int audioindex_a = -1, audioindex_out = -1;
    int frame_index = 0;
    int64_t cur_pts_v = 0, cur_pts_a = 0;
    AVCodecContext *codec_ctx_v = NULL;
    AVCodecContext *codec_ctx_a = NULL;

    const char *in_filename_v = "128x128.264";
    const char *in_filename_a = "aa.aac";

    const char *out_filename = "cuc_ieschool.mp4";//Output file URL
    //av_register_all(); //4.0 后不需要调用这个函数了
    //Input
    if ((ret = avformat_open_input(&ifmt_ctx_v, in_filename_v, 0, 0)) < 0) {
        printf("Could not open input file.");
        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx_v, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        goto end;
    }

    if ((ret = avformat_open_input(&ifmt_ctx_a, in_filename_a, 0, 0)) < 0) {
        printf("Could not open input file.");
        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx_a, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        goto end;
    }
    printf("===========Input Information==========\n");
    av_dump_format(ifmt_ctx_v, 0, in_filename_v, 0);
    av_dump_format(ifmt_ctx_a, 0, in_filename_a, 0);
    printf("======================================\n");


    //Output
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
        printf("Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    ofmt = ofmt_ctx->oformat;
    //ofmt->video_codec //是否指定为ifmt_ctx_v的视频的codec_type.同理音频也一样
    //测试下来即使video_codec和ifmt_ctx_v的视频的codec_type不一样也是没有问题的

    for (i = 0; i < ifmt_ctx_v->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        if (ifmt_ctx_v->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {

            AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
            if (!out_stream) {
                printf("Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                goto end;
            }

            videoindex_v = i;
            videoindex_out = out_stream->index;
            out_stream->codecpar->codec_tag = 0;
            
            avcodec_parameters_copy(out_stream->codecpar, ifmt_ctx_v->streams[i]->codecpar);
            
            break;
        }
    }

    for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        if (ifmt_ctx_a->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {

            AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
            if (!out_stream) {
                printf("Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                goto end;
            }

            audioindex_a = i;
            audioindex_out = out_stream->index;
            out_stream->codecpar->codec_tag = 0;
            
            avcodec_parameters_copy(out_stream->codecpar, ifmt_ctx_a->streams[i]->codecpar);

            break;
        }
    }

    printf("==========Output Information==========\n");
    av_dump_format(ofmt_ctx, 0, out_filename, 1);
    printf("======================================\n");
    //Open output file
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0) {
            printf("Could not open output file '%s'", out_filename);
            goto end;
        }
    }
    //Write file header
    int erno = 0;
    if ((erno = avformat_write_header(ofmt_ctx, NULL)) < 0) {
        char errstr[512] = { 0 };
        av_strerror(erno, errstr, sizeof(errstr));
        printf("Error occurred when opening output file:%s\n", errstr);
        goto end;
    }


#if USE_H264BSF
    AVBSFContext * h264bsf;
    init_mp4toannexb(&h264bsf,  ifmt_ctx_v->streams[videoindex_v]->codecpar);
#endif
#if USE_AACBSF
    AVBSFContext * aacbsfc;
    init_aacbsfc(&aacbsfc,  ifmt_ctx_a->streams[audioindex_a]->codecpar);
#endif

    while (1) {
        AVFormatContext *ifmt_ctx;
        int stream_index = 0;
        AVStream *in_stream, *out_stream;

        //Get an AVPacket
        if (av_compare_ts(cur_pts_v, ifmt_ctx_v->streams[videoindex_v]->time_base,
            cur_pts_a, ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0) {

            ifmt_ctx = ifmt_ctx_v;
            stream_index = videoindex_out;

            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                do {
                    in_stream = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];

                    if (pkt.stream_index == videoindex_v) {
                        //FIX：No PTS (Example: Raw H.264)
                        //Simple Write PTS
                        if (pkt.pts == AV_NOPTS_VALUE) {
                            //Write PTS
                            AVRational time_base1 = in_stream->time_base;
                            //Duration between 2 frames (us)
                            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts = (double)(frame_index*calc_duration) / (double)(av_q2d(time_base1)*AV_TIME_BASE);
                            pkt.dts = pkt.pts;
                            pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1)*AV_TIME_BASE);
                            frame_index++;
                        }

                        cur_pts_v = pkt.pts;
                        break;
                    }
                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
            }
            else {
                break;
            }
        }
        else {
            ifmt_ctx = ifmt_ctx_a;
            stream_index = audioindex_out;
            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                do {
                    in_stream = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];

                    if (pkt.stream_index == audioindex_a) {

                        //FIX：No PTS
                        //Simple Write PTS
                        if (pkt.pts == AV_NOPTS_VALUE) {
                            //Write PTS
                            AVRational time_base1 = in_stream->time_base;
                            //Duration between 2 frames (us)
                            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts = (double)(frame_index*calc_duration) / (double)(av_q2d(time_base1)*AV_TIME_BASE);
                            pkt.dts = pkt.pts;
                            pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1)*AV_TIME_BASE);
                            frame_index++;
                        }
                        cur_pts_a = pkt.pts;

                        break;
                    }
                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
            }
            else {
                break;
            }

        }

        //FIX:Bitstream Filter
#if USE_H264BSF
        av_bsf_send_packet(h264bsfc, &pkt);
        while (av_bsf_receive_packet(h264bsfc, &pkt) == 0);
#endif
#if USE_AACBSF
        av_bsf_send_packet(aacbsfc, &pkt);
        while (av_bsf_receive_packet(aacbsfc, &pkt) == 0);
#endif


        //Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
            (enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base,
            (enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        pkt.stream_index = stream_index;

        printf("Write 1 Packet. size:%5d\tpts:%lld\n", pkt.size, pkt.pts);
        //Write
        if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
            printf("Error muxing packet\n");
            break;
        }
        av_packet_unref(&pkt);

    }
    //Write file trailer
    av_write_trailer(ofmt_ctx);

#if USE_H264BSF
    av_bsf_free(h264bsfc);
#endif
#if USE_AACBSF
    av_bsf_free(aacbsfc);
#endif

end:
    avformat_close_input(&ifmt_ctx_v);
    avformat_close_input(&ifmt_ctx_a);
    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        printf("Error occurred.\n");
        return -1;
    }
    return 0;
}
