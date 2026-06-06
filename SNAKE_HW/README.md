# Snake Homework

## Overview

A homework dedicated to building a concurrent multi-threaded server to play Snake.

## Features

The compiled program is a command-line utility for running the snake server that supports the following features:

- Attempt the server on the given port (required)
- Set a custom board size
- Set a random seed for apple placement
- Set the number of maximum concurrent players (up to 8)
- Adjust the game tick speed by modifying the `TICK_INTERVAL_MS` found in `include/global.h`

## Running the Server

1. Compile the code by running: `make clean all`, or `make clean debug` to enable debug flags.
2. Run the program `bin/snake_server` with desired flags (run `-h` for more info).

## Running the Client

Assuming the server started on port 8080,

1. Run `pip3 install pygame`
2. Run `python3 client/snake_client.py --host 127.0.0.1 --port 8080`

## Limitations

- Supports at most 8 concurrent players.

- Collision checks prioritize the player with the lowest assigned number (e.g., if player 1 and 2 end up in the same spot on the next frame, player 1 wins).

## Demo

![](https://raw.githubusercontent.com/atacoi/cse320hws/main/SNAKE_HW/preview.gif)
