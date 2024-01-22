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
#include <stdbool.h>

#define PI 3.141592654

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

#define MODE_IOC_MAGIC 'U'
#define CURRENT_MODE _IOW(MODE_IOC_MAGIC, 1, unsigned long)


int main(int argc, char *argv[]) {
	int fd, packet_count;
        char command, read_buf[500];
	fd = open(DEVICE, O_RDWR);
	if(fd == -1) {
		printf("File %s either does not exist or has been locked by another "
				"process\n", DEVICE);
		exit(-1);
	}
        while (1) {
        if(ioctl(fd, CURRENT_MODE, 0) == 1)
        {
            printf("Currently in scan mode\n");
        }
        else
        {
            printf("Currently in stop mode\n");
        }
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
                // Prompt the user for input
                printf("Enter the number of packets you wish to parse: ");

                // Read the user input
                if (scanf("%d", &packet_count) != 1 || packet_count <= 0) {
                //Invalid input or non-positive integer
                printf("Invalid input. Please enter a positive integer.\n");
                    break;
                }

   for(int i = 0; i < packet_count; i++){
                if(read(fd, read_buf, sizeof(read_buf)) != 0)
                {
                    printf("Read failed (HINT: scan mode must be set first)!\n");
                    continue;
                }
                if(read_buf[0] == 0xAA && read_buf[1] == 0x55){
                    printf("\nValid header found\n");
                    printf("Packet type: %x\n",read_buf[2]);
                    int packet_size = (int)read_buf[3];
                    printf("Packet size: %d\n",packet_size);
                    uint8_t low_byte, high_byte;
                    u_int16_t raw_value;
                    float distance, diff, fsa, lsa, fsa_correct, lsa_correct, fsa_distance, lsa_distance, angle, angle_correct;
                    low_byte = read_buf[4];
                    high_byte = read_buf[5];
                    raw_value = (u_int16_t)((high_byte << 8) | low_byte);
                    fsa = (float)(raw_value>>1)/64;
                    low_byte = read_buf[6];
                    high_byte = read_buf[7];
                    raw_value = (u_int16_t)((high_byte << 8) | low_byte);
                    lsa = (float)(raw_value>>1)/64;

                    //If last angle is more than first add 360, this happens when the last angle passes 360 degrees
                    //This is done to preserve the interpolation of values between fsa and lsa
                    if(lsa < fsa)
                    {
                        lsa += 360;
                    }
                    diff = lsa - fsa;

                    //TO:DO - use checksum to validate packet

                    //Calculate FSA corrected angle value
                    low_byte = read_buf[10];
                    high_byte = read_buf[11];
                    raw_value = (u_int16_t)((high_byte << 8) | low_byte);
                    fsa_distance = (float)raw_value / 4;

                    if(fsa_distance == 0)
                    {
                        angle_correct = 0;
                    }
                    else
                    {
                        angle_correct = atan(21.8 * ((155.3 - fsa_distance)/(155.3*fsa_distance)));
                        angle_correct = (angle_correct * 180) / PI;
                    }
                    fsa_correct = fsa + angle_correct;

                    //Calculate LSA corrected angle value
                    low_byte = read_buf[packet_size + 10];
                    high_byte = read_buf[packet_size + 11];
                    raw_value = (u_int16_t)((high_byte << 8) | low_byte);
                    lsa_distance = (float)raw_value / 4;

                    if(lsa_distance == 0)
                    {
                        angle_correct = 0;
                    }
                    else
                    {
                        angle_correct = atan(21.8 * ((155.3 - lsa_distance)/(155.3*lsa_distance)));
                        angle_correct = (angle_correct * 180) / PI;
                    }
                    lsa_correct = lsa + angle_correct;

                    //Normalize fsa to 0-360 degrees
                    if(fsa_correct > 360)
                    {
                        fsa_correct -= 360;
                    }
                    else if(fsa_correct < 0)
                    {
                        fsa_correct += 360;
                    }
                    printf("Scan 1: Distance: %.2fmm Angle: %.2f\n", fsa_distance, fsa_correct);
                    //TO:DO we must actually correct the lsa and fsa first before doing other math
                    for(int x = 2; x < 2*packet_size - 2; x+=2)
                    {
                       int i = (x/2) + 1;
                       low_byte = read_buf[x+10];
                       high_byte = read_buf[x+11];
                       raw_value = (u_int16_t)((high_byte << 8) | low_byte);
                       distance = (float)raw_value / 4;
                       printf("Scan %d: Distance: %.2fmm ", i, distance);

                       if(distance == 0)
                       {
                           angle_correct = 0;
                       }
                       else
                       {
                           angle_correct = atan(21.8 * ((155.3 - distance)/(155.3*distance)));
                           angle_correct = (angle_correct * 180) / PI;
                       }

                       angle = ((diff/(packet_size-1)) * (i-1)) + fsa + angle_correct;
                       //Normalize angle to 0-360 degrees
                       if(angle > 360)
                       {
                           angle -= 360;
                       }
                       else if(angle < 0)
                       {
                           angle += 360;
                       }
                       printf("Angle: %.2f\n", angle);
                    }
                    //Normalize angle to 0-360 degrees
                    if(lsa_correct > 360)
                    {
                        lsa_correct -= 360;
                    }
                    else if(lsa_correct < 0)
                    {
                        lsa_correct += 360;
                    }
                    printf("Scan %d: Distance: %.2fmm Angle: %.2f\n", packet_size, lsa_distance, lsa_correct);

                }
                else{
                    printf("NO VALID HEADER\n");
                }
}
                break;
            case '0':
                // Exit the loop and close the file descriptor
                close(fd);
                return 0;
            default:
                printf("Invalid command. Please enter a valid command (0-6).\n");
        }
    }
    close(fd);
    return 0;
}


