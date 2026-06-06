#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#include "protocol.h"
#include "global.h"
#include "server.h"
#include "debug.h"

#define STR_TO_INT(s, out)                          \
({                                                   \
    const char *_s = (s);                            \
    char *_end;                                      \
    unsigned long _val;                              \
    int _result;                                     \
                                                     \
    if (_s == NULL || *_s == '\0') {                 \
        _result = -1;                                \
    } else {                                         \
        errno = 0;                                   \
        _val = strtoul(_s, &_end, 10);               \
        if (_end == _s)         _result = -1;        \
        else if (*_end != '\0') _result = -1;        \
        else if (errno == ERANGE || _val > INT_MAX)  \
                                _result = -1;        \
        else { *(out) = (int)_val; _result = 0; }    \
    }                                                \
    _result;                                         \
})

int main(int argc, char *argv[]) {
	int port = -1;
	bool port_is_set   = false;
	char *invalid_port = NULL; 

	int c;
	extern char *optarg;
	extern int optind, optopt, opterr;
	/* First pass */
	while ((c = getopt(argc, argv, ":p:hb:s:m:")) != -1) {
		switch(c) {
			case 'p':
				if(port_is_set) break;
				port_is_set = true;
				int tmp;
				if(STR_TO_INT(optarg, &tmp) == -1 || tmp < 0 || tmp > 65535) {
					invalid_port = optarg;
				} else {
					port = tmp;
				}
				break;
			case 'h':
				PRINT_USAGE();
				return EXIT_SUCCESS;
			case ':':
				break;
			case '?':
				break;
			}
	}

	if(!port_is_set) {
		ERR_PORT_REQUIRED();
		return EXIT_FAILURE;
	}

	if(invalid_port) {
		ERR_INVALID_PORT(invalid_port);
		return EXIT_FAILURE;
	}

	int board_size = BOARD_SIZE_DEFAULT;
	unsigned int seed = (unsigned int) time(NULL);
	int max_snakes = MAX_PLAYERS_DEFAULT;

	bool board_seen = false;
	bool seed_seen  = false;
	bool max_seen   = false;

	optind = 0; 
	while ((c = getopt(argc, argv, ":p:b:s:m:")) != -1) {
		switch(c) {
			case 'b':
				if(board_seen) break;
				if(STR_TO_INT(optarg, &board_size) == -1) {
					ERR_MSG("Failed to parse board size");
					return EXIT_FAILURE;
				}
				if(board_size < BOARD_SIZE_MIN || board_size > BOARD_SIZE_MAX) {
					ERR_INVALID_BOARD_SIZE(board_size);
					return EXIT_FAILURE;
				}
				board_seen = true;
				break;
			case 's':
				if(seed_seen) break;
				if(STR_TO_INT(optarg, (int*) &seed) == -1) {
					ERR_MSG("Failed to parse seed");
					return EXIT_FAILURE;
				}
				seed_seen = true;
				break;
			case 'm':
				if(max_seen) break;
				debug("%s", optarg);
				if(STR_TO_INT(optarg, &max_snakes) == -1) {
					ERR_MSG("Failed to parse max players");
					return EXIT_FAILURE;
				}
				if(max_snakes < MAX_PLAYERS_MIN || max_snakes > MAX_PLAYERS_MAX) {
					ERR_INVALID_MAX_PLAYERS(max_snakes);
					return EXIT_FAILURE;
				}
				max_seen = true;
				break;
			case 'p':
				break;
			case ':':
				ERR_MSG("Required Argument for command %d", optopt);
				return EXIT_FAILURE;
			case '?':
				ERR_MSG("Unknown arg: %d", optopt);
				return EXIT_FAILURE;
				break;
			}
	}

	server_t server;
	if(port == -1 || server_init(&server, port, board_size, max_snakes, seed) == -1) {
		debug("could not init the server");
		return EXIT_FAILURE;
	}

	PRINT_SERVER_STARTED(port, board_size, max_snakes);

	if(server_start(&server) == -1) {
		ERR_MSG("could not start the server");
		server_cleanup(&server);
		return EXIT_FAILURE;
	}

	server_cleanup(&server);
	return EXIT_SUCCESS;
}