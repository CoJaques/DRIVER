#ifndef PARROT_H
#define PARROT_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#define PARROT_IOC_MAGIC   '+'
#define PARROT_CMD_TOGGLE  _IO(PARROT_IOC_MAGIC, 0)
#define PARROT_CMD_ALLCASE _IOW(PARROT_IOC_MAGIC, 1, int)

#define TO_UPPERCASE 0
#define TO_LOWERCASE 1

#endif /* PARROT_H */
