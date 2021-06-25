# Hello Video
This is an example program to play h.264 and hevc video hardware accelerated on the Raspberry Pi 4.
It uses the latest version of ffmpeg libraries with v4l2_request and outputs the image on an drm plane.

## Installation
We assume you installed Raspberry Pi lite.

### Update /boot/config.txt
Please change the following in /boot/config.txt:
- Enable KMS  by adding 
		dtoverlay=vc4-kms-v3d	
- Enable v4l_request by adding
		dtoverlay=rpivid-v4l2

### Install the needed libraries and tools

- Install the following tools:
	
		sudo apt install git autoconf

- Install the following libraries

		sudo apt install libdrm-dev libepoxy-dev libudev-dev

### Install ffmpeg from source

At the time of writing the latest development version of ffmpeg is at https://github.com/jc-kynesim/rpi-ffmpeg/ in the branch dev/4.3.2/clean_3.
In the home directory run:

	git clone --branch dev/4.3.2/clean_3 https://github.com/jc-kynesim/rpi-ffmpeg/
	cd rpi-ffmpeg
	./configure --disable-mmal --enable-sand --enable-v4l2-request --enable-libdrm  --enable-libudev
	make -j4
	sudo make install

### Install this library from source

TODO

### Disable command line on screen

TODO

## Running

TODO



# OLD NOTES

	sudo apt-get install autoconf

Disable command line on screen:
	sudo systemctl disable getty@tty1.service

-- Clone latests ffmpeg
git clone --branch dev/4.3.2/clean_3 https://github.com/jc-kynesim/rpi-ffmpeg/

	./configure --disable-mmal --enable-sand --enable-v4l2-request --enable-libdrm  --enable-libudev --enable-vout-drm

(In that branch can also use pi-utils/conf-native.sh to configure)

make

FFmpeg with DRM output using v4l2_m2m example:

	https://github.com/BayLibre/ffmpeg-drm
		with bugfixes:
	https://github.com/mark-kendall/ffmpeg-drm

We need to update build file such that it workes with the self compiled ffmpeg version

	#!/bin/bash
	gcc -o ffmpeg-drm main.c -I/usr/include/libdrm -lz -lm -lpthread -lavcodec -lavformat -lavutil -lswresample -ldrm -ludev -L/usr/local/lib

Finally we need to change run.sh:
	
	./ffmpeg-drm --video ./h264.FVDO_Freeway_720p.264 --codec h264_v4l2m2m --width 1920 --height 1080 --device /dev/dri/card1


-- TEST Scripts for ffmpeg
	ffmpeg -no_cvt_hw -hwaccel drm -vcodec hevc -i jellyfish-3-mbps-hd-hevc.mkv -show_all 1 -f vout_drm -
	ffmpeg -no_cvt_hw -vcodec h264_v4l2m2m -i jellyfish-3-mbps-hd-h264.mkv -f vout_drm -


-- Reference question: https://www.raspberrypi.org/forums/viewtopic.php?f=29&t=313322&p=1874437&hilit=wasalm#p1874127


-- interesting for later (overlays etc.)

	https://github.com/dvdhrm/docs/tree/master/drm-howto

-- interesting to look at ffmpeg/docs/example/hw_decode.c
	- how to use hw accels
	- how to implement container decoding...

-- Or look at drm_vout.c in ffmpeg source.
	- shows how to display it
	- solves also tearing