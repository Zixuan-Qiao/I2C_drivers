// SPDX-License-Identifier: GPL-2.0-or-later
/* STMicroelectronics temperature sensor STTS22H driver
   
   Allow users to manage STTS22H from user-space, 
   change operation modes and obtain data from it

   Copyright (c) 2024,2024 Zixuan Qiao <zqiao104@uottawa.ca> */

#ifndef STTS22H_HEADER
#define STTS22H_HEADER

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
#	define PDEBUG(format, args...) printk(KERN_ERR "STTS22H: " format, ## args)
#else 
#	define PDEBUG(format, args...)
#endif

#define WRITE_LEN 2
#define READ_LEN 2
#define MAX_ATTP 100

#define PR_LIST _IO('S', 1)
#define CHG_MODE _IOW('S', 2, int)
#define CHG_ODR _IOW('S', 3, int)

// Registers
#define WHOAMI_REG 0x01
#define CHIP_ID 0xA0

#define HIGH_LIMIT_REG 0x02
#define LOW_LIMIT_REG 0x03

#define CTRL_REG 0x04

#define STATUS_REG 0x05

#define TEMP_LSB_REG 0x06
#define TEMP_MSB_REG 0x07

// Configurations
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

MODULE_AUTHOR("Zixuan Qiao <zqiao104@uottawa.ca>");
MODULE_DESCRIPTION("Providing user-interface for STTS22H");
MODULE_LICENSE("GPL");

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

LIST_HEAD(client_list);
struct mutex list_lock;

#endif
