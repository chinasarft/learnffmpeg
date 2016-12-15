topcm:
	gcc -g -I./include -L./lib  -g -Wall topcm.c  -pthread ./lib/libavdevice.a ./lib/libavfilter.a ./lib/libpostproc.a ./lib/libavformat.a ./lib/libavcodec.a ./lib/libswresample.a ./lib/libswscale.a -lx264 ./lib/libavutil.a ./lib/libfdk-aac.a -lz -llzma -lm -ldl  -o topcm
pcmto:
#	gcc -g -Wall lxh.c  -pthread -L./lib -lavdevice -lavfilter -lpostproc -lavformat -lavcodec -lswresample -lswscale -lavutil  -lfdk-aac -lz -lx264 -llzma -lm -ldl -o lxh
	#gcc -I./include -L./lib  -g -Wall pcmto.c  -pthread -lavdevice -lavfilter -lpostproc -lavformat -lavcodec -lswresample -lswscale -lx264 -lavutil -lfdk-aac -lz -llzma -lm -ldl  -o pcmto
	gcc -g -I./include -L./lib  -g -Wall pcmto.c  -pthread ./lib/libavdevice.a ./lib/libavfilter.a ./lib/libpostproc.a ./lib/libavformat.a ./lib/libavcodec.a ./lib/libswresample.a ./lib/libswscale.a -lx264 ./lib/libavutil.a ./lib/libfdk-aac.a -lz -llzma -lm -ldl  -o pcmto
