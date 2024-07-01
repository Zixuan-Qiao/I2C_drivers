# BMA400 Driver
BMA400 is a 3-axis accelerometer manufactured by Bosch. It is a relatively complex sensor with over 80 registers and 3 performance modes includeing sleep, low-power and normal. 

## Implemented Driver
### I2C driver

The implemented driver not only provide usual I2C client functions but also performs power management with 3 operation modes that are configurable at module load time.

**Low-power mode (default, mode=0)**

In low-power mode, the driver will instruct the device to operate at a low sampling rate (25Hz) to conserve energy. Auto wake-up interrupt is setup to monitor unusual events. 

Workflow

Sleep mode -> serial command -> low-power mode -> wake-up interrupt -> read register data -> serial command -> low-power mode (loop)

**Normal mode (mode=1)**

In normal mode, the device will operate at a configurable sampling rate. Data-ready interrupt is setup tp ensure timely processing. 

Workflow

Sleep mode -> serial command -> normal mode -> data-ready interrupt -> read register data -> normal mode (loop)

**Tap mode (mode=2)**

In tap mode, the device will operate at a higher sampling rate (>200Hz) to detect single taps. Interrupt will be sent out when a tap is detected. 

Workflow

Sleep mode -> normal mode -> tap inetrrupt -> log the system time when the interrupt is detected -> normal mode (loop)

## Schematic
<img width="450" alt="1" src="https://github.com/Zixuan-Qiao/I2C_drivers/assets/102449059/3b3bf3f0-5251-42ef-af8a-cfe48b40c9b9">


## References
1. https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bma400-ds000.pdf
2. https://lwn.net/Kernel/LDD3/
3. https://stackoverflow.com/questions/42018032/what-is-use-of-struct-i2c-device-id-if-we-are-already-using-struct-of-device-id
4. https://stackoverflow.com/questions/22901282/hard-time-in-understanding-module-device-tableusb-id-table-usage
