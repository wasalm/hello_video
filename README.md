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
		
- Improve the performance of the video driver
		
		gpu_mem=256
		hdmi_enable_4kp60=1

### Install the needed libraries and tools

- Install the following tools:
	
		sudo apt install git autoconf

- Install the following libraries

		sudo apt install libdrm-dev libudev-dev

### Install ffmpeg from source
At the time of writing the latest development version of ffmpeg is at https://github.com/jc-kynesim/rpi-ffmpeg/ in the branch dev/4.3.2/clean_3.
In the home directory run:

	git clone --branch dev/4.3.2/clean_3 https://github.com/jc-kynesim/rpi-ffmpeg/
	cd rpi-ffmpeg
	./configure --disable-mmal --enable-sand --enable-v4l2-request --enable-libdrm  --enable-libudev
	make -j4
	sudo make install

### Install this library from source
Clone this repository in your home directory and run the build script
	
	git clone https://github.com/wasalm/hello_video
	cd hello_video
	build.sh

## Running
You can run the program using the following way

	./ffmpeg-drm <input file>

You can also download some example files and run the added run scripts:

	cd ~
	wget https://jell.yfish.us/media/jellyfish-3-mbps-hd-h264.mkv
	./hello_video/ffmpeg-drm jellyfish-3-mbps-hd-h264.mkv

	wget https://jell.yfish.us/media/jellyfish-3-mbps-hd-hevc.mkv
	./hello_video/ffmpeg-drm jellyfish-3-mbps-hd-hevc.mkv

	wget https://jell.yfish.us/media/jellyfish-3-mbps-hd-hevc-10bit.mkv
	./hello_video/ffmpeg-drm jellyfish-3-mbps-hd-hevc-10bit.mkv
