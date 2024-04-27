# ISL29125 Driver
ISL29125 is a RGB light intensity sensor manufactured by Renesas Electronics. 

## Implemented Driver
**I2C driver**

Standard I2C client driver, handles initialization, interrupts, reading and writing of data. The driver adopted SMBus interface to communicate with the device, which makes it compatible with more types of platforms.  

Its workflow is implemented by the following functions:

ISL29125_init -> registers I2C adapter and client information

ISL29125_probe -> initializes the device control registers and requests for an interrupt line

isl_int_handler -> top-half of the interrupt handling process, schedule delayed work

isl_work_handler -> bottom-half of the interrupt handling process, reads data and clears interrupt state

ISL_29125_remove -> deallocates the resources obtained in probe

ISL29125_exit -> deletes the information registered in init

## Schematic
<img width="320" alt="1" src="https://github.com/Zixuan-Qiao/I2C_drivers/assets/102449059/56b338fa-3330-4c79-98cd-f7e1cfafef66">

## References
1. https://www.renesas.com/us/en/document/dst/isl29125-datasheet
2. https://www.kernel.org/doc/html/v5.10/i2c/i2c-protocol.html
3. https://stackoverflow.com/questions/7937245/how-to-use-linux-work-queue
4. https://embetronicx.com/tutorials/linux/device-drivers/i2c-linux-device-driver-using-raspberry-pi/
5. https://embetronicx.com/tutorials/linux/device-drivers/workqueue-in-linux-kernel/
6. https://embetronicx.com/tutorials/linux/device-drivers/linux-device-driver-tutorial-part-13-interrupt-example-program-in-linux-kernel/
7. https://github.com/sparkfun/ISL29125_Breakout/tree/master
8. https://www.kernel.org/doc/html/v4.12/core-api/genericirq.html

