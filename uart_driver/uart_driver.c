#include <linux/module.h>
#include <linux/init.h>
#include <linux/serdev.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Caleb Steinmetz");
MODULE_DESCRIPTION("A simple loopback driver for an UART port. This will be used to create a UART driver for the YDLIDAR X4");

/* Declate the probe and remove functions */
static int uart_driver_probe(struct serdev_device *serdev);
static void uart_driver_remove(struct serdev_device *serdev);

const unsigned char start_scan_mode_command[2]      = {0xA5,0x60};
const unsigned char stop_scan_mode_command[2]       = {0xA5,0x65};
const unsigned char device_info_command[2]          = {0xA5,0x90};
const unsigned char health_status_command[2]        = {0xA5,0x91};
const unsigned char reboot_command[2]               = {0xA5,0x80};

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
         pr_info("Raw bytes read");
         for(int x = 0; x < size; x++)
         {
            pr_info("%x\n",buffer[x]);
         }
         return 0;
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

	status = serdev_device_write_buf(serdev, start_scan_mode_command, 2);
	pr_info("uart_driver - Start scan mode command: %d bytes sent.\n", status);

	return 0;
}

/**
 * @brief This function is called on unloading the driver 
 */
static void uart_driver_remove(struct serdev_device *serdev) {
	pr_info("uart_driver - Now I am in the remove function\n");
	serdev_device_close(serdev);
}

/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init my_init(void) {
	pr_info("uart_driver - Loading the driver...\n");
	if(serdev_device_driver_register(&uart_driver_driver)) {
		printk("uart_driver - Error! Could not load driver\n");
		return -1;
	}
	return 0;
}

/**
 * @brief This function is called, when the module is removed from the kernel
 */
static void __exit my_exit(void) {
	pr_info("uart_driver - Unload driver");
	serdev_device_driver_unregister(&uart_driver_driver);
}

module_init(my_init);
module_exit(my_exit);
