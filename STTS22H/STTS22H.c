// SPDX-License-Identifier: GPL-2.0-or-later
/* STMicroelectronics temperature sensor STTS22H driver
   
   Allow users to manage STTS22H from user-space,
   change operation modes and obtain data from it

   Copyright (c) 2024,2024 Zixuan Qiao <zqiao104@uottawa.ca> */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/mutex.h>

#define DEBUG
#ifdef DEBUG
#define PDEBUG(format, args...) printk(KERN_ERR "STTS22H: " format, ## args)
#else 
#define PDEBUG(format, args...)
#endif

#define WRITE_LEN 2
#define READ_LEN 2
#define MAX_ATTP 100

#define PR_LIST _IO('S', 1)
#define CHG_MODE _IOW('S', 2, int)
#define CHG_ODR _IOW('S', 3, int)

/* Register address */
#define WHOAMI_REG 0x01
#define CHIP_ID 0xA0

#define HIGH_LIMIT_REG 0x02
#define LOW_LIMIT_REG 0x03

#define CTRL_REG 0x04

#define STATUS_REG 0x05

#define TEMP_LSB_REG 0x06
#define TEMP_MSB_REG 0x07

/* Configuration value */
#define BLK_DATA_UD 0x40
#define AUTO_ADDR_INC 0x08
#define TIME_OUT_EN 0x00

#define ODR_CLEAR 0xCF
#define ODR_RATE_25 0x00
#define ODR_RATE_50 0x10
#define ODR_RATE_100 0x20
#define ODR_RATE_200 0x30

#define LOW_ODR_EN 0x80
#define LOW_ODR_DIS 0x7F
#define FREE_RUN_EN 0x04
#define FREE_RUN_DIS 0xFB
#define ONE_SHOT_GET 0x01

#define CONV_IN_PROG 0x01

enum mode {
	one_shot = 0,
	free_run = 1,
	low_odr = 2,
};

enum odr_rate {
	HZ_25 = 0,
	HZ_50 = 1,
	HZ_100 = 2,
	HZ_200 = 3,
};

struct STTS22H_data {
	struct i2c_client *client;
	struct list_head entry;
	struct mutex lock;
	int mode;
	int adpt;
};

static LIST_HEAD(client_list);
static struct mutex list_lock;

/*
 * config_register - Write value to a register and verify the result
 * Return error number on error, 0 on success
 */
static int
config_register(struct i2c_client *i2c_client, u8 reg_addr, u8 config)
{
	int res;
	s32 data;
	
	res = i2c_smbus_write_byte_data(i2c_client, reg_addr, config);
	if (res) {
		PDEBUG(
		"Failed when sending command to register: %02X\n", reg_addr);
		return res;
	}
	
	data = i2c_smbus_read_byte_data(i2c_client, reg_addr);
	if (data < 0) {
		PDEBUG(
		"Failed when reading the value of register: %02X\n", reg_addr);
		return data;
	}
	
	if ((u8)data != config) {
		PDEBUG(
		"Failed when initializing register: %02X, current value: %02X\n",
							reg_addr, (u8)data);
		return -EAGAIN;
	}
	
	return 0;
}

/*
 * STTS22H_write - Allow the user to setup the address and adapter
 * 		   number of an i2c cilent and perform validation
 * Return 0 on error, number of written bytes on success
 */
static ssize_t
STTS22H_write(struct file *filp, const char __user *data,
				size_t size, loff_t *loff)
{
	u8 addr_nr, config;
	int adpt_nr;
	s32 result;
	struct STTS22H_data *STTS22H_data;
	struct i2c_adapter *adpt_ptr;
	char addr_adpt[WRITE_LEN + 1];
	
	if (sizeof(char) * WRITE_LEN != size) {
		PDEBUG(
		"Incorrect argument format, requires a len %d char array\n",
								WRITE_LEN);
		return 0;
	}
	
	addr_adpt[WRITE_LEN] = '\0';
	
	if (copy_from_user(addr_adpt, data, WRITE_LEN)) {
		PDEBUG("Copying failed\n");
		return 0;
	}
	
	addr_nr = (u8)addr_adpt[0];
	adpt_nr = (int)addr_adpt[1];
	
	adpt_ptr = i2c_get_adapter(adpt_nr);
	if (!adpt_ptr) {
		PDEBUG("Invalid adapter number\n");
		return 0;
	}
	
	if (mutex_lock_interruptible(&list_lock)) {
		i2c_put_adapter(adpt_ptr);
		PDEBUG("Cannot perform mutex locking, restart system\n");
		return 0;
	}
	
	if (!list_empty(&client_list)) {
		list_for_each_entry(STTS22H_data, &client_list, entry) {
			if (STTS22H_data->client->addr == addr_nr 
					&& STTS22H_data->adpt == adpt_nr) {
				mutex_unlock(&list_lock);
				i2c_put_adapter(adpt_ptr);
				PDEBUG(
				"Device busy, try a different address/adapter\n");
				return 0;
			}
		}
	}
	
	STTS22H_data = filp->private_data; 
	
	if (mutex_lock_interruptible(&STTS22H_data->lock)) {
		mutex_unlock(&list_lock);
		i2c_put_adapter(adpt_ptr);
		PDEBUG("Cannot perform mutex locking, restart system\n");
		return 0;
	}	
	
	STTS22H_data->client = 
	(struct i2c_client *)kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
	
	if (!STTS22H_data->client) {
		mutex_unlock(&STTS22H_data->lock);
		mutex_unlock(&list_lock);
		i2c_put_adapter(adpt_ptr);
		PDEBUG("No memory\n");
		return 0;
	}
	
	STTS22H_data->client->addr = addr_nr;
	STTS22H_data->client->adapter = adpt_ptr;
	
	/* The list behaves like a queue */
	list_add_tail(&STTS22H_data->entry, &client_list);
	
	/* A non-negative adpt value indicates that the device is on list */
	STTS22H_data->adpt = adpt_nr;
	
	mutex_unlock(&list_lock);
	
	/* Check chip id */
	result = i2c_smbus_read_byte_data(STTS22H_data->client, WHOAMI_REG);
	if (result < 0) {
		mutex_unlock(&STTS22H_data->lock);
		PDEBUG("Failed when getting chip id\n");
		return 0;
	}
	
	if ((u8)result != CHIP_ID) {
		PDEBUG("Specified device is not STTS22H\n");
		
		/* Remove record from list */
		if (mutex_lock_interruptible(&list_lock)) {
			mutex_unlock(&STTS22H_data->lock);
			PDEBUG(
			"Cannot perform mutex locking, restart system\n");
			return 0;
		}
		
		list_del(&STTS22H_data->entry);
		STTS22H_data->adpt = -1;
		
		mutex_unlock(&list_lock);
		
		kfree(STTS22H_data->client);
		STTS22H_data->client = NULL;
		
		mutex_unlock(&STTS22H_data->lock);
		
		i2c_put_adapter(adpt_ptr);
		
		return 0;
	}
	
	/* Initialize device to default mode (one-shot) */
	result = i2c_smbus_read_byte_data(STTS22H_data->client, CTRL_REG);
	if (result < 0) {
		mutex_unlock(&STTS22H_data->lock);
		PDEBUG(
		"Failed when getting current config for initialization\n");
		return 0;
	}
	
	config = (u8)result & LOW_ODR_DIS & FREE_RUN_DIS;
	
	result = config_register(STTS22H_data->client, CTRL_REG, config);
	if (result < 0) {
		mutex_unlock(&STTS22H_data->lock);
		PDEBUG("Failed when changing mode for initialization\n");
		return 0;
	}
	
	mutex_unlock(&STTS22H_data->lock);
	
	return WRITE_LEN * sizeof(char);
}

/*
 * STTS22H_read - Obtain temperature data based on current operation mode 
 * Return 0 on error, number of read bytes on success
 */
static ssize_t
STTS22H_read(struct file *filp, char __user *buff, size_t size, loff_t *loff)
{
	u8 config;
	s32 result;
	int counter;
	char data[READ_LEN + 1];
	struct STTS22H_data *STTS22H_data;
	
	if (READ_LEN * sizeof(char) > size) {
		PDEBUG(
		"Insufficient buffer size, requires a len %d char array\n",
								READ_LEN);
		return 0;
	}
	
	STTS22H_data = filp->private_data;
	
	if (mutex_lock_interruptible(&STTS22H_data->lock)) {
		PDEBUG("Cannot perform mutex locking, restart system\n");
		return 0;
	}
	
	/* In case the user tries to read an uninitialized device */
	if (!STTS22H_data->client) {
		mutex_unlock(&STTS22H_data->lock);
		PDEBUG("Reading failed, unconfigured device\n");
		return 0;
	}
	
	/* For one-shot mode */
	if (STTS22H_data->mode == one_shot) {
		/* Preserve current config */
		result =
		i2c_smbus_read_byte_data(STTS22H_data->client, CTRL_REG);
		if (result < 0) {
			mutex_unlock(&STTS22H_data->lock);
			PDEBUG("Failed when getting current config\n");
			return 0;
		}
		
		config = (u8)result | ONE_SHOT_GET;
		
		if (config_register(STTS22H_data->client, 
						CTRL_REG, config) < 0) {
			mutex_unlock(&STTS22H_data->lock);
			PDEBUG(
			"Failed when setting up one-shot acquisition bit\n");
			return 0;
		}
		
		counter = 0;
		while (counter < MAX_ATTP) {
			result = i2c_smbus_read_byte_data(
					STTS22H_data->client, STATUS_REG);
			if (result < 0) {
				mutex_unlock(&STTS22H_data->lock);
				PDEBUG("Failed when getting data\n");
				return 0;
			}
			
			if ((u8)result & 0x01) {
				PDEBUG("Data conversion in process\n");
				udelay(100);
				counter++;
			} else {
				break;
			}
		}
		
		if (counter == MAX_ATTP) {
			mutex_unlock(&STTS22H_data->lock);
			PDEBUG("Data conversion failed\n");
			return 0;
		}
	}
	
	data[READ_LEN] = '\0';
	
	result = i2c_smbus_read_byte_data(STTS22H_data->client, TEMP_LSB_REG);
	if (result < 0) {
		mutex_unlock(&STTS22H_data->lock);
		PDEBUG("Failed when getting LSB data\n");
		return 0;
	}
	
	data[0] = (char)result;
	
	result = i2c_smbus_read_byte_data(STTS22H_data->client, TEMP_MSB_REG);
	if (result < 0) {
		mutex_unlock(&STTS22H_data->lock);
		PDEBUG("Failed when getting MSB data\n");
		return 0;
	}
	
	data[1] = (char)result;
	
	mutex_unlock(&STTS22H_data->lock);
	
	if (copy_to_user(buff, data, READ_LEN * sizeof(char))) {
		PDEBUG("Failed when copying data to user\n");
		return 0;
	}
	
	return READ_LEN * sizeof(char);
}

/*
 * STTS22H_ioctl - Allow the user to check currently in-used devices, 
 *		   change operation mode and sampling rate 
 * Return error code on error, 0 on success
 */
static long
STTS22H_ioctl(struct file *filp, unsigned int command, unsigned long buff)
{
	u8 config;
	int cur, result, mode_nr, odr;
	struct STTS22H_data *STTS22H_data;
	
	switch (command) {
	case PR_LIST:
		if (mutex_lock_interruptible(&list_lock)) {
			PDEBUG(
			"Cannot perform mutex locking, restart system\n");
			return -ERESTARTSYS;
		}
	
		if (!list_empty(&client_list)) {
			list_for_each_entry(STTS22H_data, &client_list, entry) {
				PDEBUG(
				"Device address: %02X, adapter: %d, mode: %d\n",
				STTS22H_data->client->addr, 
				STTS22H_data->adpt,
				STTS22H_data->mode);
			}
		} else {
			PDEBUG("Device list empty\n");
		}
		
		mutex_unlock(&list_lock);	
		
		break;
			
	case CHG_MODE:
		if (copy_from_user(&mode_nr, (int *)buff, sizeof(int))) {
			PDEBUG("Copying from user failed\n");
			return -EFAULT;
		}
	
		STTS22H_data = filp->private_data;
		
		if (mutex_lock_interruptible(&STTS22H_data->lock)) {
			PDEBUG(
			"Cannot perform mutex locking, restart system\n");
			return -ERESTARTSYS;
		}
		
		/* In case the user tries to config an uninitialized device */
		if (!STTS22H_data->client) {
			mutex_unlock(&STTS22H_data->lock);
			PDEBUG("Mode change failed, unconfigured device\n");
			return -EFAULT;
		}
	
		/* Preserve current config */
		result =
		i2c_smbus_read_byte_data(STTS22H_data->client, CTRL_REG);
		if (result < 0) {
			mutex_unlock(&STTS22H_data->lock);
			PDEBUG("Failed when getting current config\n");
			return result;
		}
			
		/* Power down device before changing mode */
		config = (u8)result & LOW_ODR_DIS & FREE_RUN_DIS;
		
		result =
		config_register(STTS22H_data->client, CTRL_REG, config);
		if (result < 0) {
			mutex_unlock(&STTS22H_data->lock);
			PDEBUG("Failed when powering down device\n");
			return result;
		}
		
		switch (mode_nr) {
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
			mutex_unlock(&STTS22H_data->lock);
			PDEBUG("Invalid mode number\n");
			return -EFAULT;
		}
		
		result =
		config_register(STTS22H_data->client, CTRL_REG, config);
		if (result < 0) {
			mutex_unlock(&STTS22H_data->lock);
			PDEBUG("Failed when changing mode\n");
			return result;
		}
		
		STTS22H_data->mode = mode_nr;
		
		mutex_unlock(&STTS22H_data->lock);
		
		break;
			
	case CHG_ODR:
		if (copy_from_user(&odr, (int *)buff, sizeof(int))) {
			PDEBUG("Copying from user failed\n");
			return -EFAULT;
		}
	
		STTS22H_data = filp->private_data;
		
		if (mutex_lock_interruptible(&STTS22H_data->lock)) {
			PDEBUG(
			"Cannot perform mutex locking, restart system\n");
			return -ERESTARTSYS;
		}
		
		/* In case the user tries to config an uninitialized device */
		if (!STTS22H_data->client) {
			mutex_unlock(&STTS22H_data->lock);
			PDEBUG("ODR change failed, unconfigured device\n");
			return -EFAULT;
		}
	
		/* Preserve current config */
		result =
		i2c_smbus_read_byte_data(STTS22H_data->client, CTRL_REG);
		if (result < 0) {
			mutex_unlock(&STTS22H_data->lock);
			PDEBUG("Failed when getting current config\n");
			return result;
		}
		
		cur = (u8)result;
		
		/* Power down device before changing ODR */
		config = cur & LOW_ODR_DIS & FREE_RUN_DIS;
		
		result =
		config_register(STTS22H_data->client, CTRL_REG, config);
		if (result < 0) {
			mutex_unlock(&STTS22H_data->lock);
			PDEBUG("Failed when powering down device\n");
			return result;
		}
		
		config = cur & ODR_CLEAR;
		
		switch (odr) {
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
			mutex_unlock(&STTS22H_data->lock);
			PDEBUG("Invalid ODR value\n");
			return -EFAULT;
		}
		
		result =
		config_register(STTS22H_data->client, CTRL_REG, config);
		if (result < 0) {
			mutex_unlock(&STTS22H_data->lock);
			PDEBUG("Failed when changing ODR rate\n");
			return result;
		}
		
		mutex_unlock(&STTS22H_data->lock);
		
		break;
		
	default:
		PDEBUG("Invalid command\n");
		return -EFAULT;
	}
	
	return 0;
}

static int STTS22H_open(struct inode *inode, struct file *filp)
{
	struct STTS22H_data *STTS22H_data;
	
	STTS22H_data = (struct STTS22H_data*)
	kzalloc(sizeof(struct STTS22H_data), GFP_KERNEL);
	
	if (!STTS22H_data)
		return -ENOMEM;
	
	STTS22H_data->adpt = -1;
	
	mutex_init(&STTS22H_data->lock);
	
	filp->private_data = STTS22H_data;
	
	return 0;
}

static int STTS22H_release(struct inode *inode, struct file *filp)
{
	struct STTS22H_data *STTS22H_data;
	
	STTS22H_data = filp->private_data;
	
	/* Check if the device is on the list */
	if (STTS22H_data->adpt != -1) {
		if (mutex_lock_interruptible(&list_lock)) {
			PDEBUG(
			"Cannot perform mutex locking, restart system\n");
			return -ERESTARTSYS;
		}
	
		list_del(&STTS22H_data->entry);
		
		mutex_unlock(&list_lock);
	}
	
	if (STTS22H_data->client) {
		i2c_put_adapter(STTS22H_data->client->adapter);
		kfree(STTS22H_data->client);
		STTS22H_data->client = NULL;
	}
	
	kfree(STTS22H_data);
	
	filp->private_data = NULL;

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

static int __init STTS22H_init(void)
{
	int result;
	struct device *dev_res;
	
	result = alloc_chrdev_region(&STTS22H_id, 0, 1, "STTS22H");
	if (result < 0) {
		PDEBUG("Failed when requesting device number\n");
		return result;
	}
	
	STTS22H_class = class_create(THIS_MODULE, "temp_sensor");
	result = (int)PTR_ERR_OR_ZERO(STTS22H_class);
	if (result) {
		PDEBUG("Failed when creating device class\n");
		goto class_fail;
	}

	cdev_init(&STTS22H_cdev, &STTS22H_fops);
	
	STTS22H_cdev.owner = THIS_MODULE;
	
	result = cdev_add(&STTS22H_cdev, STTS22H_id, 1);
	if (result < 0) {
		PDEBUG("Failed when registering cdev\n");
		goto cdev_fail;
	}
	
	dev_res =
	device_create(STTS22H_class, NULL, STTS22H_id, NULL, "STTS22H");
	result = (int)PTR_ERR_OR_ZERO(dev_res);
	if (result) {
		PDEBUG("Failed when creating device under class\n");
		goto create_fail;
	}
	
	mutex_init(&list_lock);
	
	PDEBUG("Driver loaded\n");
	
	return 0;
	
create_fail:		
	cdev_del(&STTS22H_cdev);

cdev_fail:
	class_destroy(STTS22H_class);

class_fail:
	unregister_chrdev_region(STTS22H_id, 1);
	
	return result;
}

static void __exit STTS22H_exit(void)
{
	device_destroy(STTS22H_class, STTS22H_id);
	
	cdev_del(&STTS22H_cdev);
	
	class_destroy(STTS22H_class);
	
	unregister_chrdev_region(STTS22H_id, 1);
	
	PDEBUG("Driver unloaded\n");
}

module_init(STTS22H_init);
module_exit(STTS22H_exit);

MODULE_AUTHOR("Zixuan Qiao <zqiao104@uottawa.ca>");
MODULE_DESCRIPTION("Providing user-interface for STTS22H");
MODULE_LICENSE("GPL");
