// SPDX-License-Identifier: GPL-2.0
/*
 * REDS-adder driver, v3.1, 20.07.2022
 * (put your nice shiny header here --- we simply omit it since it takes up too
 * much space, you can cut&paste from the previous version)
 *
 * NOTE: in this driver we COULD have used again the MISC framework, but we opted
 * instead --- for educational purposes --- to create a separate device (and,
 * therefore, the major number will not be 10 and we will have to do some more
 * work before having our stuff in the correct places).
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("REDS");
MODULE_DESCRIPTION("REDS-adder driver v3.1");

/*
 * Offsets for the registers detailed in the documentation.
 */
#define ID_REG_OFF 0x00
#define INCR_REG_OFF 0x04
#define VALUE_REG_OFF 0x08
#define INIT_REG_OFF 0x0C
#define THRESH_REG_OFF 0x10
#define IRQ_MASK_REG_OFF 0x80
#define IRQ_CAPT_REG_OFF 0x84

/*
 * Some constants that come handy when manipulating the registers.
 * We could have hardcoded them in the code, but that could have backfired:
 * if, for instance, the customer changes its mind (happens 99% of the time) and
 * wants two possibile initializations, or if the hardware engineer discovers
 * that a bug requires that the value 0x02 is written in the re-initialization
 * register (instead of 0x01), then we might be forced to pass through the whole
 * code base fixing this (surely forgetting the fix it somewhere, and thus
 * introducing a very subtle and difficult-to-catch bug).
 */
/* Reinitialize the counter. */
#define REINIT_CNT 0x01
/*
 * Acknowledge a received interrupt (resetting the counter of the interrupt
 * register).
 */
#define ACK_IRQ 0x01
/* Default threshold for interrupt triggering. */
#define DEFAULT_THR 0x03
/* Disable the interrupt. */
#define INT_DISABLE 0x00
/* Enable the interrupt. */
#define INT_ENABLE 0x01
/* Disable the increments. */
#define INCR_DISABLE 0x00
/* Enable the increments. */
#define INCR_ENABLE 0x01

/* Name of the device. */
#define DEV_NAME "reds-adder"

/*
 * Maximum length of a vector to encrypt/decrypt.
 * Since we're going to use a KFIFO, this MUST be a power of 2.
 */
#define MAX_VEC_LEN 256

/**
 * @struct priv
 * @brief Private data for our driver.
 *
 * We will store here all the pointers and informations that are useful throughout
 * our driver. This information can be retrieved in the different functions of the
 * driver, thus allowing us to use the information that we stored there.
 *
 * @var priv::MEM_ptr
 * Pointer to the ioremap()ed memory.
 * @var priv::IRQ_num
 * IRQ number, retrieved from the DT.
 * @var priv::dev
 * Pointer to our device (will be useful when printing out messages).
 * @var priv::dev_num
 * Major number for our device in /dev.
 * @var priv::dev_class
 * This device family's class.
 * @var priv::cdev
 * Character device associated with the REDS-adder.
 * @var priv::dev_file
 * Pointer to the created device file.
 * @var priv::threshold
 * Encryption/decryption threshold in use.
 * @var priv::encrypt
 * Operation currently selected (encrypt when true, decrypt when false).
 * @var priv::read_mutex
 * Mutex protecting read() operations from changes to operation and threshold.
 * @var priv::read_queue
 * Wait queue used to have a read() that can block.
 * @var priv::data_fifo
 * KFIFO where the data to be encrypted/decrypted will be stored.
 * @var priv::tmp_buf
 * Temporary buffer used for encryption/decryption.
 */
struct priv {
	void *MEM_ptr;
	int IRQ_num;
	struct device *dev;

	dev_t dev_num;
	struct class *dev_class;
	struct cdev cdev;
	struct device *dev_file;

	int threshold;
	bool encrypt;
	struct mutex read_mutex;
	wait_queue_head_t read_queue;
	struct kfifo data_fifo;
	int tmp_buf[MAX_VEC_LEN];
};

/* Prototypes for the functions that operate on files. */
static ssize_t ra_file_read(struct file *filp, char __user *buf, size_t count,
			    loff_t *ppos);
static ssize_t ra_file_write(struct file *filp, const char __user *buf,
			     size_t count, loff_t *ppos);
static int ra_file_open(struct inode *inode, struct file *filp);
static int ra_file_release(struct inode *inode, struct file *filp);

/*
 * This is the list of functions relative to the file operations we perform in our
 * driver. We're not forced to fill the whole list. For instance, if your driver
 * has no write function, no need to fill the corresponding field with a pointer
 * to a function that does nothing.
 */
static const struct file_operations ra_fops = {

	.owner = THIS_MODULE,
	.open = ra_file_open,
	.release = ra_file_release, /* This is the file close() */
	.read = ra_file_read,
	.write = ra_file_write,
};

/* Prototypes for sysfs functions. */
static ssize_t show_max_str_len(struct device *dev,
				struct device_attribute *attr, char *buf);
static ssize_t store_operation(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count);
static ssize_t show_operation(struct device *dev, struct device_attribute *attr,
			      char *buf);
static ssize_t store_threshold(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count);
static ssize_t show_threshold(struct device *dev, struct device_attribute *attr,
			      char *buf);

/*
 * Declare a sysfs file, read-only, that allows the user to see the maximum length
 * of a string to by encrypted/decrypted.
 */
static DEVICE_ATTR(max_str_len, 0400, show_max_str_len, NULL);
/*
 * Declare a sysfs file that allows to see (and change) the current operation
 * performed by the device.
 */
static DEVICE_ATTR(operation, 0600, show_operation, store_operation);
/*
 * Declare a sysfs file that allows to see (and set) the threshold used in the
 * encryption/decryption process.
 */
static DEVICE_ATTR(threshold, 0600, show_threshold, store_threshold);

/* Group these sysfs attributes in a single sysfs group */
static struct attribute *ra_device_attrs[] = {

	/* Show the current string's maximum length. */
	&dev_attr_max_str_len.attr,
	/* Choose between encryption and decryption. */
	&dev_attr_operation.attr,
	/* Encryption/decryption threshold. */
	&dev_attr_threshold.attr,
	NULL,
};

/*
 * Put our sysfs attributes in the group we're going to create (this will save us
 * the inconvenience of creating them one-by-one.
 */
static const struct attribute_group ra_device_attribute_group = {

	.name = "ra_sysfs",
	.attrs = ra_device_attrs,
};

/*
 * !!!! WARNING !!!!
 * In the functions below, why do we divide the offset by 4 ???
 * 'priv->MEM_ptr' is a 'void *'-type pointer. According to the C11 standard,
 * pointer arithmetic on 'void *' is undefined (therefore Undefined Behavior),
 * but GCC chose to treat this kind of pointers as pointers to characters (thus,
 * bytes).
 * We are, however, casting this pointer to an 'int *'-type pointer, the integer
 * pointer arithmetic applies and thus adding '1' to a pointer moves it by 4
 * bytes.
 * The offsets given in the documentation are specified in bytes, so we have to
 * divide them by '4' to get the corresponding integer offset.
 * The advantage of having two dedicated functions for I/O is that we can rely on
 * them everywhere in our code; having ioread()/iowrite() in our driver instead is
 * not wrong, but we might forget that cast/division/offset/... and then spend a
 * day wondering why our peripheral behaves so strangely...
 */

/**
 * @brief Read a register from the REDS-adder device.
 *
 * @param priv: pointer to driver's private data
 * @param reg_offset: offset (in bytes) of the desired register
 *
 * @return: value read from the specified register.
 */
static int ra_read(struct priv const *const priv, int const reg_offset)
{
	/*
	 * Assertions that will print a stacktrace when the condition in
	 * parentheses is true. They will alert us if we, by accident, were to
	 * call a ra_read() before having a valid pointer to the mapped registers.
	 */
	WARN_ON(priv == NULL);
	WARN_ON(priv->MEM_ptr == NULL);

	/*
	 * As discussed above, the casting on 'priv->MEM_ptr' is important. If you
	 * remove it, ugly things will happen...
	 */
	return ioread32((int *)priv->MEM_ptr + (reg_offset / 4));
}

/**
 * @brief Write an integer value to a register of the REDS-adder device.
 *
 * @param priv: pointer to driver's private data
 * @param reg_offset: offset (in bytes) of the desired register
 * @param value: value that has to be written
 */
static void ra_write(struct priv const *const priv, int const reg_offset,
		     int const value)
{
	/*
	 * Assertions that will print a stacktrace when the condition in
	 * parentheses is true. They will alert us if we, by accident, were to
	 * call a ra_write() before having a valid pointer to the mapped
	 * registers.
	 */
	WARN_ON(priv == NULL);
	WARN_ON(priv->MEM_ptr == NULL);

	/*
	 * As discussed above, the casting on 'priv->MEM_ptr' is important. If you
	 * remove it, ugly things will happen...
	 */
	iowrite32(value, (int *)priv->MEM_ptr + (reg_offset / 4));
}

/**
 * @brief Initialization of the device file.
 *
 * The encryption/decryption process must start from a known state, therefore we
 * reset the internal counter.
 *
 * @param inode: structure used by the kernel to hold file information
 * @param filp: higher-level file description, that tracks the current cursor
 * position, thus used when the file is actually open.
 *
 * @return: 0.
 */
static int ra_file_open(struct inode *inode, struct file *filp)
{
	/*
	 * Retrieve the pointer to the private data embedded into the 'inode'
	 * structure using the 'container_of' macro.
	 * More info on this macro here:
	 * https://www.linuxjournal.com/files/linuxjournal.com/linuxjournal/articles/067/6717/6717s2.html
	 */
	struct priv *priv = container_of(inode->i_cdev, struct priv, cdev);
	/*
	 * Store the pointer to the private data in the 'file' structure for later
	 * use.
	 */
	filp->private_data = priv;

	/* Reset the counter used by the encryption/decryption process. */
	ra_write(priv, INIT_REG_OFF, REINIT_CNT);

	/*
	 * Since the device file we created does not support lseek(), we have to
	 * do this (instead of simply returning 0.
	 */
	return nonseekable_open(inode, filp);
}

/**
 * @brief Function called after a file close() is requested by the user.

 * @param inode: structure used by the kernel to hold file information
 * @param filp: higher-level file description, that tracks the current cursor
 * position, thus used when the file is actually open.

 * @return: 0.
 */
static int ra_file_release(struct inode *inode, struct file *filp)
{
	/*
	 * Invalidate the pointer stored in the 'file' structure.
	 */
	filp->private_data = NULL;

	return 0;
}

/**
 * @brief Retrieve an "encrypted/decrypted" vector from the device.
 *
 * Partial reads are allowed -- but will have a weird effect! Can you spot why?
 * (this is a potential bug or a feature, as it could add another "security"
 * measure in our device).
 * Analyze the problem and try to give a solution to this issue!
 *
 * If more data is requested than that currently in the KFIFO, the read() will
 * block until enough data is given.
 *
 * @param filp: pointer to the file descriptor in use
 * @param buf: data buffer used to discuss with the user space
 * @param count: size of the transfer requested
 * @param ppos: start position from which data should be read (ignored)
 *
 * @return: Number of bytes read from the internal KFIFO, or a negative error
 * code if an error occurred.
 */
static ssize_t ra_file_read(struct file *filp, char __user *buf, size_t count,
			    loff_t *ppos)
{
	/*
	 * Retrieve the pointer to our private data.
	 */
	struct priv *priv = filp->private_data;

	/*
	 * Counter used to iterate over the data while encrypting/decrypting it.
	 */
	int i;

	/*
	 * To simplify our life, if the user asks for more than our KFIFO can
	 * hold, we simply reject its request.
	 */
	if (count > MAX_VEC_LEN) {
		dev_err(priv->dev,
			"read(): cannot ask for more than buffer size !\n");
		return 0;
	}

	/*
	 * Since we operate on integers, we expect that the user asks for a number
	 * of bytes that is a multiple of the size of an integer.
	 */
	if (count % sizeof(int) != 0) {
		dev_err(priv->dev,
			"read(): the device operates on integers !\n");
		return 0;
	}

	/* First thing, acquire the lock that prevent conflicts with sysfs. */
	mutex_lock(&priv->read_mutex);

	if (count > kfifo_len(&(priv->data_fifo))) {
		/*
		 * Here the user is trying to read more than what we have in
		 * store, we have to sleep until its request can be satisfied...
		 */
		wait_event_interruptible(priv->read_queue,
					 kfifo_len(&(priv->data_fifo)) >=
						 count);
		dev_dbg(priv->dev, "read(): received wake up!\n");
	}

	/*
	 * Instead of operating on a value at a time, we dump all the interesting
	 * KFIFO content in a temporary buffer, and then encrypt/decrypt on the
	 * go.
	 */
	/* Transfer all the data we are interested in the buffer. */
	if (kfifo_out(&priv->data_fifo, priv->tmp_buf, count) < count) {
		mutex_unlock(&priv->read_mutex);
		dev_err(priv->dev, "read(): missing data in kfifo_out() !\n");
		return -EFAULT;
	}

	/* Reset the counter used by the encryption/decryption process. */
	ra_write(priv, INIT_REG_OFF, REINIT_CNT);

	/*
	 * Perform the hardware-assisted encryption/decryption. This for loop will
	 * trigger a set of interrupts each time we reach the threshold, resetting
	 * each time the internal counter. For efficiency, we encrypt/decrypt only
	 * the number of values requested by the user.
	 */
	if (priv->encrypt) {
		for (i = 0; i < count / sizeof(int); ++i) {
			priv->tmp_buf[i] += ra_read(priv, VALUE_REG_OFF);
			/*
			 * ??????????
			 * A bit of black magic happens here...
			 * Try removing the delay below and check what happens.
			 * You might want to add some fprintf()s to the test C
			 * code to help you in figuring out what happens...
			 */
			udelay(1000);
		}
	} else {
		for (i = 0; i < count / sizeof(int); ++i) {
			priv->tmp_buf[i] -= ra_read(priv, VALUE_REG_OFF);
			/*
			 * ??????????
			 * A bit of black magic happens here...
			 * Try removing the delay below and check what happens.
			 * You might want to add some fprintf()s to the test C
			 * code to help you in figuring out what happens...
			 */
			udelay(1000);
		}
	}

	/* Copy the data to the user. */
	if (copy_to_user(buf, priv->tmp_buf, count) != 0) {
		mutex_unlock(&priv->read_mutex);
		dev_err(priv->dev,
			"read(): error occurred in copy_to_user() operation !\n");
		return -EFAULT;
	}

	mutex_unlock(&priv->read_mutex);
	return count;
}

/**
 * @brief Store a vector to encode in the internal KFIFO.
 *
 * @param filp: pointer to the file descriptor in use
 * @param buf: data buffer coming from user space
 * @param count: size of the transfer requested
 * @param ppos: start position from which data should be written (ignored)
 *
 * @return: Number of bytes written in the internal KFIFO, or a negative error
 * code if an error occurred.
 */
static ssize_t ra_file_write(struct file *filp, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	/*
	 * Retrieve the pointer to our private data.
	 */
	struct priv *priv = filp->private_data;

	int rc;

	/*
	 * Since we operate on integers, we expect that the user offers a number
	 * of bytes that is a multiple of the size of an integer.
	 */
	if (count % sizeof(int) != 0) {
		dev_err(priv->dev,
			"write(): the device operates on integers !\n");
		return -EINVAL;
	}

	/*
	 * Check that we are not trying to overflow our internal KFIFO.
	 */
	if (count > kfifo_avail(&priv->data_fifo)) {
		dev_err(priv->dev,
			"write(): overflow attempt on internal KFIFO !\n");
		return -EINVAL;
	}

	/* Copy the data from the user into the KFIFO. */
	if (kfifo_from_user(&priv->data_fifo, buf, count, &rc) != 0) {
		dev_err(priv->dev,
			"write(): error occurred in kfifo_from_user() operation !\n");
		return -EFAULT;
	}

	if (rc != count) {
		dev_err(priv->dev,
			"write(): missing data in kfifo_from_user() operation !\n");
		return -EFAULT;
	}

	/* Wake the read() up (if it was sleeping). */
	wake_up_interruptible(&priv->read_queue);

	return count;
}

/**
 * @brief Display the maximum length of an input message to encode/decode.
 *
 * This function is pretty useless, since the returned value is a constant.
 * However, it shows you that it is not mandatory to implement both the show() and
 * the store() operations -- please check the permissions on the corresponding
 * file in sysfs !
 *
 * @param dev: pointer to our device
 * @param attr: pointer to the associated attributes (ignored)
 * @param buf: output buffer (where data for the user will be put)
 *
 * @returns: number of bytes produced
 */
static ssize_t show_max_str_len(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	/*
	 * Where does this 'PAGE_SIZE' constant come from? It's a symbol exported
	 * by the kernel. It's important in this case, since sysfs' I/Os are
	 * limited to a page.
	 */
	return snprintf(buf, PAGE_SIZE, "%d\n", MAX_VEC_LEN);
}

/**
 * @brief Change the operation performed by the device
 * ("encryption" <-> "decryption).
 *
 * Since this operation heavily impacts the encryption/decryption process, we
 * prevent the user from using it when a read() operation is in progress.
 * This is achieved thanks to a mutex that is acquired by the read() operation
 * when it starts, and not released until the operation is completed (warning: the
 * read might be put to sleep if the KFIFO does not contain enough samples -- to
 * avoid having a read on this sysfs entry stuck too, we use a trylock on the
 * mutex).
 *
 * @param dev: pointer to our device
 * @param attr: pointer to the associated attributes (ignored)
 * @param buf: input buffer (where user input will show up)
 * @param count: number of bytes to read from the input buffer
 *
 * @returns: number of bytes processed
 */
static ssize_t store_operation(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct priv *priv = dev_get_drvdata(dev);

	if (mutex_trylock(&priv->read_mutex)) {
		/*
		 * !! WARNING !!
		 * If we pass an operation with 'echo', for instance
		 * echo "encrypt" > operation
		 * sysfs will take a '\n' character at the end, which will
		 * invalidate the strcmp() results. To avoid this issue, we force
		 * the number of characters in the comparison (in this way, any
		 * additional character will be ignored). This has the side effect
		 * that 'encryptABCD' will also turn on the encryption, but we
		 * can live with that...
		 */
		if (strncmp(buf, "encrypt", strlen("encrypt")) == 0) {
			priv->encrypt = true;
		} else {
			if (strncmp(buf, "decrypt", strlen("decrypt")) == 0) {
				priv->encrypt = false;
			} else {
				dev_err(priv->dev,
					"Invalid operation requested!\n");
				return -EINVAL;
			}
		}
		mutex_unlock(&priv->read_mutex);
	} else {
		dev_err(priv->dev,
			"read() undergoing, cannot change operation now!\n");
		return -EINVAL;
	}

	return count;
}

/**
 * @brief Display the operation currently performed by the device.
 *
 * @param dev: pointer to our device
 * @param attr: pointer to the associated attributes (ignored)
 * @param buf: output buffer (where data for the user will be put)
 *
 * @returns: number of bytes produced
 */
static ssize_t show_operation(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);

	/*
	 * Where does this 'PAGE_SIZE' constant come from? It's a symbol exported
	 * by the kernel. It's important in this case, since sysfs' I/Os are
	 * limited to a page.
	 */
	/*
	 * Why snprintf() here? It is not a time-critical code, so even if it's
	 * not the most efficient call, we are not worried. As a bonus, we could
	 * easily replace the three show() operations with a macro -- with just
	 * three functions it is not worth the effort, but I have once written a
	 * driver with several dozen values to be put in sysfs, and explicitly
	 * rewriting each time the show() operation would have made the driver
	 * huge and error prone.
	 */
	return snprintf(buf, PAGE_SIZE, "%s\n",
			priv->encrypt ? "encrypt" : "decrypt");
}

/**
 * @brief Set a new threshold for the encryption/decryption process.
 *
 * Since this operation heavily impacts the encryption/decryption process, we
 * prevent the user from using it when a read() operation is in progress.
 * This is achieved thanks to a mutex that is acquired by the read() operation
 * when it starts, and not released until the operation is completed (warning: the
 * read might be put to sleep if the KFIFO does not contain enough samples -- to
 * avoid having a read on this sysfs entry stuck too, we use a trylock on the
 * mutex).
 *
 * @param dev: pointer to our device
 * @param attr: pointer to the associated attributes (ignored)
 * @param buf: input buffer (where user input will show up)
 * @param count: number of bytes to read from the input buffer
 *
 * @returns: number of bytes processed
 */
static ssize_t store_threshold(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct priv *priv = dev_get_drvdata(dev);

	if (mutex_trylock(&priv->read_mutex)) {
		int tmp;
		int rc;

		/*
		 * Do NOT forget to release the lock if things go bad...
		 */
		rc = kstrtoint(buf, 10, &tmp);
		if (rc != 0) {
			mutex_unlock(&priv->read_mutex);
			return rc;
		}
		if (tmp <= 0) {
			dev_err(priv->dev, "Invalid threshold specified!\n");
			mutex_unlock(&priv->read_mutex);
			return -EINVAL;
		}

		priv->threshold = tmp;
		ra_write(priv, THRESH_REG_OFF, priv->threshold);

		mutex_unlock(&priv->read_mutex);
		return count;
	}
	dev_err(priv->dev, "read() undergoing, cannot change threshold now!\n");
	return -EINVAL;
}

/**
 * @brief Display the current threshold used in encryption/decryption.

 * @param dev: pointer to our device
 * @param attr: pointer to the associated attributes (ignored)
 * @param buf: output buffer (where data for the user will be put)

 * @returns: number of bytes produced
 */
static ssize_t show_threshold(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);

	/*
	 * Use sysfs_emit as it will be aware of PAGE_SIZE
	 */
	return sysfs_emit(buf, "%d\n", priv->threshold);
}

/**
 * @brief IRQ handler.

 * React to the interrupts received from the board. For this tutorial we just have
 * to reset the counter to the initial value.

 * @param irq: interrupt's numerical value (the same used in devm_request_irq())
 * @param dev_id: pointer to the variable passed as last parameter in the
 * devm_request_irq() function

 * @returns: IRQ_HANDLED, since it always deals successfully with the interrupt
 */
irq_handler_t irq_handler(int irq, void *dev_id)
{
	/* We cast back the parameter to get our private data. */
	struct priv *priv = (struct priv *)dev_id;

	dev_info(
		priv->dev,
		"called %s()--- shouldn't print in an interrupt in real life, but this useful for debugging/understanding\n",
		__func__);

	/* Reinitialize the counter and acknowledge the interrupt. */
	ra_write(priv, INIT_REG_OFF, REINIT_CNT);
	ra_write(priv, IRQ_CAPT_REG_OFF, ACK_IRQ);

	/*
	 * We successfully handled the interrupt, so we inform the kernel that
	 * we're ok.
	 */
	return (irq_handler_t)IRQ_HANDLED;
}

/**
 * @brief Driver's probe function.

 * Since we have no init() function, all the initializations we require are made
 * here. Proper error checking is mandatory !
 * If this function fails, the module will be left in a sort of "zombie" state --
 * that is, it will still be loaded (we could see it with 'lsmod') but it won't do
 * much.

 * @param pdev: pointer to platform device's structure

 * @return: 0 when we were able to successfully initialize our driver, the failing
 * operation's error code otherwise.
 */
static int reds_adder_probe(struct platform_device *pdev)
{
	/*
	 * This will be our private data structure, used to share the variables
	 * we need for our job with all the other functions of the driver.
	 * NO, global variables are NOT allowed for that !
	 */
	struct priv *priv;
	/* DT entry referring to the memory region for registers. */
	struct resource *MEM_info;

	/* This variable will store our return codes. */
	int rc;

	/*
	 * Allocate the memory for our private data.
	 * If this call fails, something's *very* wrong, but we have to check
	 * anyway. The 'unlikely()' here is a hint for the compiler for
	 * optimization purposes (if the memory allocation fails, we have much
	 * bigger problems than "oh no, I'm going to get the performance hit of
	 * backtracking my subsequent commands...").
	 * We use kzalloc() instead of kmalloc() since zeroing out memory is
	 * always best  (if we can afford it).
	 */
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (unlikely(!priv)) {
		rc = -ENOMEM;
		goto return_fail;
	}
	/*
	 * This call stores the pointer to our private data in the platform
	 * device. This allows to retrieve this pointer in functions that we do
	 * not call directly (where we could pass this pointer as a parameter),
	 * such as the module's remove function.
	 */
	platform_set_drvdata(pdev, priv);
	/*
	 * We sometimes need a pointer to the device (e.g., for printing messages
	 * with dev_X()), and so we store it in our private data to ensure that
	 * we have it in whatever function we end up with.
	 */
	priv->dev = &pdev->dev;

	/* Set the threshold to its default value. */
	priv->threshold = DEFAULT_THR;
	/* The default operation is encryption. */
	priv->encrypt = true;
	/* Initialize the read mutex. */
	mutex_init(&priv->read_mutex);
	/* Initialize the wait-queue used for blocking read()s. */
	init_waitqueue_head(&priv->read_queue);
	/* Initialize the KFIFO used to store the data. */
	rc = kfifo_alloc(&priv->data_fifo, MAX_VEC_LEN * sizeof(int),
			 GFP_KERNEL);
	if (rc) {
		dev_err(&pdev->dev, "Failed to allocate the KFIFO!\n");
		rc = -ENOMEM;
		goto free_kfifo;
	}

	/*
	 * Retrieve the address of the register's region from the DT.
	 */
	MEM_info = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!MEM_info)) {
		dev_err(&pdev->dev,
			"Failed to get memory resource from device tree!\n");
		rc = -EINVAL;
		goto free_kfifo;
	}

	/*
	 * The address we have just retrieved is a physical (IO) address, we
	 * cannot access it directly so we have to map it to a virtual address.
	 */
	priv->MEM_ptr = devm_ioremap_resource(priv->dev, MEM_info);
	if (IS_ERR(priv->MEM_ptr)) {
		dev_err(&pdev->dev, "Failed to map memory!\n");
		rc = PTR_ERR(priv->MEM_ptr);
		goto free_kfifo;
	}

	/* Create our sysfs group entry. */
	rc = sysfs_create_group(&pdev->dev.kobj, &ra_device_attribute_group);
	if (rc) {
		dev_err(&pdev->dev, "Failed to create a sysfs group for RA!\n");
		goto free_kfifo;
	}

	/*
	 * Before enabling interrupts on the device, we register the function
	 * that is supposed to be called when one occurs.
	 */
	dev_info(&pdev->dev, "Registering interrupt handler\n");
	/* Retrieve the IRQ number from the DT. */
	priv->IRQ_num = platform_get_irq(pdev, 0);
	if (priv->IRQ_num < 0) {
		dev_err(&pdev->dev,
			"Failed to get interrupt resource from device tree!\n");
		rc = -EINVAL;
		goto destroy_sysfs_group;
	}
	/* Register the ISR function associated with the interrupt. */
	rc = devm_request_irq(
		&pdev->dev, /* Our device */
		priv->IRQ_num, /* IRQ number */
		(irq_handler_t)irq_handler, /* ISR */
		IRQF_SHARED, /* Flags */
		"reds_adder_irq_handler", /* Name in /proc/interrupts */
		(void *)priv /* Used to identify the device
			      * and to allow us access to
			      * private data in the IRQ
			      * handler
			      */
	);
	if (rc != 0) {
		dev_err(&pdev->dev,
			"REDS-adder_irq_handler: cannot register IRQ, error code: %d\n",
			rc);
		goto destroy_sysfs_group;
	}

	/*
	 * Since we have access to the device's registers, we can initialise
	 * the device. In particular, we can initialise the threshold for
	 * interrupt triggering and enable the interrupts.
	 * We also ensure that the increment register is set to '1' (we do not
	 * trust these hw people that much ;P).
	 */
	ra_write(priv, THRESH_REG_OFF, DEFAULT_THR);
	ra_write(priv, IRQ_MASK_REG_OFF, INT_ENABLE);
	ra_write(priv, INCR_REG_OFF, INCR_ENABLE);

	/*
	 * We now have to prepare the device file and register the associated
	 * operations -- so that the kernel knows how to react when the user
	 * performs an action on them.
	 * The "modern" way requires us to use the 'cdev' structure and the
	 * associated functions.
	 */
	/*
	 * Get a major and minor number from the kernel. This is way better than
	 * imposing these values by ourselves (Murphy's law will otherwise ensure
	 * that these values are already taken!).
	 */
	rc = alloc_chrdev_region(
		&priv->dev_num, /* Will contain the assigned numbers */
		0, /* First minor we request */
		1, /* Number of minors we want */
		DEV_NAME); /* Name of the device */
	if (rc != 0) {
		dev_err(&pdev->dev, "Cannot get a major/minor number pair !\n");
		goto destroy_sysfs_group;
	}
	/*
	 * We then have to create a class for our device (which will be visible in
	 * /sys/class). A class in an abstraction of our device. Example classes
	 * could be 'disk' and 'printer'.
	 * More details here:
	 * https://static.lwn.net/kerneldoc/driver-api/infrastructure.html#c.class
	 */
	priv->dev_class = class_create(THIS_MODULE, "ra");
	if (!priv->dev_class) {
		dev_err(&pdev->dev, "Failed to allocate device's class !\n");
		goto free_chrdev;
	}

	/*
	 * Initialize a cdev structure, register the file operations associated
	 * with the character device.
	 */
	cdev_init(&priv->cdev, &ra_fops);
	priv->cdev.owner = THIS_MODULE;

	/* We can now add the character device. */
	rc = cdev_add(&priv->cdev, /* This is our handle to the cdev */
		      priv->dev_num, /* The cdev will have this major/minor */
		      1); /* Number of minors to be added */
	if (rc != 0) {
		dev_err(&pdev->dev, "Failed to add cdev !\n");
		goto free_cdev;
	}

	/*
	 * We can finally create the device file in /dev and register it in sysfs.
	 */
	priv->dev_file = device_create(priv->dev_class, /* Device's class */
				       priv->dev, /* Parent device */
				       priv->dev_num, /* Major/minor numbers */
				       priv, /* Pointer to private data */
				       "reds-adder%d",
				       0); /* Device file's name */
	/*
	 * IS_ERR() is a macro that allows to test a pointer to check whether it's
	 * valid or not.
	 * See https://dev.gentoo.org/~cardoe/man-pages/man9/IS_ERR.9.html
	 */
	if (IS_ERR(priv->dev_file)) {
		dev_err(&pdev->dev, "Failed to create device file !\n");
		goto delete_cdev;
	}

	dev_info(&pdev->dev, "REDS-adder ready !\n");

	return 0;

delete_cdev:
	cdev_del(&priv->cdev);
free_cdev:
	class_destroy(priv->dev_class);
free_chrdev:
	unregister_chrdev(MAJOR(priv->dev_num), DEV_NAME);
destroy_sysfs_group:
	sysfs_remove_group(&pdev->dev.kobj, &ra_device_attribute_group);
free_kfifo:
	kfree(priv);
return_fail:
	return rc;
}

/**
 * @brief Remove the module from the kernel, cleaning up the allocated structures.
 *
 * @param pdev: pointer to platform driver's structure
 *
 * @return: 0.
 */
static int reds_adder_remove(struct platform_device *pdev)
{
	/*
	 * Since we have given the kernel the pointer to the private data in the
	 * probe() function by using:
	 * platform_set_drvdata(pdev, priv);
	 * we can now retrieve it from the platform_device structure and use it
	 * to get the pointers to the resources we have to free.
	 * We are not calling directly the remove() function, therefore the only
	 * other way we would have to get these values is by using a global
	 * variable <-- which would be bad bad bad bad, the karma will hit you
	 * hard if you use global variables !!!
	 */
	struct priv *priv = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "Removing driver...\n");

	/* Disable further interrupts. */
	ra_write(priv, IRQ_MASK_REG_OFF, INT_DISABLE);

	/*
	 * Unsurprisingly, the operations below are the same we perform in the
	 * probe() function when failures occur...
	 * We are lucky, most of the stuff is taken care of by the devm_X()
	 * functions.
	 */
	/* Destroy the device in /dev. */
	device_destroy(priv->dev_class, priv->dev_num);
	cdev_del(&priv->cdev);
	/* No other devices use this same class, so we can delete it. */
	class_destroy(priv->dev_class);
	/* De-register the character device. */
	unregister_chrdev(MAJOR(priv->dev_num), DEV_NAME);
	/* Free KFIFO memory. */
	kfifo_free(&priv->data_fifo);

	return 0;
}

/*
 * Here we list the identifiers that might appear in the DT for the devices that
 * are supported by this driver. At boot the kernel reads the list of devices on
 * the board from the DT, and once a module claims to be responsible for one of
 * these devices (via this compatible string), the kernel knows that it has to
 * call that driver's probe() function.
 */
static const struct of_device_id reds_adder_driver_id[] = {

	{ .compatible = "reds,reds-adder" },
	{ /* END */ },
};
MODULE_DEVICE_TABLE(of, reds_adder_driver_id);

/*
 * This structure gives the kernel a list of properties associated with this
 * module, for instance the name of the probe and remove functions.
 */
static struct platform_driver reds_adder_driver = {

	 .driver = {
		    .name = "reds-adder",
		    .owner = THIS_MODULE,
		    .of_match_table = of_match_ptr(reds_adder_driver_id),
		    },
	 .probe = reds_adder_probe,
	 .remove = reds_adder_remove,
	};

/*
 * We declare this as a platform driver. As a benefit, we get the init and exit
 * functions (that will take care of the register/unregister operations) for free.
 */
module_platform_driver(reds_adder_driver);
