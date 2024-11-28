#include "irq_manager.h"
#include "io_manager.h"
#include "linux/interrupt.h"

static bool shouldStartPlayMusic(struct priv *priv)
{
	return (!priv->is_playing &&
		(priv->playlist_data.current_music != NULL ||
		 !kfifo_is_empty(priv->playlist_data.playlist)));
}

irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct priv *priv = (struct priv *)dev_id;
	uint8_t last_pressed_button = ioread8(priv->io.button_edge);

	switch (last_pressed_button) {
	case KEY_0:
		if (shouldStartPlayMusic(priv)) {
			priv->is_playing = true;

			// reset if a new music was requested
			priv->playlist_data.next_music_requested = false;

			hrtimer_start(&priv->time.music_timer, ktime_set(1, 0),
				      HRTIMER_MODE_REL);
			set_running_led(true, &priv->io);
		} else {
			priv->is_playing = false;
			hrtimer_cancel(&priv->time.music_timer);
			set_running_led(false, &priv->io);
		}
		break;
	case KEY_1:
		priv->time.current_time = 0;
		set_time_segment(priv->time.current_time, &priv->io);
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
	struct resource *mem_info;
	void __iomem *base_address;
	int irq_num, ret;

	mem_info = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_info)
		return -EINVAL;

	base_address = devm_ioremap_resource(&pdev->dev, mem_info);
	if (IS_ERR(base_address))
		return PTR_ERR(base_address);

	irq_num = platform_get_irq(pdev, 0);
	if (irq_num < 0)
		return -EINVAL;

	ret = devm_request_irq(&pdev->dev, irq_num, irq_handler, 0, name, priv);
	if (ret)
		return ret;

	priv->io.segment1 = base_address + SEGMENT1_OFFSET;
	priv->io.led = base_address + LED_OFFSET;
	priv->io.button = base_address + BUTTON_OFFSET;
	priv->io.button_edge = base_address + BUTTON_EDGE_OFFSET;
	priv->io.button_interrupt_mask = base_address + BUTTON_INTERRUPT_MASK;

	iowrite8(0xF, priv->io.button_interrupt_mask);
	iowrite8(0x0F, priv->io.button_edge);

	return 0;
}

void cleanup_irq(struct priv *priv)
{
	iowrite8(0x0, priv->io.button_interrupt_mask);
}
