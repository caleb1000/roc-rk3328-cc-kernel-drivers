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

static struct serdev_device *uartdev;
char *my_buffer;

long driver_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
	if (cmd == SEND_START_COMMAND) {
            serdev_device_write_buf(uartdev, start_scan_mode_command, 2);
            pr_info("uart_driver - Start scan mode command");
            return 1;
	}
	else if (cmd == SEND_STOP_COMMAND) {
            serdev_device_write_buf(uartdev, stop_scan_mode_command, 2);
            pr_info("uart_driver - Stop scan mode command");
            return 1;
	}
	else if (cmd == SEND_INFO_COMMAND) {
            serdev_device_write_buf(uartdev, device_info_command, 2);
            pr_info("uart_driver - Device info command");
            return 1;
	}
	else if (cmd == SEND_STATUS_COMMAND) {
            serdev_device_write_buf(uartdev, health_status_command, 2);
            pr_info("uart_driver - Health status command");
            return 1;
	}
	else if (cmd == SEND_REBOOT_COMMAND) {
            serdev_device_write_buf(uartdev, reboot_command, 2);
            pr_info("uart_driver - Reboot command");
            return 1;
	}
        pr_err("uart_driver: No supported command issued");
	return -1;
}

static ssize_t driver_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
        //return my_buffer to users space

        while (1) {
            // Attempt to acquire the semaphore
            if (down_interruptible(&my_semaphore)) {
                //Failed to acquire semaphore, retry
                continue;
            }
            //Semaphore acquired
            break;
        }
        if(copy_to_user(buf, my_buffer, 500))
        {
            up(&my_semaphore);
	    pr_err("Error, invalid read!");
            return -EINVAL;
        }
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

         //Semaphore acquired
         printk("Size %ld\n", size);
         for(int x = 0; x < size; x++)
         {
            pr_info("%x\n",buffer[x]);
         }
         //TO:DO check header and check size, if buffer doesn't have complete packet make next write to buffer and do not memset

         //Clear my_buffer
         memset(my_buffer, 0, 500);
         //Save uart buffer to my_buffer
         memcpy(my_buffer, buffer, size);
         //Release semaphore
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
	pr_info("uart_driver - probe function!\n");

	serdev_device_set_client_ops(serdev, &uart_driver_ops);
	status = serdev_device_open(serdev);
	if(status) {
		pr_err("uart_driver - Error opening serial port!\n");
		return -status;
	}
	serdev_device_set_baudrate(serdev, 128000);
	serdev_device_set_flow_control(serdev, false);
	serdev_device_set_parity(serdev, SERDEV_PARITY_NONE);
        uartdev = serdev;

	return 0;
}

/**
 * @brief This function is called on unloading the driver 
 */
static void uart_driver_remove(struct serdev_device *serdev) {
	pr_info("uart_driver - Now I am in the remove function\n");
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

	pr_info("uart_driver - Loading the driver...\n");
	if(serdev_device_driver_register(&uart_driver_driver)) {
		printk("uart_driver - Error! Could not load driver\n");
		return -1;
	}

	/* Allocate a device nr */
	if( alloc_chrdev_region(&my_device_nr, 0, 1, DRIVER_NAME) < 0) {
		pr_err("uart_driver - Device Nr. could not be allocated!\n");
		return -1;
	}
	pr_info("uart_driver - read_write - Device Nr. Major: %d, Minor: %d was registered!\n", my_device_nr >> 20, my_device_nr && 0xfffff);

	/* Create device class */
	if((my_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) {
		pr_err("uart_driver - Device class can not be created!\n");
		goto ClassError;
	}

	/* create device file */
	if(device_create(my_class, NULL, my_device_nr, NULL, DRIVER_NAME) == NULL) {
		pr_err("uart_driver - Can not create device file!\n");
		goto FileError;
	}

	/* Initialize device file */
	cdev_init(&my_device, &fops);

	/* Regisering device to kernel */
	if(cdev_add(&my_device, my_device_nr, 1) == -1) {
		pr_err("uart_driver - Registering of device to kernel failed!\n");
		goto AddError;
	}

        //initialize semaphore for lidar data buffer
        sema_init(&my_semaphore, 1);

        //to:do - use this buffer to fill with lidar data and write to user space
        my_buffer = kmalloc(500, GFP_ATOMIC);

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
	pr_info("uart_driver - Unload driver");
	serdev_device_driver_unregister(&uart_driver_driver);
        cdev_del(&my_device);
	device_destroy(my_class, my_device_nr);
	class_destroy(my_class);
	unregister_chrdev_region(my_device_nr, 1);
	pr_info("uart_driver - Removing Module\n");
}

module_init(my_init);
module_exit(my_exit);
