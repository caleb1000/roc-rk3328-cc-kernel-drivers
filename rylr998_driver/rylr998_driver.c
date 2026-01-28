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
MODULE_DESCRIPTION("This device driver is for the rylr998 LoRa");

/* Declate the probe and remove functions */
static int rylr998_probe(struct serdev_device *serdev);
static void rylr998_remove(struct serdev_device *serdev);


/* Enum used to determine the state of the rylr998 */
enum rylr998_state
{
    IDLE,          // Ready to send command
    WAITING,       // Command sent, waiting for reply
    ERROR,         // Error occured
};

/*TODO This should eventually be a per-device state */
static volatile enum rylr998_state state;

static struct of_device_id rylr998_ids[] = {
	{
		.compatible = "reyax,rylr998",
	}, { /* sentinel */ }
};

//TODO: Uncomment out when module isn't buggy
/* Used to auto load module if placed in modules properly*/
//MODULE_DEVICE_TABLE(of, rylr998_ids);

static struct serdev_device_driver rylr998_driver = {
	.probe = rylr998_probe,
	.remove = rylr998_remove,
	.driver = {
		.name = "rylr998",
		.of_match_table = rylr998_ids,
	},
};

/**
 * @brief Callback is called whenever a packet is recieved
 */
static int rylr998_recv(struct serdev_device *serdev, const unsigned char *buffer, size_t size) {
        //TODO: Add buffering and packet parsing to cover case where data is split up over recv callbacks 
	printk("rylr998 - Received %ld bytes with\n", size);

        print_hex_dump(KERN_INFO, "rylr998_recv: ",
               DUMP_PREFIX_NONE, 16, 1,
               buffer, size, true);

	printk("rylr998 - Setting idle");
        state = IDLE;
        return 	size;
}

static const struct serdev_device_ops rylr998_ops = {
	.receive_buf = rylr998_recv,
};


static int rylr998_send_command(struct serdev_device *serdev, const unsigned char* buffer, size_t count)
{
    int status;
    //Idle if we are currently waiting for a reply
    //TODO: replace with wait_event so we aren't busy waiting
    while(state == WAITING){}
    if(state == ERROR){return -1;}
    printk("rylr998 - Setting waiting");
    state = WAITING;
    status = serdev_device_write_buf(serdev, buffer, count);
    return status;
}


/**
 * @brief This function is called on loading the driver
 */
static int rylr998_probe(struct serdev_device *serdev) {
	int status;
	printk("rylr998 - Probe called.");

	serdev_device_set_client_ops(serdev, &rylr998_ops);
	status = serdev_device_open(serdev);
	if(status) {
		printk("rylr998 - Error opening serial port!\n");
		return -status;
	}
        state = IDLE;
	serdev_device_set_baudrate(serdev, 115200);
	serdev_device_set_flow_control(serdev, false);
	serdev_device_set_parity(serdev, SERDEV_PARITY_NONE);

        //Check if device can respond to commands
	status = rylr998_send_command(serdev, "AT\r\n", 4);
	printk("rylr998 - Check if rylr998 can respond to commands. Wrote %d bytes.\n", status);

        //Check what mode the device is in
	status = rylr998_send_command(serdev, "AT+MODE?\r\n", 10);
	printk("rylr998 - Check current device mode. Wrote %d bytes.\n", status);

        //Check what band the device is configured to use
	status = rylr998_send_command(serdev, "AT+BAND?\r\n", 10);
	printk("rylr998 - Check current device band. Wrote %d bytes.\n", status);

	return 0;
}

/**
 * @brief This function is called on unloading the driver
 */
static void rylr998_remove(struct serdev_device *serdev) {
	printk("rylr998 - Now I am in the remove function\n");
	serdev_device_close(serdev);
}

/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init my_init(void) {
	printk("rylr998 - Loading the driver...\n");
	if(serdev_device_driver_register(&rylr998_driver)) {
		printk("rylr998 - Error! Could not load driver\n");
		return -1;
	}
	return 0;
}

/**
 * @brief This function is called, when the module is removed from the kernel
 */
static void __exit my_exit(void) {
	printk("rylr998 - Unload driver");
	serdev_device_driver_unregister(&rylr998_driver);
}

module_init(my_init);
module_exit(my_exit);
