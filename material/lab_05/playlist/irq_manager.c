#include "irq_manager.h"
#include "io_manager.h"
#include "playlist_manager.h"
#include "linux/interrupt.h"

static bool shouldStartPlayMusic(struct priv *priv)
{
	return (!atomic_read(&priv->is_playing) &&
		(priv->playlist_data.current_music != NULL ||
		 !kfifo_is_empty(priv->playlist_data.playlist)));
}

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct priv *priv = (struct priv *)dev_id;
	uint8_t last_pressed_button = ioread8(priv->io.button_edge);

	switch (last_pressed_button) {
	case KEY_0:
		handle_play_pause(shouldStartPlayMusic(priv), priv);
		break;
	case KEY_1:
		atomic_set(&priv->time.current_time, 0);
		set_time_segment(atomic_read(&priv->time.current_time),
				 &priv->io);
		break;
	case KEY_2:
		priv->playlist_data.next_music_requested = true;
		break;
	}

	iowrite8(0x0F, priv->io.button_edge);
	return IRQ_HANDLED;
}

int setup_hw_irq(struct priv *priv, struct platform_device *pdev,
		 const char *name)
{
	int ret, irq_num;

	irq_num = platform_get_irq(pdev, 0);
	if (irq_num < 0)
		return -EINVAL;

	ret = devm_request_irq(&pdev->dev, irq_num, irq_handler, 0, name, priv);
	if (ret)
		return ret;

	iowrite8(0xF, priv->io.button_interrupt_mask);
	iowrite8(0x0F, priv->io.button_edge);

	return 0;
}

void cleanup_irq(struct priv *priv)
{
	iowrite8(0x0, priv->io.button_interrupt_mask);
}
