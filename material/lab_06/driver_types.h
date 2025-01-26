#ifndef DRIVER_TYPES_H
#define DRIVER_TYPES_H

#include <linux/hrtimer.h>
#include <linux/kfifo.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/types.h>

#define MAX_SETS	  5
#define MAX_SCORE_HISTORY 50 // Maximum number of matches in history

/**
 * Enum for player selection
 */
enum player { PLAYER1, PLAYER2, PLAYER_NONE };

/**
 * Enum for display mode
 */
enum display_mode {
	DISPLAY_NORMAL, // Normal game display
	DISPLAY_ANNOUNCE, // Showing announcement
	DISPLAY_SUMMARY // End of match summary
};

/**
 * Enum for game announcements
 */
enum announcement {
	GAME,
	GAME_AND_SET,
	GAME_AND_SET_AND_MATCH,
	GAME_POINT,
	SET_POINT,
	MATCH_POINT,
	SIDE_CHANGE
};

/**
 * Structure for storing completed match data in history
 */
struct match_history {
	uint8_t sets_score[MAX_SETS][2]; // Final score of each set
	uint16_t total_points; // Total points played in match
	struct list_head list; // For kernel's linked list implementation
};

/**
 * Structure for storing current match data
 */
struct party_data {
	uint8_t game_score[2]; // Current game score (0,15,30,40)
	uint8_t set_score[2]; // Current set score
	uint8_t match_score[2]; // Sets won by each player
	enum player current_serving; // Current serving player
	enum player
		advantage_player; // Player with advantage (PLAYER_NONE if no advantage)
	uint8_t sets_history_score[MAX_SETS][2]; // History of completed sets
	bool match_in_progress; // Whether a match is currently being played
	uint8_t format; // Number of sets needed to win (2 or 3)
	uint16_t total_points; // Running count of total points played
};

/**
 * Structure for display state
 */
struct display_state {
	enum display_mode mode; // Current display mode
	bool summary_alternate; // For alternating between sets 1-3 and 4-5
	struct list_head pending_announcements; // List of pending announcements
};

/**
 * Structure for IO register mapping
 */
struct io_registers {
	struct device *dev;
	void __iomem *segment1; // Register for HEX3-0 (0xFF200020)
	void __iomem *segment2; // Register for HEX5-4 (0xFF200030)
	void __iomem *led; // LED control register
	void __iomem *button; // Button input register
	void __iomem *button_edge; // Button edge capture register
	void __iomem *button_interrupt_mask; // Button interrupt mask register
	uint8_t last_button; // Last button pressed
};

/**
 * Structure for announcement work
 */
struct announcement_work {
	struct work_struct work;
	enum announcement type;
	struct priv *priv; // Added pointer to priv for access to display state
	struct list_head list; // Added for linking in pending_announcements
};

/**
 * Main private structure for driver data
 */
struct priv {
	struct io_registers io;
	struct party_data party;
	struct display_state display; // Added display state
	struct list_head match_history; // Head of match history linked list
	struct mutex history_lock; // Protect history access
	spinlock_t score_lock; // Protect score updates
	struct workqueue_struct *announcement_wq; // Workqueue for announcements
	struct cdev cdev; // Character device
	dev_t devt; // Device number
	struct hrtimer
		summary_timer; // Added timer for summary display alternation
};

#endif // DRIVER_TYPES_H
