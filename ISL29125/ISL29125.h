#ifndef ISL29125_HEADER
#define ISL29125_HEADER

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>

#define DEBUG
#ifdef DEBUG
#	define PDEBUG(format, args...) printk(KERN_DEBUG "ISL29125: " format, ## args)
#else 
#	define PDEBUG(format, args...)
#endif

#define ADPT_NUM 2
#define ISL29125_ADDR 0x44
#define INT_GPIO_NR 48
#define INT_GPIO_LABEL "P9_15"

#define CONFG_REG_1 0x01
#define CONFG_REG_2 0x02
#define CONFG_REG_3 0x03

#define INT_REG_LTL 0x04
#define INT_REG_LTH 0x05
#define INT_REG_HTL 0x06
#define INT_REG_HTH 0x07

#define ST_FLG_REG 0x08

#define DATA_REG_GL 0x09
#define DATA_REG_GH 0x0A
#define DATA_REG_RL 0x0B
#define DATA_REG_RH 0x0C
#define DATA_REG_BL 0x0D
#define DATA_REG_BH 0x0E

#define DEFAULT_CONFG 0x00

#define CONFG_MODE_G 0x01
#define CONFG_MODE_R 0x02
#define CONFG_MODE_B 0x03

#define CONFG_RANGE_LOW 0x00
#define CONFG_RANGE_HIGH 0x08

#define CONFG_RES_HIGH 0x00
#define CONFG_RES_LOW 0x10

#define CONFG_NO_INT 0x00
#define CONFG_INT_G 0x01
#define CONFG_INT_R 0x02
#define CONFG_INT_B 0x03

#define CONFG_INT_PERS_1 0x00
#define CONFG_INT_PERS_2 0x04
#define CONFG_INT_PERS_4 0x08
#define CONFG_INT_PERS_8 0x0C

#define INT_VAL_LTL 0x00
#define INT_VAL_LTH 0x00
#define INT_VAL_HTL 0x00
#define INT_VAL_HTH 0x01

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Testing interrupt line with ISL29125");

static struct i2c_board_info ISL29125_info = {
	I2C_BOARD_INFO("ISL29125", ISL29125_ADDR),
};

static struct i2c_device_id ISL29125_id_table[] = {
	{"ISL29125", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ISL29125_id_table);

struct ISL29125_data {
	struct work_struct w;
	struct workqueue_struct *wq;
	struct i2c_client *client;
	int irq_nr;
};	//only contain dynamically allocated data

#endif
