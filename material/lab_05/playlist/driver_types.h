#ifndef DRIVER_TYPES_H
#define DRIVER_TYPES_H

#include <linux/hrtimer.h>
#include <linux/kfifo.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>

/*
* This structure is used to store the address of i/o.
*/
struct io_registers {
	struct device *dev;
	void __iomem *segment1;
	void __iomem *led;
	void __iomem *button;
	void __iomem *button_edge;
	void __iomem *button_interrupt_mask;
};

/*
* This structure is used to store the current time and the timer used to manage the music time.
*/
struct time_management {
	struct hrtimer music_timer;
	struct task_struct *display_thread;
	struct completion display_thread_completion;
	atomic_t current_time;
};

/*
* This structure is used to represent a music data.
*/
struct music_data {
	uint16_t duration;
	char title[25];
	char artist[25];
};

/*
* This structure is used to store all de structures used for playlist management and userspace communication.
*/
struct playlist_data {
	struct device *dev;
	struct cdev cdev;
	struct class *cl;
	dev_t majmin;
	struct kfifo *playlist;
	struct music_data *current_music;
	struct mutex lock;
	bool next_music_requested;
};

/*
* This structure is used to store all the structures used for the driver management.
*/
struct priv {
	struct io_registers io;
	struct time_management time;
	struct playlist_data playlist_data;
	atomic_t is_playing;
};

#endif // DRIVER_TYPES_H
