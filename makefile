all:
	gcc -g -I/usr/local/include   -Wall -g   -c -o de.o de.c
	gcc   de.o  -pthread -L/usr/local/lib -lavdevice -lavfilter -lpostproc -lavformat -lavcodec -lx264 -lz -lswresample -lswscale -lavutil -lfdk-aac -lm -ldl -o de
	rm -f my.mp4
2yuv:
	gcc -g -Wall toyuv.c  -pthread -L/usr/local/lib -lavdevice -lavfilter -lpostproc -lavformat -lavcodec -lx264  -lswresample -lswscale -lavutil -lfdk-aac -lm -ldl -o toyuv
yuv2:
	gcc -g -Wall yuvto.c  -pthread -L/usr/local/lib -lavdevice -lavfilter -lpostproc -lavformat -lavcodec -lx264 -lswresample -lswscale -lavutil  -lfdk-aac -lm -ldl -o yuvto
