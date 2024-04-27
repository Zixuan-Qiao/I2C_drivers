# I2C Drivers
A collection of linux kernel drivers developed for I2C sensors. 

## Introduction
For some included sensors, two different drivers are developed: 
a char device driver that provides interface to user-space and 
an I2C client driver that is more consistent with Linux conventions. 

## Currently Supported Devices
| Number | Functionality |
|:------:|:------:|
| DHT20 | Humidity and temperature sensor |

## Tested Platform
| Hardward host | Linux kernel version | Compiler version |
|:------:|:------:|:------:|
|BeagleBone Black Rev C3 | 5.10.168-ti-r71 | Debian gcc 10.2.1-6 |
