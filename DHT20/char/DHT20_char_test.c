#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#define INIT_SET_ADPT _IOW('D', 1, int)

int main(int argc, char* argv[]) {

	int adapter = 2;
	unsigned char data[6];
	double rh;
	double t;
	int i;
	unsigned int temp;
	
	rh = 0.0;
	t = 0.0;

	int fd = open("/dev/DHT20", O_RDWR);
	printf("Open\n");

	ioctl(fd, INIT_SET_ADPT, &adapter);
	printf("Init\n");
		
	data[5] = '\0';
	read(fd, data, 5); 

	temp = data[0];
	temp <<= 12;
	rh += temp;

	temp = data[1];
	temp <<= 4;
	rh += temp;

	temp = data[2];
	temp >>= 4;
	temp &= 0x0F;
	rh += temp;

	temp = data[2];
	temp &= 0x0F;
	temp <<= 16;
	t += temp;

	temp = data[3];
	temp <<= 8;
	t += temp;

	temp = data[4];
	t += temp;
    
    	rh = rh / (1024 * 1024) * 100;
    	t = t / (1024 * 1024) * 200 - 50;
    	
    	printf("Relative humidity: %lf%%, temperature: %lfC. \n", rh, t);
    	
	close(fd);
	printf("Closed\n");

	return 0;
}
