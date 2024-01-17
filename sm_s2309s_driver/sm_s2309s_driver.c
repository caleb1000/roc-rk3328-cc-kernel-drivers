#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/pwm.h>

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Caleb Steinmetz");
MODULE_DESCRIPTION("A PWM kernel driver for the sm-s2309s servo");

/* Variables for device and device class */
static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

#define DRIVER_NAME "my_pwm_driver"
#define DRIVER_CLASS "MyModuleClass"

#define CDRV_IOC_MAGIC 'Z'
#define SET_PWM_DUTY _IOW(CDRV_IOC_MAGIC, 1, unsigned long)

/* Variables for pwm  */
struct pwm_device *pwm2 = NULL;
struct pwm_state state;
int           pwm_period   = 20000000;
unsigned long pwm_duty_min =  1000000;
unsigned long pwm_duty_max =  2000000;
int           pwm_duty     =  1000000;

long driver_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
	if (cmd == SET_PWM_DUTY) {
            if(arg >= pwm_duty_min && arg <= pwm_duty_max){
                pwm_duty = (int)arg;
                pwm_config(pwm2, pwm_duty, pwm_period);
                return 0;
            }
            else{
                pr_err("sm_s2309s: Invalid PWM duty cycle");
                return -1;
            }
	}

        pr_err("sm_s2309s: No supported command issued");
	return -1;
}

static ssize_t driver_write(struct file *File, const char *user_buffer, size_t count, loff_t *offs) {
	return 0;
}
static int driver_open(struct inode *device_file, struct file *instance) {
	return 0;
}

static int driver_close(struct inode *device_file, struct file *instance) {
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = driver_open,
	.release = driver_close,
	.write = driver_write,
        .unlocked_ioctl = driver_ioctl
};

/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init ModuleInit(void) {

        int ret = 0;

	/* Allocate a device nr */
	if( alloc_chrdev_region(&my_device_nr, 0, 1, DRIVER_NAME) < 0) {
		pr_err("sm_s2309s: Device Nr. could not be allocated!\n");
		return -1;
	}
	pr_info("sm_s2309s: read_write - Device Nr. Major: %d, Minor: %d was registered!\n", my_device_nr >> 20, my_device_nr && 0xfffff);

	/* Create device class */
	if((my_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) {
		pr_err("sm_s2309s: Device class can not be created!\n");
		goto ClassError;
	}

	/* create device file */
	if(device_create(my_class, NULL, my_device_nr, NULL, DRIVER_NAME) == NULL) {
		pr_err("sm_s2309s: Can not create device file!\n");
		goto FileError;
	}

	/* Initialize device file */
	cdev_init(&my_device, &fops);

	/* Regisering device to kernel */
	if(cdev_add(&my_device, my_device_nr, 1) == -1) {
		pr_err("sm_s2309s: Registering of device to kernel failed!\n");
		goto AddError;
	}
        //This is the legacy way to do it
	pwm2 = pwm_request(0, "my-pwm");
	if(pwm2 == NULL) {
		pr_err("sm_s2309s: Could not get pwm2!\n");
		goto AddError;
	}

	pwm_config(pwm2, pwm_duty_min, pwm_period);
	pwm_enable(pwm2);
        pwm_get_state(pwm2, &state);
        state.polarity = PWM_POLARITY_NORMAL;
        ret = pwm_apply_state(pwm2, &state);
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
static void __exit ModuleExit(void) {
	pwm_disable(pwm2);
	pwm_free(pwm2);
	cdev_del(&my_device);
	device_destroy(my_class, my_device_nr);
	class_destroy(my_class);
	unregister_chrdev_region(my_device_nr, 1);
	pr_info("sm_s2309s: Removing Module\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);
