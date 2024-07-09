# STTS22H Driver
STTS22H	is a temperature sensor manufactured by STMicroelectronics. 

## Implemented Driver

**Char Device Driver**

The functionality of STTS22H is rather user-centric. Thus, a character device driver is implemented to provide interface to user-space. 
It allocates an unregistered i2c_client when the file is open, and allows the user to setup its address and adapter number. 
To avoid conflicts and racing conditions, a kernel linked list is maintained to record the address and adapter combinations that are currently in use. 
Mutex locks are adopted as well to accommodate multi-processing environment. 
The driver also allows the user to change the sensor's operation mode as needed, providing flexible power management. 

The workflow of the driver is implemented by the following functions:

STTS22H_init -> initializes one char device, create directory /dev/STTS22H to allow user access

STTS22H_open -> allocates a STTS22H_data structure which contains an i2c_client, then associate it with filp->private_data

STTS22H_write -> allows the user to setup the address and adapter number of the i2c_client, maintains a list of devices that are currently in-use 

STTS22H_ioctl -> allows user to check the device list, change operation mode and sampling rate

STTS22H_read -> obtains data according to operation mode and return it to user

STTS22H_release -> frees the allocated data structure when a filp's use count drops to 0, updates the list of devices

STTS22H_exit -> deletes the char device

## Schematic


## References
1. https://www.st.com/resource/en/datasheet/stts22h.pdf
2. https://www.kernel.org/doc/html/latest/core-api/kernel-api.html?highlight=kfifo
3. https://www.kernel.org/doc/Documentation/ioctl/ioctl-number.txt
4. https://0xax.gitbooks.io/linux-insides/content/DataStructures/linux-datastructures-1.html
5. https://medium.com/@414apache/kernel-data-structures-linkedlist-b13e4f8de4bf

