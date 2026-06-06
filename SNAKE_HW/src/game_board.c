#include <string.h>
#include <stdbool.h>

#include "game_board.h"
#include "debug.h"
#include "global.h"

int board_init(game_board_t *board, int size, int max_snakes,
               unsigned int seed) {
	if(!board || size < BOARD_SIZE_MIN || size > BOARD_SIZE_MAX || max_snakes < MAX_PLAYERS_MIN || max_snakes > MAX_PLAYERS_MAX) {
		debug("null checks");
		return -1;
	}
	
	/* Implicitly sets snakes to default state */
	memset(board, 0, sizeof(game_board_t)); 

	int board_size = size * size;
	cell_t *cells = malloc(sizeof(cell_t) * board_size);
	if(!cells) {
		debug("malloc failed");
		return -1;
	}

	/* Clear all cells */
	for(int i = 0; i < board_size; i++) cells[i] = CELL_EMPTY;

	/* Place walls */
	
	/* Top and Bottom */
	for(int x = 0; x < size; x++) {
		cells[x]                     = CELL_WALL; /* top */
		cells[(size - 1) * size + x] = CELL_WALL; /* bottom */
	}

	/* Sides */
	for(int y = 0; y < size; y++) {
		cells[y * size]            = CELL_WALL; /* left  */
		cells[y * size + size - 1] = CELL_WALL; /* right */
	}

	board->size = size;
	board->cells = cells;
	board->max_snakes = max_snakes;
	board->rng_state = seed;

	if(board_place_apple(board) == -1) {
		debug("could not place apple");
		free(cells);
		board->cells = NULL;
		return -1;
	}
	
	return 0;
}

void board_free(game_board_t *board) { 
	if(board) {
		free(board->cells);
		board->cells = NULL;
	}
}

unsigned int board_random(game_board_t *board) {
	if(!board) {
		debug("board is null");
		return 0u;
	}
    board->rng_state = board->rng_state * 1103515245 + 12345;
    return (board->rng_state / 65536) % 32768;
}

int board_place_apple(game_board_t *board) {
	if(!board || !board->cells) {
		debug("null pointers");
		return -1;
	}

	/* Count the number of empty cells */
	int empty_count = 0;
	cell_t *cells = board->cells;
	int size = board->size;
	int board_size = size * size;
	for(int i = 0; i < board_size; i++) if(cells[i] == CELL_EMPTY) empty_count++;

	if(!empty_count) {
		debug("no empty cells");
		return -1;
	}

	unsigned int random_value = board_random(board);
	int target_index = random_value % empty_count;

	empty_count = 0;
	for(int y = 0; y < size; y++) {
		for(int x = 0; x < size; x++) {
			int i = y * size + x;
			if(cells[i] == CELL_EMPTY) { 
				if(empty_count == target_index) {
					board->apple.x = x;
					board->apple.y = y;
					cells[i] = CELL_APPLE;
					return 0;
				}
        		empty_count++;
			}
		}
	}

	return -1;
}

int board_add_snake(game_board_t *board, int *out_id) {
	if(!board || !board->cells || !out_id || board->num_snakes >= board->max_snakes) {
		if(board && board->num_snakes > board->max_snakes) debug("snake overflow");
		debug("null pointers");
		return -1;
	}

	cell_t *cells = board->cells;
	int size = board->size;

	/* Find the first snake slot where alive == 0. If none available, return -1. */
	snake_t *snake = NULL;
	int id = -1;
	for(int i = 0; i < MAX_PLAYERS; i++) {
		if(board->snakes[i].alive == 0) {
			snake = &board->snakes[i];
			id = i;
			break;
		}
	}

	if(!snake) {
		debug("could not find a free snake");
		return -1;
	}

	/* Compute starting position based on the snake's ID */
	position_t starting_position;
	switch(id % 4) {
		case 0:
			starting_position.x = size / 4; 
			starting_position.y = size / 4; 
			break;
		case 1:
			starting_position.x = 3 * size / 4;
			starting_position.y = size / 4;      
			break;
		case 2:
			starting_position.x = size / 4;      
			starting_position.y = 3 * size / 4; 
			break;
		case 3:
			starting_position.x = 3 * size / 4;
			starting_position.y = 3 * size / 4;
			break;
	}

	bool can_place = false;
	int y = 0, x = 0;
	for(y = starting_position.y; y < size; y++) {
		for(x = (y == starting_position.y) ? starting_position.x : 0; x < size; x++) {
			int i = y * size + x;
			if(cells[i] == CELL_EMPTY) {
				can_place = true;
				cells[i] = CELL_SNAKE_0 + id;
				break;
			}
		}

		if(can_place) break;
	}

	if(!can_place) {
		debug("couldn't find an empty cell");
		return -1;
	}

	starting_position.x = x;
	starting_position.y = y;

	snake->id = id;
	snake->body[0] = starting_position;
	snake->length = 1;
	snake->direction = snake->next_direction = DIR_RIGHT;
	snake->alive = 1;

	*out_id = id;
	board->num_snakes++;

	debug("added snake %d to (%d, %d)", id, x, y);

	return 0;
}

int board_remove_snake(game_board_t *board, int snake_id) {
	if(!board || !board->cells || snake_id < 0 || snake_id >= MAX_PLAYERS) {
		debug("null pointers/invalid snake id");
		return -1;
	}

	snake_t *snake = &board->snakes[snake_id];

	if(snake->alive != 1) {
		debug("found snake is not alive");
		return -1;
	}

	/* Remove snake from board */
	cell_t *cells = board->cells;
	int board_size = board->size * board->size;
	cell_t snakes_cell = CELL_SNAKE_0 + snake_id;
	for(int i = 0; i < board_size; i++) {
		if(cells[i] == snakes_cell) {
			cells[i] = CELL_EMPTY;
		}
	}

	snake->alive = 0;
	snake->length = 0;

	board->num_snakes--;

	return 0;
}

int board_tick(game_board_t *board) {
	if(!board || !board->cells) {
		debug("null pointers");
		return -1;
	}

	for(int i = 0; i < MAX_PLAYERS; i++) {
		snake_t *snake = &board->snakes[i];
		if(snake->alive == 1) {
			if(snake_advance(board, i) == -1) {
				debug("snake with id %d could not advance due to an error", i);
				return -1;
			}
		}
	}

	return 0;
}
