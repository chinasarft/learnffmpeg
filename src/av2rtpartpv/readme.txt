从文件读取两路视频和音频然后分别用两路rtp发送出去

ffplay -protocol_whitelist "file,http,https,rtp,udp,tcp,tls" test.sdp

SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=No Name
t=0 0
a=tool:libavformat 57.83.100
m=audio 4320 RTP/AVP 97
c=IN IP4 127.0.0.1
b=AS:34
a=rtpmap:97 MPEG4-GENERIC/16000/1
a=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3; config=1408
m=video 4322 RTP/AVP 96
c=IN IP4 127.0.0.1
b=AS:356
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z2QADazZQWCWhAAAAwAEAAADAMg8UKZY,aOvjyyLA; profile-level-id=64000D
