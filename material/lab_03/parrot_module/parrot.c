#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* Needed for KERN_INFO */
#include <linux/init.h> /* Needed for the macros */
#include <linux/fs.h> /* Needed for file_operations */
#include <linux/slab.h> /* Needed for kmalloc */
#include <linux/uaccess.h> /* copy_(to|from)_user */

#include <linux/string.h>

#include "parrot.h"

#define MAJOR_NUM 97
#define DEVICE_NAME "parrot"

static char *global_buffer;
static int buffer_size;

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
		return 0;
	}

	// This a simple usage of ppos to avoid infinit loop with `cat`
	// it may not be the correct way to do.
	if (*ppos != 0) {
		return 0;
	}
	*ppos = buffer_size;

	copy_to_user(buf, global_buffer, buffer_size);

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
		return 0;
	}

	*ppos = 0;

	if (buffer_size != NULL) {
		kfree(global_buffer);
	}

	global_buffer = kmalloc(count + 1, GFP_KERNEL);

	copy_from_user(global_buffer, buf, count);
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
	if (buffer_size == NULL) {
		return -1;
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
			return -1;
		}
		break;

	default:
		break;
	}
	return 0;
}

const static struct file_operations parrot_fops = {
	.owner = THIS_MODULE,
	.read = parrot_read,
	.write = parrot_write,
	.unlocked_ioctl = parrot_ioctl,
};

static int __init parrot_init(void)
{
	register_chrdev(MAJOR_NUM, DEVICE_NAME, &parrot_fops);

	buffer_size = 0;

	pr_info("Parrot ready!\n");
	pr_info("ioctl PARROT_CMD_TOGGLE: %u\n", PARROT_CMD_TOGGLE);
	pr_info("ioctl PARROT_CMD_ALLCASE: %lu\n", PARROT_CMD_ALLCASE);

	return 0;
}

static void __exit parrot_exit(void)
{
	if (global_buffer != NULL) {
		kfree(global_buffer);
	}

	unregister_chrdev(MAJOR_NUM, DEVICE_NAME);

	pr_info("Parrot done!\n");
}

MODULE_AUTHOR("REDS");
MODULE_LICENSE("GPL");

module_init(parrot_init);
module_exit(parrot_exit);
