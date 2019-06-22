#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define IOCTL_START 0x80
#define IOCTL_STATUS IOCTL_START+1

#define IOCTL_MAGICNUM 'z'
#define IOCTL_STATUS_NUM _IOWR(IOCTL_MAGICNUM, IOCTL_STATUS, unsigned long *)

int main(void) 
{
	pid_t pid;
	int status;

	pid = fork();

	if(pid == 0) {
		system("python3 openDisplay.py");
		kill(getpid(), SIGKILL);
	}
	else {
		int dev;
		dev = open("/dev/touch_dev", O_RDWR);
		
		int statusToSet;
		while(pid != 0) {
			statusToSet = ioctl(dev, IOCTL_STATUS_NUM, NULL);
			if(statusToSet == 1) {
				printf("Change to Display Mode\n");
				pid = fork();
				if(pid == 0) {
					system("python3 closeBrowser.py");
					system("python3 openDisplay.py");
					//kill this pid
					kill(getpid(), SIGKILL);
				}
				else {

				}
			}
			else if(statusToSet == 2) {
				printf("Change to Exterior Mode\n");
				pid = fork();
				if(pid == 0) {
					system("python3 closeBrowser.py");
					system("python3 openExterior.py");
					//kill this pid
					kill(getpid(), SIGKILL);
				}
				else {

				}
			}
			else {
				//printf("Do Nothing\n");
			}
			sleep(3);
		}
		close(dev);
	}
	return 0;
}