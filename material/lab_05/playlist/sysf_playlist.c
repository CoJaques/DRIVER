#include "driver_types.h"
#include "linux/device.h"
#include "linux/sysfs.h"

static ssize_t current_title_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);
	return sysfs_emit(buf, "%s\n",
			  priv->playlist_data.current_music->title);
}

static ssize_t current_artist_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);
	return sysfs_emit(buf, "%s\n",
			  priv->playlist_data.current_music->artist);
}

static ssize_t playlist_size_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);
	return sysfs_emit(buf, "%d",
			  kfifo_len(priv->playlist_data.playlist) /
				  sizeof(struct music_data));
}

static ssize_t play_pause_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);
	return sysfs_emit(buf, "%d\n", priv->is_playing);
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

	priv->is_playing = (play_state == 1);
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
	return sysfs_emit(buf, "%u\n",
			  priv->playlist_data.current_music->duration);
}

static ssize_t total_duration_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);
	struct music_data *buffer;
	unsigned int count, num_elements, i, total_duration = 0;

	count = kfifo_len(priv->playlist_data.playlist);
	if (count == 0)
		return sysfs_emit(buf, "0\n");

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
