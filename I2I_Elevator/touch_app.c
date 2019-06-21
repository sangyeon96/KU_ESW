#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

#define IOCTL_START 0x80
#define IOCTL_STATUS IOCTL_START+1

#define IOCTL_MAGICNUM 'z'
#define IOCTL_STATUS_NUM _IOWR(IOCTL_MAGICNUM, IOCTL_STATUS, unsigned long *)

void main(void) 
{
	//system("python3 openDisplay.py");

	int dev;
	dev = open("/dev/touch_dev", O_RDWR);

	int statusToSet;
	while(1) {
		statusToSet = ioctl(dev, IOCTL_STATUS_NUM, NULL);
		if(statusToSet == 1) {
			printf("Change to Display Mode\n");
			system("python3 closeBrowser.py");
			system("python3 openDisplay.py");
		}
		else if(statusToSet == 2) {
			printf("Change to Exterior Mode\n");
			system("python3 closeBrowser.py");
			system("python3 openExterior.py");
		}
		//when to close?
	}
	
	close(dev);
}