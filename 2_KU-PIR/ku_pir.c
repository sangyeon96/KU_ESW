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
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/rculist.h>

#include "ku_pir.h"

MODULE_LICENSE("GPL");

struct pir_data
{
	struct list_head plist;
	long unsigned int timestamp;
	char rf_flag;
};

struct data_queue
{
	struct list_head qlist;
	int fd;
	int dataCnt;
	struct pir_data *pir;
};

struct data_queue ku_queue;
int count;
spinlock_t count_lock;
static int irq_num;
wait_queue_head_t ku_wq;

struct data_queue *search_queue(int fd)
{
	struct data_queue *tmp_q;

	rcu_read_lock();
	list_for_each_entry_rcu(tmp_q, &ku_queue.qlist, qlist) {
		if(tmp_q->fd == fd) {
			return tmp_q;
		}
	}
	rcu_read_unlock();
	return NULL;
}

void flush_data_queue(struct data_queue *flush_q)
{
	struct pir_data *tmp_p;
	struct list_head *pos;
	struct list_head *q;

	list_for_each_safe(pos, q, &(flush_q->pir->plist)) {
		tmp_p = list_entry(pos, struct pir_data, plist);
		list_del_rcu(pos);
		kfree(tmp_p);
	}
	flush_q->dataCnt = 0;
}

void print_queue_status(void)
{
	struct data_queue *tmp_q;
	struct pir_data *tmp_p;

	rcu_read_lock();
	list_for_each_entry_rcu(tmp_q, &ku_queue.qlist, qlist) {
		printk("-----%d Queue Status-----\n", tmp_q->fd);
		list_for_each_entry_rcu(tmp_p, &tmp_q->pir->plist, plist) {
			printk("TimeStamp %ld with rf_flag %c\n", tmp_p->timestamp, tmp_p->rf_flag);
		}
		printk("-------------------------\n");
	}
	rcu_read_unlock();
}

static long ku_pir_mod(struct file * file, unsigned int cmd, unsigned long arg)
{
	int rtnValue = -1;
	int fd;

	struct data_queue *tmp_q;
	struct pir_data *tmp_p;
	struct fd_pir_data *fd_user_data;
	struct ku_pir_data *user_data;
	struct pir_data *kern_data;
	struct pir_data *head_data;
	struct list_head *pos;
	struct list_head *q;

	struct data_queue *flush_q;
	struct data_queue *del_q;
	struct data_queue *q_to_read;

	switch(cmd)
	{
		case IOCTL_PIRQ_CREATE_NUM:
			printk("[OPEN] Creating NEW DATA QUEUE\n");

			spin_lock(&count_lock);
			count++;
			spin_unlock(&count_lock);

			//kmalloc data_queue and data inside data_queue
			tmp_q = (struct data_queue *)kmalloc(sizeof(struct data_queue), GFP_KERNEL);
			
			tmp_q->fd = count;
			tmp_q->dataCnt = 0;
			tmp_q->pir = (struct pir_data *)kmalloc(sizeof(struct pir_data), GFP_KERNEL);

			// data->list head init
			INIT_LIST_HEAD(&(tmp_q->pir->plist));

			// add data_queue in qlist
			list_add_rcu(&(tmp_q->qlist), &(ku_queue.qlist));

			rtnValue = tmp_q->fd;
			break;
		case IOCTL_PIRQ_RESET_NUM:
			fd = *(int *)arg;
			flush_q = search_queue(fd);
			if(flush_q == NULL)
				printk("[FLUSH] No %d Queue Existing\n", fd);
			else {
				flush_data_queue(flush_q);
				printk("[FLUSH] %d Queue Flush Completed\n", fd);
			}
			break;
		case IOCTL_PIRQ_DELETE_NUM:
			fd = *(int *)arg;
			del_q = search_queue(fd);
			if(del_q == NULL) {
				printk("[CLOSE] No %d Queue Existing\n", fd);
				rtnValue = -1;
			}
			else {
				flush_data_queue(del_q);
				list_del_rcu(&del_q->qlist);
				kfree(del_q);
				rtnValue = 0;
				printk("[CLOSE] %d Queue Close Completed\n", fd);
			}
			break;
		case IOCTL_PIR_READ_NUM:
			fd_user_data = (struct fd_pir_data *)kmalloc(sizeof(struct fd_pir_data), GFP_KERNEL);
			rtnValue = copy_from_user(fd_user_data, (struct fd_pir_data *)arg, sizeof(struct fd_pir_data));
			printk("[READ] Queue to read is %d\n", fd_user_data->fd);
			q_to_read = search_queue(fd_user_data->fd);
			if(q_to_read->dataCnt == 0) { //blocking
				printk("[READ] Sleep until it's possible to read(Queue %d)\n", q_to_read->fd);
				wait_event(ku_wq, q_to_read->dataCnt > 0);
			}
			//read and erase
			rcu_read_lock();
			list_for_each_safe(pos, q, &(q_to_read->pir->plist)){
				tmp_p = list_entry(pos, struct pir_data, plist);
				fd_user_data->data->timestamp = tmp_p->timestamp;
				fd_user_data->data->rf_flag = tmp_p->rf_flag;
				printk("[READ] Timestamp %ld with rf_flag %c\n", tmp_p->timestamp, tmp_p->rf_flag);
				rtnValue = copy_to_user((struct fd_pir_data *)arg, fd_user_data, sizeof(struct fd_pir_data));
				list_del_rcu(pos);
				kfree(tmp_p);
				printk("[READ] Deleted head data of %d queue\n", q_to_read->fd);
				break;
			}
			rcu_read_unlock();
			q_to_read->dataCnt--;
			print_queue_status();
			break;
		case IOCTL_PIR_WRITE_NUM:
			user_data = (struct ku_pir_data *)kmalloc(sizeof(struct ku_pir_data), GFP_KERNEL);
			rtnValue = copy_from_user(user_data, (struct ku_pir_data *)arg, sizeof(struct ku_pir_data));
			printk("[INSERT DATA] Timestamp %ld with rf_flag %c\n", user_data->timestamp, user_data->rf_flag);
			
			list_for_each_entry_rcu(tmp_q, &ku_queue.qlist, qlist) {
				if(tmp_q->dataCnt == KUPIR_MAX_MSG) {
					printk("[INSERT DATA] Queue %d is full\n", tmp_q->fd);
					list_for_each_safe(pos, q, &tmp_q->pir->plist){
						head_data = list_entry(pos, struct pir_data, plist);
						list_del_rcu(pos);
						kfree(head_data);
						break;
					}
					tmp_q->dataCnt--;
					printk("[INSERT DATA] Deleted head data of %d queue\n", tmp_q->fd);
				}
				
				kern_data = (struct pir_data *)kmalloc(sizeof(struct pir_data), GFP_KERNEL);
				kern_data->timestamp = user_data->timestamp;
				kern_data->rf_flag = user_data->rf_flag;

				list_add_tail_rcu(&(kern_data->plist), &(tmp_q->pir->plist));
				tmp_q->dataCnt++;
			}

			print_queue_status();
			wake_up(&ku_wq);
			break;
		default:
			break;
	}
	return rtnValue;
}

static int ku_pir_mod_open(struct inode *inode, struct file *file)
{
	printk("KU_PIR Module Open\n");
	return 0;
}

static int ku_pir_mod_release(struct inode *inode, struct file *file)
{
	printk("KU_PIR Module Release\n");
	return 0;
}

struct file_operations ku_pir_mod_fops = 
{
	.unlocked_ioctl = ku_pir_mod,
	.open = ku_pir_mod_open,
	.release = ku_pir_mod_release,
};

static irqreturn_t ku_pir_isr(int irq, void* dev_id) {

	struct data_queue *tmp_q;
	struct pir_data *tmp_data;
	struct pir_data *head_data;
	struct list_head *pos;
	struct list_head *q;
	
	int gpio_flag;

	list_for_each_entry_rcu(tmp_q, &ku_queue.qlist, qlist) {
		if(tmp_q->dataCnt >= KUPIR_MAX_MSG) {
			printk("[ISR] Queue %d is full\n", tmp_q->fd);
			list_for_each_safe(pos, q, &tmp_q->pir->plist){
				head_data = list_entry(pos, struct pir_data, plist);
				list_del_rcu(pos);
				kfree(head_data);
				break;
			}
			tmp_q->dataCnt--;
			printk("[ISR] Deleted head data of %d queue\n", tmp_q->fd);
		}

		tmp_data = (struct pir_data *)kmalloc(sizeof(struct pir_data), GFP_KERNEL);
		tmp_data->timestamp = jiffies;
		gpio_flag = gpio_get_value(KUPIR_SENSOR);
		if(gpio_flag == 0){
			printk("[ISR] RISING Detected\n");
			tmp_data->rf_flag = '0';

		} else if(gpio_flag == 1){
			printk("[ISR] FALLING Detected\n");
			tmp_data->rf_flag = '1';
		}	
		
		list_add_tail_rcu(&(tmp_data->plist), &(tmp_q->pir->plist));
		printk("[ISR] Added Timestamp %ld with rf_flag %c\n", tmp_data->timestamp, tmp_data->rf_flag);
		tmp_q->dataCnt++;
	}

	wake_up(&ku_wq);
	return IRQ_HANDLED;		
}

static dev_t dev_num;
static struct cdev *cd_cdev;

static int __init ku_pir_mod_init(void)
{
	int ret;
	printk("Init KU_PIR Module\n");

	//initialize ku_queue
	INIT_LIST_HEAD(&ku_queue.qlist);

 	//initialize count
	count = 0;

	alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
	cd_cdev = cdev_alloc();
	cdev_init(cd_cdev, &ku_pir_mod_fops);
	ret = cdev_add(cd_cdev, dev_num, 1);
	if(ret < 0) {
		printk("fail to add character device\n");
		return -1;
	}

	gpio_request_one(KUPIR_SENSOR, GPIOF_IN, "KU_PIR");
	irq_num = gpio_to_irq(KUPIR_SENSOR);
	ret = request_irq(irq_num, ku_pir_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "pir_irq", NULL);
	if(ret) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		free_irq(irq_num, NULL);
	}
	
	spin_lock_init(&count_lock);
	init_waitqueue_head(&ku_wq);
	return 0;
}

static void __exit ku_pir_mod_exit(void)
{
	printk("Exit KU_PIR Module\n");

	cdev_del(cd_cdev);
	unregister_chrdev_region(dev_num, 1);

	free_irq(irq_num, NULL);
	gpio_free(KUPIR_SENSOR);
}

module_init(ku_pir_mod_init);
module_exit(ku_pir_mod_exit);