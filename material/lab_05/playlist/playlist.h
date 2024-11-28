#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/kfifo.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/fs.h>

// Constants
#define DEVICE_NAME	  "drivify"
#define MAJOR_NUM	  99
#define MAJMIN		  MKDEV(MAJOR_NUM, 0)
#define MAX_PLAYLIST_SIZE 16

#endif // PLAYLIST_H
