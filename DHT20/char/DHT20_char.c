#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/ioctl.h>
#include <linux/mutex.h>

#define DHT20_ADDR 0x38
#define INIT_SET_ADPT _IOW('D', 1, int)

/*
init -> init one char device, create inode /dev/DHT20
open -> allocate a client, then associate it with filp->private_data
ioctl -> allow user app to specify adapter number (address is fixed)
read -> send bit sequences according to data sheet to obtain data and return them to user space
release -> free the client, called when a filps use count drops to 0
exit -> unregister the cdev
*/

static dev_t DHT20_id;
static unsigned int DHT20_major;
static unsigned int DHT20_minor;
static struct cdev DHT20_cdev;
static struct class *DHT20_class;

ssize_t DHT20_read(struct file *filp, char __user *buff, size_t size, loff_t *loff) {

	int i;
	int res;
	
	char cmd_r[4];
	unsigned char data[7];
	
	struct i2c_client *curr_client;
	
	cmd_r[0] = 0xAC;
	cmd_r[1] = 0x33;
	cmd_r[2] = 0x00;
	cmd_r[3] = '\0';
	
	curr_client = filp->private_data;
	res = i2c_master_send(curr_client, cmd_r, 3);
	if(IS_ERR_VALUE(res)) {
		printk("Read command transmission failed. \n");
		return res;
	}
	
	mdelay(80);
	
	data[6] = '\0';
	
	res = i2c_master_recv(curr_client, data, 6);
	if(IS_ERR_VALUE(res)) {
		printk("Return data transmission failed. \n");
		return res;
	}
	
	printk("Received data: ");
	for(i = 0; i < 6; i++) {
		printk("%#x", data[i]);
	}	
	
	res = copy_to_user(buff, data + 1, 5);
	if(res) {
		printk("Copying failed, error code %d. \n", res);
		return res;
	}
			
	return 0;
}

long DHT20_ioctl(struct file *filp, unsigned int command, unsigned long buff) {
	int adpt_nr;
	int res;
	struct i2c_adapter *adpt;
	struct i2c_client *curr_client;
	char init_byte[2];
	char status[2];
	
	switch(command) {
		case INIT_SET_ADPT:
			printk("Ioctl INIT_SET_ADPT invoked. \n");
			if(copy_from_user(&adpt_nr, (int *)buff, sizeof(int))) {
				printk("Copying from user failed. \n");
				return -EFAULT;
			}
			
			printk("Adapter number: %d. \n", adpt_nr);
			
			adpt = i2c_get_adapter(adpt_nr);
			if(!adpt) {
				printk("Invaild adapter number. \n");
				return -ENODEV;
			}
			
			curr_client = filp->private_data;
			curr_client->adapter = adpt;
			
			mdelay(100);
			
			init_byte[0] = 0x71;
			init_byte[1] = '\0';
			
			res = i2c_master_send(curr_client, init_byte, 1);
			
			if(IS_ERR_VALUE(res)) {
				printk("Init byte transmission failed, error code: %d. \n", res);
				return res;
			}
			
			status[1] = '\0';
			
			res = i2c_master_recv(curr_client, status, 1);
			if(IS_ERR_VALUE(res)) {
				printk("Status byte transmission failed. \n");
				return res;
			}
			
			printk("Status received: %#x. \n", status[0]);
			
			status[0] |= 0x18;
			if(status[0] != 0x18) {
				printk("Error in initialization. \n");
				return -ENOTTY;
			}
			
			printk("Initialization succeed! \n");
			
			mdelay(10);
			
			break;
		default:
			printk("Command not found. \n");
	}
	
	return 0;

}

int DHT20_open(struct inode *inode, struct file *filp) {
	struct i2c_client *new_client;
	printk("DHT20 opened. \n");
	
	new_client = (struct i2c_client *)kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if(!new_client) {
		return -ENOMEM;
	}
	
	new_client->addr = DHT20_ADDR;
	
	filp->private_data = new_client;
	
	return 0;
}

int DHT20_release(struct inode *inode, struct file *filp) {
	kfree(filp->private_data);
	
	printk("DHT20 file pointer released. \n");
	return 0;
}

static struct file_operations DHT20_fops = {
	.owner = THIS_MODULE, 
	.open = DHT20_open,
	.read = DHT20_read,
	.unlocked_ioctl = DHT20_ioctl,
	.release = DHT20_release,
};

static int __init DHT20_init(void) {

	int result;
	struct device *dev_res;
	result = alloc_chrdev_region(&DHT20_id, 0, 1, "DHT20");
	
	if(result < 0) {
		printk("Failed when requesting device number. \n");
		return result;
	}
	DHT20_major = MAJOR(DHT20_id);
	DHT20_minor = MINOR(DHT20_id);
	
	DHT20_class = class_create(THIS_MODULE, "DHT20");
	if(IS_ERR(DHT20_class)) {
		goto class_fail;
	}

	cdev_init(&DHT20_cdev, &DHT20_fops);
	printk("DHT20 cdev driver init. \n");
	
	DHT20_cdev.owner = THIS_MODULE;
	
	printk("DHT20 cdev add. \n");
	if(cdev_add(&DHT20_cdev, DHT20_id, 1)) {
		goto cdev_fail;
	}
	
	printk("DHT20 cdev added. \n");
	
	dev_res= device_create(DHT20_class, NULL, DHT20_id, NULL, "DHT20");
	if(PTR_ERR_OR_ZERO(dev_res)) {
		goto create_fail;
	}
	printk("DHT20 directory created. \n");
	
	return 0;
	
create_fail:		
	cdev_del(&DHT20_cdev);

cdev_fail:
	class_destroy(DHT20_class);

class_fail:
	unregister_chrdev_region(DHT20_id, 1);
	
	return result;
}

static void __exit DHT20_exit(void) {
	device_destroy(DHT20_class, DHT20_id);
		
	cdev_del(&DHT20_cdev);

	class_destroy(DHT20_class);
	
	unregister_chrdev_region(DHT20_id, 1);
	
	printk("DHT20 unloaded\n");
}

module_init(DHT20_init);
module_exit(DHT20_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Providing interface to user-space for DHT20 sensor");
