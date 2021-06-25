#!/bin/bash
#gcc -o ffmpeg-drm main.c -I/usr/include/libdrm -I /usr/local/include/mythtv -lz -lm -lpthread -lmythavcodec -lmythavformat -lmythavutil -lmythswresample -ldrm -L/usr/local/lib
gcc -o ffmpeg-drm main.c -I/usr/include/libdrm -lz -lm -lpthread -lavfilter -lavformat -lavcodec -lswscale -lavutil -lswresample -lavdevice -ldrm -ludev -L/usr/local/lib
