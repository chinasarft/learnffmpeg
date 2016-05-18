all:
	gcc -g -I/usr/local/include   -Wall -g   -c -o de.o de.c
	gcc   de.o  -pthread -L/usr/local/lib -lavdevice -lavfilter -lpostproc -lavformat -lavcodec -lx264 -lz -lswresample -lswscale -lavutil -lfdk-aac -lm -ldl -o de
	rm -f my.mp4
h264toyuv:
	gcc -g h264toyuv.c  -pthread -L/usr/local/lib -lavdevice -lavfilter -lpostproc -lavformat -lavcodec -lx264 -lz -lswresample -lswscale -lavutil -lfdk-aac -lm -ldl -o h2y
yuvtoh264:
	gcc -g yuvtoh264.c  -pthread -L/usr/local/lib -lavdevice -lavfilter -lpostproc -lavformat -lavcodec -lx264 -lz -lswresample -lswscale -lavutil  -lfdk-aac -lm -ldl -o y2h
