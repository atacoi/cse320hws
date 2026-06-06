#include <stdbool.h>

#include "snake.h"
#include "game_board.h"
#include "server.h"
#include "debug.h"

int snake_set_direction(snake_t *snake, direction_t dir) {
	if(!snake || dir < 0 || dir >= 4) {
		debug("null pointers");
		return -1;
	}

	direction_t direction = snake->direction;

	if(direction == DIR_UP && dir == DIR_DOWN) {
		debug("tried going down when currently going up");
		return 0;
	} 
	
	else if(direction == DIR_RIGHT && dir == DIR_LEFT) {
		debug("tried going left when currently going right");
		return 0;
	}

	else if(direction == DIR_DOWN && dir == DIR_UP) {
		debug("tried going up when currently going down");
		return 0;
	}

	else if(direction == DIR_LEFT && dir == DIR_RIGHT) {
		debug("tried going right when currently going left");
		return 0;
	}

	snake->next_direction = dir;

	return 0;
}

int snake_advance(struct game_board *board, int snake_id) {
	if(!board || !board->cells || snake_id < 0 || snake_id >= MAX_PLAYERS) {
		debug("null pointers/invalid snake id");
		return -1;
	}

	snake_t *snake = &board->snakes[snake_id];

	if(snake->alive != 1) {
		debug("Snake with id: %d is no longer alive", snake_id);
		return -1;
	}

	cell_t *cells = board->cells;
	int size = board->size;

	snake->direction = snake->next_direction;

	position_t new_head_position = snake->body[0];
	if(snake->direction == DIR_UP) {
		new_head_position.y--;
	}

	else if(snake->direction == DIR_RIGHT) {
		new_head_position.x++;
	}

	else if(snake->direction == DIR_DOWN) {
		new_head_position.y++;
	}

	else if(snake->direction == DIR_LEFT) {
		new_head_position.x--;
	}

	else {
		debug("could not find direction %d", snake->direction);
		return -1;
	}

	/* Should never occur but check anyways */
	if(new_head_position.x < 0 || new_head_position.x >= size ||
	   new_head_position.y < 0 || new_head_position.y >= size) 
	{
		debug("new position is out of bounds (%d, %d)", new_head_position.x, new_head_position.y);
		return -1;
	}

	int cell_index = new_head_position.y * size + new_head_position.x;
	cell_t hit_cell = cells[cell_index];

	debug("hit cell %d; (%d, %d)", hit_cell, new_head_position.x, new_head_position.y);

	if(hit_cell == CELL_WALL || (hit_cell >= CELL_SNAKE_0 && hit_cell < (CELL_SNAKE_0 + MAX_PLAYERS))) {
		if(board_remove_snake(board, snake_id) == -1) {
			debug("could not remove snake from the board");
			return -1;
		}
		return 1;
	}

	if(hit_cell == CELL_APPLE && snake->length < MAX_SNAKE_LENGTH) {
		/* Move the body forward */
		for (int i = snake->length; i > 0; i--) {
			snake->body[i] = snake->body[i - 1];
		}
		snake->body[0] = new_head_position; 

		cells[cell_index] = CELL_SNAKE_0 + snake_id;

		snake->length++;

		if(board_place_apple(board) == -1) {
			debug("couldn't place a new apple");
			return -1;
		} 
		return 2;
	}

	position_t tail = snake->body[snake->length - 1];
	/* Should never occur but check anyways */
	if(tail.x < 0 || tail.x >= size ||
	   tail.y < 0 || tail.y >= size) 
	{
		debug("tail position is out of bounds (%d, %d)", tail.x, tail.y);
		return -1;
	}

	cells[tail.y * size + tail.x] = CELL_EMPTY;

	for (int i = snake->length - 1; i > 0; i--) snake->body[i] = snake->body[i - 1];
	snake->body[0] = new_head_position;

	cells[cell_index] = CELL_SNAKE_0 + snake_id;

	if(hit_cell == CELL_APPLE) {
		if(board_place_apple(board) == -1) {
			debug("couldn't place a new apple");
			return -1;
		}
		return 2;
	}

	return 0;
}