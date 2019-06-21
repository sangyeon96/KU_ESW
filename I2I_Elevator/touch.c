#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>

MODULE_LICENSE("GPL");

#define TOUCH 17 //GPIO17
#define DEV_NAME "touch_dev"

#define IOCTL_START 0x80
#define IOCTL_STATUS IOCTL_START+1

#define IOCTL_MAGICNUM 'z'
#define IOCTL_STATUS_NUM _IOWR(IOCTL_MAGICNUM, IOCTL_STATUS, unsigned long *)

static int irq_num;
int status; //1: Display, 2: Exterior
int touched;

static long touch_mod(struct file * file, unsigned int cmd, unsigned long arg)
{
	int rtnValue = -1;

	switch(cmd)
	{
		case IOCTL_STATUS_NUM:
			//printk("Waiting for touch to happen\n");
			
			if(touched) {
				if(status == 1) //Current Status is Display Mode
					rtnValue = 2;
				else if(status == 2) //Current Status is Exterior Mode
					rtnValue = 1;

				touched = 0;
			}
			
			break;
		default:
			break;
	}
	return rtnValue;
}

static int touch_open(struct inode *inode, struct file* file) {
	printk("touch sensor open\n");
	enable_irq(irq_num);
	return 0;
}

static int touch_release(struct inode *inode, struct file* file) {
	printk("touch sensor close\n");
	disable_irq(irq_num);
	return 0;
}

struct file_operations touch_fops =
{
	.unlocked_ioctl = touch_mod,
	.open = touch_open,
	.release = touch_release,
};

static irqreturn_t touch_isr(int irq, void* dev_id) {
	//interrupt handler function
	if(gpio_get_value(TOUCH) == 1) {
		printk("Touched (value: %d)\n", gpio_get_value(TOUCH));
		touched = 1;
		if(status == 1)
			status = 2;
		else if(status == 2)
			status = 1;
	}	

	return IRQ_HANDLED;
}

static dev_t dev_num;
static struct cdev *cd_cdev;

static int __init touch_init(void) {
	int ret;
	status = 1; //default: Display
	touched = 0;
	printk("Init Touch Module\n");

	//allocate character device
	alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
	cd_cdev = cdev_alloc();
	cdev_init(cd_cdev, &touch_fops);
	cdev_add(cd_cdev, dev_num, 1);

	//request GPIO and interrupt handler
	gpio_request_one(TOUCH, GPIOF_IN, "touch");
	irq_num = gpio_to_irq(TOUCH);
	ret = request_irq(irq_num, touch_isr, IRQF_TRIGGER_RISING, "sensor_irq", NULL);

	return 0;
}

static void __exit touch_exit(void) {
	printk("Exit Touch Module\n");
	cdev_del(cd_cdev);
	unregister_chrdev_region(dev_num, 1);

	free_irq(irq_num, NULL);
	gpio_free(TOUCH);
}

module_init(touch_init);
module_exit(touch_exit);