#include <linux/init.h>
#include <linux/module.h>
#include <linux/keyboard.h>
#include <linux/semaphore.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <asm/atomic.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vlad Kozlovskiy");
MODULE_DESCRIPTION("Calculates the number of pressed buttons per minute");
MODULE_VERSION("0.01");


static atomic_t cnt =  ATOMIC_INIT(0);
static struct timer_list timer;
static int irq = 1;

static irqreturn_t irq_handler(int irq, void *device){	
	atomic_inc(&cnt);
	return IRQ_HANDLED;
}

static void timer_func(struct timer_list *t){
	
	pr_info("[COUNTER] %d buttons were pressed last minute\n", cnt);
	atomic_set(&cnt, 0);
	mod_timer(t, jiffies + msecs_to_jiffies(60000));

}


static int __init press_counter_init(void){

	if (request_irq(irq, irq_handler, IRQF_SHARED, "my_keyboard_irq_handler", (void *)(irq_handler))) {
        	printk(KERN_ERR "[COUNTER] cannot register IRQ %d\n", irq);
        return -EIO;
    	}

	
	timer_setup(&timer, timer_func, 0);
	mod_timer(&timer, jiffies + msecs_to_jiffies(60000));

	pr_info("[COUNTER] module succesfully loaded\n");
	return 0;
}

static void __exit press_counter_exit(void){
	
	synchronize_irq(irq);
	free_irq(irq, (void *)(irq_handler));
    	del_timer(&timer);

	printk(KERN_INFO "[COUNTER] module succesfully unloaded\n");
}

module_init(press_counter_init);
module_exit(press_counter_exit);

