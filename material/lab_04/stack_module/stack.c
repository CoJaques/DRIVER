// License-Identifier: GPL-2.0
/*
 * Stack file
 */

#include "linux/device/class.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <linux/string.h>

#define MAJOR_NUM   98
#define MAJMIN	    MKDEV(MAJOR_NUM, 0)
#define DEVICE_NAME "stack"

struct stack_node {
	struct list_head list;
	uint32_t value;
};

struct stack_data {
	struct cdev cdev;
	struct class *cl;
	struct list_head head;
	ssize_t stack_size;
};

struct device *stack_device;

/**
 * @brief Pop and return latest added element of the stack.
 *
 * @param filp pointer to the file descriptor in use
 * @param buf destination buffer in user space
 * @param count maximum number of byte to read
 * @param ppos ignored
 *
 * @return Actual number of bytes read from internal buffer,
 *         or a negative error code
 */

static ssize_t stack_read(struct file *filp, char __user *buf, size_t count,
			  loff_t *ppos)
{
	struct stack_data *stack_data;
	ssize_t nb_values, i;
	uint32_t *read_values;
	struct stack_node *node;

	// get stack data from the class device contained into the file
	stack_data =
		container_of(filp->f_inode->i_cdev, struct stack_data, cdev);
	if (!stack_data) {
		pr_err("Stack: Unable to retrieve stack_data\n");
		return -ENODEV;
	}

	if (count % sizeof(uint32_t) != 0)
		return -EINVAL;

	nb_values = count / sizeof(uint32_t);

	if (stack_data->stack_size == 0)
		return 0;

	// check if the stack is smaller than the requested number of values
	// If so, we should return the actual number of values in the stack
	if (nb_values > stack_data->stack_size)
		nb_values = stack_data->stack_size;

	read_values = kmalloc(nb_values * sizeof(uint32_t), GFP_KERNEL);
	if (!read_values)
		return -ENOMEM;

	for (i = 0; i < nb_values; i++) {
		node = list_first_entry(&stack_data->head, struct stack_node,
					list);

		read_values[i] = node->value;
		list_del(&node->list);
		kfree(node);
	}

	stack_data->stack_size -= nb_values;

	if (copy_to_user(buf, read_values, nb_values * sizeof(uint32_t)) != 0) {
		kfree(read_values);
		pr_err("Stack: Failed to copy data to user\n");
		return -EFAULT;
	}

	kfree(read_values);

	// do not return count, because if the stack is smaller than count we
	// should return the actual number of bytes read
	return nb_values * sizeof(uint32_t);
}

/**
 * @brief Push the element on the stack
 *
 * @param filp pointer to the file descriptor in use
 * @param buf source buffer in user space
 * @param count number of byte to write in the buffer
 * @param ppos ignored
 *
 * @return Actual number of bytes writen to internal buffer,
 *         or a negative error code
 */

static ssize_t stack_write(struct file *filp, const char __user *buf,
			   size_t count, loff_t *ppos)
{
	struct stack_data *stack_data;
	ssize_t nb_values;
	uint32_t *new_values;
	struct stack_node *elements;

	// get stack data from the class device contained into the file
	stack_data =
		container_of(filp->f_inode->i_cdev, struct stack_data, cdev);
	if (!stack_data) {
		pr_err("Stack: Unable to retrieve stack_data\n");
		return -ENODEV;
	}

	if (count % sizeof(uint32_t) != 0)
		return -EINVAL;

	nb_values = count / sizeof(uint32_t);
	new_values = kmalloc(count, GFP_KERNEL);

	if (!new_values)
		return -ENOMEM;

	if (copy_from_user(new_values, buf, count) != 0) {
		kfree(new_values);
		pr_err("Stack: Failed to copy buffer from user\n");
		return -EFAULT;
	}

	elements =
		kmalloc_array(nb_values, sizeof(struct stack_node), GFP_KERNEL);
	if (!elements) {
		kfree(new_values);
		pr_err("Stack: Failed to allocate memory for stack elements\n");
		return -ENOMEM;
	}

	for (ssize_t i = 0; i < nb_values; i++) {
		elements[i].value = new_values[i];
		list_add(&elements[i].list, &stack_data->head);
	}

	stack_data->stack_size += nb_values;

	kfree(new_values);
	return count;
}

/**
 * @brief uevent callback to set the permission on the device file
 *
 * @param dev pointer to the device
 * @param env uevent environnement corresponding to the device
 */
static int stack_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	// Set the permissions of the device file
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}

static const struct file_operations stack_fops = {
	.owner = THIS_MODULE,
	.read = stack_read,
	.write = stack_write,
};

static int __init stack_init(void)
{
	int err;
	struct stack_data *stack_data;

	// Allocate memory for the device
	stack_data = kzalloc(sizeof(struct stack_data), GFP_KERNEL);
	if (stack_data == NULL) {
		pr_err("Stack: Error allocating memory for the device\n");
		return -ENOMEM;
	}

	// Initialize the stack
	INIT_LIST_HEAD(&stack_data->head);
	stack_data->stack_size = 0;

	// Register the device
	err = register_chrdev_region(MAJMIN, 1, DEVICE_NAME);
	if (err != 0) {
		pr_err("Stack: Registering char device failed\n");
		kfree(stack_data);
		return err;
	}

	stack_data->cl = class_create(THIS_MODULE, DEVICE_NAME);
	if (stack_data->cl == NULL) {
		pr_err("Stack: Error creating class\n");
		unregister_chrdev_region(MAJMIN, 1);
		kfree(stack_data);
		return -1;
	}
	stack_data->cl->dev_uevent = stack_uevent;
	stack_device =
		device_create(stack_data->cl, NULL, MAJMIN, NULL, DEVICE_NAME);

	if (stack_device == NULL) {
		pr_err("Stack: Error creating device\n");
		class_destroy(stack_data->cl);
		unregister_chrdev_region(MAJMIN, 1);
		kfree(stack_data);
		return -1;
	}

	// Set the device data
	dev_set_drvdata(stack_device, stack_data);

	cdev_init(&stack_data->cdev, &stack_fops);

	err = cdev_add(&stack_data->cdev, MAJMIN, 1);
	if (err < 0) {
		pr_err("Stack: Adding char device failed\n");
		device_destroy(stack_data->cl, MAJMIN);
		class_destroy(stack_data->cl);
		unregister_chrdev_region(MAJMIN, 1);
		kfree(stack_data);
		return err;
	}

	pr_info("Stack ready!\n");
	return 0;
}

static void __exit stack_exit(void)
{
	struct stack_data *stack_data;
	struct stack_node *node, *tmp;

	stack_data = dev_get_drvdata(stack_device);
	if (!stack_data) {
		pr_err("Stack: Unable to retrieve stack_data\n");
		return;
	}

	list_for_each_entry_safe(node, tmp, &stack_data->head, list) {
		list_del(&node->list);
		kfree(node);
	}

	cdev_del(&stack_data->cdev);
	device_destroy(stack_data->cl, MAJMIN);
	class_destroy(stack_data->cl);
	unregister_chrdev_region(MAJMIN, 1);

	kfree(stack_data);

	pr_info("Stack cleaned up successfully!\n");
}

MODULE_AUTHOR("REDS");
MODULE_LICENSE("GPL");

module_init(stack_init);
module_exit(stack_exit);
