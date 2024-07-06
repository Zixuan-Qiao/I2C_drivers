#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#define PR_LIST _IO('S', 1)
#define CHG_MODE _IOW('S', 2, int)
#define CHG_ODR _IOW('S', 3, int)

double data_conver(unsigned char *data) {
	int temp;
	
	temp = data[1];	
	temp <<= 8;
	temp += data[0];
	
	if(temp < (1 << 15)) 
		return (double)temp / 100.0;
	
	return (double)(temp - (1 << 16)) / 100.0;
}

int main(int argc, char* argv[]) {
	int fd, mode, odr; 
	double value;
	char data[3];
	
	fd = open("/dev/STTS22H", O_RDWR);
	printf("Device file opened. \n");
	
	data[0] = 0x3C;
	data[1] = 2;
	data[2] = '\0';
	
	printf("Trying to set up address and adapter. \n");
	write(fd, data, 2 * sizeof(char));

	printf("Trying to print list. \n");
	ioctl(fd, PR_LIST);
	
	printf("Trying to change to one shot mode. \n");
	mode = 0;
	ioctl(fd, CHG_MODE, &mode);
	
	read(fd, data, 2 * sizeof(char));
	value = data_conver(data);
	printf("One-shot mode data: %lf, %02X %02X. \n", value, data[1], data[0]);
	
	printf("Trying to change to low ODR mode. \n");
	mode = 2;
	ioctl(fd, CHG_MODE, &mode);
	
	read(fd, data, 2 * sizeof(char));
	value = data_conver(data);
	printf("Low ODR mode data: %lf, %02X %02X. \n", value, data[1], data[0]);
	
	printf("Trying to change to free run mode. \n");
	mode = 1;
	ioctl(fd, CHG_MODE, &mode);
	
	printf("Trying to change to 50 Hz. \n");
	odr = 1;
	ioctl(fd, CHG_ODR, &odr);
	
	read(fd, data, 2 * sizeof(char));
	value = data_conver(data);
	printf("Free run mode data: %lf, %02X %02X. \n", value, data[1], data[0]);

	close(fd);
	printf("Device file closed. \n");

	return 0;
}
