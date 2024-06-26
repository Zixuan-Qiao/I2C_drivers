# DHT20 Drivers
DHT20	is a humidity and temperature sensor manufactured by Aosong Electronic. 

## Implemented Drivers
**1. I2C Client Driver**

Standard I2C client driver, implements probe and remove functions to manage the device from kernel-space. 

Since DHT20 does not come with an interrupt line, workqueue is adopted to poll the device regularly. 

**2. Char Device Driver**

As stated in the Linux documentation, I2C devices are usually managed by kernel-space drivers. Such approach provides security but lacks convenience. Thus, a loadable module i2c-dev is included in the Linux source tree to make I2C devices accessible in user-space. 

The i2c-dev module is designed to control any I2C client, thus is very generic. To control a device with i2c-dev, the user needs to go through its data sheet and determine the byte sequences to send, which is not user-friendly and prone to errors. 

The DHT20 char device driver is developed on the basis of i2c-dev with specialization. It provides user interface as well as operations packaged as functions. The user application can control the DHT20 by issuing system calls such as read and ioctl whithout sending commands directly.  

The workflow of DHT20 char device driver is implemented by the following functions:

DHT20_init -> initializes one char device, create directory /dev/DHT20 to provide user interface

DHT20_open -> allocates an unregistered I2C client, then associate it with filp->private_data

DHT20_ioctl -> allows user application to specify an adapter number (client address is fixed), and sends byte sequences to perform device initialization

DHT20_read -> sends byte sequences according to data sheet to obtain data and return them to user

DHT20_release -> frees the allocated client when a filp's use count drops to 0

DHT20_exit -> deletes the char device

## Schematic
<img width="364" alt="DHT20" src="https://github.com/Zixuan-Qiao/I2C_drivers/assets/102449059/7e745cc8-dc0d-49ec-90f4-ad52b19bd1e3">

## References
1. https://www.kernel.org/doc/html/v5.5/i2c/writing-clients.html
2. https://www.kernel.org/doc/Documentation/i2c/instantiating-devices
3. https://www.kernel.org/doc/Documentation/i2c/dev-interface
4. https://embetronicx.com/tutorials/linux/device-drivers/i2c-linux-device-driver-using-raspberry-pi/
5. https://stackoverflow.com/questions/33549211/how-to-add-i2c-devices-on-the-beaglebone-black-using-device-tree-overlays
6. https://forum.beagleboard.org/t/on-bbb-how-to-use-i2c-1-it-exists-but-maybe-needs-pins-setup-bbb-rev-c-debian-9/1335

