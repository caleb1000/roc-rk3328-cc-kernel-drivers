#include <linux/ioctl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#define DEVICE "/dev/my_pwm_driver"

#define CDRV_IOC_MAGIC 'Z'
#define SET_PWM_DUTY _IOW(CDRV_IOC_MAGIC, 1, unsigned long)


int main(int argc, char *argv[]) {
	int fd;
	fd = open(DEVICE, O_RDWR);
	if(fd == -1) {
		printf("File %s either does not exist or has been locked by another "
				"process\n", DEVICE);
		exit(-1);
	}
    while(1){
        printf("Please enter the desired PWM duty cycle\n");
        unsigned long duty_cycle;
        scanf("%lu", &duty_cycle);
	int rc = ioctl(fd, SET_PWM_DUTY, duty_cycle);
	if (rc == -1) {
        printf("IOCTL: ASP_CLEAR_BUF=%ld", SET_PWM_DUTY);
            perror("Error: Failed to set to said duty cycle");
	}
    }
	close(fd);
	return 0; 
}
