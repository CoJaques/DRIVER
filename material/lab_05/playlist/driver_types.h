#ifndef DRIVER_TYPES_H
#define DRIVER_TYPES_H

#include <linux/hrtimer.h>
#include <linux/kfifo.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>

// Shared structures
struct io_registers {
	struct device *dev;
	void __iomem *segment1;
	void __iomem *led;
	void __iomem *button;
	void __iomem *button_edge;
	void __iomem *button_interrupt_mask;
	spinlock_t led_running_spinlock;
	spinlock_t segments_spinlock;
};

struct time_management {
	uint32_t current_time;
	struct hrtimer music_timer;
	struct task_struct *display_thread;
};

struct music_data {
	uint16_t duration;
	char title[25];
	char artist[25];
};

struct playlist_data {
	struct device *dev;
	struct cdev cdev;
	struct class *cl;
	struct kfifo *playlist;
	struct music_data *current_music;
	bool next_music_requested;
};

struct priv {
	struct io_registers io;
	struct time_management time;
	struct playlist_data playlist_data;
	bool is_playing;
};

#endif // DRIVER_TYPES_H