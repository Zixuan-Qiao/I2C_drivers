#ifndef DHT20_HEADER
#define DHT20_HEADER

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/ioctl.h>

#define DHT20_DEBUG
#ifdef DHT20_DEBUG
#	define PDEBUG(format, args...) printk(KERN_EDEBUG "DHT20: " format, ## args)
#else 
#	define PDEBUG(format, args...)
#endif

#define DHT20_ADDR 0x38
#define READ_LEN 5

#define INIT_SET_ADPT _IOW('D', 1, int)

#define INIT_CMD 0x71
#define CMD_LEN 3
#define CMD_1 0xAC
#define CMD_2 0x33
#define CMD_3 0x00

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Providing user interface to DHT20");

#endif
