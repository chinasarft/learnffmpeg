# ffmpegtest
ffmpeg 编程小例子
toyuv.c
h264裸文件是可以直接检测的，.mp4 .avi的容器文件也是没有问题的
所以这个文件是实现.mp4 .avi .h264等格式转码为yuv

yuvto.c
从yuv编码h264文件，也不止是h264, 可以改变后缀名如.mp4变为mp4的文件.

可以直接安装ffmpeg可以它的依赖库
sudo apt-get install libavutil-dev
sudo apt-get install libswscale-dev
sudo apt-get install libx264-dev
sudo apt-get install libavdev
sudo apt-get install libpostproc-dev
但是最新的ffmpeg并且和libfdk_aac一起的只能自己编译
