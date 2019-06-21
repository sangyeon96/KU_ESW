#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");

#define LED 4
#define PIR 18

static int irq_num;
static struct timer_list my_timer;

static void my_timer_func(unsigned long data) {
	if(gpio_get_value(PIR)) {
		printk("No one is in the elevator\n");
		gpio_set_value(LED, 0);
	}
	else {
		printk("Somebody is in the elevator\n");
		gpio_set_value(LED, 1);
	}
}

static irqreturn_t pir_isr(int irq, void* dev_id) {
	//Interrupt Handler Function

	//RISING: 0, FALLING: 1
	if(gpio_get_value(PIR)) {
		printk("Falling detected\n");
	}
	else {
		printk("Rising detected\n");
	}

	del_timer(&my_timer);
	init_timer(&my_timer);
	my_timer.function = my_timer_func;
	my_timer.expires = jiffies + (5*HZ); //5 second
	add_timer(&my_timer);

	return IRQ_HANDLED;
}

static int __init pir_init(void) {
	int ret;

	printk("Init PIR Module\n");

	//request GPIO and interrupt handler
	gpio_request_one(LED, GPIOF_OUT_INIT_LOW, "LED");
	gpio_set_value(LED, 0);
	
	gpio_request_one(PIR, GPIOF_IN, "PIR");
	irq_num = gpio_to_irq(PIR);
	ret = request_irq(irq_num, pir_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "sensor_irq", NULL); //Interrupt occurs (Low -> High, High -> Low)
	enable_irq(irq_num);

	printk("Start PIR Timer\n");
	init_timer(&my_timer);

	my_timer.function = my_timer_func;
	my_timer.data = 1L;
	my_timer.expires = jiffies + (5*HZ);
	add_timer(&my_timer);

	return 0;
}

static void __exit pir_exit(void) {
	printk("Exit PIR Module\n");

	del_timer(&my_timer);
	disable_irq(irq_num);
	free_irq(irq_num, NULL);
	gpio_free(PIR);
	gpio_free(LED);
}

module_init(pir_init);
module_exit(pir_exit);