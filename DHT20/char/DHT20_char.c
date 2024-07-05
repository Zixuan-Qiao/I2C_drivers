#include "DHT20_char.h"

ssize_t DHT20_read(struct file *filp, char __user *buff, size_t size, loff_t *loff) {
	int i, result;
	char cmd_r[CMD_LEN + 1], data[READ_LEN + 2];
	struct i2c_client *client;
	
	if(size < sizeof(char) * READ_LEN) {
		PDEBUG("Insufficient buffer size, %d required. \n", READ_LEN);
		return 0;
	}
	
	cmd_r[0] = CMD_1;
	cmd_r[1] = CMD_2;
	cmd_r[2] = CMD_3;
	cmd_r[3] = '\0';
	
	client = filp->private_data;
	
	result = i2c_master_send(client, cmd_r, CMD_LEN);
	if(result < 0) {
		PDEBUG("Read command transmission failed. \n");
		return 0;
	}
	
	mdelay(80);
	
	data[READ_LEN + 1] = '\0';
	
	result = i2c_master_recv(client, data, READ_LEN + 1);
	if(result < 0) {
		PDEBUG("Return data transmission failed. \n");
		return 0;
	}	
	
	if(copy_to_user(buff, data + 1, sizeof(char) * READ_LEN)) {
		PDEBUG("Copying failed. \n");
		return 0;
	}
			
	return sizeof(char) * READ_LEN;
}

long DHT20_ioctl(struct file *filp, unsigned int command, unsigned long buff) {
	int result, adpt_nr;
	struct i2c_adapter *adpt_ptr;
	struct i2c_client *client;
	char init_byte[2], status[2];
	
	switch(command) {
		case INIT_SET_ADPT:
			if(copy_from_user(&adpt_nr, (int *)buff, sizeof(int))) {
				PDEBUG("Copying failed. \n");
				return -EFAULT;
			}
			
			PDEBUG("Received adapter number: %d. \n", adpt_nr);
			
			adpt_ptr = i2c_get_adapter(adpt_nr);
			if(!adpt_ptr) {
				PDEBUG("Invaild adapter number. \n");
				return -ENODEV;
			}
			
			client = filp->private_data;
			client->adapter = adpt_ptr;
			
			mdelay(100);
			
			init_byte[0] = INIT_CMD;
			init_byte[1] = '\0';
			
			result = i2c_master_send(client, init_byte, 1);
			if(result < 0) {
				PDEBUG("Init command transmission failed. \n");
				return result;
			}
			
			status[1] = '\0';
			
			result = i2c_master_recv(client, status, 1);
			if(result < 0) {
				PDEBUG("Status transmission failed. \n");
				return result;
			}
			
			PDEBUG("Status received: %02X. \n", status[0]);
			
			status[0] |= 0x18;
			if(status[0] != 0x18) {
				PDEBUG("Error in initialization. \n");
				return -EFAULT;
			}
			
			mdelay(10);
			
			break;
			
		default:
			PDEBUG("Command not found. \n");
			return -EFAULT;
	}
	
	return 0;

}

int DHT20_open(struct inode *inode, struct file *filp) {
	struct i2c_client *new_client;
	
	PDEBUG("Device file opened. \n");
	
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
	
	PDEBUG("File pointer released. \n");
	
	return 0;
}

static struct file_operations DHT20_fops = {
	.owner = THIS_MODULE, 
	.open = DHT20_open,
	.read = DHT20_read,
	.unlocked_ioctl = DHT20_ioctl,
	.release = DHT20_release,
};

static dev_t DHT20_id;
static struct cdev DHT20_cdev;
static struct class *DHT20_class;

static int __init DHT20_init(void) {
	int result;
	struct device *dev_res;
	
	result = alloc_chrdev_region(&DHT20_id, 0, 1, "DHT20");
	if(result < 0) {
		PDEBUG("Failed when requesting device number. \n");
		return result;
	}
	
	DHT20_class = class_create(THIS_MODULE, "DHT20");
	result = (int)PTR_ERR_OR_ZERO(DHT20_class);
	if(result) {
		PDEBUG("Failed when creating device class. \n");
		goto class_fail;
	}

	cdev_init(&DHT20_cdev, &DHT20_fops);
	
	DHT20_cdev.owner = THIS_MODULE;
	
	result = cdev_add(&DHT20_cdev, DHT20_id, 1);
	if(result < 0) {
		goto cdev_fail;
	}
	
	PDEBUG("cdev added. \n");
	
	dev_res= device_create(DHT20_class, NULL, DHT20_id, NULL, "DHT20");
	result = (int)PTR_ERR_OR_ZERO(dev_res);
	if(result) {
		goto create_fail;
	}
	
	PDEBUG("Device file created. \n");
	
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
	
	PDEBUG("Driver unloaded. \n");
}

module_init(DHT20_init);
module_exit(DHT20_exit);
