#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <asm/delay.h>

#include "ku_ipc.h"

MODULE_LICENSE("GPL");

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

// Message Structure
struct message
{
	struct list_head mlist;
	long type;
	char *text;
	int textLen;
};

// Message Queue Structure
struct message_queue
{
	struct list_head qlist;
	int key;
	int msgCnt;
	int vol;
	struct message ku_msg;
};

// Global Variables
struct message_queue ku_mqueue;
spinlock_t ku_lock;
int msq_id = 0;

void delay(int sec)
{
	int i, j;
	for(j = 0; j < sec; j++)
	{
		for(i = 0; i < 1000; i++)
		{
			udelay(1000);
		}
	}
}

static long ku_ipc_mod(struct file * file, unsigned int cmd, unsigned long arg)
{
	int key;
	int rtnValue = -1;
	struct message_queue *tmp = 0;

	switch(cmd)
	{
		case IOCTL_MSGQCHECK_NUM:
			printk("Checking KU MESSAGE QUEUE\n");
			key = *(int *)arg;
			//check if same key queue already exists
			list_for_each_entry(tmp, &ku_mqueue.qlist, qlist)
			{
				if(tmp->key == key)
				{ //already exists
					rtnValue = key;
				}
			}
			break;
		case IOCTL_MSGQCREAT_NUM:
			printk("Creating NEW MESSAGE QUEUE\n");
			key = *(int *)arg;
			tmp = (struct message_queue *)kmalloc(sizeof(struct message_queue), GFP_KERNEL);

			//initialize message queue structure
			tmp->key = key;
			tmp->msgCnt = 0;
			tmp->vol = 0;
			INIT_LIST_HEAD(&tmp->ku_msg.mlist);

			//add to ku_mqueue
			list_add_tail(&tmp->qlist, &ku_mqueue.qlist);

			rtnValue = tmp->key;
			break;
		case IOCTL_MSGQCLOSE_NUM:

			break;
		default:
			break;
	}
	return rtnValue;
}

static int ku_ipc_mod_open(struct inode *inode, struct file *file)
{
	printk("KU_IPC Module Open\n");
	return 0;
}

static int ku_ipc_mod_release(struct inode *inode, struct file *file)
{
	printk("KU_IPC Module Release\n");
	return 0;
}

static int ku_ipc_mod_read(struct file *file, char *buf, size_t buflen, loff_t *f_pos)
{
	int rtnValue = -1;
	struct userbuf *from_userbuf = 0;
	struct message_queue *tmp = 0;
	struct message_queue *selected_queue = 0;
	struct message *tmp_msg = 0;
	struct message *selected_msg = 0;
	int first = 1;
	int exist = 0;

	//search msqid queue
	from_userbuf = (struct userbuf *)buf;
	list_for_each_entry(tmp, &ku_mqueue.qlist, qlist)
	{
		if(tmp->key == from_userbuf->msqid)
		{
			printk("[KU_IPC_READ] Found message queue to read\n"); 
			selected_queue = tmp;
			if(selected_queue->msgCnt == 0) {
				printk("[KU_IPC_READ] No message in the selected message queue\n");
				rtnValue = -1;
			}
			else
			{
				list_for_each_entry(tmp_msg, &selected_queue->ku_msg.mlist, mlist)
				{
					if(tmp_msg->type == from_userbuf->msgp->type && first)
					{
						selected_msg = tmp_msg;
						first = 0;
						exist = 1;
					}
				}
				if(!exist)
					rtnValue = -1;
				else
				{
					printk("[KU_IPC_READ] Found type %ld message in the queue\n", selected_msg->type);
					if(selected_msg->textLen > buflen)
					{
						if(from_userbuf->cutmsg)
						{
							printk("[KU_IPC_READ] Possible to read but the message would be cut\n");
							delay(2);
							spin_lock(&ku_lock);
							printk("[KU_IPC_READ] Text to read is %s\n", selected_msg->text);
							rtnValue = copy_to_user(from_userbuf->msgp->text, selected_msg->text, buflen);
							rtnValue = buflen;

							//after read
							list_del(&selected_msg->mlist);
							kfree(selected_msg->text);
							kfree(selected_msg);

							selected_queue->msgCnt -= 1;
							selected_queue->vol -= buflen;

							spin_unlock(&ku_lock);
						}
						else
						{
							rtnValue = -2;
						}
					}
					else
					{ //possible to read
						printk("[KU_IPC_READ] Possible to read complete message\n");

						delay(2);
						spin_lock(&ku_lock);
						printk("[KU_IPC_READ] Text to read is %s\n", selected_msg->text);
						rtnValue = copy_to_user(from_userbuf->msgp->text, selected_msg->text, buflen);
						rtnValue = buflen;

						//after read
						list_del(&selected_msg->mlist);
						kfree(selected_msg->text);
						kfree(selected_msg);

						selected_queue->msgCnt -= 1;
						selected_queue->vol -= buflen;
						printk("[KU_IPC_READ] msgCnt %d, volume %d left in the selected %d queue\n", selected_queue->msgCnt, selected_queue->vol, selected_queue->key);

						spin_unlock(&ku_lock);
					}

				}
				
			}
		}
	}
	printk("[KU_IPC_READ] Return Value is %d\n", rtnValue);
	return rtnValue;
}

static int ku_ipc_mod_write(struct file *file, const char *buf, size_t buflen, loff_t *f_pos)
{
	int rtnValue = -1;
	int ret;
	struct userbuf *from_userbuf = 0;
	struct message_queue *tmp = 0;
	struct message_queue *selected_queue = 0;
	struct message *tmp_msg = 0;

	//search msqid queue
	from_userbuf = (struct userbuf *)buf;
	list_for_each_entry(tmp, &ku_mqueue.qlist, qlist)
	{
		if(tmp->key == from_userbuf->msqid)
		{
			selected_queue = tmp;
			printk("[KU_IPC_WRITE] Found message queue to write\n");
			if(selected_queue->msgCnt + 1 > KUIPC_MAXMSG) {
				printk("[KU_IPC_WRITE] Queue is full of messages\n");
				rtnValue = -1;
			}
			else if(selected_queue->vol + buflen > KUIPC_MAXVOL) {
				printk("[KU_IPC_WRITE] Queue is full\n");
				rtnValue = -1;
			}
			else
				rtnValue = 0;
		}
	}

	if(rtnValue != -1)
	{ // possible to put
		printk("[KU_IPC_WRITE] Possible to Write\n");

		printk("[KU_IPC_WRITE] Getting buf from app..\n");

		spin_lock(&ku_lock);
		tmp_msg = (struct message *)kmalloc(sizeof(struct message), GFP_KERNEL);
		tmp_msg->type = from_userbuf->msgp->type;
		tmp_msg->text = (char *)kmalloc(buflen - sizeof(long), GFP_KERNEL);
		tmp_msg->textLen = from_userbuf->msglen;
		
		ret = copy_from_user(tmp_msg->text, from_userbuf->msgp->text, buflen - sizeof(long));
		printk("[KU_IPC_WRITE] The text to write is type: %ld, message: %s\n", tmp_msg->type, tmp_msg->text);
		
		list_add_tail(&tmp_msg->mlist, &selected_queue->ku_msg.mlist);

		selected_queue->msgCnt += 1;
		selected_queue->vol += buflen;
		spin_unlock(&ku_lock);

		printk("Succeeded to put message in %d message queue\n", selected_queue->key);
		printk("%d message queue status- msgCnt %d, volume %d bytes\n", selected_queue->key, selected_queue->msgCnt, selected_queue->vol);
	}
	
	return rtnValue;
}

struct file_operations ku_ipc_mod_fops = 
{
	.unlocked_ioctl = ku_ipc_mod,
	.open = ku_ipc_mod_open,
	.release = ku_ipc_mod_release,
	.write = ku_ipc_mod_write,
	.read = ku_ipc_mod_read,
};

static dev_t dev_num;
static struct cdev *cd_cdev;

static int __init ku_ipc_mod_init(void)
{
	int ret;
	printk("Init KU_IPC Module\n");

	alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
	cd_cdev = cdev_alloc();
	cdev_init(cd_cdev, &ku_ipc_mod_fops);
	ret = cdev_add(cd_cdev, dev_num, 1);
	if(ret < 0) {
		printk("fail to add character device\n");
		return -1;
	}
	
	spin_lock_init(&ku_lock);
	INIT_LIST_HEAD(&ku_mqueue.qlist);
	return 0;
}

static void __exit ku_ipc_mod_exit(void)
{
	struct message_queue *tmp = 0;
	struct message *tmp_msg = 0;
	struct list_head *tmp_pos = 0;
	struct list_head *tmp_q = 0;
	struct list_head *tmp_msg_pos = 0;
	struct list_head *tmp_msg_q = 0;

	printk("Exit KU_IPC Module\n");

	cdev_del(cd_cdev);
	unregister_chrdev_region(dev_num, 1);

	spin_lock(&ku_lock);
	list_for_each_safe(tmp_pos, tmp_q, &ku_mqueue.qlist)
	{
		tmp = list_entry(tmp_pos, struct message_queue, qlist);
		list_for_each_safe(tmp_msg_pos, tmp_msg_q, &tmp->ku_msg.mlist)
		{
			tmp_msg = list_entry(tmp_msg_pos, struct message, mlist);
			list_del(tmp_msg_pos);
			kfree(tmp_msg->text);
			kfree(tmp_msg);
		}
		list_del(tmp_pos);
		kfree(tmp);
	}
	spin_unlock(&ku_lock);
}

module_init(ku_ipc_mod_init);
module_exit(ku_ipc_mod_exit);