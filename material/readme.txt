ffmpeg -i rtmp://live.hkstv.hk.lxdns.com/live/hks -vcodec libx265  -acodec aac -ar 16000 -ac 1 h265_aac_1_16000.ts
ffmpeg -i h265_aac_1_16000.ts -vcodec copy -an h265_aac_1_16000_v.h265 -acodec copy -vn h265_aac_1_16000_a.aac
ffmpeg -i h265_aac_1_16000.ts -acodec pcm_mulaw -vn -ar 8000 -f mulaw h265_aac_1_16000_pcmu_8000.mulaw
ffmpeg -i h265_aac_1_16000.ts -vcodec libx264 -profile:v baseline -an h265_aac_1_16000_h264.h264

ffmpeg -i h265_aac_1_16000.ts -vcodec libx265 -x265-params keyint=50:no-open-gop -acodec copy h265_aac_1_16000.ts
-x265-params keyint=50:no-open-gop



