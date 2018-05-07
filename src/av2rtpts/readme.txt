从文件读取两路视频和音频然后合为一个内存的ts流，然后在写到rtp

ffplay -protocol_whitelist "file,http,https,rtp,udp,tcp,tls" test.sdp
SDP:
v=0   
o=- 0 0 IN IP4 127.0.0.1
s=rtp PS stream
i=N/A 
c=IN IP4 127.0.0.1
t=0 0 
a=tool:libavformat 57.83.100
a=recvonly
a=type:broadcast
a=charset:UTF-8
m=video 4320 RTP/AVP 33
b=RR:0
a=rtpmap:33 MP2P/90000 


