#include "STTS22H.h"

int config_register(struct i2c_client *i2c_client, u8 reg_addr, u8 config) {
	int res;
	s32 data;
	
	res = i2c_smbus_write_byte_data(i2c_client, reg_addr, config);
	if(res) {
		PDEBUG("Failed when sending command to register: %02X. \n", reg_addr);
		return res;
	}
	
	// return value of i2c_smbus_read_byte_data is s32 type
	data = i2c_smbus_read_byte_data(i2c_client, reg_addr);
	if(data < 0) {
		PDEBUG("Failed when reading the value of register: %02X. \n", reg_addr);
		return data;
	}
	
	if((u8)data != config) {
		PDEBUG("Failed when initializing register: %02X, config: %02X, value: %02X. \n", reg_addr, config, (u8)data);
		return -EAGAIN;
	}
	
	return 0;
}

/**
 * set the address and adapter number of i2c cilent
 * check if it is already occupied
 * if it is not, add it to the list
 */
ssize_t STTS22H_write(struct file *filp, const char __user *data, size_t size, loff_t *loff) {
	u8 addr_nr;
	int adpt_nr;
	s32 result;
	struct STTS22H_data *STTS22H_data;
	struct i2c_adapter *adpt_ptr;
	char addr_adpt[WRITE_LEN + 1];
	
	PDEBUG("Write invoked. \n");
	
	if(sizeof(char) * WRITE_LEN != size) {
		PDEBUG("Incorrect argument format (requires a len 2 char array). \n");
		return 0;
	}
	
	addr_adpt[WRITE_LEN] = '\0';
	
	if(copy_from_user(addr_adpt, data, WRITE_LEN)) {
		PDEBUG("Copying failed. \n");
		return 0;
	}
	
	addr_nr = (u8)addr_adpt[0];
	adpt_nr = (int)addr_adpt[1];
	
	PDEBUG("Received address number: %02X, adapter number: %d. \n", addr_nr, adpt_nr);
	
	adpt_ptr = i2c_get_adapter(adpt_nr);
	if(!adpt_ptr) {
		PDEBUG("Invalid adapter number. \n");
		return 0;
	}
	
	if(mutex_lock_interruptible(&list_lock)) {
		PDEBUG("Cannot perform mutex locking, restart system. \n");
		return 0;
	}
	
	if(!list_empty(&client_list)) {
		list_for_each_entry(STTS22H_data, &client_list, entry) {
			if(STTS22H_data->client->addr == addr_nr && STTS22H_data->adpt == adpt_nr) {
				PDEBUG("Device busy, try a different address / adapter. \n");
				return 0;
			}
		}
	}
	
	STTS22H_data = filp->private_data; 
	
	STTS22H_data->adpt = adpt_nr;
	
	STTS22H_data->client = (struct i2c_client *)kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if(!STTS22H_data->client) {
		mutex_unlock(&list_lock);
		PDEBUG("No memory. \n");
		return 0;
	}
	
	STTS22H_data->client->addr = addr_nr;
	STTS22H_data->client->adapter = adpt_ptr;
	
	list_add_tail(&STTS22H_data->entry, &client_list);	// queue
	
	mutex_unlock(&list_lock);
	
	// check whoima
	result = i2c_smbus_read_byte_data(STTS22H_data->client, WHOAMI_REG);
	if(result < 0) {
		PDEBUG("Failed when getting chip id. \n");
		return 0;
	}
	
	PDEBUG("Chip id: %02X. \n", (u8)result);
	
	if((u8)result != CHIP_ID) {
		PDEBUG("Specified device is not STTS22H. \n");
		if(mutex_lock_interruptible(&list_lock)) {
			PDEBUG("Cannot perform mutex locking, restart system. \n");
			return 0;
		}
		
		list_del(&STTS22H_data->entry);
		
		mutex_unlock(&list_lock);
		
		STTS22H_data->adpt = -1;
		
		return 0;
	}
	
	return WRITE_LEN * sizeof(char);
}

ssize_t STTS22H_read(struct file *filp, char __user *buff, size_t size, loff_t *loff) {
	u8 config;
	s32 result;
	int counter;
	char data[READ_LEN + 1];
	struct STTS22H_data *STTS22H_data;
	
	if(READ_LEN * sizeof(char) > size) {
		PDEBUG("Insufficient buffer size (requires a len 2 char array). \n");
		return 0;
	}
	
	STTS22H_data = filp->private_data;
	
	if(mutex_lock_interruptible(&STTS22H_data->lock)) {
		PDEBUG("Cannot perform mutex locking, restart system. \n");
		return 0;
	}
	
	if(STTS22H_data->mode == 0) {	// one-shot mode
		// get current config
		result = i2c_smbus_read_byte_data(STTS22H_data->client, CTRL_REG);
		if(result < 0) {
			PDEBUG("Failed when getting current config. \n");
			return 0;
		}
		
		config = (u8)result | ONE_SHOT_GET;
		
		if(config_register(STTS22H_data->client, CTRL_REG, config) < 0) {
			PDEBUG("Failed when setting up one-shot acquisition bit. \n");
			return 0;
		}
		
		counter = 0;
		while(counter < MAX_ATTP) {
			result = i2c_smbus_read_byte_data(STTS22H_data->client, STATUS_REG);
			if(result < 0) {
				PDEBUG("Failed when getting data. \n");
				return 0;
			}
			
			if((u8)result & 0x01) {
				PDEBUG("Data conversion in process. \n");
				counter++;
			} else
				break;
		}
		
		if(counter == MAX_ATTP) {
			PDEBUG("Data conversion failed. \n");
			return 0;
		}
	}
	
	data[READ_LEN] = '\0';
	
	result = i2c_smbus_read_byte_data(STTS22H_data->client, TEMP_LSB_REG);
	if(result < 0) {
		PDEBUG("Failed when getting LSB data. \n");
		return 0;
	}
	
	data[0] = (char)result;
	
	result = i2c_smbus_read_byte_data(STTS22H_data->client, TEMP_MSB_REG);
	if(result < 0) {
		PDEBUG("Failed when getting MSB data. \n");
		return 0;
	}
	
	data[1] = (char)result;
	
	mutex_unlock(&STTS22H_data->lock);
	
	if(copy_to_user(buff, data, READ_LEN * sizeof(char))) {
		PDEBUG("Failed when copying data to user. \n");
		return 0;
	}
	
	return READ_LEN * sizeof(char);
}

long STTS22H_ioctl(struct file *filp, unsigned int command, unsigned long buff) {
	u8 config;
	int counter, cur, result, mode_nr, odr;
	struct STTS22H_data *STTS22H_data;
	
	switch(command) {
		case PR_LIST:
			if(mutex_lock_interruptible(&list_lock)) {
				PDEBUG("Cannot perform mutex locking, restart system. \n");
				return -ERESTARTSYS;
			}
		
			if(!list_empty(&client_list)) {
				counter = 1;
				list_for_each_entry(STTS22H_data, &client_list, entry) {
					PDEBUG("Device number: %d, address: %02X, adapter: %d, mode: %d. \n", counter, STTS22H_data->client->addr, STTS22H_data->adpt, STTS22H_data->mode);
					counter++;
				}
			} else {
				PDEBUG("Device list empty. \n");
			}
			
			mutex_unlock(&list_lock);	
			
			break;
			
		case CHG_MODE:
			if(copy_from_user(&mode_nr, (int *)buff, sizeof(int))) {
				PDEBUG("Copying from user failed. \n");
				return -EFAULT;
			}
		
			STTS22H_data = filp->private_data;
			
			if(mutex_lock_interruptible(&STTS22H_data->lock)) {
				PDEBUG("Cannot perform mutex locking, restart system. \n");
				return -ERESTARTSYS;
			}
		
			// get current config
			result = i2c_smbus_read_byte_data(STTS22H_data->client, CTRL_REG);
			if(result < 0) {
				PDEBUG("Failed when getting current config. \n");
				return result;
			}
			
			// power down device before changing mode
			config = (u8)result & LOW_ODR_DIS & FREE_RUN_DIS;
			
			result = config_register(STTS22H_data->client, CTRL_REG, config);
			if(result < 0) {
				PDEBUG("Failed when powering down device. \n");
				return result;
			}
			
			switch(mode_nr) {
				case one_shot:
					STTS22H_data->mode = 0;
					mutex_unlock(&STTS22H_data->lock);
					return 0;
					
				case free_run:
					config |= FREE_RUN_EN;		
					break;
					
				case low_odr:
					config |= LOW_ODR_EN;					
					break;
					
				default:
					PDEBUG("Invalid mode number. \n");
					return -EFAULT;
			}
			
			result = config_register(STTS22H_data->client, CTRL_REG, config);
			if(result < 0) {
				PDEBUG("Failed when changing mode. \n");
				return result;
			}
			
			STTS22H_data->mode = mode_nr;
			
			mutex_unlock(&STTS22H_data->lock);
			
			break;
			
		case CHG_ODR:
			if(copy_from_user(&odr, (int *)buff, sizeof(int))) {
				PDEBUG("Copying from user failed. \n");
				return -EFAULT;
			}
		
			STTS22H_data = filp->private_data;
			
			if(mutex_lock_interruptible(&STTS22H_data->lock)) {
				PDEBUG("Cannot perform mutex locking, restart system. \n");
				return -ERESTARTSYS;
			}
		
			// get current config
			result = i2c_smbus_read_byte_data(STTS22H_data->client, CTRL_REG);
			if(result < 0) {
				PDEBUG("Failed when getting current config. \n");
				return result;
			}
			
			cur = (u8)result;
			
			// power down device before changing ODR
			config = cur & LOW_ODR_DIS & FREE_RUN_DIS;
			
			result = config_register(STTS22H_data->client, CTRL_REG, config);
			if(result < 0) {
				PDEBUG("Failed when powering down device. \n");
				return result;
			}
			
			config = cur & ODR_CLEAR;
			
			switch(odr) {
				case HZ_25:
					config |= ODR_RATE_25;		
					break;
					
				case HZ_50:
					config |= ODR_RATE_50;		
					break;
					
				case HZ_100:
					config |= ODR_RATE_100;					
					break;
					
				case HZ_200:
					config |= ODR_RATE_200;					
					break;
					
				default:
					PDEBUG("Invalid mode number. \n");
					return -EFAULT;
			}
			
			result = config_register(STTS22H_data->client, CTRL_REG, config);
			if(result < 0) {
				PDEBUG("Failed when changing ODR rate. \n");
				return result;
			}
			
			mutex_unlock(&STTS22H_data->lock);
			
			break;
			
		default:
			PDEBUG("Invalid command. \n");
			return -EFAULT;
	}
	
	return 0;
}

int STTS22H_open(struct inode *inode, struct file *filp) {
	struct STTS22H_data *STTS22H_data;
	
	PDEBUG("Device file opened. \n");
	
	STTS22H_data = (struct STTS22H_data*)kzalloc(sizeof(struct STTS22H_data), GFP_KERNEL);
	if(!STTS22H_data) {
		return -ENOMEM;
	}
	
	STTS22H_data->adpt = -1;
	
	mutex_init(&STTS22H_data->lock);
	
	filp->private_data = STTS22H_data;
	
	return 0;
}

int STTS22H_release(struct inode *inode, struct file *filp) {
	struct STTS22H_data *STTS22H_data;
	
	STTS22H_data = filp->private_data;
	if(STTS22H_data->adpt != -1) {	// device is in the list
		if(mutex_lock_interruptible(&list_lock)) {
			PDEBUG("Cannot perform mutex locking, restart system. \n");
			return -ERESTARTSYS;
		}
	
		list_del(&STTS22H_data->entry);
		
		mutex_unlock(&list_lock);
	}
	
	kfree(STTS22H_data->client);
	
	kfree(STTS22H_data);
	
	PDEBUG("File pointer released. \n");
	return 0;
}

static struct file_operations STTS22H_fops = {
	.owner = THIS_MODULE, 
	.open = STTS22H_open,
	.write = STTS22H_write,
	.read = STTS22H_read,
	.unlocked_ioctl = STTS22H_ioctl,
	.release = STTS22H_release,
};


static dev_t STTS22H_id;
static struct cdev STTS22H_cdev;
static struct class *STTS22H_class;

static int __init STTS22H_init(void) {
	int result;
	struct device *dev_res;
	
	result = alloc_chrdev_region(&STTS22H_id, 0, 1, "STTS22H");
	if(result < 0) {
		PDEBUG("Failed when requesting device number. \n");
		return result;
	}
	
	STTS22H_class = class_create(THIS_MODULE, "temp_sensor");
	result = (int)PTR_ERR_OR_ZERO(STTS22H_class);
	if(result) {
		PDEBUG("Failed when creating device class. \n");
		goto class_fail;
	}

	cdev_init(&STTS22H_cdev, &STTS22H_fops);	// returns void
	PDEBUG("Character device driver initialized. \n");
	
	STTS22H_cdev.owner = THIS_MODULE;
	
	result = cdev_add(&STTS22H_cdev, STTS22H_id, 1);
	if(result < 0) {
		goto cdev_fail;
	}
	
	PDEBUG("cdev added. \n");
	
	dev_res= device_create(STTS22H_class, NULL, STTS22H_id, NULL, "STTS22H");
	result = (int)PTR_ERR_OR_ZERO(dev_res);
	if(result) {
		goto create_fail;
	}
	
	PDEBUG("Device file created. \n");
	
	mutex_init(&list_lock);
	
	return 0;
	
create_fail:		
	cdev_del(&STTS22H_cdev);

cdev_fail:
	class_destroy(STTS22H_class);

class_fail:
	unregister_chrdev_region(STTS22H_id, 1);
	
	return result;
}

static void __exit STTS22H_exit(void) {
	device_destroy(STTS22H_class, STTS22H_id);
		
	cdev_del(&STTS22H_cdev);

	class_destroy(STTS22H_class);
	
	unregister_chrdev_region(STTS22H_id, 1);
	
	PDEBUG("Device unloaded. \n");
}

module_init(STTS22H_init);
module_exit(STTS22H_exit);
