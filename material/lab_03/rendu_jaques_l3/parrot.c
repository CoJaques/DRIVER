#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* Needed for KERN_INFO */
#include <linux/init.h> /* Needed for the macros */
#include <linux/fs.h> /* Needed for file_operations */
#include <linux/slab.h> /* Needed for kmalloc */
#include <linux/uaccess.h> /* copy_(to|from)_user */
#include <linux/cdev.h>
#include <linux/device.h>

#include "parrot.h"

#define DEVICE_NAME "parrot"

static char *global_buffer;
static char *original_buffer;
static int buffer_size;

// Device number (major + mino)
static dev_t dev_num;

// Character device structure
static struct cdev parrot_cdev;

// Class for auto-creating /dev node
static struct class *parrot_class;

/**
 * @brief String manipulation to put all char in upper/lower case or invert them.
 *
 * @param str        String on which the manipulation are done.
 * @param swap_lower Swap all lower case letters to upper case.
 * @param swap_upper Swap all upper case letters to lower case.
 */
static void str_manip(char *str, int swap_lower, int swap_upper)
{
	while (*str != '\0') {
		if (*str >= 'a' && *str <= 'z' && swap_lower) {
			*str = *str + ('A' - 'a');
		} else if (*str >= 'A' && *str <= 'Z' && swap_upper) {
			*str = *str + ('a' - 'A');
		}

		str++;
	}
}

/**
 * @brief Device file read callback to get the current value.
 *
 * @param filp  File structure of the char device from which the value is read.
 * @param buf   Userspace buffer to which the value will be copied.
 * @param count Number of available bytes in the userspace buffer.
 * @param ppos  Current cursor position in the file (ignored).
 *
 * @return Number of bytes written in the userspace buffer.
 */
static ssize_t parrot_read(struct file *filp, char __user *buf, size_t count,
			   loff_t *ppos)
{
	if (buf == 0 || count < buffer_size) {
		return -EINVAL;
	}

	// This a simple usage of ppos to avoid infinit loop with `cat`
	// it may not be the correct way to do.
	if (*ppos != 0) {
		return 0;
	}
	*ppos = buffer_size;

	if (copy_to_user(buf, global_buffer, buffer_size)) {
		return -EFAULT;
	}

	return buffer_size;
}

/**
 * @brief Device file write callback to set the current value.
 *
 * @param filp  File structure of the char device to which the value is written.
 * @param buf   Userspace buffer from which the value will be copied.
 * @param count Number of available bytes in the userspace buffer.
 * @param ppos  Current cursor position in the file.
 *
 * @return Number of bytes read from the userspace buffer.
 */
static ssize_t parrot_write(struct file *filp, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	if (count == 0) {
		return -EINVAL;
	}

	*ppos = 0;

	if (global_buffer != NULL) {
		kfree(global_buffer);
	}
	if (original_buffer != NULL) {
		kfree(original_buffer);
	}

	global_buffer = kmalloc(count + 1, GFP_KERNEL);
	original_buffer = kmalloc(count + 1, GFP_KERNEL);
	if (global_buffer == NULL || original_buffer == NULL) {
		kfree(global_buffer);
		kfree(original_buffer);
		return -ENOMEM;
	}

	// Copy data from user space and save it as both original and current buffer
	if (copy_from_user(global_buffer, buf, count)) {
		kfree(global_buffer);
		kfree(original_buffer);
		global_buffer = NULL;
		original_buffer = NULL;
		return -EFAULT;
	}

	strcpy(original_buffer, global_buffer);
	global_buffer[count] = '\0';
	buffer_size = count + 1;

	return count;
}

/**
 * @brief Device file ioctl callback. This permits to modify the stored string.
 *        - If the command is PARROT_CMD_TOGGLE, then the letter case in inverted.
 *        - If the command is PARROT_CMD_ALLCASE, then all letter will be set to
 *          upper case (arg = TO_UPPERCASE) or lower case (arg = TO_LOWERCASE)
 *
 * @param filp File structure of the char device to which ioctl is performed.
 * @param cmd  Command value of the ioctl
 * @param arg  Optionnal argument of the ioctl
 *
 * @return 0 if ioctl succeed, -1 otherwise.
 */

static long parrot_ioctl(struct file *filep, unsigned int cmd,
			 unsigned long arg)
{
	if (global_buffer == NULL || original_buffer == NULL) {
		return -ENOMEM;
	}

	switch (cmd) {
	case PARROT_CMD_TOGGLE:
		str_manip(global_buffer, 1, 1);
		break;

	case PARROT_CMD_ALLCASE:
		switch (arg) {
		case TO_UPPERCASE:
			str_manip(global_buffer, 1, 0);
			break;

		case TO_LOWERCASE:
			str_manip(global_buffer, 0, 1);
			break;

		default:
			return -EINVAL;
		}
		break;
	case PARROT_CMD_RESET:
		strcpy(global_buffer, original_buffer);
		break;

	default:
		return -EINVAL;
		break;
	}
	return 0;
}

static const struct file_operations parrot_fops = {
	.owner = THIS_MODULE,
	.read = parrot_read,
	.write = parrot_write,
	.unlocked_ioctl = parrot_ioctl,
};

static int parrot_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}

static int __init parrot_init(void)
{
	int res;

	// Allocate a major number dynamically
	res = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
	if (res < 0) {
		pr_err("Failed to allocate a major number\n");
		return res;
	}

	// Initialize the cdev structure and add it to the kernel
	cdev_init(&parrot_cdev, &parrot_fops);
	parrot_cdev.owner = THIS_MODULE;

	res = cdev_add(&parrot_cdev, dev_num, 1);
	if (res < 0) {
		pr_err("Failed to add cdev\n");
		unregister_chrdev_region(dev_num, 1);
		return res;
	}

	// Create class for auto-creating the /dev entry
	parrot_class = class_create(DEVICE_NAME);
	if (IS_ERR(parrot_class)) {
		pr_err("Failed to create class\n");
		cdev_del(&parrot_cdev);
		unregister_chrdev_region(dev_num, 1);
		return PTR_ERR(parrot_class);
	}
	parrot_class->dev_uevent = parrot_uevent;

	// Create the device node /dev/parrot
	if (device_create(parrot_class, NULL, dev_num, NULL, DEVICE_NAME) ==
	    NULL) {
		pr_err("Failed to create device\n");
		class_destroy(parrot_class);
		cdev_del(&parrot_cdev);
		unregister_chrdev_region(dev_num, 1);
		return -1;
	}

	buffer_size = 0;

	pr_info("Parrot ready! Major: %d\n", MAJOR(dev_num));
	pr_info("ioctl PARROT_CMD_TOGGLE: %u\n", PARROT_CMD_TOGGLE);
	pr_info("ioctl PARROT_CMD_ALLCASE: %lu\n", PARROT_CMD_ALLCASE);
	pr_info("ioctl PARROT_CMD_ALLCASE: %u\n", PARROT_CMD_RESET);

	return 0;
}

static void __exit parrot_exit(void)
{
	if (global_buffer != NULL) {
		kfree(global_buffer);
	}
	if (original_buffer != NULL) {
		kfree(original_buffer);
	}

	device_destroy(parrot_class, dev_num);
	class_destroy(parrot_class);

	cdev_del(&parrot_cdev);
	unregister_chrdev_region(dev_num, 1);

	pr_info("Parrot done!\n");
}

MODULE_AUTHOR("REDS");
MODULE_LICENSE("GPL");

module_init(parrot_init);
module_exit(parrot_exit);
