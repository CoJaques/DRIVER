// SPDX-License-Identifier: GPL-2.0
/*
 * REDS-adder driver, v1.1, 20.07.2022
 *
 * This driver initializes the peripheral and offers a read() and write()
 * function to the user. The user can use these functions to "encrypt" a vector
 * by adding to each number in the vector the current value of the counter,
 * with the counter resetting when it reaches its maximum value (specified below
 * by the constant DEFAULT_THR).
 *
 * This might look like a poor man's encryption scheme, but it could even be more
 * robust than ROT13 (https://en.wikipedia.org/wiki/ROT13) ;-). *
 * Of course the beginning of the vector can always be decrypted by subtracting
 * 1, 2, ... from the encrypted vector, and the transition to unintelligible
 * text marks the "key" of our encryption scheme. Also, changing the key requires
 * the user to recompile the driver.

 * We'll find a solution to these issues in the future tutorials. Why? Drivers
 * are written in an incremental way:
 * - first, write something that can be insmod'ed and that prints an hello
 *   -> I can compile my driver and I can load it
 * - then, read a known value from the device
 *   -> I can at least read a register of my device
 * - perform basic read/write operations
 *   -> I can interact with the user somehow
 * - ...
 * Trying to do everthing at once is a proven recipe for failure !
 *
 * !!! Warning !!!
 * Our device is quite generic, in that it just takes some integer values as
 * inputs and sums a counter (modulo DEFAULT_THR) to these values. The resulting
 * integer values are then given as output.
 * The driver has thus to operate on integers, and it will be the task of the
 * associated user space program to perform the required conversions.
 *
 * Notes:
 * - please observe the prototypes of the functions we call directly (i.e., whose
 *   prototype is not imposed by the kernel. If we take e.g. ra_read() below, we
 *   have:
 *   static int ra_read(struct priv const * const priv,
 *                      int const reg_offset);
 *   + 'static' here ensures that this function stays "local" to this file, that
 *     is, it will not pollute the kernel's naming space (actually, this is not
 *     required as the kernel already "sandboxes" this namespace, but it is good
 *     practice).
 *   + 'struct priv const * const priv' is a constant pointer (second 'const') to
 *     a struct of type 'priv' whose content is also constant (first 'const').
 *   The 'const' keyword is sadly not used much in kernel development, but is good
 *   engineering practice -- since we know that we are not expected to modify
 *   neither the pointed structure, nor the pointer itself, any attempt to do so
 *   in the function is a bug for sure, and the 'const' keyword would allow us to
 *   let the compiler catch this mistake for us.
 * - we can only deal with a single string of maximum fixed length at a time,
 *   that is, once the user writes a vector to encode, he/she has to read the
 *   encoded version before being able to encode the subsequent vector. Can we do
 *   any better? Of course, we could store the encoded strings in a list and
 *   return them once the user asks for them.
 * - our driver allows for encryption only -- can't we decrypt ? Yes, but we
 *   would need a mechanism to tell the driver what we want to do when we write a
 *   vector (encode or decode it?). We will see this in the coming weeks.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>

/*
 * Offsets for the registers detailed in the documentation.
 */
#define ID_REG_OFF	 0x00
#define INCR_REG_OFF	 0x04
#define VALUE_REG_OFF	 0x08
#define INIT_REG_OFF	 0x0C
#define THRESH_REG_OFF	 0x10
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
#define REINIT_CNT	 0x01
/*
 * Acknowledge a received interrupt (resetting the counter of the interrupt
 * register).
 */
#define ACK_IRQ		 0x01
/* Default threshold for interrupt triggering. */
#define DEFAULT_THR	 0x03
/* Disable the interrupt. */
#define INT_DISABLE	 0x00
/* Enable the interrupt. */
#define INT_ENABLE	 0x01
/* Disable the increments. */
#define INCR_DISABLE	 0x00
/* Enable the increments. */
#define INCR_ENABLE	 0x01

/* Name of the device. */
#define DEV_NAME	 "reds-adder"

/* Maximum length of a vector to encrypt. */
#define MAX_VEC_LEN	 256

/*
 * @struct priv
 * @brief Private data for our driver.
 *
 * We will store here all the pointers and informations that are useful throughout
 * our driver. This information can be retrieved in the different functions of the
 * driver, thus allowing us to use the information that we stored there.
 *
 * @var priv::mem_ptr
 * Pointer to the ioremap()ed memory.
 * @var priv::irq_num
 * IRQ number, retrieved from the DT.
 * @var priv::dev
 * Pointer to our device (will be useful when printing out messages).
 * @var priv::miscdev
 * MISC device file.
 * @var priv::buffer
 * Internal buffer for vector encryption.
 * @var priv::data_size
 * Number of elements currently in the internal buffer.
 */
struct priv {
	void *mem_ptr;
	int irq_num;
	struct device *dev;
	struct miscdevice miscdev;

	int buffer[MAX_VEC_LEN + 1];
	int data_size;
};

/*
 * !!!! WARNING !!!!
 * In the functions below, why do we divide the offset by 4 ???
 * 'priv->mem_ptr' is a 'void *'-type pointer. According to the C11 standard,
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

/*
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
	WARN_ON(priv->mem_ptr == NULL);

	/*
	 * As discussed above, the casting on 'priv->mem_ptr' is important. If you
	 * remove it, ugly things will happen...
	 */
	return ioread32((int *)priv->mem_ptr + (reg_offset / 4));
}

/*
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
	WARN_ON(priv->mem_ptr == NULL);

	/*
	 * As discussed above, the casting on 'priv->mem_ptr' is important. If you
	 * remove it, ugly things will happen...
	 */
	iowrite32(value, (int *)priv->mem_ptr + (reg_offset / 4));
}

/*
 * @brief Initialization of the device file.
 *
 * The encryption process must start from a known state, therefore we reset the
 * internal counter.
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
	 * Retrieve the pointer to the private data embedde into the 'inode'
	 * structure using the 'container_of' macro.
	 * More info on this macro here:
	 * https://www.linuxjournal.com/files/linuxjournal.com/linuxjournal/articles/067/6717/6717s2.html
	 */
	struct miscdevice *miscdev = filp->private_data;
	struct priv *priv = container_of(miscdev, struct priv, miscdev);

	dev_info(priv->dev,
		 "called %s, resetting counter and initializing stuff\n", __func__);

	/*
	 * Store the pointer to the private data in the 'file' structure for later
	 * use.
	 */
	filp->private_data = priv;

	/* Reinitialize the length of the vector stored in the buffer. */
	priv->data_size = 0;

	/* Reset the counter used by the encryption process. */
	ra_write(priv, INIT_REG_OFF, REINIT_CNT);

	/*
	 * Since the device file we created does not support lseek(), we have to
	 * do this (instead of simply returning 0.
	 */
	return nonseekable_open(inode, filp);
}

/*
 * @brief Function called after a file close() is requested by the user.
 *
 * @param inode: structure used by the kernel to hold file information
 * @param filp: higher-level file description, that tracks the current cursor
 * position, thus used when the file is actually open.
 *
 * @return: 0.
 */
static int ra_file_release(struct inode *inode, struct file *filp)
{
	struct priv *priv = filp->private_data;

	dev_info(priv->dev, "called %s\n", __func__);

	/*
	 * Invalidate the pointer stored in the 'file' structure.
	 */
	filp->private_data = NULL;

	return 0;
}

/*
 * @brief Retrieve an "encrypted" vector from the device.
 *
 * This is where the actual encryption is performed. For simplicity, we assume
 * that once you read from the device, even if only part of the vector has been
 * read, the current content of the whole buffer has to be discarded.
 * If more than the actual content of the buffer is asked, we return what we have
 * and then return an EOF.
 *
 * @param filp: pointer to the file descriptor in use
 * @param buf: data buffer used to discuss with the user space
 * @param count: size of the transfer requested
 * @param ppos: start position from which data should be read (ignored)
 *
 * @return: Number of bytes read from the internal buffer, or a negative error
 * code if an error occurred.
 */
static ssize_t ra_file_read(struct file *filp, char __user *buf, size_t count,
			    loff_t *ppos)
{
	/*
	 * Retrieve the pointer to our private data.
	 */
	struct priv *priv = filp->private_data;

	/* Counter used to iterate over the buffer while encrypting it. */
	int i;

	/* Number of data values requested by the user. */
	int ndata;

	/*
	 * To simplify our life, if the user asks for more than our buffer can
	 * hold, we simply reject its request.
	 */
	if (count > MAX_VEC_LEN) {
		dev_err(priv->dev,
			"read(): cannot ask for more than buffer size !\n");
		return 0;
	}

	dev_info(priv->dev,
		 "called %s, count = %d, internal buf len = %d\n",
		 __func__, count, priv->data_size);

	/*
	 * Check that the internal buffer is not empty, otherwise the user hasn't
	 * performed the required write().
	 */
	if (priv->data_size == 0) {
		/* Return EOF. */
		return 0;
	}

	/*
	 * Since we operate on integers, we expect that the user asks for a number
	 * of bytes that is a multiple of the size of an integer.
	 */
	if (count % sizeof(int) != 0) {
		dev_err(priv->dev,
			"read(): the encryption device operates on integers !\n");
		return 0;
	}

	ndata = count / sizeof(int);
	if (ndata > priv->data_size) {
		/*
		 * Here the user is trying to read more than what we have in
		 * store, simply threshold his/her request.
		 */
		ndata = priv->data_size;
	}

	/*
	 * Perform the hardware-assisted encryption. This for loop will trigger a
	 * set of interrupts each time we reach DEFAULT_THR, resetting each time
	 * the internal counter. For efficiency, we encrypt only the number of
	 * values requested by the user.
	 */
	for (i = 0; i < ndata; ++i) {
		priv->buffer[i] += ra_read(priv, VALUE_REG_OFF);
		/*
		 * ??????????
		 * A bit of black magic happens here...
		 * Try removing the delay below and check what happens. You might
		 * want to add some fprintf()s to the test C code to help you in
		 * figuring out what happens...
		 */
		udelay(1000);
	}

	/* Copy the data to the user. */
	if (copy_to_user(buf, priv->buffer, ndata * sizeof(int)) != 0) {
		dev_err(priv->dev,
			"read(): error occurred in copy_to_user() operation !\n");
		return -EFAULT;
	}

	/*
	 * Reset the internal counter (equivalent to emptying our buffer).
	 */
	priv->data_size = 0;

	return ndata * sizeof(int);
}

/*
 * @brief Store a vector to encode in the internal buffer.
 *
 * For simplicity we assume that writes are one-shot, that is, the whole vector
 * to encrypt is written at once, and no subsequent writes are possible until the
 * encrypted vector is read.
 *
 * @param filp: pointer to the file descriptor in use
 * @param buf: data buffer coming from user space
 * @param count: size of the transfer requested
 * @param ppos: start position from which data should be written (ignored)
 *
 * @return: Number of bytes written in the internal buffer, or a negative error
 * code if an error occurred.
 */
static ssize_t ra_file_write(struct file *filp, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	/*
	 * Retrieve the pointer to our private data.
	 */
	struct priv *priv = filp->private_data;

	/* Number of data values requested by the user. */
	int const ndata = count / sizeof(int);

	dev_info(priv->dev,
		 "called %s, count = %d, internal buf len = %d\n",
		 __func__, count, priv->data_size);

	/*
	 * Since we operate on integers, we expect that the user offers a number
	 * of bytes that is a multiple of the size of an integer.
	 */
	if (count % sizeof(int) != 0) {
		dev_err(priv->dev,
			"write(): the encryption device operates on integers !\n");
		return -EINVAL;
	}

	/*
	 * Check that we are not trying to overflow our internal buffer.
	 */
	if (ndata > MAX_VEC_LEN) {
		dev_err(priv->dev,
			"write(): overflow attempt on internal buffer !\n");
		return -EINVAL;
	}

	/*
	 * The internal buffer is not empty, therefore the user hasn't performed
	 * the required read().
	 */
	if (priv->data_size != 0) {
		dev_err(priv->dev,
			"write(): internal buffer not empty (missing read()?)\n");
		return -EINVAL;
	}

	/* Copy the data from the user into the buffer. */
	if (copy_from_user(priv->buffer, buf, count) != 0) {
		dev_err(priv->dev,
			"write(): error occurred in copy_from_user() operation !\n");
		return -EFAULT;
	}

	/* Reset the counter used by the encryption process. */
	ra_write(priv, INIT_REG_OFF, REINIT_CNT);

	/*
	 * Set our internal data length.
	 */
	priv->data_size = ndata;

	return count;
}

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

/*
 * @brief IRQ handler.
 *
 * React to the interrupts received from the board. For this tutorial we just have
 * to reset the counter to the initial value.
 *
 * @param irq: interrupt's numerical value (the same used in devm_request_irq())
 * @param dev_id: pointer to the variable passed as last parameter in the
 * devm_request_irq() function
 *
 * @returns: IRQ_HANDLED, since it always deals successfully with the interrupt
 */
irq_handler_t irq_handler(int irq, void *dev_id)
{
	/* We cast back the parameter to get our private data. */
	struct priv *priv = (struct priv *)dev_id;

	dev_info(priv->dev,
		 "called %s --- shouldn't print in an interrupt in real life, but this useful for debugging/understanding\n",
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

/*
 * @brief Driver's probe function.
 *
 * Since we have no init() function, all the initializations we require are made
 * here. Proper error checking is mandatory !
 * If this function fails, the module will be left in a sort of "zombie" state --
 * that is, it will still be loaded (we could see it with 'lsmod') but it won't do
 * much.
 *
 * @param pdev: pointer to platform device's structure
 *
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
	struct resource *mem_info;

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

	/* Retrieve the address of the register's region from the DT. */
	mem_info = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!mem_info)) {
		dev_err(&pdev->dev,
			"Failed to get memory resource from device tree!\n");
		rc = -EINVAL;
		/*
		 * Since we have used the devm_kmalloc, we do not need to free
		 * our memory anymore --- we can just bail out.
		 */
		goto return_fail;
	}

	/*
	 * The address we have just retrieved is a physical (IO) address, we
	 * cannot access it directly so we have to map it to a virtual address.
	 */
	priv->mem_ptr = devm_ioremap_resource(priv->dev, mem_info);
	if (IS_ERR(priv->mem_ptr)) {
		dev_err(&pdev->dev, "Failed to map memory!\n");
		rc = PTR_ERR(priv->mem_ptr);
		goto return_fail;
	}

	/*
	 * Before enabling interrupts on the device, we register the function
	 * that is supposed to be called when one occurs.
	 */
	dev_info(&pdev->dev, "Registering interrupt handler\n");
	/* Retrieve the IRQ number from the DT. */
	priv->irq_num = platform_get_irq(pdev, 0);
	if (priv->irq_num < 0) {
		dev_err(&pdev->dev,
			"Failed to get interrupt resource from device tree!\n");
		rc = -EINVAL;
		goto return_fail;
	}
	/* Register the ISR function associated with the interrupt. */
	rc = devm_request_irq(&pdev->dev, /* Our device */
			      priv->irq_num, /* IRQ number */
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
		goto return_fail;
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
	 * Our "bus" is platform (i.e., we have no real bus), now as a framework
	 * we choose the "misc" framework.
	 * What is the difference between a "misc device" and a traditional
	 * "character device" ? NONE! A misc device IS a character device, but by
	 * using it we avoid all the hassle of getting a major number (it will
	 * have by default major = 10), and it will be distinguished from other
	 * misc devices thanks to the minor number.
	 */

	/*
	 * The structure we fill below allows us to get a minor number dynamically
	 * and --- "automagically" --- get a device file in /dev (called with the
	 * name we have chosen).
	 * It will also add an entry in /sys/class/misc.
	 */
	memset(&priv->miscdev, 0, sizeof(priv->miscdev));
	/* The major will be 10, let the kernel choose the minor. */
	priv->miscdev.minor = MISC_DYNAMIC_MINOR;
	/* Name of the device in /dev (and /sys). */
	priv->miscdev.name = DEV_NAME;
	/* Permissions for the file in /dev. */
	priv->miscdev.mode = 0666;
	/* Operations associated with it. */
	priv->miscdev.fops = &ra_fops;
	/* This misc device is spawn from our device. */
	priv->miscdev.parent = &pdev->dev;

	/* Register our device with the misc framework. */
	rc = misc_register(&priv->miscdev);
	if (rc != 0) {
		dev_err(&pdev->dev,
			"Failed to register MISC device, error code: %d\n",
			rc);
		goto return_fail;
	}
	dev_info(&pdev->dev, "Device file /dev/%s created with minor = %d\n",
		 priv->miscdev.name, priv->miscdev.minor);

	dev_info(&pdev->dev, "REDS-adder ready !\n");

	return 0;

return_fail:
	return rc;
}

/*
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
	 * Here we should do, in reverse, the operations we did in the probe()
	 * function. However, since we were nice and we used the devm_X() version,
	 * there is not much we should do here! We just have to unregister our
	 * device file and that's it.
	 */
	misc_deregister(&priv->miscdev);

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
	.probe = reds_adder_probe,
	.remove = reds_adder_remove,
	.driver = {
		.name = "reds-adder",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(reds_adder_driver_id),
	},
};

/*
 * We declare this as a platform driver. As a benefit, we get the init and exit
 * functions (that will take care of the register/unregister operations) for free.
 */
module_platform_driver(reds_adder_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("REDS");
MODULE_DESCRIPTION("REDS-adder driver v1.1");
