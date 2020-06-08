#include <linux/module.h>

#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/kernel.h>
#include <linux/major.h>

#include <linux/stat.h>
#include <linux/init.h>
#include <linux/device.h>

#include <linux/ide.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/interrupt.h>



#define LED_ON 1
#define LED_OFF 0

//define irq struct
struct key_irq_desc{
	int gpio;
	int irqnum;
	irqreturn_t (*handler)(int, void *);
	int flag;
};


//set dev struct
struct char_dev{
	dev_t devid;
	struct cdev cdev;		//struct for file operation
	struct class *class;	//class created by kernel in init operation
	struct device *device;  //struct created by kernel in init operation
	int major;
	int minor;
	struct device_node *nd_led; //device tree node led
	int gpio_led;
	struct device_node *nd_key; //device tree node key
	struct device_node *nd_key2; //device tree node key
 //	int gpio_key;
	struct key_irq_desc key1irqdesc;
	struct key_irq_desc key2irqdesc;
	struct tasklet_struct key_tasklet;
	struct tasklet_struct key2_tasklet;	
};

//define variables
static struct char_dev *led_dev;
static char dev_buf[100];
static DECLARE_WAIT_QUEUE_HEAD(key_wait);

//tasklet
void delay_short(volatile unsigned int n)
{
	while(n--){}
}

void delay(volatile unsigned int n)
{
	while(n--)
	{
		delay_short(0x7ff);
	}
}

void key_tasklet_opr(unsigned long data){
	struct key_irq_desc *keydesc;
	struct char_dev *devc = (struct char_dev *)data;
	keydesc = &devc->key1irqdesc;
	delay(10);
	if(gpio_get_value(devc->key1irqdesc.gpio) == 0){
		printk("key1 press confirmed.\r\n");		
		if(gpio_get_value(devc->gpio_led) == 0){
			gpio_set_value(devc->gpio_led, 1);
		}
		else if(gpio_get_value(devc->gpio_led) == 1){
			gpio_set_value(devc->gpio_led, 0);	
		}
		printk("key operated in tasklet, gpio_led=%d \r\n",gpio_get_value(devc->gpio_led));	
	}
}

void key2_tasklet_opr(unsigned long data){
 	struct key_irq_desc *keydesc;
	struct char_dev *devc = (struct char_dev *)data;
	keydesc = &devc->key2irqdesc;
	delay(10);
	if(gpio_get_value(devc->key2irqdesc.gpio) == 0){
		printk("key2 press confirmed, app wake up.\r\n");
		devc->key2irqdesc.flag = 1;
		wake_up_interruptible(&key_wait);
	}
/*
	if(gpio_get_value(devc->key2irqdesc.gpio) == 0){
	printk("key2 press confirmed.\r\n");		
		if(gpio_get_value(devc->gpio_led) == 0){
			gpio_set_value(devc->gpio_led, 1);
		}
		else if(gpio_get_value(devc->gpio_led) == 1){
			gpio_set_value(devc->gpio_led, 0);	
		}
	printk("key2 operated in tasklet, gpio_led=%d \r\n",gpio_get_value(devc->gpio_led));	
	}
*/

}

//irq handler
static irqreturn_t key1_irq_handler(int irq, void *dev_id){
	struct char_dev *devc = (struct char_dev *)dev_id;
	devc->key_tasklet.data = (unsigned long)dev_id;
	tasklet_schedule(&devc->key_tasklet);	
	return IRQ_RETVAL(IRQ_HANDLED);
	
}

static irqreturn_t key2_irq_handler(int irq, void *dev_id){

	
	struct char_dev *devc = (struct char_dev *)dev_id;
	devc->key2_tasklet.data = (unsigned long)dev_id;
	tasklet_schedule(&devc->key2_tasklet);	
	return IRQ_RETVAL(IRQ_HANDLED);
	
	
}


//hardware init
static int hw_init(struct char_dev *led_dev){
	int err = 0;	
//led init	
	printk("ready to init led dev.\r\n");

	led_dev->nd_led = of_find_node_by_path("/tstled");
	if(led_dev->nd_led == NULL){
		printk("node led not found.\r\n");
		return -1;
	}
	else{
		printk("node led has been found.\r\n");
	}

	led_dev->gpio_led = of_get_named_gpio(led_dev->nd_led, "gpios", 0);
	if(led_dev->gpio_led < 0){
		printk("get gpio_led failed.");
		return -1;
	}
	err = gpio_direction_output(led_dev->gpio_led, 1);
	if(err < 0) printk("set led gpio direction failed");

//key1 init
/*to do list:
1.find node in devicetree;
2.get gpio from node;
3.set up gpio-sub-system;
4.prepare irq num, init tasklet and irq_handler function;
5.register interrupt with irqnum, irq handler and tasklet.
*/
	printk("ready to init key dev.\r\n");

	led_dev->nd_key = of_find_node_by_path("/key1");
	if(led_dev->nd_key == NULL){
		printk("node key not found.\r\n");
		return -1;
	}
	else{
		printk("node key has been found.\r\n");
	}

	led_dev->key1irqdesc.gpio = of_get_named_gpio(led_dev->nd_key, "key-gpio", 0);
	if(led_dev->key1irqdesc.gpio < 0){
		printk("get key gpio failed.");
		return -1;
	}

	err = gpio_request(led_dev->key1irqdesc.gpio, "key1");
	err = gpio_direction_input(led_dev->key1irqdesc.gpio);
	if(err < 0) {
		printk("set key gpio direction failed");
		return -1;
	}
	
	led_dev->key1irqdesc.handler = key1_irq_handler;	
//	led_dev->key1irqdesc.irqnum = irq_of_parse_and_map(led_dev->nd_key, 0);
	led_dev->key1irqdesc.irqnum = gpio_to_irq(led_dev->key1irqdesc.gpio);	
	printk("key irqnum is:%d \r\n",led_dev->key1irqdesc.irqnum);
	if(led_dev->key1irqdesc.irqnum == 0){
		printk("failed to get an irq num.");
		return -1;
	}	
	tasklet_init(&led_dev->key_tasklet, key_tasklet_opr, led_dev->key_tasklet.data);
	err = request_irq(led_dev->key1irqdesc.irqnum, led_dev->key1irqdesc.handler, IRQF_TRIGGER_FALLING, "key1", led_dev);
	if(err < 0) {
		printk("key1 irq request failed");
		return -1;
	}

//key2 init
	//key2 init
	/*to do list:
	1.find node in devicetree;
	2.get gpio from node;
	3.set up gpio-sub-system;
	4.prepare irq num, irq_handler function;
	5.register interrupt with irqnum, irq handler.
	*/

	printk("ready to init key2 dev.\r\n");

	led_dev->nd_key2 = of_find_node_by_path("/key2");
	if(led_dev->nd_key2 == NULL){
		printk("node key2 not found.\r\n");
		return -1;
	}
	else{
		printk("node key2 has been found.\r\n");
	}

	led_dev->key2irqdesc.gpio = of_get_named_gpio(led_dev->nd_key2, "key-gpio", 0);
	if(led_dev->key2irqdesc.gpio < 0){
		printk("get key2 gpio failed.");
		return -1;
	}

	err = gpio_request(led_dev->key2irqdesc.gpio, "key2");
	err = gpio_direction_input(led_dev->key2irqdesc.gpio);
	if(err < 0) {
	printk("key2 direction set failed");
	return -1;
	}
	
	led_dev->key2irqdesc.handler = key2_irq_handler;	
	led_dev->key2irqdesc.irqnum = gpio_to_irq(led_dev->key2irqdesc.gpio);
	led_dev->key2irqdesc.flag = 0;
	printk("key2 irqnum is:%d \r\n",led_dev->key2irqdesc.irqnum);
	if(led_dev->key2irqdesc.irqnum == 0){
		printk("key2 failed to get an irq num.");
		return -1;
	}
	tasklet_init(&led_dev->key2_tasklet, key2_tasklet_opr, led_dev->key2_tasklet.data);
	err = request_irq(led_dev->key2irqdesc.irqnum, led_dev->key2irqdesc.handler, IRQF_TRIGGER_FALLING, "key2", led_dev);
	if(err < 0) {
		printk("key2 irq request failed");
		return -1;
	}
		
	return 0;
	
}

//read write open release func
static int led_open(struct inode *inode, struct file *file)
{
	file->private_data = led_dev;
	printk("chardev led opened.\r\n");
	return 0;
	
}


static ssize_t led_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	int err = 0;
	struct char_dev *devc = file->private_data;
	int key_status;

	wait_event_interruptible(key_wait, devc->key2irqdesc.flag);
	devc->key2irqdesc.flag = 0;
	if(gpio_get_value(devc->gpio_led) == 0){
		gpio_set_value(devc->gpio_led, 1);
	}
	else if(gpio_get_value(devc->gpio_led) == 1){
		gpio_set_value(devc->gpio_led, 0);	
	}
	printk("key2 operated, gpio_key=%d gpio_led=%d \r\n",gpio_get_value(devc->key2irqdesc.gpio),gpio_get_value(devc->gpio_led)); 


	key_status = gpio_get_value(devc->key2irqdesc.gpio);
	err = copy_to_user(buf, &key_status, sizeof(key_status));
	
	return 0;

}

static ssize_t led_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
	int err = 0;
	struct char_dev *devc = file->private_data;

	err = copy_from_user(dev_buf, buf, size);
 	printk("led operating, gpio_led=%d \r\n",gpio_get_value(devc->gpio_led));
	
	if(dev_buf[0] == LED_ON){
		gpio_set_value(devc->gpio_led, 0);
	}
	else if(dev_buf[0] == LED_OFF){
		gpio_set_value(devc->gpio_led, 1);
	}
	
	printk("led operated, gpio_led=%d \r\n",gpio_get_value(devc->gpio_led));

	return 0;
	
}

static int led_release(struct inode *inode, struct file *file)
{
	printk("chardev led released.\r\n");
	return 0;
}


//struct file opr
static struct file_operations led_dev_fops = {
	.owner = THIS_MODULE,
	.open=  led_open,
	.read = led_read,
	.write = led_write,
	.release = led_release,
};

//init and exit
static int __init led_init(void)
{
	int err;
	led_dev = kzalloc(sizeof(struct char_dev), GFP_KERNEL);

//hardware init	
	err = hw_init(led_dev);
	if(err < 0)printk("hardware init failed");

//register chardev
	//applicate a dev num
	if(led_dev->major){
		led_dev->devid = MKDEV(led_dev->major, 0);
		register_chrdev_region(led_dev->devid, 1, "led_dev");
	}
	else{
		alloc_chrdev_region(&led_dev->devid, 0, 1, "led_dev");
		led_dev->major = MAJOR(led_dev->devid);
		led_dev->minor = MINOR(led_dev->devid);
	}
	printk("led_dev major=%d minor=%d",led_dev->major,led_dev->minor);

	//initialize cdev
	led_dev->cdev.owner = THIS_MODULE;
	cdev_init(&led_dev->cdev, &led_dev_fops);
	
	//add cdev
	cdev_add(&led_dev->cdev, led_dev->devid, 1);

	//create class
	led_dev->class = class_create(THIS_MODULE, "led_dev");
	if(IS_ERR(led_dev->class)) return PTR_ERR(led_dev->class);

	//create device
	led_dev->device = device_create(led_dev->class, NULL, led_dev->devid, NULL, "led_dev");
	if(IS_ERR(led_dev->device)) return PTR_ERR(led_dev->device);
	
	return 0;
}

static void __exit led_exit(void)
{
	//release interrupt
	free_irq(led_dev->key1irqdesc.irqnum, led_dev);
	free_irq(led_dev->key2irqdesc.irqnum, led_dev);
	
	//unregister dev
	cdev_del(&led_dev->cdev);
	unregister_chrdev_region(led_dev->devid, 1);

	device_destroy(led_dev->class, led_dev->devid);
	class_destroy(led_dev->class);
	
	kfree(led_dev);
	
	printk("led dev unregisted.\r\n");
}

module_init(led_init);
module_exit(led_exit);

//appendix
MODULE_LICENSE("GPL");
MODULE_AUTHOR("XJJ");




