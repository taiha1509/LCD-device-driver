#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/kdev_t.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("hant173085");
MODULE_DESCRIPTION("device driver LCD");

// contain major and minor number
static dev_t dev_type;
static struct class *dev_class;
static struct cdev my_device;

#define DRIVER_NAME "lcd"
#define DRIVER_CLASS_NAME "lcd1602"
#define LOW = 0;
#define HIGH = 1;

static char lcd_buffer[17];

unsigned int gpios[] = {
	1, /* Enable Pin */
	2, /* Register Select Pin */
	3, /* Data Pin 0*/
	4, /* Data Pin 1*/
	5, /* Data Pin 2*/
	6, /* Data Pin 3*/
	7, /* Data Pin 4*/
	8, /* Data Pin 5*/
	9, /* Data Pin 6*/
	10, /* Data Pin 7*/
};

#define REGISTER_SELECT gpios[1]

/**
 * Tu suon duong xuong suon am
 */
void lcd_enable(void) {
	// set enable pin to high
	gpio_set_value(gpios[0], HIGH);
	// delay 5 ms
	msleep(5);
	// set enable pin to low
	gpio_set_value(gpios[0], LOW);
}

/**
 * Che do 8 bit
 */
void lcd_send_byte(char data) {
	int i;
	for(i=0; i<8; i++)
		// send data to pin data 0xff => 11111111
		gpio_set_value(gpios[i+2], ((data) & (1<<i)) >> i);
	lcd_enable();
	msleep(5);
}

/**
 *
 * set RS pin to low (instruction mod) so data receive as instruction data
 * Set che do gui lenh va gui lenh
 */
 /* 
 	For Hex Code-01, the LCD command will be the clear LCD screen
	For Hex Code-02, the LCD command will be returning home
	For Hex Code-04, the LCD command will be decrement cursor
	For Hex Code-06, the LCD command will be Increment cursor
	For Hex Code-05, the LCD command will be Shift display right
	For Hex Code-07, the LCD command will be Shift display left
	For Hex Code-08, the LCD command will be Display off, cursor off
	For Hex Code-0A, the LCD command will be cursor on and display off
	For Hex Code-0C, the LCD command will be cursor off, display on
	For Hex Code-0E, the LCD command will be cursor blinking, Display on
	For Hex Code-0F, the LCD command will be cursor blinking, Display on
	For Hex Code-10, the LCD command will be Shift cursor position to left
	For Hex Code-14, the LCD command will be Shift cursor position to the right
	For Hex Code-18, the LCD command will be Shift the entire display to the left
	For Hex Code-1C, the LCD command will be Shift the entire display to the right
	For Hex Code-80, the LCD command will be Force cursor to the beginning ( 1st line)
	For Hex Code-C0, the LCD command will be Force cursor to the beginning ( 2nd line)
	For Hex Code-38, the LCD command will be 2 lines and 5×7 matrix
 */

void lcd_command(uint8_t data) {
 	gpio_set_value(REGISTER_SELECT, LOW);	
	lcd_send_byte(data);
}

/**
 * Set che do gui du lieu va gui du lieu
 *
 * 
 */
void lcd_data(uint8_t data) {
 	gpio_set_value(REGISTER_SELECT, HIGH);	
	lcd_send_byte(data);
}


/**
 * sned buffer data or command
 * send data to device, return the number of bute success written
 * ssize_t (*write) (struct file *, const char _ _user *, size_t, loff_t *);
 */
static ssize_t driver_write(struct file *File, const char *user_buffer, size_t count, loff_t *offs) {
	int to_copy, not_copied, delta, i;
	to_copy = min(count, sizeof(lcd_buffer));
	not_copied = copy_from_user(lcd_buffer, user_buffer, to_copy);
	delta = to_copy - not_copied;
	lcd_command(0x1);
	for(i=0; i<to_copy; i++)
		lcd_data(lcd_buffer[i]);

	return delta;
}

/**
 * Ham khoi tao file
 */
static int driver_open(struct inode *device_file, struct file *instance) {
	printk("LCD driver open\n");
	return 0;
}

/**
 * Ham khi dong file
 */
static int driver_close(struct inode *device_file, struct file *instance) {
	printk("LCD driver close\n");
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = driver_open,
	.release = driver_close,
	.write = driver_write
};

/**
 * Ham khoi tao driver
 */
static int __init ModuleInit(void) {
	int i;
	//khoi tao GPIO
	char *names[] = {"ENABLE_PIN", "REGISTER_SELECT", "DATA_PIN0", "DATA_PIN1", "DATA_PIN2", "DATA_PIN3", "DATA_PIN4", "DATA_PIN5", "DATA_PIN6", "DATA_PIN7"};
	printk("Hello LCD driver!\n");

	// cap phat dong cho major va minor number, 
	// int alloc_chrdev_region(dev_t *dev, unsigned int firstminor, unsigned int count, char *name);
	// Check the major number using cat /proc/devices
	if( alloc_chrdev_region(&dev_type, 0, 1, DRIVER_NAME) < 0) {
		printk("Device Nr. could not be allocated!\n");
		return -1;
	}
	printk("Nr. Major: %d, Minor: %d was registered!\n", dev_type >> 20, dev_type && 0xfffff);

	//This will create the struct class for our device driver. It will create a structure under/sys/class/.
	//struct class * class_create (struct module *owner, const char *name);
	//owner – pointer to the module that is to “own” this struct class
	//name – pointer to a string for the name of this class
	if((dev_class = class_create(THIS_MODULE, DRIVER_CLASS_NAME)) == NULL) {
		printk("Device class can not e created!\n");
		goto ClassError;
	}

	//A struct device will be created in sysfs, registered to the specified class (dev_class)
	// struct device *device_create (struct *class, struct device *parent, dev_t dev, const char *fmt, ...);
	// class – pointer to the struct class that this device should be registered to
	// parent – pointer to the parent struct device of this new device, if any
	// devt – the dev_t for the char device to be added
	// fmt – string for the device’s name
	if(device_create(dev_class, NULL, dev_type, NULL, DRIVER_NAME) == NULL) {
		printk("Can not create device file!\n");
		goto FileError;
	}

	// allocating and init cdev structure
	// void cdev_init(struct cdev *cdev, struct file_operations *fops);
	init(&my_device, &fops);

	// int add(struct cdev *dev, dev_t num, unsigned int count);
	// tell the kernel about it
	// after this step, your device is immediately alive
	if(cdev_add(&my_device, dev_type, 1) < 0) {
		printk("lcd-driver - Registering of device to kernel failed!\n");
		goto AddError;
	}

	/*Khoi tao GPIO, request chan*/
	printk("lcd-driver - GPIO Init\n");
	for(i=0; i<10; i++) {
		if(gpio_request(gpios[i], names[i])) {
			printk("lcd-driver - Error Init GPIO %d\n", gpios[i]);
			goto GpioInitError;
		}
	}
	
	/*Set che do output cac chan GPIO*/
	printk("lcd-driver - Set GPIOs to output\n");
	for(i=0; i<10; i++) {
		if(gpio_direction_output(gpios[i], 0)) {
			printk("lcd-driver, pin %d is not initialized\n", i);
			goto GpioDirectionError;
		}
	}

	/* Khoi tao LCD */
	lcd_command(0x30);	/* Set che do 8 bit */

	lcd_command(0xf);	/* Bat man hinh, bat con tro, dat nhap nhay con tro*/

	lcd_command(0x1);

	char text[] = "Hello World!";
	for(i=0; i<sizeof(text)-1;i++)
		lcd_data(text[i]);

	return 0;
GpioDirectionError:
	i=9;
GpioInitError:
	for(;i>=0; i--)
		gpio_free(gpios[i]);
AddError:
	device_destroy(dev_class, dev_type);
FileError:
	class_destroy(dev_class);
ClassError:
	// Device numbers are freed with
	//void unregister_chrdev_region(dev_t first, unsigned int count);
	unregister_chrdev_region(dev_type, 1);
	return -1;
}

/**
 * exit driver function
 */
static void __exit ModuleExit(void) {
	int i;
	lcd_command(0x1);	/* Clear the display */
	for(i=0; i<10; i++){
		gpio_set_value(gpios[i], LOW);
		gpio_free(gpios[i]);
	}
	//void cdev_del(struct cdev *dev);
	// remove char device from system
	cdev_del(&my_device);
	// free memory for cdev class
	device_destroy(dev_class, dev_type);
	class_destroy(dev_class);
	//  unregister a range of device numbers
	unregister_chrdev_region(dev_type, 1);
	printk("Goodbye, Kernel\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);