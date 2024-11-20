#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>

/**
 * thread_data - Thread data that are passed as arguments
 *
 * @id:		An id that will be shown by the thread.
 * @sleep_time:	The time in MS to sleep between each message.
 */
struct thread_data {
	int id;
	unsigned int sleep_time;
};

static struct task_struct *thread;

/**
 * thread_function - Function executed in the kthread
 *
 * @data:	thread_data passed as arguments
 */
static int thread_function(void *data)
{
	struct thread_data *td = (struct thread_data *)data;

	while (!kthread_should_stop()) {
		// Do some work
		pr_info("Thread %d is running...\n", td->id);
		// Use interruptilbe version to allow kthread_stop to wake up the thread
		msleep_interruptible(td->sleep_time);
	}

	// Freeup the thread_data
	kfree(td);
	return 0;
}

static int __init thread_example_init(void)
{
	struct thread_data *thread_data;
	int rc;

	thread_data = kmalloc(sizeof(*thread_data), GFP_KERNEL);
	if (thread_data == NULL) {
		pr_err("Cloudn't allocate memory for thread data\n");
		rc = -ENOMEM;
		goto thread_data_err;
	}

	thread_data->id = 1;
	thread_data->sleep_time = 5000;

	/* Create kthread */
	thread = kthread_run(thread_function, thread_data, "thread");
	if (IS_ERR(thread)) {
		pr_err("Unable to create thread 1\n");
		rc = PTR_ERR(thread);
		goto thread_run_err;
	}

	return 0;

thread_run_err:
	kfree(thread_data);
thread_data_err:
	return rc;
}

/* Module exit function */
static void __exit thread_example_exit(void)
{
	/* Stop the kthread */
	kthread_stop(thread);

	pr_info("Device Driver Remove...Done!!\n");
}

module_init(thread_example_init);
module_exit(thread_example_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("REDS");
MODULE_DESCRIPTION("A simple Kthread example");
