#ifndef SYSFS_PLAYLIST_H
#define SYSFS_PLAYLIST_H

#include "driver_types.h"

/*
 * Function to create sysfs files for the device.
 */
int initialize_sysfs(struct platform_device *pdev);

/*
 * Function to remove sysfs files for the device.
 */
void uninitialize_sysfs(struct platform_device *pdev);

#endif /* SYSFS_PLAYLIST_H */
