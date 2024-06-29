#ifndef BMA400_HEADER
#define BMA400_HEADER

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
#include <linux/time.h>
#include <linux/jiffies.h>

#define DEBUG
#ifdef DEBUG
#	define PDEBUG(format, args...) printk(KERN_ERR "BMA400: " format, ## args)
#else 
#	define PDEBUG(format, args...)
#endif

#define ADPT_NUM 2
#define BMA400_ADDR 0x14
#define READ_LEN 6
#define NUM_INT_REG 3

#define INT_GPIO_NR 48
#define INT_GPIO_LABEL "P9_15"

// Register addresses
#define CHIPID_REG 0x00

#define ERR_REG 0x02
#define STATUS_REG 0x03

#define ACC_X_LSB_REG 0x04
#define ACC_X_MSB_REG 0x05
#define ACC_Y_LSB_REG 0x06
#define ACC_Y_MSB_REG 0x07
#define ACC_Z_LSB_REG 0x08
#define ACC_Z_MSB_REG 0x09

#define INT_STAT0_REG 0x0E
#define INT_STAT1_REG 0x0F
#define INT_STAT2_REG 0x10

// power modes
#define ACC_CONFIG0_REG 0x19
// sampling rate
#define ACC_CONFIG1_REG 0x1A
// select data source
#define ACC_CONFIG2_REG 0x1B

// interrupt config
#define INT_CONFIG0_REG 0x1F
#define INT_CONFIG1_REG 0x20
#define INT1_MAP_REG 0x21
#define INT2_MAP_REG 0x22
#define INT12_MAP_REG 0x23

// physical behaviour of int pins
#define INT12_IOCTL_REG 0x24

#define AUTO_LPW0_REG 0x2A
#define AUTO_LPW1_REG 0x2B
#define AUTO_WKUP0_REG 0x2C
#define AUTO_WKUP1_REG 0x2D
#define WKINT_CONFIG0_REG 0x2F
#define WKINT_CONFIG1_REG 0x30

#define TAP_CONFIG_REG 0x57
#define TAP_CONFIG1_REG 0x58

// Register configs
#define CHIPID_VAL 0x90

#define DEFAULT_CONFIG 0x00

#define SLEEP_MODE 0x00
#define LOW_POWER_MODE 0x01
#define NORMAL_MODE 0x02
#define SWTCH_TO_SLEEP 0x03

#define SMPL_RATE_12P5 0x05
#define SMPL_RATE_25 0x06
#define SMPL_RATE_50 0x07
#define SMPL_RATE_100 0x08
#define SMPL_RATE_200 0x09
#define SMPL_RATE_400 0x0A
#define SMPL_RATE_800 0x0B

#define OVER_SMPL_RATE0 0x00
#define OVER_SMPL_RATE1 0x10
#define OVER_SMPL_RATE2 0x20
#define OVER_SMPL_RATE3 0x30

#define ACC_RANGE_2G 0x00
#define ACC_RANGE_4G 0x40
#define ACC_RANGE_8G 0x80
#define ACC_RANGE_16G 0xC0

#define DATA_SRC_FLT1 0x00
#define DATA_SRC_FLT2 0x04

#define EN_ORCH_INT 0x02
#define EN_GEN1_INT 0x04
#define EN_GEN2_INT 0x08
#define EN_FFULL_INT 0x20
#define EN_FWM_INT 0x40
#define EN_DR_INT 0x80

#define EN_STEP_INT 0x01
#define EN_STAP_INT 0x04
#define EN_DTAP_INT 0x08
#define EN_ACTCH_INT 0x10
#define EN_LATCH_INT 0x80

#define MAP_WKUP_INT1 0x01
#define MAP_ORCH_INT1 0x02
#define MAP_GEN1_INT1 0x04
#define MAP_GEN2_INT1 0x08
#define MAP_OVRUN_INT1 0x10
#define MAP_FFULL_INT1 0x20
#define MAP_FWM_INT1 0x40
#define MAP_DR_INT1 0x80

#define MAP_STEP_INT1 0x01
#define MAP_TAP_INT1 0x04
#define MAP_ACTCH_INT1 0x08
#define MAP_STEP_INT2 0x10
#define MAP_TAP_INT2 0x40
#define MAP_ACTCH_INT2 0x80

#define INT1_LOW_ACT 0x00
#define INT1_HIGH_ACT 0x02
#define INT1_PUSH_PULL 0x00
#define INT1_OPEN_DRN 0x04
#define INT2_LOW_ACT 0x00
#define INT2_HIGH_ACT 0x20
#define INT2_PUSH_PULL 0x00
#define INT2_OPEN_DRN 0x40

#define DR_TO_LOWP 0x01
#define GEN1_TO_LOWP 0x02

#define WKINT_TO_AWK 0x02
#define WKTOUT_TO_AWK 0x04

#define WKINT_REF_MAN 0x00
#define WKINT_REF_ONCE 0x01
#define WKINT_REF_EVERY 0x02

#define WKINT_SMP_NUM1 0x00
#define WKINT_SMP_NUM2 0x04
#define WKINT_SMP_NUM3 0x08
#define WKINT_SMP_NUM4 0x0C
#define WKINT_SMP_NUM5 0x10
#define WKINT_SMP_NUM6 0x14
#define WKINT_SMP_NUM7 0x18
#define WKINT_SMP_NUM8 0x1C

#define WKINT_EN_X 0x20
#define WKINT_EN_Y 0x40
#define WKINT_EN_Z 0x80

#define USE_Z_AXIS 0x00
#define USE_Y_AXIS 0x08
#define USE_X_AXIS 0x10 

#define TAP_ALG_SENS0 0x00
#define TAP_ALG_SENS1 0x01
#define TAP_ALG_SENS2 0x02
#define TAP_ALG_SENS3 0x03
#define TAP_ALG_SENS4 0x04
#define TAP_ALG_SENS5 0x05
#define TAP_ALG_SENS6 0x06
#define TAP_ALG_SENS7 0x07

#define TAP_PEAK_SMP6 0x04
#define TAP_PEAK_SMP9 0x05
#define TAP_PEAK_SMP12 0x06
#define TAP_PEAK_SMP18 0x07

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Testing different modes of BMA400");

enum Mode {
	LOW_POWER = 0,
	NORMAL = 1,
	TAP = 2,
};

static int mode = LOW_POWER;

module_param(mode, int, 0644);
MODULE_PARM_DESC(mode, "Mode of operation");

// used to initialize i2c client
static struct i2c_board_info BMA400_info = {
	I2C_BOARD_INFO("BMA400", BMA400_ADDR),
};

// also included in driver structure to perform matching
static struct i2c_device_id BMA400_id_table[] = {
	{"BMA400", 0},
	{},
};

// used by the kernel to construct a list of ids of supported devices
MODULE_DEVICE_TABLE(i2c, BMA400_id_table);

struct BMA400_data {	//only contain dynamically allocated data
	struct work_struct w;
	struct workqueue_struct *wq;
	struct i2c_client *client;
	int irq_nr;
};

#endif
