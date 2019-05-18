#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include "ku_ipc.h"

#define DEV_NAME "ku_ipc_dev"

#define IOCTL_START 0x80
#define IOCTL_MSGQCHECK IOCTL_START+1
#define IOCTL_MSGQCREAT IOCTL_START+2
#define IOCTL_MSGQCLOSE IOCTL_START+3

#define IOCTL_MAGICNUM 'z'
#define IOCTL_MSGQCHECK_NUM _IOWR(IOCTL_MAGICNUM, IOCTL_MSGQCHECK, unsigned long *)
#define IOCTL_MSGQCREAT_NUM _IOWR(IOCTL_MAGICNUM, IOCTL_MSGQCREAT, unsigned long *)
#define IOCTL_MSGQCLOSE_NUM _IOWR(IOCTL_MAGICNUM, IOCTL_MSGQCLOSE, unsigned long *)

struct userbuf
{
	int msqid;
	struct msgbuf *msgp;
	int msglen;
	int cutmsg;
};

struct msgbuf
{
    long type;
    char text[256];
};

int ku_msgget(int key, int msgflg);
int ku_msgclose(int msqid);
int ku_msgsnd(int msqid, void *msgp, int msgsz, int msgflg);
int ku_msgrcv(int msqid, void *msgp, int msgsz, long msgtyp, int msgflg);

int ku_msgget(int key, int msgflg)
{
	int dev;
	dev = open("/dev/ku_ipc_dev", O_RDWR);

	int msqid;
	msqid = ioctl(dev, IOCTL_MSGQCHECK_NUM, &key);
	//check if same key queue already exists
	if(msqid == -1)
	{ //doesn't exist
		if(msgflg != KU_IPC_CREAT)
			msqid = -1;
		else //create
			msqid = ioctl(dev, IOCTL_MSGQCREAT_NUM, &key);
	}

	close(dev);
	return msqid;
}

int ku_msgclose(int msqid)
{
	int dev;
	unsigned long value = 0;

	dev = open("/dev/ku_ipc_dev", O_RDWR);
	int rtnValue;
	int flag;
	rtnValue = ioctl(dev, IOCTL_MSGQCHECK_NUM, &msqid);
	//check if msqid exists
	if(rtnValue == -1)
		flag = -1;
	else
	{
		flag = ioctl(dev, IOCTL_MSGQCLOSE_NUM, &msqid);
	}
	close(dev);
	return flag;
}

int ku_msgsnd(int msqid, void *msgp, int msgsz, int msgflg)
{
	int dev;
	dev = open("/dev/ku_ipc_dev", O_RDWR);

	int rtnValue;
	int flag;
	rtnValue = ioctl(dev, IOCTL_MSGQCHECK_NUM, &msqid);
	//check if msqid exists
	if(rtnValue == -1)
		flag = -1; //failed to send
	else
	{
		//cannot use str_st -> malloc userbuf and pass it
		struct userbuf *tmpbuf = 0;
		tmpbuf = (struct userbuf *)malloc(sizeof(struct userbuf));
		tmpbuf->msqid = msqid;
		tmpbuf->msgp = msgp;//msgbuf(type, text)
		tmpbuf->msglen = strlen(tmpbuf->msgp->text);
		tmpbuf->cutmsg = 0;

		rtnValue = write(dev, tmpbuf, msgsz);
		if(rtnValue == -1)
		{ //no vacancy in message queue
			if(msgflg & KU_IPC_NOWAIT == 0)
			{ //wait until sending is possible
				while(write(dev, tmpbuf, msgsz) == -1) 
				{

				}
				flag = 0; //succeeded to send
			}
			else
			{ //just return -1 without sending
				flag = -1;
			}
		}
		else
		{
			flag = 0; //succeeded to send
		}
		free(tmpbuf);
	}
	close(dev);
	return flag;
}

int ku_msgrcv(int msqid, void *msgp, int msgsz, long msgtyp, int msgflg)
{
	int dev;
	unsigned long value = 0;

	dev = open("/dev/ku_ipc_dev", O_RDWR);

	int rtnValue;
	int rcvmsglen;
	rtnValue = ioctl(dev, IOCTL_MSGQCHECK_NUM, &msqid);
	//check if msqid exists
	if(rtnValue == -1)
		rcvmsglen = -1;
	else
	{
		struct userbuf *tmpbuf = 0;
		struct msgbuf *movebuf = 0;
		tmpbuf = (struct userbuf *)malloc(sizeof(struct userbuf));
		tmpbuf->msqid = msqid;
		tmpbuf->msgp = msgp;
		tmpbuf->msgp = (struct msgbuf *)malloc(sizeof(struct msgbuf));
		tmpbuf->msgp->type = msgtyp;
		memset(tmpbuf->msgp->text, 0x00, msgsz - sizeof(long)); 
		tmpbuf->msglen = msgsz - sizeof(long); //msglen to read
		tmpbuf->cutmsg = 0;

		rcvmsglen = read(dev, tmpbuf, msgsz);
		if(rcvmsglen == -1)
		{ //no message in message queue
			if(msgflg & KU_IPC_NOWAIT == 0)
			{ //wait until receiving is possible
				while(rcvmsglen == -1) 
				{
					rcvmsglen = read(dev, tmpbuf, msgsz);
				}
				if(rcvmsglen == -2)
				{ //read possible but message should be cut
					if(msgflg & KU_MSG_NOERROR == 0)
					{ //just return -1
						rcvmsglen = -1;
					}
					else
					{ //cut the message
						tmpbuf->cutmsg = 1;
						rcvmsglen = read(dev, tmpbuf, msgsz);
					}
				}
				else
				{ //complete read possible
					movebuf = (struct msgbuf *)msgp;
					movebuf->type = msgtyp;
					memset(movebuf->text, 0x00, msgsz - sizeof(long));
					memcpy(movebuf->text, tmpbuf->msgp->text, msgsz - sizeof(long));
				}
			}
			else
			{ //just return -1 without receiving
				rcvmsglen = -1;
			} 
		}
		else if(rcvmsglen == -2)
		{ //read possible but message should be cut or not depending on MSG_NOERROR flag
			if(msgflg & KU_MSG_NOERROR == 0)
			{ //just return -1
				rcvmsglen = -1;
			}
			else
			{ //cut the message
				tmpbuf->cutmsg = 1;
				rcvmsglen = read(dev, tmpbuf, msgsz);
			}
		}
		else
		{ //complete read possible
			movebuf = (struct msgbuf *)msgp;
			movebuf->type = msgtyp;
			memset(movebuf->text, 0x00, msgsz - sizeof(long));
			memcpy(movebuf->text, tmpbuf->msgp->text, msgsz - sizeof(long));
		}
	}
	close(dev);
	return rcvmsglen;
}