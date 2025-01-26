#include "io_manager.h"
#include <linux/types.h>
#include <linux/io.h>
#include <linux/delay.h>

// Lookup table for 7-segment display numbers and special characters
static const uint32_t segments_lookup[] = {
	0x3F, // 0
	0x06, // 1
	0x5B, // 2
	0x4F, // 3
	0x66, // 4
	0x6D, // 5
	0x7D, // 6
	0x07, // 7
	0x7F, // 8
	0x6F, // 9
	0x77, // A (for Ad)
	0x7C, // b
	0x39, // C
	0x5E, // d
	0x79, // E
	0x71, // F
	0x40, // - (minus)
	0x74, // h
	0x38, // L
	0x37, // M
	0x3E, // P
	0x5B, // S (reuse 2 representation)
};

/**
 * Updates display value for HEX3-0 (first 32-bit register)
 */
static void update_hex_low(uint32_t hex0, uint32_t hex1, uint32_t hex2,
			   uint32_t hex3, struct io_registers *io)
{
	uint32_t value = 0;

	value |= segments_lookup[hex0 & 0xFF];
	value |= segments_lookup[hex1 & 0xFF] << HEX1_SHIFT;
	value |= segments_lookup[hex2 & 0xFF] << HEX2_SHIFT;
	value |= segments_lookup[hex3 & 0xFF] << HEX3_SHIFT;

	iowrite32(value, io->segment1);
}

/**
 * Updates display value for HEX5-4 (second 32-bit register)
 */
static void update_hex_high(uint32_t hex4, uint32_t hex5,
			    struct io_registers *io)
{
	uint32_t value = 0;

	value |= segments_lookup[hex4 & 0xFF];
	value |= segments_lookup[hex5 & 0xFF] << HEX5_SHIFT;

	iowrite32(value, io->segment2);
}

/**
* Function to display announcements
*/
static void display_announcement(enum announcement type,
				 struct io_registers *io)
{
	uint32_t segment1_value = 0;
	uint32_t segment2_value = 0;

	switch (type) {
	case GAME:
		segment1_value = LETTER_G;
		break;

	case GAME_AND_SET:
		segment1_value = LETTER_G;
		segment1_value |= LETTER_S << HEX1_SHIFT;
		break;

	case GAME_AND_SET_AND_MATCH:
		segment1_value = LETTER_G;
		segment1_value |= LETTER_S << HEX1_SHIFT;
		segment1_value |= LETTER_M << HEX2_SHIFT;
		break;

	case GAME_POINT:
		segment1_value = LETTER_G;
		segment1_value |= LETTER_P << HEX1_SHIFT;
		break;

	case SET_POINT:
		segment1_value = LETTER_S;
		segment1_value |= LETTER_P << HEX1_SHIFT;
		break;

	case MATCH_POINT:
		segment1_value = LETTER_M;
		segment1_value |= LETTER_P << HEX1_SHIFT;
		break;

	case SIDE_CHANGE:
		segment1_value = LETTER_S;
		segment1_value |= LETTER_C << HEX1_SHIFT;
		break;

	default:
		// Clear display in case of unknown announcement
		break;
	}

	iowrite32(segment1_value, io->segment1);
	iowrite32(segment2_value, io->segment2);
}

/**
 * Function to display match summary
 */
static void display_match_summary(struct party_data *party,
				  struct io_registers *io, bool alternate)
{
	uint32_t segment1_value = 0;
	uint32_t segment2_value = 0;

	if (!alternate) {
		// Display sets 1-3
		// Set 1 on HEX5-4
		segment2_value =
			segments_lookup[party->sets_history_score[0][1]] |
			(segments_lookup[party->sets_history_score[0][0]]
			 << HEX5_SHIFT);

		// Set 2 on HEX3-2
		segment1_value =
			segments_lookup[party->sets_history_score[1][1]] |
			(segments_lookup[party->sets_history_score[1][0]]
			 << HEX1_SHIFT);

		// Set 3 on HEX1-0
		segment1_value |=
			segments_lookup[party->sets_history_score[2][1]]
				<< HEX2_SHIFT |
			segments_lookup[party->sets_history_score[2][0]]
				<< HEX3_SHIFT;
	} else {
		// Display sets 4-5
		// Set 4 on HEX3-2
		segment1_value =
			segments_lookup[party->sets_history_score[3][1]] |
			(segments_lookup[party->sets_history_score[3][0]]
			 << HEX1_SHIFT);

		// Set 5 on HEX1-0
		segment1_value |=
			segments_lookup[party->sets_history_score[4][1]]
				<< HEX2_SHIFT |
			segments_lookup[party->sets_history_score[4][0]]
				<< HEX3_SHIFT;
	}

	iowrite32(segment1_value, io->segment1);
	iowrite32(segment2_value, io->segment2);
}

/**
 * Work function for announcements
 */
void announcement_work_func(struct work_struct *work)
{
	struct announcement_work *announce_work =
		container_of(work, struct announcement_work, work);
	struct priv *priv = announce_work->priv;

	// Change display mode
	priv->display.mode = DISPLAY_ANNOUNCE;

	// Update display with announcement
	set_score_segment(&priv->party, &priv->display, &priv->io);

	// Display for 5 seconds
	msleep(5000);

	// Remove from list and free
	list_del(&announce_work->list);
	kfree(announce_work);

	priv->display.mode = DISPLAY_NORMAL;

	// Update display
	set_score_segment(&priv->party, &priv->display, &priv->io);
}

enum hrtimer_restart summary_display_timer(struct hrtimer *timer)
{
	struct priv *priv = container_of(timer, struct priv, summary_timer);

	priv->display.summary_alternate = !priv->display.summary_alternate;
	set_score_segment(&priv->party, &priv->display, &priv->io);

	hrtimer_forward_now(timer, ms_to_ktime(3000)); // 3 seconds
	return HRTIMER_RESTART;
}

/**
 * Updates all score displays
 */
void display_normal_score(struct party_data *party, struct io_registers *io)
{
	uint32_t hex0, hex1, hex2, hex3, hex4, hex5;

	// Prepare values for each display
	if (party->advantage_player == PLAYER_NONE) {
		// Normal score display
		hex5 = party->game_score[PLAYER1] / 10;
		hex4 = party->game_score[PLAYER1] % 10;
		hex1 = party->game_score[PLAYER2] / 10;
		hex0 = party->game_score[PLAYER2] % 10;
	} else {
		// Advantage display
		if (party->advantage_player == PLAYER1) {
			hex5 = 10; // A
			hex4 = 13; // d
			hex1 = 16; // -
			hex0 = 16; // -
		} else {
			hex5 = 16; // -
			hex4 = 16; // -
			hex1 = 10; // A
			hex0 = 13; // d
		}
	}

	// Set score display
	hex3 = party->set_score[PLAYER1];
	hex2 = party->set_score[PLAYER2];

	// Update both registers
	update_hex_low(hex0, hex1, hex2, hex3, io);
	update_hex_high(hex4, hex5, io);
}

void set_score_segment(struct party_data *party, struct display_state *display,
		       struct io_registers *io)
{
	switch (display->mode) {
	case DISPLAY_NORMAL:
		display_normal_score(party, io);
		break;

	case DISPLAY_ANNOUNCE: {
		struct announcement_work *work;
		if (!list_empty(&display->pending_announcements)) {
			work = list_first_entry(&display->pending_announcements,
						struct announcement_work, list);
			display_announcement(work->type, io);
		}
	} break;

	case DISPLAY_SUMMARY:
		display_match_summary(party, io, display->summary_alternate);
		break;
	}
}

/**
 * Updates set indicator LEDs
 */
void set_sets_leds(struct party_data *party, struct io_registers *io)
{
	uint32_t led_value = ioread32(io->led);

	// Clear set LEDs (0-2 and 7-9)
	led_value &= ~(0x7 | (0x7 << 7));

	// Set LEDs for player1's sets (0-2)
	led_value |= (((1 << party->match_score[PLAYER1]) - 1) & 0x7);

	// Set LEDs for player2's sets (7-9)
	led_value |= (((1 << party->match_score[PLAYER2]) - 1) & 0x7) << 7;

	iowrite32(led_value, io->led);
}

/**
 * Updates serving player indicator LED
 */
void set_serving_led(enum player server, struct io_registers *io)
{
	uint32_t led_value = ioread32(io->led);

	// Clear serving LEDs (4-5)
	led_value &= ~(0x3 << 4);

	// Set LED for current server
	led_value |= (1 << (4 + server));

	iowrite32(led_value, io->led);
}

void start_summary_display(struct priv *priv)
{
	priv->display.mode = DISPLAY_SUMMARY;
	priv->display.summary_alternate = false;

	// Start timer to alternate display every 3 seconds
	hrtimer_start(&priv->summary_timer,
		      ms_to_ktime(3000), // 3 seconds
		      HRTIMER_MODE_REL);
}

void stop_summary_display(struct priv *priv)
{
	hrtimer_cancel(&priv->summary_timer);
	priv->display.mode = DISPLAY_NORMAL;
}

/**
 * Maps IO registers to their virtual addresses
 */
void map_io(struct io_registers *io, void __iomem *base)
{
	io->segment1 = base + HEX0_OFFSET; // Registre pour HEX3-0
	io->segment2 = base + HEX4_OFFSET; // Registre pour HEX5-4
	io->led = base + LED_OFFSET;
	io->button = base + BUTTON_OFFSET;
	io->button_edge = base + BUTTON_EDGE_OFFSET;
	io->button_interrupt_mask = base + BUTTON_INTERRUPT_MASK;
}

/**
 * Queue a new announcement for display
 */
void queue_announcement(struct priv *priv, enum announcement type)
{
	struct announcement_work *work;

	// Allocate new work
	work = kzalloc(sizeof(*work), GFP_KERNEL);
	if (!work)
		return;

	// Initialize work
	INIT_WORK(&work->work, announcement_work_func);
	work->type = type;
	work->priv = priv;

	// Add to pending announcements list
	list_add_tail(&work->list, &priv->display.pending_announcements);

	// Queue the work
	queue_work(priv->announcement_wq, &work->work);
}
