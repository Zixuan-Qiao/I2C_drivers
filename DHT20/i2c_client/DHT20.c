#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/mutex.h>

#define ADPT_NUM 2

int DHT20_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	int res;
	char init_byte[2];
	char status[2];
	int i;
	char cmd_r[4];
	unsigned char data[7];

	printk("DHT20 probed on adapter: %d. \n", ADPT_NUM);
	
	mdelay(100);
			
	init_byte[0] = 0x71;
	init_byte[1] = '\0';
	
	res = i2c_master_send(client, init_byte, 1);
	if(IS_ERR_VALUE(res)) {
		printk("Init byte transmission failed. \n");
		return res;
	}
	
	status[1] = '\0';
	
	res = i2c_master_recv(client, status, 1);
	if(IS_ERR_VALUE(res)) {
		printk("Status byte transmission failed. \n");
		return res;
	}
	
	printk("Status received: %#x. \n", status[0]);
			
	status[0] |= 0x18;
	if(status[0] != 0x18) {
		printk("Error in initialization. \n");
		return -ENOTTY;
	}
	
	printk("Initialization succeed! \n");
	
	mdelay(10);
	
	cmd_r[0] = 0xAC;
	cmd_r[1] = 0x33;
	cmd_r[2] = 0x00;
	cmd_r[3] = '\0';
	
	res = i2c_master_send(client, cmd_r, 3);
	if(IS_ERR_VALUE(res)) {
		printk("Read command transmission failed. \n");
		return res;
	}
	
	mdelay(80);
	
	data[6] = '\0';
	
	res = i2c_master_recv(client, data, 6);
	if(IS_ERR_VALUE(res)) {
		printk("Return data transmission failed. \n");
		return res;
	}

	printk("Received data: ");
	for(i = 0; i < 6; i++) {
		printk("%#x", data[i]);
	}
	
	return 0;
}

int DHT20_remove(struct i2c_client *client) {
	printk("DHT20 removed. \n");
	return 0;
}

static struct i2c_board_info DHT20_info = {
	I2C_BOARD_INFO("DHT20", 0x38),
};

static struct i2c_device_id DHT20_id_table[] = {
	{"DHT20", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, DHT20_id_table);

static struct i2c_driver DHT20_driver = {
	.driver = {
		.name = "DHT20",
		.owner = THIS_MODULE,
	},
	.probe = DHT20_probe,
	.remove = DHT20_remove,
	.id_table = DHT20_id_table,
};

static struct i2c_client *DHT20_client;
static struct i2c_adapter *DHT20_adpt;

static int __init DHT20_init(void) {	
	int result;
	
	DHT20_adpt = i2c_get_adapter(ADPT_NUM);
	if(!DHT20_adpt) {
		printk("I2C adapter number %d does not exist. \n", ADPT_NUM);
		return -ENODEV;
	}
	
	DHT20_client = i2c_new_client_device(DHT20_adpt, &DHT20_info);
	if(PTR_ERR_OR_ZERO(DHT20_client)) {
		printk("Client creation failed. \n");
		result = PTR_ERR(DHT20_client);
		goto client_fail;
	}
	
	result = i2c_add_driver(&DHT20_driver);
	if(result) {
		printk("Failed when adding driver. \n");
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
	
	printk("DHT20 I2C driver unloaded. \n");
}

module_init(DHT20_init);
module_exit(DHT20_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Managing DHT20 from kernel space");
