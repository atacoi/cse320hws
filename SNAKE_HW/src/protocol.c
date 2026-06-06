#include <arpa/inet.h>
#include <string.h>

#include "protocol.h"
#include "debug.h"

int protocol_serialize_welcome(uint8_t *buf, size_t buf_len, int player_id,
                               int board_size, int max_players) {
	if(!buf || buf_len < 4 || 
		player_id < 0 || player_id >= MAX_PLAYERS || 
		max_players < 0 || max_players > MAX_PLAYERS ||
		board_size < BOARD_SIZE_MIN || board_size > BOARD_SIZE_MAX) 
	{
		debug("null checks");
		return -1;
	}

	/* Type */
	buf[0] = 0x10;
	/* Assigned player ID (0-7) */
	buf[1] = player_id & 0xFF;
	/* Board Size */
	buf[2] = board_size & 0xFF;
	/* Max Players */
	buf[3] = max_players & 0xFF;

	return 4;
}

int protocol_serialize_game_state(uint8_t *buf, size_t buf_len,
                                  const game_board_t *board) {
	/* We need at least six bytes if all snakes are dead */
	if(!buf || buf_len < 6 || !board) {
		debug("null checks");
		return -1;
	}

	/* Type */
	buf[0] = 0x20;

	/* Number of alive snakes */
	int num_of_alive_snakes = 0;
	const snake_t *snakes[MAX_PLAYERS];
	for(int i = 0; i < MAX_PLAYERS; i++) {
		if(board->snakes[i].alive == 1) {
			snakes[num_of_alive_snakes] = &board->snakes[i];
			num_of_alive_snakes++;
		}
	}

	buf[1] = num_of_alive_snakes & 0xFF;

	position_t apple_pos = board->apple;

	if(apple_pos.x < 0 || apple_pos.x >= board->size ||
	   apple_pos.y < 0 || apple_pos.y >= board->size) 
	{
		debug("invalid position for apple (%d, %d)", apple_pos.x, apple_pos.y);
		return -1;
	}

	/* Apple X position (big-endian uint16) */
	uint16_t x_pos = htons((uint16_t) apple_pos.x);
	memcpy(buf + 2, &x_pos, 2);

	/* Apple Y position (big-endian uint16) */
	uint16_t y_pos = htons((uint16_t) apple_pos.y);
	memcpy(buf + 4, &y_pos, 2);

	size_t num_written_bytes = 6lu;

	/* Snake data (repeated for each alive snake) */
	for(int i = 0; i < num_of_alive_snakes; i++) {
		const snake_t *snake = snakes[i];

		/* Each snake contributes 4 + (length * 4) bytes */
		if(buf_len < num_written_bytes + 4 + (snake->length * 4)) {
			debug("buffer is too small for %d alive snakes. Failed for snake %d.", num_of_alive_snakes, i);
			return -1;
		}

		/* Snake ID */
		if(snake->id < 0 || snake->id >= MAX_PLAYERS) {
			debug("invalid snake id");
			return -1;
		}
		buf[num_written_bytes++] = snake->id & 0xFF;

		/* Snake length */
		if(snake->length <= 0 || snake->length > MAX_SNAKE_LENGTH) {
			debug("invalid snake length");
			return -1;
		}
		uint16_t length = htons((uint16_t) (snake->length));
		memcpy(buf + num_written_bytes, &length, 2);
		num_written_bytes += 2;

		/* Direction (0-3) */
		if(snake->direction < 0 || snake->direction > 3) {
			debug("invalid snake direction");
			return -1;
		}
		buf[num_written_bytes++] = snake->direction & 0xFF;

		/* Body segments */
		for(int j = 0; j < snake->length; j++) {
			position_t p = snake->body[j];

			if(p.x < 0 || p.x >= board->size ||
			   p.y < 0 || p.y >= board->size) 
			{
				debug("invalid position for body (%d, %d)", p.x, p.y);
				return -1;
			}

			/* Pairs of (uint16 x, uint16 y) */
			uint16_t x_pos = htons((uint16_t) p.x);
			memcpy(buf + num_written_bytes, &x_pos, 2);
			num_written_bytes += 2;

			uint16_t y_pos = htons((uint16_t) p.y);
			memcpy(buf + num_written_bytes, &y_pos, 2);
			num_written_bytes += 2;
		}
	}
	return num_written_bytes;
}

int protocol_serialize_dead(uint8_t *buf, size_t buf_len, int player_id) {
	if(!buf || buf_len < 2 || 
		player_id < 0 || player_id >= MAX_PLAYERS) 
	{
		debug("null checks");
		return -1;
	}

	/* Type */
	buf[0] = 0x30;

	/* Player ID that died */
	buf[1] = player_id & 0xFF;

	return 2;
}

int protocol_serialize_game_over(uint8_t *buf, size_t buf_len, int winner_id) {
	if(!buf || buf_len < 2 || 
		(winner_id != 0xFF && (winner_id < 0 || winner_id >= MAX_PLAYERS))) 
	{
		debug("null checks");
		return -1;
	}

	/* Type */
	buf[0] = 0x40;

	/* Player ID that won (or no one won) */
	buf[1] = winner_id & 0xFF;

	return 2;
}

int protocol_serialize_error(uint8_t *buf, size_t buf_len, uint8_t error_code) {
	if(!buf || buf_len < 2 ||
	   error_code < 1 || error_code > 3) {
		debug("null checks");
		return -1;
	}

	/* Type */
	buf[0] = 0xF0;

	/* Error code */
	buf[1] = error_code & 0xFF;

	return 2;
}

int protocol_deserialize_client_msg(const uint8_t *buf, size_t buf_len,
                                    uint8_t *out_type, uint8_t *out_payload) {
	if(!buf || buf_len < 2 || !out_type || !out_payload) {
		debug("null checks");
		return -1;
	}

	uint8_t type = buf[0];
	if(type < 1 || type > 3) {
		debug("invalid type");
		return -1;
	}

	uint8_t payload = buf[1];
	if((type == 1 || type == 3) && payload != 0) {
		debug("invalid payload of %d for type %d (Join = 1, Leave = 3)", payload, type);
		return -1;
	}	

	if(type == 2 && payload > 3) {
		debug("invalid payload of %d for type %d (direction)", payload, type);
		return -1;
	}

	*out_type    = type;
	*out_payload = payload;

	return 0;
}
