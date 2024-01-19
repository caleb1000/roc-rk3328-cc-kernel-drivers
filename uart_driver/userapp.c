#include <linux/ioctl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>

#define DEVICE "/dev/my_uart_driver"

#define START_IOC_MAGIC 'Z'
#define SEND_START_COMMAND _IOW(START_IOC_MAGIC, 1, unsigned long)

#define STOP_IOC_MAGIC 'Y'
#define SEND_STOP_COMMAND _IOW(STOP_IOC_MAGIC, 1, unsigned long)

#define INFO_IOC_MAGIC 'X'
#define SEND_INFO_COMMAND _IOW(INFO_IOC_MAGIC, 1, unsigned long)

#define STATUS_IOC_MAGIC 'W'
#define SEND_STATUS_COMMAND _IOW(STATUS_IOC_MAGIC, 1, unsigned long)

#define REBOOT_IOC_MAGIC 'V'
#define SEND_REBOOT_COMMAND _IOW(REBOOT_IOC_MAGIC, 1, unsigned long)


int main(int argc, char *argv[]) {
	int fd;
        char command, read_buf[500];
	fd = open(DEVICE, O_RDWR);
	if(fd == -1) {
		printf("File %s either does not exist or has been locked by another "
				"process\n", DEVICE);
		exit(-1);
	}
        while (1) {
        // Prompt user for input
        printf("Select a command:\n"
               "0 - Exit\n"
               "1 - Start\n"
               "2 - Stop\n"
               "3 - Info\n"
               "4 - Status\n"
               "5 - Reboot\n"
               "6 - Read Raw Data\n"
               "Enter command (0-6): ");
        scanf(" %c", &command);

        // Process user input
        switch (command) {
            case '1':
                ioctl(fd, SEND_START_COMMAND, 0);
                break;
            case '2':
                ioctl(fd, SEND_STOP_COMMAND, 0);
                break;
            case '3':
                ioctl(fd, SEND_INFO_COMMAND, 0);
                break;
            case '4':
                ioctl(fd, SEND_STATUS_COMMAND, 0);
                break;
            case '5':
                ioctl(fd, SEND_REBOOT_COMMAND, 0);
                break;
            case '6':
                read(fd, read_buf, sizeof(read_buf));
                //for(int x = 0; x < sizeof(read_buf); x++)
                //{
                //   printf("%x",read_buf[x]);
                //}
                if(read_buf[0] == 0xAA && read_buf[1] == 0x55){
                    printf("Valid header found\n");
                    printf("Packet type: %x\n",read_buf[2]);
                    int packet_size = (int)read_buf[3];
                    printf("Packet size: %d\n",packet_size);
                    uint8_t low_byte, high_byte;
                    u_int16_t raw_value;
                    float diff, fsa, lsa, angle;
                    low_byte = read_buf[6];
                    high_byte = read_buf[5];
                    raw_value = (u_int16_t)((high_byte << 8) | low_byte);
                    fsa = (float)(raw_value>>1)/64;
                    printf("Start angle - FSA: %f\n", fsa);
                    low_byte = read_buf[8];
                    high_byte = read_buf[7];
                    raw_value = (u_int16_t)((high_byte << 8) | low_byte);
                    lsa = (float)(raw_value>>1)/64;
                    printf("Stop angle - LSA: %f\n", lsa);
                    diff = lsa - fsa;
                    for(int x = 0; x < 2*packet_size; x+=2)
                    {
                       low_byte = read_buf[x+11];
                       high_byte = read_buf[x+10];
                       raw_value = (u_int16_t)((high_byte << 8) | low_byte);
                       float distance = (float)raw_value / 4;
                       printf("Scan %d: Distance: %.2fmm ", x/2, distance);
                       int i = (x/2) + 1;
                       if(i == 1)
                       {
                           printf("Angle: %.2f\n", fsa);
                       }
                       else if(i == packet_size)
                       {
                           printf("Angle: %.2f\n", lsa);
                       }
                       else{
                       angle = (diff/(packet_size-1)) * (i-1) + fsa;
                       printf("Angle: %.2f\n", angle);
                       }
                    }

                }
                else{
                    printf("NO VALID HEADER\n");
                }
                //printf("\n");
                break;
            case '0':
                // Exit the loop and close the file descriptor
                close(fd);
                return 0;
            default:
                printf("Invalid command. Please enter a valid command (0-5).\n");
        }
    }
    close(fd);
    return 0;
}


