#ifndef DHT20_HEADER
#define DHT20_HEADER

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>

#define DHT20_DEBUG

#ifdef DHT20_DEBUG
#	define PDEBUG(format, args...) printk(KERN_DEBUG "DHT20: " format, ## args)
#else 
#	define PDEBUG(format, args...)
#endif

#define ADPT_NUM 2

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Polling DHT20 from kernel space");

static struct i2c_board_info DHT20_info = {
	I2C_BOARD_INFO("DHT20", 0x38),
};

static struct i2c_device_id DHT20_id_table[] = {
	{"DHT20", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, DHT20_id_table);

struct DHT20_data {
	struct delayed_work dw;
	struct workqueue_struct *wq;
	struct i2c_client *client;
};

#endif
