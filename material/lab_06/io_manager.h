#ifndef IO_MANAGER_H
#define IO_MANAGER_H

#include <linux/types.h>
#include "driver_types.h"

// Segment register offsets and shifts
#define HEX0_OFFSET	      0x20 // Register for HEX3-0
#define HEX4_OFFSET	      0x30 // Register for HEX5-4

// Bit shifts for segments within registers
#define HEX1_SHIFT	      8 // HEX1 position in first register
#define HEX2_SHIFT	      16 // HEX2 position in first register
#define HEX3_SHIFT	      24 // HEX3 position in first register
#define HEX5_SHIFT	      8 // HEX5 position in second register

// Other IO registers
#define LED_OFFSET	      0x00
#define BUTTON_OFFSET	      0x50
#define BUTTON_EDGE_OFFSET    0x5C
#define BUTTON_INTERRUPT_MASK 0x58

// Display values for letters
#define LETTER_G	      0x7D // G
#define LETTER_S	      0x6D // S
#define LETTER_M	      0x37 // M
#define LETTER_P	      0x73 // P
#define LETTER_C	      0x39 // C

/**
 * Updates all 7-segment displays according to current display mode
 * @param party: Current party data containing scores
 * @param display: Current display state
 * @param io: IO registers structure
 */
void set_score_segment(struct party_data *party, struct display_state *display,
		       struct io_registers *io);

/**
 * Updates LEDs to show sets won by each player
 * LEDs 0-2 for player 1
 * LEDs 7-9 for player 2
 * @param party: Current party data containing match scores
 * @param io: IO registers structure
 */
void set_sets_leds(struct party_data *party, struct io_registers *io);

/**
 * Updates LEDs to show current serving player
 * LED4 for player 1
 * LED5 for player 2
 * @param player: Current serving player
 * @param io: IO registers structure
 */
void set_serving_led(enum player player, struct io_registers *io);

/**
 * Maps virtual addresses for all IO registers
 * @param io: IO registers structure to fill
 * @param base: Base address of the device
 */
void map_io(struct io_registers *io, void __iomem *base);

void announcement_work_func(struct work_struct *work);

enum hrtimer_restart summary_display_timer(struct hrtimer *timer);

void start_summary_display(struct priv *priv);

void stop_summary_display(struct priv *priv);

void queue_announcement(struct priv *priv, enum announcement type);

#endif // IO_MANAGER_H
