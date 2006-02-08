#include <lib/driver/avswitch.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <lib/base/init.h>
#include <lib/base/init_num.h>
#include <lib/base/eerror.h>

eAVSwitch *eAVSwitch::instance = 0;

eAVSwitch::eAVSwitch()
{
	ASSERT(!instance);
	instance = this;
}

eAVSwitch::~eAVSwitch()
{
}

eAVSwitch *eAVSwitch::getInstance()
{
	return instance;
}

void eAVSwitch::setInput(int val)
{
	/*
	0-encoder
	1-scart
	2-aux
	*/

	char *input[] = {"encoder", "scart", "aux"};

	int fd;
	
	if((fd = open("/proc/stb/avs/0/input", O_WRONLY)) < 0) {
		printf("cannot open /proc/stb/avs/0/input\n");
		return;
	}

	write(fd, input[val], strlen(input[val]));
	close(fd);
	
	if (val == 1)
		setFastBlank(2);
}

void eAVSwitch::setFastBlank(int val)
{
	int fd;
	char *fb[] = {"low", "high", "vcr"};

	if((fd = open("/proc/stb/avs/0/fb", O_WRONLY)) < 0) {
		printf("cannot open /proc/stb/avs/0/fb\n");
		return;
	}

	write(fd, fb[val], strlen(fb[0]));
	close(fd);
}

void eAVSwitch::setColorFormat(int format)
{
	/*
	0-CVBS
	1-RGB
	2-S-Video
	*/
	char *cvbs="cvbs";
	char *rgb="rgb";
	char *svideo="svideo";
	char *yuv="yuv";
	int fd;
	
	if((fd = open("/proc/stb/avs/0/colorformat", O_WRONLY)) < 0) {
		printf("cannot open /proc/stb/avs/0/colorformat\n");
		return;
	}
	switch(format) {
		case 0:
			write(fd, cvbs, strlen(cvbs));
			break;
		case 1:
			write(fd, rgb, strlen(rgb));
			break;
		case 2:
			write(fd, svideo, strlen(svideo));
			break;
		case 3:
			write(fd, yuv, strlen(yuv));
			break;
	}	
	close(fd);
}

void eAVSwitch::setAspectRatio(int ratio)
{
	/*
	0-4:3 Letterbox
	1-4:3 PanScan
	2-16:9
	3-16:9 forced
	*/
	
	char *aspect[] = {"4:3", "4:3", "any", "16:9"};
	char *policy[] = {"letterbox", "panscan", "bestfit", "panscan"};

	int fd;
	if((fd = open("/proc/stb/video/aspect", O_WRONLY)) < 0) {
		printf("cannot open /proc/stb/video/aspect\n");
		return;
	}
//	eDebug("set aspect to %s", aspect[ratio]);
	write(fd, aspect[ratio], strlen(aspect[ratio]));
	close(fd);

	if((fd = open("/proc/stb/video/policy", O_WRONLY)) < 0) {
		printf("cannot open /proc/stb/video/policy\n");
		return;
	}
//	eDebug("set ratio to %s", policy[ratio]);
	write(fd, policy[ratio], strlen(policy[ratio]));
	close(fd);

}

void eAVSwitch::setVideomode(int mode)
{
	char *pal="pal";
	char *ntsc="ntsc";
	int fd;

	if((fd = open("/proc/stb/video/videomode", O_WRONLY)) < 0) {
		printf("cannot open /proc/stb/video/videomode\n");
		return;
	}
	switch(mode) {
		case 0:
			write(fd, pal, strlen(pal));
			break;
		case 1:
			write(fd, ntsc, strlen(ntsc));
			break;
	}
	close(fd);
}

void eAVSwitch::setWSS(int val) // 0 = auto, 1 = auto(4:3_off)
{
	int fd;
	if((fd = open("/proc/stb/denc/0/wss", O_WRONLY)) < 0) {
		printf("cannot open /proc/stb/denc/0/wss\n");
		return;
	}
	char *wss[] = {
		"off", "auto", "auto(4:3_off)", "4:3_full_format", "16:9_full_format",
		"14:9_letterbox_center", "14:9_letterbox_top", "16:9_letterbox_center",
		"16:9_letterbox_top", ">16:9_letterbox_center", "14:9_full_format"
	};
	write(fd, wss[val], strlen(wss[val]));
//	eDebug("set wss to %s", wss[val]);
	close(fd);
}

void eAVSwitch::setSlowblank(int val)
{
	int fd;
	if((fd = open("/proc/stb/avs/0/sb", O_WRONLY)) < 0) {
		printf("cannot open /proc/stb/avs/0/sb\n");
		return;
	}
	char *sb[] = {"0", "6", "12", "vcr", "auto"};
	write(fd, sb[val], strlen(sb[val]));
//	eDebug("set slow blanking to %s", sb[val]);
	close(fd);
}

//FIXME: correct "run/startlevel"
eAutoInitP0<eAVSwitch> init_avswitch(eAutoInitNumbers::rc, "AVSwitch Driver");
