#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/cdev.h>

MODULE_LICENSE("GPL");

#define LED 4 //GPIO 4
#define PIR 18 //GPIO 18
#define TOUCH 17 //GPIO 17
#define DEV_NAME "touch_dev"

#define IOCTL_START 0x80
#define IOCTL_STATUS IOCTL_START+1

#define IOCTL_MAGICNUM 'z'
#define IOCTL_STATUS_NUM _IOWR(IOCTL_MAGICNUM, IOCTL_STATUS, unsigned long *)

static int irq_touch;
static int irq_pir;
static struct timer_list pir_timer;

int touch_status;
int mode_status; //1: Display, 2: Exterior
int touched;

static void pir_timer_func(unsigned long data) {
	if(gpio_get_value(PIR)) {
		printk("No one is in the elevator\n");
		gpio_set_value(LED, 0);
		if(touch_status == 1) {
			disable_irq(irq_touch); //pir should be really correct to do this!
			touch_status = 0;
		}
	}
	else {
		printk("Somebody is in the elevator\n");
		gpio_set_value(LED, 1);
	}
}

static irqreturn_t pir_isr(int irq, void* dev_id) {
	disable_irq(irq_touch);

	//RISING: 0, FALLING: 1
	if(gpio_get_value(PIR)) {
		printk("Falling detected\n");
	}
	else {
		printk("Rising detected\n");
		gpio_set_value(LED, 1);
		if(touch_status == 0) {
			enable_irq(irq_touch); //pir should be really correct to do this!
			touch_status = 1;
		}
	}

	del_timer(&pir_timer);
	init_timer(&pir_timer);
	pir_timer.function = pir_timer_func;
	pir_timer.expires = jiffies + (5*HZ); //5 second
	add_timer(&pir_timer);

	enable_irq(irq_touch);
	return IRQ_HANDLED;
}

static long touch_mod(struct file * file, unsigned int cmd, unsigned long arg)
{
	int rtnValue = -1;

	switch(cmd)
	{
		case IOCTL_STATUS_NUM:
			//printk("Waiting for touch to happen\n");
			
			if(touched) {
				rtnValue = mode_status;
				touched = 0;
				printk("Return Value is %d\n", rtnValue);
			}
			break;
		default:
			printk("default\n");
			break;
	}
	return rtnValue;
}

static int touch_open(struct inode *inode, struct file* file) {
	printk("touch sensor open\n");
	enable_irq(irq_touch);
	touch_status = 1;
	return 0;
}

static int touch_release(struct inode *inode, struct file* file) {
	printk("touch sensor close\n");
	disable_irq(irq_touch);
	touch_status = 0;
	return 0;
}

struct file_operations touch_fops =
{
	.unlocked_ioctl = touch_mod,
	.open = touch_open,
	.release = touch_release,
};

static irqreturn_t touch_isr(int irq, void* dev_id) {
	disable_irq(irq_pir);
	//interrupt handler function
	if(gpio_get_value(TOUCH) == 1) {
		printk("Touched (value: %d)\n", gpio_get_value(TOUCH));
		touched = 1;
		if(mode_status == 1) {
			printk("Change to Exterior Mode\n");
			mode_status = 2;
		}
		else if(mode_status == 2) {
			printk("Change to Display Mode\n");
			mode_status = 1;
		}
	}	

	enable_irq(irq_pir);
	return IRQ_HANDLED;
}

static dev_t dev_num;
static struct cdev *cd_cdev;

static int __init display_mod_init(void) {
	int ret;
	mode_status = 1; //default: Display
	touched = 0;
	printk("Init Display Pi Module\n");

	//allocate character device
	alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
	cd_cdev = cdev_alloc();
	cdev_init(cd_cdev, &touch_fops);
	cdev_add(cd_cdev, dev_num, 1);

	//request GPIO and interrupt handler
	gpio_request_one(LED, GPIOF_OUT_INIT_LOW, "LED");
	gpio_set_value(LED, 0);
	
	gpio_request_one(PIR, GPIOF_IN, "PIR");
	irq_pir = gpio_to_irq(PIR);
	ret = request_irq(irq_pir, pir_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "sensor_irq", NULL); //Interrupt occurs (Low -> High, High -> Low)
	if(ret) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		free_irq(irq_pir, NULL);
	}
	else {
		disable_irq(irq_pir);
	}
	enable_irq(irq_pir);

	gpio_request_one(TOUCH, GPIOF_IN, "touch");
	irq_touch = gpio_to_irq(TOUCH);
	ret = request_irq(irq_touch, touch_isr, IRQF_TRIGGER_RISING, "sensor_irq", NULL);
	if(ret) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		free_irq(irq_touch, NULL);
	}
	else {
		disable_irq(irq_touch);
	}

	printk("Start PIR Timer\n");
	del_timer(&pir_timer);
	init_timer(&pir_timer);

	pir_timer.function = pir_timer_func;
	pir_timer.data = 1L;
	pir_timer.expires = jiffies + (5*HZ);
	add_timer(&pir_timer);

	return 0;
}

static void __exit display_mod_exit(void) {
	printk("Exit Display Pi Module\n");
	cdev_del(cd_cdev);
	unregister_chrdev_region(dev_num, 1);

	del_timer(&pir_timer);

	disable_irq(irq_pir);

	free_irq(irq_pir, NULL);
	free_irq(irq_touch, NULL);

	gpio_free(PIR);
	gpio_free(LED);
	gpio_free(TOUCH);
}

module_init(display_mod_init);
module_exit(display_mod_exit);