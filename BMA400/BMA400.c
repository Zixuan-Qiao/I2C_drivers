#include "BMA400.h"

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

void print_data(u8 *values) {
	int acc_x, acc_y, acc_z;
	
	acc_x = (int)values[0] + ((int)values[1] << 8);
	if(acc_x > 2047)
		acc_x -= 4096;
		
	acc_y = (int)values[2] + ((int)values[3] << 8);
	if(acc_y > 2047)
		acc_y -= 4096;
		
	acc_z = (int)values[4] + ((int)values[5] << 8);
	if(acc_z > 2047)
		acc_z -= 4096;
		
	PDEBUG("Acceleration data x-axis: %d, y-axis: %d, z-axis: %d. \n", acc_x, acc_y, acc_z);
	
	return;
}

void BMA400_dr_work_handler(struct work_struct *work) {
	u8 *values;
	s32 result;
	struct BMA400_data *BMA400_data;
	
	PDEBUG("Data ready interrupt handler bottom half invoked. \n");
	
	BMA400_data = container_of(work, struct BMA400_data, w);
	if(!BMA400_data) {
		PDEBUG("Failed when retrieving data. \n");
		return;
	}
	
	values = kzalloc(sizeof(u8) * READ_LEN, GFP_KERNEL);
	if(!values) {
		PDEBUG("Failed when allocating buffer. \n");
		return;
	}

	// getting acceleration data with burst read
	result = i2c_smbus_read_i2c_block_data(BMA400_data->client, ACC_X_LSB_REG, READ_LEN, values);
	if(result < 0) {
		PDEBUG("Failed when reading the acceleration data. \n");
		return;
	}
	
	print_data(values);
	
	kfree(values);
	
	result = i2c_smbus_read_byte_data(BMA400_data->client, INT_STAT0_REG);
	if(result < 0) {
		PDEBUG("Failed when clearing interrupt state. \n");
		return;
	}
	
	return;
}

void BMA400_wu_work_handler(struct work_struct *work) {
	u8 *values;
	s32 result;
	struct BMA400_data *BMA400_data;
	
	PDEBUG("Wake-up interrupt handler bottom half invoked. \n");
	
	BMA400_data = container_of(work, struct BMA400_data, w);
	if(!BMA400_data) {
		PDEBUG("Failed when retrieving data. \n");
		return;
	}
	
	values = kzalloc(sizeof(u8) * READ_LEN, GFP_KERNEL);
	if(!values) {
		PDEBUG("Failed when allocating buffer. \n");
		return;
	}

	// getting acceleration data with burst read
	result = i2c_smbus_read_i2c_block_data(BMA400_data->client, ACC_X_LSB_REG, READ_LEN, values);
	if(result < 0) {
		PDEBUG("Failed when reading the acceleration data. \n");
		return;
	}
	
	print_data(values);
	
	kfree(values);
	
	if(config_register(BMA400_data->client, ACC_CONFIG0_REG, LOW_POWER_MODE) < 0) {
		PDEBUG("Failed when returning to low-power mode. \n");
		return;
	}
	
	mdelay(2);
	
	return;
}

irqreturn_t BMA400_dr_int_handler(int irq, void *dev_id) {
	struct BMA400_data *BMA400_data;
	
	PDEBUG("Data ready interrupt detected. \n");
	
	BMA400_data = dev_id;
	if(!BMA400_data) {
		PDEBUG("Failed when retrieving data. \n");
		return IRQ_NONE;
	}
	
	queue_work(BMA400_data->wq, &(BMA400_data->w));
	
	return IRQ_HANDLED;
}

irqreturn_t BMA400_wu_int_handler(int irq, void *dev_id) {
	struct BMA400_data *BMA400_data;
	
	PDEBUG("Wake-up interrupt detected. \n");
	
	BMA400_data = dev_id;
	if(!BMA400_data) {
		PDEBUG("Failed when retrieving data. \n");
		return IRQ_NONE;
	}
	
	queue_work(BMA400_data->wq, &(BMA400_data->w));
	
	return IRQ_HANDLED;
}

irqreturn_t BMA400_tap_int_handler(int irq, void *dev_id) {
	ktime_t cur;
	cur = ktime_get(); 

	PDEBUG("System time: %lld, tap interrupt detected. \n", cur);
	
	return IRQ_HANDLED;
}

int BMA400_probe(struct i2c_client *i2c_client, const struct i2c_device_id *id) {
	u8 config, values[NUM_INT_REG];
	s32 chip_id;
	int result;
	struct BMA400_data *BMA400_data;
	struct device *dev;
	irqreturn_t (*mode_irq_handler)(int, void *);
	void (*mode_work_handler)(struct work_struct *);
	
	PDEBUG("I2C_client addr: %p. \n", i2c_client);
	
	PDEBUG("BMA400 probed on adapter: %d. \n", ADPT_NUM);
	
	// confirm chip id
	chip_id = i2c_smbus_read_byte_data(i2c_client, CHIPID_REG);
	if(chip_id < 0) {
		PDEBUG("Failed when reading the chip id at: %02X. \n", CHIPID_REG);
		return (int)chip_id;
	}
	
	if((u8)chip_id != CHIPID_VAL) {
		PDEBUG("Failed when trying to establish communication. \n");
		return -EAGAIN;
	}
	
	switch(mode) {
		case LOW_POWER:
			// config power mode	
			result = config_register(i2c_client, ACC_CONFIG0_REG, LOW_POWER_MODE);
			if(result) {
				PDEBUG("Failed when configuring power mode. \n");
				return result;
			}
			
			mdelay(2);
			
			// accelerometer configuration
			config =  SMPL_RATE_25 | OVER_SMPL_RATE0 | ACC_RANGE_4G;
			
			result = config_register(i2c_client, ACC_CONFIG1_REG, config);
			if(result) {
				PDEBUG("Failed when initializing accelerometer configuration. \n");
				return result;
			}
			
			// default data source
			
			// map wake-up interrupt to interrupt pin 1
			result = config_register(i2c_client, INT1_MAP_REG, MAP_WKUP_INT1);
			if(result) {
				PDEBUG("Failed when mapping wake-up interrupt to interrupt pin 1. \n");
				return result;
			}
			
			// keep default interrupt pin physical settings
			
			// config wake-up interrupt as auto-wake-up condition
			result = config_register(i2c_client, AUTO_WKUP1_REG, WKINT_TO_AWK);
			if(result) {
				PDEBUG("Failed when configuring auto-wake-up condition. \n");
				return result;
			}
			
			// config features of wake-up interrupt
			config = WKINT_REF_ONCE | WKINT_SMP_NUM1 | WKINT_EN_X | WKINT_EN_Y | WKINT_EN_Z;
			
			result = config_register(i2c_client, WKINT_CONFIG0_REG, config);
			if(result) {
				PDEBUG("Failed when configuring eatures of wake-up interrupt. \n");
				return result;
			}
			
			// config the MSB of wake-up interrupt threshold
			config = 1 << 1;
			
			result = config_register(i2c_client, WKINT_CONFIG1_REG, config);
			if(result) {
				PDEBUG("Failed when configuring the MSB of wake-up interrupt threshold. \n");
				return result;
			}
			
			mode_irq_handler = BMA400_wu_int_handler;
			mode_work_handler = BMA400_wu_work_handler;
			
			break;
		
		case NORMAL:
			// config power mode	
			result = config_register(i2c_client, ACC_CONFIG0_REG, NORMAL_MODE);
			if(result) {
				PDEBUG("Failed when configuring power mode. \n");
				return result;
			}
			
			mdelay(2);
			
			// accelerometer configuration
			config = SMPL_RATE_200 | OVER_SMPL_RATE1 | ACC_RANGE_4G;
			
			result = config_register(i2c_client, ACC_CONFIG1_REG, config);
			if(result) {
				PDEBUG("Failed when initializing accelerometer configuration. \n");
				return result;
			}
			
			// config data source
			result = config_register(i2c_client, ACC_CONFIG2_REG, DATA_SRC_FLT2);
			if(result) {
				PDEBUG("Failed when configuring data source. \n");
				return result;
			}
			
			// enable data ready interrupt
			result = config_register(i2c_client, INT_CONFIG0_REG, EN_DR_INT);
			if(result) {
				PDEBUG("Failed when enabling data ready interrupt. \n");
				return result;
			}
			
			// enable latched interrupt
			result = config_register(i2c_client, INT_CONFIG1_REG, EN_LATCH_INT);
			if(result) {
				PDEBUG("Failed when enabling latch interrupt. \n");
				return result;
			}
			
			// map data ready interrupt to interrupt pin 1
			result = config_register(i2c_client, INT1_MAP_REG, MAP_DR_INT1);
			if(result) {
				PDEBUG("Failed when mapping data ready interrupt to interrupt pin 1. \n");
				return result;
			}
			
			// keep default interrupt pin physical settings
			
			mode_irq_handler = BMA400_dr_int_handler;
			mode_work_handler = BMA400_dr_work_handler;
			
			break;
			
		case TAP:
			// config power mode	
			result = config_register(i2c_client, ACC_CONFIG0_REG, NORMAL_MODE);
			if(result) {
				PDEBUG("Failed when configuring power mode. \n");
				return result;
			}
			
			mdelay(2);
			
			// accelerometer configuration
			config = SMPL_RATE_200 | OVER_SMPL_RATE1 | ACC_RANGE_4G;
			
			result = config_register(i2c_client, ACC_CONFIG1_REG, config);
			if(result) {
				PDEBUG("Failed when initializing accelerometer configuration. \n");
				return result;
			}
			
			// keep default data source settings
			
			// enable single tap interrupt
			result = config_register(i2c_client, INT_CONFIG1_REG, EN_STAP_INT);
			if(result) {
				PDEBUG("Failed when enabling single tap interrupt. \n");
				return result;
			}
			
			// map tap interrupt to interrupt pin 1
			result = config_register(i2c_client, INT12_MAP_REG, MAP_TAP_INT1);
			if(result) {
				PDEBUG("Failed when mapping tap interrupt to interrupt pin 1. \n");
				return result;
			}
			
			// keep default interrupt pin physical settings
			
			// config tap interrupt features
			result = config_register(i2c_client, TAP_CONFIG_REG, TAP_ALG_SENS1);
			if(result) {
				PDEBUG("Failed when configuring tap interrupt features. \n");
				return result;
			}
			
			mode_irq_handler = BMA400_tap_int_handler;
			mode_work_handler = NULL;
			
			break;
			
		default:
			PDEBUG("Invalid mode! \n");
	
	}
	
	// initialize device data	
	dev = &(i2c_client->dev);
	
	BMA400_data = devm_kzalloc(dev, sizeof(struct BMA400_data), GFP_KERNEL);
	if(!BMA400_data) {
		PDEBUG("Failed when allocating BMA400 data. \n");
		return -ENOMEM;
	}
	
	BMA400_data->client = i2c_client;
	
	if(mode_work_handler) {
		BMA400_data->wq = create_workqueue("BMA400_queue");
		if(!BMA400_data->wq) {
			PDEBUG("Failed when creating workqueue. \n");
			return -ENOMEM;
		}
		
		INIT_WORK(&(BMA400_data->w), mode_work_handler);
	}	

	// requesting irq number
	if(!gpio_is_valid(INT_GPIO_NR)) {
		PDEBUG("Invalid GPIO number %d. \n", INT_GPIO_NR);
		return -ENOTTY;
	}
	
	result = gpio_request(INT_GPIO_NR, INT_GPIO_LABEL);
	if(result < 0) {
		PDEBUG("Failed when requesting %s. \n", INT_GPIO_LABEL);
		return result;
	}
	
	result = gpio_direction_input(INT_GPIO_NR);
	if(result < 0) {
		PDEBUG("Failed when setting port diection. \n");
		goto irq_fail;
	}
	
	BMA400_data->irq_nr = gpio_to_irq(INT_GPIO_NR);
	if(BMA400_data->irq_nr < 0) {
		PDEBUG("Could not get irq number of %d. \n", INT_GPIO_NR);
		result = BMA400_data->irq_nr;
		goto irq_fail;
	}
	
	i2c_set_clientdata(i2c_client, BMA400_data);
	
	// initializing interrupt state 
	result = i2c_smbus_read_i2c_block_data(i2c_client, INT_STAT0_REG, NUM_INT_REG, values);
	if(result < 0) {
		PDEBUG("Failed when initializing interrupt state. \n");
		goto irq_fail;
	}
	
	// must be last step!
	result = request_irq(BMA400_data->irq_nr, mode_irq_handler, IRQF_TRIGGER_RISING, 
				"BMA400", BMA400_data);
	if(result < 0) {
		PDEBUG("Failed when requesting irq number. \n");
		goto irq_fail;
	}
	
	return 0;
	
irq_fail:
	gpio_free(INT_GPIO_NR);
	
	return result;
}

int BMA400_remove(struct i2c_client *i2c_client) {
	struct BMA400_data *BMA400_data;
	
	BMA400_data = i2c_get_clientdata(i2c_client);
	
	if(!BMA400_data) {
		PDEBUG("Failed when retrieving data. \n");
		return -ENOTTY;
	}
	
	if(BMA400_data->wq) {
		cancel_work_sync(&(BMA400_data->w));
		flush_workqueue(BMA400_data->wq);
		mdelay(200);
		destroy_workqueue(BMA400_data->wq);
	}
	
	free_irq(BMA400_data->irq_nr, BMA400_data);
	
	gpio_free(INT_GPIO_NR);
	
	PDEBUG("BMA400 removed. \n");
	
	return 0;
}

static struct i2c_driver BMA400_driver = {
	.driver = {
		.name = "BMA400",
		.owner = THIS_MODULE,
	},
	.probe = BMA400_probe,
	.remove = BMA400_remove,
	.id_table = BMA400_id_table,
};

static struct i2c_adapter *BMA400_adpt;
static struct i2c_client *BMA400_client;

static int __init BMA400_init(void) {	
	int result;
	
	BMA400_adpt = i2c_get_adapter(ADPT_NUM);
	if(!BMA400_adpt) {
		PDEBUG("I2C adapter number %d does not exist. \n", ADPT_NUM);
		return -ENODEV;
	}
	
	BMA400_client = i2c_new_client_device(BMA400_adpt, &BMA400_info);
	PDEBUG("BMA400_client addr: %p", BMA400_client);
	
	if(PTR_ERR_OR_ZERO(BMA400_client)) {
		PDEBUG("Client creation failed. \n");
		result = PTR_ERR(BMA400_client);
		goto client_fail;
	}
	
	result = i2c_add_driver(&BMA400_driver);
	if(result) {
		PDEBUG("Failed when adding driver. \n");
		goto driver_fail;
	}
	
	return 0;

driver_fail:
	i2c_unregister_device(BMA400_client);
	
client_fail:
	i2c_put_adapter(BMA400_adpt);
		
	return result;
}

static void __exit BMA400_exit(void) {
	i2c_del_driver(&BMA400_driver);
	
	i2c_unregister_device(BMA400_client);
	
	i2c_put_adapter(BMA400_adpt);
	
	PDEBUG("BMA400 I2C driver unloaded. \n");
}

module_init(BMA400_init);
module_exit(BMA400_exit);
