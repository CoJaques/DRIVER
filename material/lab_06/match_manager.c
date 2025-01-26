#include <linux/interrupt.h>
#include <linux/delay.h>
#include "match_manager.h"
#include "io_manager.h"

#define KEY_0 0x1
#define KEY_3 0x8

// Forward declarations
static irqreturn_t button_irq_handler(int irq, void *dev_id);
static irqreturn_t button_thread_handler(int irq, void *dev_id);

/**
 * Check if a player can win the game on next point
 */
static bool is_game_point(struct party_data *party, enum player player)
{
	uint8_t player_score = party->game_score[player];
	uint8_t opponent_score =
		party->game_score[player == PLAYER1 ? PLAYER2 : PLAYER1];

	return (player_score >= 30 && player_score > opponent_score &&
		(player_score - opponent_score) >= 20);
}

/**
 * Check if a player can win the set on next game
 */
static bool is_set_point(struct party_data *party, enum player player)
{
	return party->set_score[player] == 5;
}

/**
 * Check if a player can win the match on next set
 */
static bool is_match_point(struct party_data *party, enum player player)
{
	return party->match_score[player] == party->format - 1 &&
	       is_set_point(party, player);
}

/**
 * Handle game win
 */
static void handle_game_win(struct priv *priv, enum player winner)
{
	struct party_data *party = &priv->party;

	// Update set score
	party->set_score[winner]++;

	// Reset game score
	party->game_score[PLAYER1] = 0;
	party->game_score[PLAYER2] = 0;
	party->advantage_player = PLAYER_NONE;

	// Change server
	party->current_serving = (party->current_serving == PLAYER1) ? PLAYER2 :
								       PLAYER1;
	set_serving_led(party->current_serving, &priv->io);

	// Check for set win
	if (party->set_score[winner] >= 6) {
		// Store set score in history
		party->sets_history_score[party->match_score[PLAYER1] +
					  party->match_score[PLAYER2]][0] =
			party->set_score[PLAYER1];
		party->sets_history_score[party->match_score[PLAYER1] +
					  party->match_score[PLAYER2]][1] =
			party->set_score[PLAYER2];

		party->match_score[winner]++;
		party->set_score[PLAYER1] = 0;
		party->set_score[PLAYER2] = 0;

		if (party->match_score[winner] >= party->format) {
			// Match is won
			queue_announcement(priv, GAME_AND_SET_AND_MATCH);
			party->match_in_progress = false;
			start_summary_display(priv);
		} else {
			queue_announcement(priv, GAME_AND_SET);
		}
	} else if ((party->set_score[PLAYER1] + party->set_score[PLAYER2]) %
			   2 ==
		   0) {
		// Side change every two games
		queue_announcement(priv, SIDE_CHANGE);
	} else {
		queue_announcement(priv, GAME);
	}
}

/**
 * Handle point win
 */
static void handle_point_win(struct priv *priv, enum player winner)
{
	struct party_data *party = &priv->party;
	enum player opponent = (winner == PLAYER1) ? PLAYER2 : PLAYER1;

	// Update points counter
	party->total_points++;

	if (party->advantage_player != PLAYER_NONE) {
		if (winner == party->advantage_player) {
			handle_game_win(priv, winner);
		} else {
			party->advantage_player = PLAYER_NONE;
			party->game_score[PLAYER1] = 40;
			party->game_score[PLAYER2] = 40;
		}
		return;
	}

	switch (party->game_score[winner]) {
	case 0:
		party->game_score[winner] = 15;
		break;
	case 15:
		party->game_score[winner] = 30;
		break;
	case 30:
		party->game_score[winner] = 40;
		break;
	case 40:
		if (party->game_score[opponent] == 40) {
			party->advantage_player = winner;
		} else {
			handle_game_win(priv, winner);
		}
		break;
	}

	// Check for various point announcements
	if (is_match_point(party, winner)) {
		queue_announcement(priv, MATCH_POINT);
	} else if (is_set_point(party, winner)) {
		queue_announcement(priv, SET_POINT);
	} else if (is_game_point(party, winner)) {
		queue_announcement(priv, GAME_POINT);
	}
}

/**
 * Top half interrupt handler - just acknowledge the interrupt
 */
static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
	struct priv *priv = (struct priv *)dev_id;

	priv->io.last_button = ioread8(priv->io.button_edge);

	// Just acknowledge and let threaded handler do the work
	iowrite8(0x0F, priv->io.button_edge);

	return IRQ_WAKE_THREAD;
}

/**
 * Bottom half threaded handler - handle the button press
 */
static irqreturn_t button_thread_handler(int irq, void *dev_id)
{
	struct priv *priv = (struct priv *)dev_id;

	dev_info(priv->io.dev, "Button pressed: KEY_%d\n",
		 priv->io.last_button == KEY_0 ? 0 : 3);

	// Handle button press
	switch (priv->io.last_button) {
	case KEY_0:
		if (!priv->party.match_in_progress) {
			dev_info(priv->io.dev,
				 "Starting new match (format: %d sets)\n",
				 priv->party.format);
			// Start new match
			memset(&priv->party, 0, sizeof(struct party_data));
			priv->party.match_in_progress = true;
			priv->party.format = 2; // Default: best of 3
			set_score_segment(&priv->party, &priv->display,
					  &priv->io);
		} else {
			dev_info(priv->io.dev, "Point won by Player 1\n");
			// Point for player 1
			handle_point_win(priv, PLAYER1);
		}
		break;

	case KEY_3:
		if (priv->party.match_in_progress) {
			dev_info(priv->io.dev, "Point won by Player 2\n");
			// Point for player 2
			handle_point_win(priv, PLAYER2);
		}
		break;
	}

	// Log current score
	dev_info(priv->io.dev,
		 "Current score - Game: %d-%d, Set: %d-%d, Match: %d-%d\n",
		 priv->party.game_score[PLAYER1],
		 priv->party.game_score[PLAYER2],
		 priv->party.set_score[PLAYER1], priv->party.set_score[PLAYER2],
		 priv->party.match_score[PLAYER1],
		 priv->party.match_score[PLAYER2]);

	if (priv->party.advantage_player != PLAYER_NONE) {
		dev_info(priv->io.dev, "Advantage: Player %d\n",
			 priv->party.advantage_player == PLAYER1 ? 1 : 2);
	}

	// Update display
	set_score_segment(&priv->party, &priv->display, &priv->io);
	set_sets_leds(&priv->party, &priv->io);

	return IRQ_HANDLED;
}

/**
 * Initialize the match interrupt handler
 */
int setup_match_irq(struct priv *priv, struct platform_device *pdev)
{
	int ret, irq_num;

	irq_num = platform_get_irq(pdev, 0);
	if (irq_num < 0)
		return -EINVAL;

	ret = devm_request_threaded_irq(&pdev->dev, irq_num, button_irq_handler,
					button_thread_handler, IRQF_SHARED,
					"tennis_buttons", priv);
	if (ret)
		return ret;

	// Enable button interrupts
	iowrite8(0xF, priv->io.button_interrupt_mask);
	iowrite8(0x0F, priv->io.button_edge);

	return 0;
}

static void reset_match_state(struct priv *priv)
{
	memset(&priv->party, 0, sizeof(struct party_data));
	priv->party.match_in_progress = false;
	priv->party.format = 2; // Default: best of 3
	priv->party.advantage_player = PLAYER_NONE;
	priv->display.mode = DISPLAY_NORMAL;

	// Clear displays
	iowrite32(0, priv->io.segment1);
	iowrite32(0, priv->io.segment2);
	iowrite32(0, priv->io.led);
}
