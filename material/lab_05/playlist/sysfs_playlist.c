#include "io_manager.h"
#include "linux/atomic/atomic-instrumented.h"
#include "linux/dev_printk.h"
#include "linux/device.h"
#include "linux/sysfs.h"
#include "sysfs_playlist.h"
#include "driver_types.h"
#include "playlist_manager.h"

#define CREATE_SYSFS_FILE(dev, attr, label)                               \
	do {                                                              \
		if (device_create_file(dev, attr)) {                      \
			dev_err(dev, "Can't create sysfs file for %s.\n", \
				#attr);                                   \
			goto label;                                       \
		}                                                         \
	} while (0)

static ssize_t current_title_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);

	if (!priv->playlist_data.current_music)
		return sysfs_emit(buf, "No music is playing\n");

	return sysfs_emit(buf, "%s\n",
			  priv->playlist_data.current_music->title);
}

static ssize_t current_artist_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);

	if (!priv->playlist_data.current_music)
		return sysfs_emit(buf, "No music is playing\n");

	return sysfs_emit(buf, "%s\n",
			  priv->playlist_data.current_music->artist);
}

static ssize_t playlist_size_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);
	return sysfs_emit(buf, "%d\n",
			  kfifo_len(priv->playlist_data.playlist) /
				  sizeof(struct music_data));
}

static ssize_t play_pause_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);
	return sysfs_emit(buf, "%d\n", atomic_read(&priv->is_playing));
}

static ssize_t play_pause_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct priv *priv = dev_get_drvdata(dev);
	int play_state;

	if (kstrtoint(buf, 10, &play_state) ||
	    (play_state != 0 && play_state != 1))
		return -EINVAL;

	if (play_state == atomic_read(&priv->is_playing))
		return count;

	handle_play_pause(play_state, priv);

	return count;
}

static ssize_t current_elapsed_time_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);
	return sysfs_emit(buf, "%u\n", priv->time.current_time);
}

static ssize_t current_elapsed_time_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct priv *priv = dev_get_drvdata(dev);
	unsigned int new_time;

	if (kstrtouint(buf, 10, &new_time))
		return -EINVAL;

	if (new_time > priv->playlist_data.current_music->duration)
		return -EINVAL;

	priv->time.current_time = new_time;

	return count;
}

static ssize_t current_duration_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);

	if (!priv->playlist_data.current_music)
		return sysfs_emit(buf, "0\n");

	return sysfs_emit(buf, "%u\n",
			  priv->playlist_data.current_music->duration);
}

static ssize_t total_duration_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);
	struct music_data *buffer;
	unsigned int count, num_elements, i, total_duration = 0;

	if (priv->playlist_data.current_music) {
		total_duration += priv->playlist_data.current_music->duration -
				  priv->time.current_time;
	}

	count = kfifo_len(priv->playlist_data.playlist);
	if (count == 0)
		return sysfs_emit(buf, "%u\n", total_duration);

	buffer = kmalloc(count, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	if (kfifo_out_peek(priv->playlist_data.playlist, buffer, count) !=
	    count) {
		kfree(buffer);
		return -EIO;
	}

	num_elements = count / sizeof(struct music_data);
	for (i = 0; i < num_elements; i++) {
		total_duration += buffer[i].duration;
	}

	kfree(buffer);

	return sysfs_emit(buf, "%u\n", total_duration);
}

DEVICE_ATTR_RO(current_title);
DEVICE_ATTR_RO(current_artist);
DEVICE_ATTR_RO(playlist_size);
DEVICE_ATTR_RW(play_pause);
DEVICE_ATTR_RW(current_elapsed_time);
DEVICE_ATTR_RO(current_duration);
DEVICE_ATTR_RO(total_duration);

int initialize_sysfs(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	CREATE_SYSFS_FILE(dev, &dev_attr_current_title, error);
	CREATE_SYSFS_FILE(dev, &dev_attr_current_artist, remove_current_title);
	CREATE_SYSFS_FILE(dev, &dev_attr_playlist_size, remove_current_artist);
	CREATE_SYSFS_FILE(dev, &dev_attr_play_pause, remove_playlist_size);
	CREATE_SYSFS_FILE(dev, &dev_attr_current_elapsed_time,
			  remove_play_pause);
	CREATE_SYSFS_FILE(dev, &dev_attr_current_duration,
			  remove_current_elapsed_time);
	CREATE_SYSFS_FILE(dev, &dev_attr_total_duration,
			  remove_current_duration);

	return 0;

remove_current_duration:
	device_remove_file(dev, &dev_attr_current_duration);
remove_current_elapsed_time:
	device_remove_file(dev, &dev_attr_current_elapsed_time);
remove_play_pause:
	device_remove_file(dev, &dev_attr_play_pause);
remove_playlist_size:
	device_remove_file(dev, &dev_attr_playlist_size);
remove_current_artist:
	device_remove_file(dev, &dev_attr_current_artist);
remove_current_title:
	device_remove_file(dev, &dev_attr_current_title);
error:
	dev_err(dev, "Failed to initialize sysfs files.");
	return -1;
}

void uninitialize_sysfs(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	device_remove_file(dev, &dev_attr_current_title);
	device_remove_file(dev, &dev_attr_current_artist);
	device_remove_file(dev, &dev_attr_playlist_size);
	device_remove_file(dev, &dev_attr_play_pause);
	device_remove_file(dev, &dev_attr_current_elapsed_time);
	device_remove_file(dev, &dev_attr_current_duration);
	device_remove_file(dev, &dev_attr_total_duration);
}
