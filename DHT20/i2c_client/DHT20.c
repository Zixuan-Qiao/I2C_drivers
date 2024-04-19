#include "DHT20.h"

void DHT20_handler(struct work_struct *work) {
	int res;
	char cmd_r[4];
	unsigned char data[7];
	struct DHT20_data *DHT20_data;
	
	DHT20_data = container_of(work, struct DHT20_data, dw.work);
	
	if(!DHT20_data) {
		PDEBUG("Failed when retrieving data. \n");
		return;
	}
	
	cmd_r[0] = 0xAC;
	cmd_r[1] = 0x33;
	cmd_r[2] = 0x00;
	cmd_r[3] = '\0';
	
	res = i2c_master_send(DHT20_data->client, cmd_r, 3);
	if(IS_ERR_VALUE(res)) {
		PDEBUG("Read command transmission failed. \n");
		return;
	}
	
	mdelay(80);
	
	data[6] = '\0';
	
	res = i2c_master_recv(DHT20_data->client, data, 6);
	if(IS_ERR_VALUE(res)) {
		PDEBUG("Return data transmission failed. \n");
		return;
	}
	
	printk("DHT20: %02X %02X %02X %02X %02X %02X \n", data[0], data[1], data[2], data[3], data[4], data[5]);
	
	queue_delayed_work(DHT20_data->wq, &(DHT20_data->dw), msecs_to_jiffies(200));
}

int DHT20_probe(struct i2c_client *i2c_client, const struct i2c_device_id *id) {
	int res;
	char init_byte[2];
	char status[2];
	struct DHT20_data *DHT20_data;
	struct device *dev;
	
	PDEBUG("I2C_client addr: %p. \n", i2c_client);
	
	PDEBUG("DHT20 probed on adapter: %d. \n", ADPT_NUM);
	
	dev = &(i2c_client->dev);
	
	DHT20_data = devm_kzalloc(dev, sizeof(struct DHT20_data), GFP_KERNEL);
	if(!DHT20_data) {
		PDEBUG("Failed when allocating DHT20 data. \n");
		return -ENOMEM;
	}
	
	DHT20_data->client = i2c_client;
	i2c_set_clientdata(i2c_client, DHT20_data);
		
	mdelay(100);
			
	/*init_byte[0] = 0x71;
	init_byte[1] = '\0';
	
	res = i2c_master_send(i2c_client, init_byte, 1);
	if(IS_ERR_VALUE(res)) {
		PDEBUG("Init byte transmission failed. \n");
		return res;
	}
	
	status[1] = '\0';
	
	res = i2c_master_recv(i2c_client, status, 1);
	if(IS_ERR_VALUE(res)) {
		PDEBUG("Status byte transmission failed. \n");
		return res;
	}
	
	PDEBUG("Status received: %#x. \n", status[0]);
			
	status[0] |= 0x18;
	if(status[0] != 0x18) {
		PDEBUG("Error in initialization. \n");
		return -ENOTTY;
	}
	
	PDEBUG("Initialization succeed! \n");*/
	
	mdelay(10);
	
	DHT20_data->wq = create_workqueue("DHT20_queue");
	if(!DHT20_data->wq) {
		PDEBUG("Failed when creating workqueue. \n");
		return -ENOMEM;
	}
	
	INIT_DELAYED_WORK(&(DHT20_data->dw), DHT20_handler);
	
	queue_delayed_work(DHT20_data->wq, &(DHT20_data->dw), msecs_to_jiffies(200));
	
	return 0;
}

int DHT20_remove(struct i2c_client *i2c_client) {
	struct DHT20_data *DHT20_data;
	
	DHT20_data = i2c_get_clientdata(i2c_client);
	
	if(!DHT20_data) {
		PDEBUG("Failed when retrieving data. \n");
		return -ENOTTY;
	}
	
	cancel_delayed_work_sync(&(DHT20_data->dw));
	
	flush_workqueue(DHT20_data->wq);
	
	mdelay(200);
	
	destroy_workqueue(DHT20_data->wq);
	
	PDEBUG("DHT20 removed. \n");
	return 0;
}

static struct i2c_driver DHT20_driver = {
	.driver = {
		.name = "DHT20",
		.owner = THIS_MODULE,
	},
	.probe = DHT20_probe,
	.remove = DHT20_remove,
	.id_table = DHT20_id_table,
};

// static struct DHT20_device DHT20_dev;
static struct i2c_adapter *DHT20_adpt;

static struct i2c_client *DHT20_client;
// static struct workqueue_struct *DHT20_queue;

static int __init DHT20_init(void) {	
	int result;
	
	DHT20_adpt = i2c_get_adapter(ADPT_NUM);
	if(!DHT20_adpt) {
		PDEBUG("I2C adapter number %d does not exist. \n", ADPT_NUM);
		return -ENODEV;
	}
	
	DHT20_client = i2c_new_client_device(DHT20_adpt, &DHT20_info);
	PDEBUG("DHT20_client addr: %p", DHT20_client);
	
	if(PTR_ERR_OR_ZERO(DHT20_client)) {
		PDEBUG("Client creation failed. \n");
		result = PTR_ERR(DHT20_client);
		goto client_fail;
	}
	
	result = i2c_add_driver(&DHT20_driver);
	if(result) {
		PDEBUG("Failed when adding driver. \n");
		goto driver_fail;
	}
	
	return 0;

driver_fail:
	i2c_unregister_device(DHT20_client);
	
client_fail:
	i2c_put_adapter(DHT20_adpt);
		
	return result;
}

static void __exit DHT20_exit(void) {
	i2c_del_driver(&DHT20_driver);
	
	i2c_unregister_device(DHT20_client);
	
	i2c_put_adapter(DHT20_adpt);
	
	PDEBUG("DHT20 I2C driver unloaded. \n");
}

module_init(DHT20_init);
module_exit(DHT20_exit);
