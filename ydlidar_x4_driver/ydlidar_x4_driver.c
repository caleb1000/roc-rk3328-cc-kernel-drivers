#include <linux/module.h>
#include <linux/init.h>
#include <linux/serdev.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>

#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/slab.h>

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Caleb Steinmetz");
MODULE_DESCRIPTION("UART driver for the YDLIDAR X4");

/* Declate the probe and remove functions */
static int uart_driver_probe(struct serdev_device *serdev);
static void uart_driver_remove(struct serdev_device *serdev);

const unsigned char start_scan_mode_command[2]      = {0xA5,0x60};
const unsigned char stop_scan_mode_command[2]       = {0xA5,0x65};
const unsigned char device_info_command[2]          = {0xA5,0x90};
const unsigned char health_status_command[2]        = {0xA5,0x91};
const unsigned char reboot_command[2]               = {0xA5,0x80};

static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

static struct semaphore my_semaphore;
static struct serdev_device *uartdev;
//Boolean that states if buffer is ready to be read
bool buffer_ready = false;
//Boolean states if buffer is fragmented
bool buffer_frag = false;
//Int that keeps current offset written to buffer, used to write fragments to end of buffer
int  buffer_size = 0;
//Buffer that contains values
char *my_buffer;

bool scan_mode = false;

#define DRIVER_NAME "my_uart_driver"
#define DRIVER_CLASS "UartClass"

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

long driver_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
	if (cmd == SEND_START_COMMAND) {
            scan_mode = true;
            serdev_device_write_buf(uartdev, start_scan_mode_command, 2);
            pr_info("ydlidar_x4_driver - Start scan mode command");
            return 1;
	}
	else if (cmd == SEND_STOP_COMMAND) {
            scan_mode = false;
            serdev_device_write_buf(uartdev, stop_scan_mode_command, 2);
            pr_info("ydlidar_x4_driver - Stop scan mode command");
            return 1;
	}
	else if (cmd == SEND_INFO_COMMAND) {
            serdev_device_write_buf(uartdev, device_info_command, 2);
            pr_info("ydlidar_x4_driver - Device info command");
            return 1;
	}
	else if (cmd == SEND_STATUS_COMMAND) {
            serdev_device_write_buf(uartdev, health_status_command, 2);
            pr_info("ydlidar_x4_driver - Health status command");
            return 1;
	}
	else if (cmd == SEND_REBOOT_COMMAND) {
            serdev_device_write_buf(uartdev, reboot_command, 2);
            pr_info("ydlidar_x4_driver - Reboot command");
            scan_mode = false;
            return 1;
	}
	else if (cmd == CURRENT_MODE) {
            if(scan_mode){
                pr_info("ydlidar_x4_driver - Scan mode");
                return 1;
            }
            else{
                pr_info("ydlidar_x4_driver - Stop mode");
                return 0;
            }
	}
        pr_err("ydlidar_x4_driver - No supported command issued");
	return -1;
}

static ssize_t driver_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
        //return my_buffer to users space
        int attempts = 10;
        if(!scan_mode)
        {
	        pr_err("ydlidar_x4 - Error, not in scan mode!");
                return -EINVAL;
        }
        while (1) {
            // Attempt to acquire the semaphore
            if(attempts == 0)
            {
                up(&my_semaphore);
	        pr_err("ydlidar_x4 - Error, invalid read!");
                return -EINVAL;
            }
            if (down_interruptible(&my_semaphore)) {
                //Failed to acquire semaphore, retry
                attempts--;
                continue;
            }
            if(!buffer_ready)
            {
                //buffer isn't ready, release semaphore and try again
                up(&my_semaphore);
                continue;
            }
            //Semaphore acquired
            break;
        }
        if(copy_to_user(buf, my_buffer, 500))
        {
            up(&my_semaphore);
	    pr_err("ydlidar_x4 - Error, invalid read!");
            return -EINVAL;
        }
        //Buffer data has been read, set flag so that this old data is not read again
        buffer_ready = false;
        up(&my_semaphore);
	return 0;
}

//Device driver will eventually be writen for the YDLIDAR X4
static struct of_device_id uart_driver_ids[] = {
	{
		.compatible = "brightlight,echodev",
	}, { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, uart_driver_ids);

static struct serdev_device_driver uart_driver_driver = {
	.probe = uart_driver_probe,
	.remove = uart_driver_remove,
	.driver = {
		.name = "serdev-echo",
		.of_match_table = uart_driver_ids,
	},
};

/**
 * @brief Callback is called whenever a character is received
 */
static int uart_driver_recv(struct serdev_device *serdev, const unsigned char *buffer, size_t size) {

         if (down_interruptible(&my_semaphore)) {
             //Failed to aquire semaphore, no values read from buffer
             return 0;
         }
         //printk("\nHeader values %x%x",buffer[0],buffer[1]);
         //Semaphore acquired
         //printk("Uart Buffer Size %ld\n", size);
         //for(int x = 0; x < size; x++)
         //{
         //    pr_info("%x\n",buffer[x]);
         //}
         //TO:DO check header and check size, if buffer doesn't have complete packet make next write to buffer and do not memset
         if(buffer[0] == 0xAA && buffer[1] == 0x55)
         {
             //Account for 10 byte header and 16 bit words
             int uart_buffer_samples = ((int)size-10)/2;
             int packet_size = (int)buffer[3];
             buffer_size = size;
             //Check if partial buffer
             if(uart_buffer_samples != packet_size)
             {
                 //printk("Buffer incomplete - found %d, expected %d", uart_buffer_samples, packet_size);
                 //Buffer size is incomplete
                 memset(my_buffer, 0, 500);
                 //Save partial packet from uart buffer to my_buffer
                 memcpy(my_buffer, buffer, size);
                 buffer_ready = false;
                 buffer_frag = true;
                 up(&my_semaphore);
                 return size;
             }
             //Clear my_buffer
             memset(my_buffer, 0, 500);
             //Save uart buffer to my_buffer
             memcpy(my_buffer, buffer, size);
             //Release semaphore
             //printk("Buffer size correct - found %d, expected %d", uart_buffer_samples, packet_size);
             buffer_ready = true;
             buffer_frag = false;
             up(&my_semaphore);
             return size;
         }
         if(buffer_frag)
         {
             //copy fragment into buffer
             memcpy(my_buffer + buffer_size, buffer, size);
             buffer_ready = true;
             buffer_frag = false;
             buffer_size += size;
             up(&my_semaphore);
             return size;
         }
         //Header not valid and buffer is not fragmented
         memset(my_buffer, 0, 500);
         buffer_ready = false;
         buffer_frag = false;
         buffer_size = 0;
         up(&my_semaphore);
         return size;
}

static const struct serdev_device_ops uart_driver_ops = {
	.receive_buf = uart_driver_recv,
};

/**
 * @brief This function is called on loading the driver
 */
static int uart_driver_probe(struct serdev_device *serdev) {
	int status;
	pr_info("ydlidar_x4_driver - probe function!\n");

	serdev_device_set_client_ops(serdev, &uart_driver_ops);
	status = serdev_device_open(serdev);
	if(status) {
		pr_err("ydlidar_x4_driver - Error opening serial port!\n");
		return -status;
	}
	serdev_device_set_baudrate(serdev, 128000);
	serdev_device_set_flow_control(serdev, false);
	serdev_device_set_parity(serdev, SERDEV_PARITY_NONE);
        //Ensure device is in stop mode
        serdev_device_write_buf(serdev, stop_scan_mode_command, 2);
        //Set pointer to allow access to serdev from within ioctl
        uartdev = serdev;

	return 0;
}

/**
 * @brief This function is called on unloading the driver
 */
static void uart_driver_remove(struct serdev_device *serdev) {
	pr_info("ydlidar_x4_driver - Now I am in the remove function\n");
	serdev_device_close(serdev);
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = driver_read,
        .unlocked_ioctl = driver_ioctl
};

/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init my_init(void) {

	pr_info("ydlidar_x4_driver - Loading the driver...\n");
	if(serdev_device_driver_register(&uart_driver_driver)) {
		printk("ydlidar_x4_driver - Error! Could not load driver\n");
		return -1;
	}

	/* Allocate a device nr */
	if( alloc_chrdev_region(&my_device_nr, 0, 1, DRIVER_NAME) < 0) {
		pr_err("ydlidar_x4_driver - Device Nr. could not be allocated!\n");
		return -1;
	}
	pr_info("ydlidar_x4_driver - read_write - Device Nr. Major: %d, Minor: %d was registered!\n", my_device_nr >> 20, my_device_nr && 0xfffff);

	/* Create device class */
	if((my_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) {
		pr_err("ydlidar_x4_driver - Device class can not be created!\n");
		goto ClassError;
	}

	/* create device file */
	if(device_create(my_class, NULL, my_device_nr, NULL, DRIVER_NAME) == NULL) {
		pr_err("ydlidar_x4_driver - Can not create device file!\n");
		goto FileError;
	}

	/* Initialize device file */
	cdev_init(&my_device, &fops);

	/* Regisering device to kernel */
	if(cdev_add(&my_device, my_device_nr, 1) == -1) {
		pr_err("ydlidar_x4_driver - Registering of device to kernel failed!\n");
		goto AddError;
	}

        //initialize semaphore for lidar data buffer
        sema_init(&my_semaphore, 1);

        //to:do - use this buffer to fill with lidar data and write to user space
        my_buffer = kmalloc(500, GFP_KERNEL);

	return 0;

AddError:
	device_destroy(my_class, my_device_nr);
FileError:
	class_destroy(my_class);
ClassError:
	unregister_chrdev_region(my_device_nr, 1);
	return -1;
}

/**
 * @brief This function is called, when the module is removed from the kernel
 */
static void __exit my_exit(void) {
        //Free buffer
        kfree(my_buffer);
        //Set to null to avoid double freeing
        my_buffer = NULL;
        //Put lidar back into idle mode (stop scan)
        serdev_device_write_buf(uartdev, stop_scan_mode_command, 2);
	pr_info("ydlidar_x4_driver - Unload driver");
	serdev_device_driver_unregister(&uart_driver_driver);
        cdev_del(&my_device);
	device_destroy(my_class, my_device_nr);
	class_destroy(my_class);
	unregister_chrdev_region(my_device_nr, 1);
	pr_info("ydlidar_x4_driver - Removing Module\n");
}

module_init(my_init);
module_exit(my_exit);
