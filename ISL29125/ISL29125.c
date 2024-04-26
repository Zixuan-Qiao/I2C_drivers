#include "ISL29125.h"

void isl_work_handler(struct work_struct *work) {
	s32 data;
	struct ISL29125_data *ISL29125_data;
	
	PDEBUG("Workqueue handler invoked. \n");
	
	ISL29125_data = container_of(work, struct ISL29125_data, w);
	if(!ISL29125_data) {
		PDEBUG("Failed when retrieving data. \n");
		return;
	}

	// getting interrupt data
	data = i2c_smbus_read_byte_data(ISL29125_data->client, DATA_REG_RL);
	if(data < 0) {
		PDEBUG("Failed when reading the value of DATA_REG_RL. \n");
		return;
	}
	printk("Red low: %02X. \n", data);
	
	data = i2c_smbus_read_byte_data(ISL29125_data->client, DATA_REG_RH);
	if(data < 0) {
		PDEBUG("Failed when reading the value of DATA_REG_RH. \n");
		return;
	}
	printk("Red high: %02X. \n", data);

	// clearing interrupt state
	data = i2c_smbus_read_byte_data(ISL29125_data->client, ST_FLG_REG);
	if(data < 0) {
		PDEBUG("Failed when reading the value of ST_FLG_REG. \n");
		return;
	}
}

irqreturn_t isl_int_handler(int irq, void *dev_id) {
	struct ISL29125_data *ISL29125_data;
	
	PDEBUG("Interrupt detected. \n");
	
	ISL29125_data = dev_id;
	if(!ISL29125_data) {
		PDEBUG("Failed when retrieving data. \n");
		return -ENOTTY;
	}
	
	queue_work(ISL29125_data->wq, &(ISL29125_data->w));
	
	return IRQ_HANDLED;
}

int config_register(struct i2c_client *i2c_client, u8 reg_addr, u8 config) {
	int res;
	s32 data;
	
	res = i2c_smbus_write_byte_data(i2c_client, reg_addr, config);
	if(res) {
		PDEBUG("Failed when sending command to register: %02X. \n", reg_addr);
		return res;
	}
	
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

int ISL29125_probe(struct i2c_client *i2c_client, const struct i2c_device_id *id) {
	u8 config;
	int result;
	struct ISL29125_data *ISL29125_data;
	struct device *dev;
	
	PDEBUG("I2C_client addr: %p. \n", i2c_client);
	
	PDEBUG("ISL29125 probed on adapter: %d. \n", ADPT_NUM);
	
	mdelay(10);
	
	// config register 1
	config = DEFAULT_CONFG | CONFG_MODE_R | CONFG_RANGE_HIGH | CONFG_RES_HIGH;
	
	result = config_register(i2c_client, CONFG_REG_1, config);
	if(result) {
		PDEBUG("Failed when initializing configuration register 1. \n");
		return result;
	}
	
	// config register 3
	config = DEFAULT_CONFG | CONFG_INT_R | CONFG_INT_PERS_8;
	
	result = config_register(i2c_client, CONFG_REG_3, config);
	if(result) {
		PDEBUG("Failed when initializing configuration register 3. \n");
		return result;
	}
	
	// config low interrupt threshold low byte register 
	result = config_register(i2c_client, INT_REG_LTL, INT_VAL_LTL);
	if(result) {
		PDEBUG("Failed when initializing low interrupt threshold low byte register. \n");
		return result;
	}
	
	// config low interrupt threshold high byte register 
	result = config_register(i2c_client, INT_REG_LTH, INT_VAL_LTH);
	if(result) {
		PDEBUG("Failed when initializing low interrupt threshold high byte register. \n");
		return result;
	}
	
	// config high interrupt threshold low byte register 
	result = config_register(i2c_client, INT_REG_HTL, INT_VAL_HTL);
	if(result) {
		PDEBUG("Failed when initializing high interrupt threshold low byte register. \n");
		return result;
	}
	
	// config high interrupt threshold high byte register 
	result = config_register(i2c_client, INT_REG_HTH, INT_VAL_HTH);
	if(result) {
		PDEBUG("Failed when initializing high interrupt threshold high byte register. \n");
		return result;
	}
	

	// initialize device data	
	dev = &(i2c_client->dev);
	
	ISL29125_data = devm_kzalloc(dev, sizeof(struct ISL29125_data), GFP_KERNEL);
	if(!ISL29125_data) {
		PDEBUG("Failed when allocating ISL29125 data. \n");
		return -ENOMEM;
	}
	
	ISL29125_data->client = i2c_client;
	
	ISL29125_data->wq = create_workqueue("ISL29125_queue");
	if(!ISL29125_data->wq) {
		PDEBUG("Failed when creating workqueue. \n");
		return -ENOMEM;
	}
	
	INIT_WORK(&(ISL29125_data->w), isl_work_handler);
	
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
	
	ISL29125_data->irq_nr = gpio_to_irq(INT_GPIO_NR);
	if(ISL29125_data->irq_nr < 0) {
		PDEBUG("Could not get irq number of %d. \n", INT_GPIO_NR);
		result = ISL29125_data->irq_nr;
		goto irq_fail;
	}
	
	i2c_set_clientdata(i2c_client, ISL29125_data);
	
	// initializing interrupt state
	result = i2c_smbus_read_byte_data(ISL29125_data->client, ST_FLG_REG);
	if(result < 0) {
		PDEBUG("Failed when reading the value of ST_FLG_REG. \n");
		goto irq_fail;
	}
	
	result = request_irq(ISL29125_data->irq_nr, isl_int_handler, IRQF_TRIGGER_FALLING, 
				"ISL29125", ISL29125_data);
	if(result < 0) {
		PDEBUG("Failed when requesting irq number. \n");
		goto irq_fail;
	}
	
	return 0;

irq_fail:
	gpio_free(INT_GPIO_NR);
	
	return result;
}

int ISL29125_remove(struct i2c_client *i2c_client) {
	struct ISL29125_data *ISL29125_data;
	
	ISL29125_data = i2c_get_clientdata(i2c_client);
	if(!ISL29125_data) {
		PDEBUG("Failed when retrieving data. \n");
		return -ENOTTY;
	}
	
	cancel_work_sync(&(ISL29125_data->w));	
	flush_workqueue(ISL29125_data->wq);	
	
	mdelay(200);	
	
	destroy_workqueue(ISL29125_data->wq);
	
	free_irq(ISL29125_data->irq_nr, ISL29125_data);
	
	gpio_free(INT_GPIO_NR);
	
	PDEBUG("ISL29125 removed. \n");
	
	return 0;
}

static struct i2c_driver ISL29125_driver = {
	.driver = {
		.name = "ISL29125",
		.owner = THIS_MODULE,
	},
	.probe = ISL29125_probe,
	.remove = ISL29125_remove,
	.id_table = ISL29125_id_table,
};

static struct i2c_adapter *ISL29125_adpt;
static struct i2c_client *ISL29125_client;

static int __init ISL29125_init(void) {	
	int result;
	
	ISL29125_adpt = i2c_get_adapter(ADPT_NUM);
	if(!ISL29125_adpt) {
		PDEBUG("I2C adapter number %d does not exist. \n", ADPT_NUM);
		return -ENODEV;
	}
	
	ISL29125_client = i2c_new_client_device(ISL29125_adpt, &ISL29125_info);
	PDEBUG("ISL29125_client addr: %p", ISL29125_client);
	
	if(PTR_ERR_OR_ZERO(ISL29125_client)) {
		PDEBUG("Client creation failed. \n");
		result = PTR_ERR(ISL29125_client);
		goto client_fail;
	}
	
	result = i2c_add_driver(&ISL29125_driver);
	if(result) {
		PDEBUG("Failed when adding driver. \n");
		goto driver_fail;
	}
	
	return 0;

driver_fail:
	i2c_unregister_device(ISL29125_client);
	
client_fail:
	i2c_put_adapter(ISL29125_adpt);
		
	return result;
}

static void __exit ISL29125_exit(void) {
	i2c_del_driver(&ISL29125_driver);
	
	i2c_unregister_device(ISL29125_client);
	
	i2c_put_adapter(ISL29125_adpt);
	
	PDEBUG("ISL29125 I2C driver unloaded. \n");
}

module_init(ISL29125_init);
module_exit(ISL29125_exit);
