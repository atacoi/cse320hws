#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>

#include "server.h"
#include "debug.h"
#include "global.h"
#include "protocol.h"

#define LISTENQ 1024

typedef struct {
	server_t *server;
	int clientfd;
} server_clientfd;

/* Used for signal handler and set by server_init */
static server_t *g_server = NULL;

static void handle_sigint(int sig) {
	int oerrno = errno;
	(void) sig;
	if(g_server) {
		g_server->running = 0;
		close(g_server->listen_fd);
	}
	errno = oerrno;
}

/* Based on the textbook's helper function; listenfd on success, -1 on error */
static int open_listenfd(int port) {
	struct addrinfo hints, *listp, *p;
	int listenfd, optval=1;

	/* Max port is 65535 (5 digits) + null terminator */
	char port_str[6]; 

    snprintf(port_str, sizeof(port_str), "%d", port);

	/* Get a list of potential server addresses */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
	hints.ai_flags |= AI_NUMERICSERV;

	if(getaddrinfo(NULL, port_str, &hints, &listp) != 0) {
		debug("failed on getaddrinfo");
		return -1;
	}

	for(p = listp; p; p = p->ai_next) {
		/* Create a socket descriptor */
		if((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
			continue; /* Socket failed, try the next */
		}

		/* Eliminates "Address already in use" error from bind */
		if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
					  (const void *)&optval, sizeof(int)) != 0) 
		{
			debug("setsockopt failed %d", errno);
			close(listenfd);
			return -1;
		}

		if(bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) break;

		close(listenfd);
	}

	/* Cleanup */
	freeaddrinfo(listp);
	if(!p) { 
		ERR_BIND_FAILED(port);
		return -1;
	}

	if(listen(listenfd, LISTENQ) < 0) {
		debug("failed on listen %d", errno);
		close(listenfd);
		return -1;
	}
	return listenfd;
}

/* Returns 0 on succes, -1 on error */
static int send_error(int fd, uint8_t error_code) {
	if(fd < 0) {
		debug("invalid clientfd");
		return -1;
	}

	size_t buffer_len = 1024;
	uint8_t buffer[buffer_len];

	if(protocol_serialize_error(buffer, buffer_len, error_code) != 2) return -1;

	if(send_all(fd, buffer, 2) == -1) return -1;
	
	return 0;
}

static void print_client_connected(struct sockaddr_storage *client_addr) {
	char ip_str[INET6_ADDRSTRLEN];
	uint16_t port;

	if (client_addr->ss_family == AF_INET) {
		struct sockaddr_in *s = (struct sockaddr_in *) client_addr;
		inet_ntop(AF_INET, &s->sin_addr, ip_str, sizeof(ip_str));
		port = ntohs(s->sin_port);
	} else {
		struct sockaddr_in6 *s = (struct sockaddr_in6 *) client_addr;
		inet_ntop(AF_INET6, &s->sin6_addr, ip_str, sizeof(ip_str));
		port = ntohs(s->sin6_port);
	}

	PRINT_CLIENT_CONNECTED(ip_str, port);
	(void)port;
}

int server_init(server_t *server, int port, int board_size, int max_snakes, unsigned int seed) {
	if(!server) {
		debug("null checks");
		return -1;
	}

	int listenfd;
	if((listenfd = open_listenfd(port)) == -1) {
		debug("failed to open listenfd");
		return -1;
	}

	game_board_t board;
	if((board_init(&board, board_size, max_snakes, seed)) == -1) {
		debug("failed to init board");
		close(listenfd);
		return -1;
	}

	if(pthread_mutex_init(&server->board_mutex, NULL) != 0) {
		debug("failed to init mutex");
		close(listenfd);
		board_free(&board);
		return -1;
	}
	
	server->listen_fd = listenfd;
	server->board     = board;
	server->running   = 1;

	for(int i = 0; i < MAX_PLAYERS; i++) {
		server->client_fds[i]       = -1;
		server->client_snake_ids[i] = -1;
	}

	/* Setting up CTRL-C signal handler */
	g_server = server;

	struct sigaction sa = { 0 };
	sa.sa_handler = handle_sigint;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if(sigaction(SIGINT, &sa, NULL) == -1) {
		debug("sigaction failed");
		close(listenfd);
		board_free(&board);
		pthread_mutex_destroy(&server->board_mutex);
		return -1;
	}

	return 0;
}

void *server_game_loop(void *arg) {
	if(!arg) return NULL;

	server_t *server = (server_t *) arg;
	game_board_t *board = &server->board;

	size_t buffer_len = GAME_STATE_BUF_SIZE;
	uint8_t buffer[buffer_len];

	while(server->running) {
		debug("\nIter Start");

		#ifdef DEBUG
		pthread_mutex_lock(&server->board_mutex);
		for(int j = 0; j < board->size; j++) {
			for(int i = 0; i < board->size; i++) {
				printf("|%d|", board->cells[j * board->size + i]);
			}
			printf("\n");
		}
		pthread_mutex_unlock(&server->board_mutex);
		#endif

		if(usleep(TICK_INTERVAL_MS * 1000) == -1) {
			debug("usleep failed %d", errno);
			return NULL;
		}	

		if(pthread_mutex_lock(&server->board_mutex) != 0) {
			debug("Could not lock mutex");
			return NULL;
		}

		debug("apple before: (%d, %d)", board->apple.x, board->apple.y);

		/* Save the alive status for each snake before the tick */
		int snake_statuses_before[MAX_PLAYERS];
		for(int i = 0; i < MAX_PLAYERS; i++) snake_statuses_before[i] = board->snakes[i].alive;

		if(board_tick(board) == -1) {
			debug("board_tick failed");
			if(pthread_mutex_unlock(&server->board_mutex) != 0) {
				debug("Could not unlock mutex");
				return NULL;
			}
			return NULL;
		}

		/* Save the alive status for each snake after the tick */
		int snake_statuses_after[MAX_PLAYERS];
		for(int i = 0; i < MAX_PLAYERS; i++) snake_statuses_after[i] = board->snakes[i].alive;

		if(pthread_mutex_unlock(&server->board_mutex) != 0) {
			debug("Could not unlock mutex");
			return NULL;
		}

		for(int i = 0; i < MAX_PLAYERS; i++) {
			debug("id: %d\tbefore: %d\tafter: %d", i, snake_statuses_before[i], snake_statuses_after[i]);
			/* Now dead but was alive */
			if(snake_statuses_before[i] == 1 && snake_statuses_after[i] == 0) {
				if(protocol_serialize_dead(buffer, buffer_len, i) != 2) {
					debug("failed to serialize dead message");
					continue;
				}

				int client_slot;

				if(pthread_mutex_lock(&server->board_mutex) != 0) {
					debug("Could not lock mutex");
					return NULL;
				}

				for(client_slot = 0; client_slot < MAX_PLAYERS; client_slot++) {
					if(server->client_snake_ids[client_slot] == i) break; /* Race */
				}

				if(client_slot == MAX_PLAYERS) {
					if(pthread_mutex_unlock(&server->board_mutex) != 0) debug("Could not unlock mutex");
					debug("client could not be found for snake id %d", i);
					continue;
				}

				int clientfd = server->client_fds[client_slot];
				if(clientfd == -1) {
					if(pthread_mutex_unlock(&server->board_mutex) != 0) debug("Could not unlock mutex");
					debug("client has not file descriptor for slot %d and snake id %d", client_slot, i);
					continue;
				}

				if(pthread_mutex_unlock(&server->board_mutex) != 0) {
					debug("Could not unlock mutex");
					return NULL;
				}

				if(send_all(clientfd, buffer, 2lu) == -1) {
					debug("failed to send dead message to client with snake id %d", i);
					continue;
				}
				debug("sent dead message to client with snake id %d", i);
			}
		}

		/* Serialize game state */
		if(pthread_mutex_lock(&server->board_mutex) != 0) {
			debug("Could not lock mutex");
			return NULL;
		}

		int actual_buffer_len;
		if((actual_buffer_len = protocol_serialize_game_state(buffer, buffer_len, board)) == -1) {
			debug("failed to serialize game state");
			return NULL;
		}

		if(pthread_mutex_unlock(&server->board_mutex) != 0) {
			debug("Could not unlock mutex");
			return NULL;
		}

		/* Broadcast game state to all alive clients */
		for(int i = 0; i < MAX_PLAYERS; i++) {
			if(pthread_mutex_lock(&server->board_mutex) != 0) {
				debug("Could not lock mutex");
				return NULL;
			}

			int clientfd = server->client_fds[i];
			int snake_id = server->client_snake_ids[i];

			if(clientfd == -1 || snake_id == -1) {
				if(pthread_mutex_unlock(&server->board_mutex) != 0) debug("Could not unlock mutex");
				debug("clientfd %d or snake_id %d are -1", clientfd, snake_id);
				continue;
			}
			
			int status = server->board.snakes[snake_id].alive;

			if(pthread_mutex_unlock(&server->board_mutex) != 0) {
				debug("Could not unlock mutex");
				return NULL;
			}

			if(clientfd != -1 && status == 1) {
				if(send_all(clientfd, buffer, (size_t) actual_buffer_len) == -1) {
					debug("failed to send the game state to client with fd %d", clientfd);
					continue;
				}
			}
		}
	}
	return NULL;
}

void *server_client_handler(void *arg) {
	if(!arg) return NULL;

	server_clientfd *sc = (server_clientfd *) arg;

	server_t *server = sc->server;

	int clientfd = sc->clientfd;
	if(clientfd < 0) { 
		debug("clientfd is invalid");
		free(arg);
		return NULL;
	}

	if(!server) {
		debug("server null");
		close(clientfd);
		free(arg);
		return NULL;
	}

	size_t buffer_len = GAME_STATE_BUF_SIZE;
	uint8_t buffer[buffer_len];

	if(recv_exact(clientfd, buffer, 2) == -1) {
		debug("Failed to read 2 bytes for JOIN");
		close(clientfd);
		free(arg);
		return NULL;
	}

	uint8_t type, payload;
	if(protocol_deserialize_client_msg(buffer, buffer_len, &type, &payload) == -1 || type != MSG_JOIN) {
		debug("Could not deserialize JOIN");
		if(send_error(clientfd, ERR_INVALID_MSG) == -1) debug("could not send error %d", ERR_INVALID_MSG);
		close(clientfd);
		free(arg);
		return NULL;
	}

	if(pthread_mutex_lock(&server->board_mutex) != 0) {
		debug("Could not lock mutex");
		close(clientfd);
		free(arg);
		return NULL;
	}

	int fd_index = -1;
	for(int i = 0; i < MAX_PLAYERS; i++) {
		if(server->client_fds[i] == -1 && server->client_snake_ids[i] == -1) {
			fd_index = i;
			break;
		}
	}

	if(fd_index == -1) {
		if(pthread_mutex_unlock(&server->board_mutex) != 0) debug("Could not unlock mutex");
		if(send_error(clientfd, ERR_GAME_FULL) == -1) debug("could not send error %d", ERR_GAME_FULL);
		close(clientfd);
		free(arg);
		return NULL;
	}

	/* Try adding the new snake */
	game_board_t *board = &server->board;
	int id;
	if(board_add_snake(board, &id) == -1) {
		if(pthread_mutex_unlock(&server->board_mutex) != 0) debug("Could not unlock mutex");
		if(send_error(clientfd, ERR_GAME_FULL) == -1) debug("could not send error %d", ERR_GAME_FULL);
		close(clientfd);
		free(arg);
		return NULL;
	}

	/* Should stay the same */
	int size       = board->size;
	int max_snakes = board->max_snakes;

	if(pthread_mutex_unlock(&server->board_mutex) != 0) {
		debug("Could not unlock mutex");
		close(clientfd);
		free(arg);
		return NULL;
	}

	/* Send a WELCOME message with the assigned player ID, board size, and max players. */
	if(protocol_serialize_welcome(buffer, buffer_len, id, size, max_snakes) != 4) {
		debug("could not serialize the welcome message");
		close(clientfd);
		free(arg);
		return NULL;
	}

	if(send_all(clientfd, buffer, 4) == -1) {
		debug("could not send the welcome message");
		close(clientfd);
		free(arg);
		return NULL;
	}

	/* Register the client fd and snake id in the client_fds and client_snake_ids arrays. */
	if(pthread_mutex_lock(&server->board_mutex) != 0) {
		debug("Could not lock mutex");
		close(clientfd);
		free(arg);
		return NULL;
	}

	server->client_fds[fd_index]       = clientfd;
	server->client_snake_ids[fd_index] = id;

	if(pthread_mutex_unlock(&server->board_mutex) != 0) {
		debug("Could not unlock mutex");
		close(clientfd);
		free(arg);
		return NULL;
	}

	while(1) {
		if(recv_exact(clientfd, buffer, 2) == -1) {
			debug("read failed");
			break;
		}

		if(protocol_deserialize_client_msg(buffer, buffer_len, &type, &payload) == -1) {
			debug("Could not deserialize client message");
			if(send_error(clientfd, ERR_INVALID_MSG) == -1) debug("could not send error %d", ERR_INVALID_MSG);
			continue;
		}

		if(type == MSG_DIRECTION) {
			if(pthread_mutex_lock(&server->board_mutex) != 0) {
				debug("Could not lock mutex");
				break;
			}

			snake_t *snake = &board->snakes[id];

			if(snake_set_direction(snake, payload) == -1) {
				if(pthread_mutex_unlock(&server->board_mutex) != 0) debug("Could not unlock mutex");
				debug("could not set the snake direction");
				break;
			}

			if(pthread_mutex_unlock(&server->board_mutex) != 0) {
				debug("Could not unlock mutex");
				break;
			}
		} 
		
		else if(type == MSG_LEAVE) {
			break;
		} 

		else if(type == MSG_JOIN) {
			debug("client with snake id %d attempted to rejoin", id);
			if(send_error(clientfd, ERR_ALREADY_JOINED) == -1) debug("could not send error %d", ERR_ALREADY_JOINED);
			break;
		}
		
		else if(type != MSG_JOIN) {
			debug("you shouldn't even be here");
			if(send_error(clientfd, ERR_INVALID_MSG) == -1) debug("could not send error %d", ERR_INVALID_MSG);
			break;
		}
	}

	/* Remove the player's snake from the gameboard */
	if(pthread_mutex_lock(&server->board_mutex) == 0) {
		if(board_remove_snake(board, id) == -1) debug("could not remove the players snake from the board");

		/* Clear entries */
		server->client_fds[fd_index]       = -1;
		server->client_snake_ids[fd_index] = -1;

		if(pthread_mutex_unlock(&server->board_mutex) != 0) debug("Could not unlock mutex");
	}
	else {
		debug("Could not lock mutex");
	}

	PRINT_CLIENT_DISCONNECTED(id);

	close(clientfd);

	free(arg);

	return NULL;
}

int server_start(server_t *server) {
	if(!server || server->listen_fd < 0) {
		debug("null pointers");
		return -1;
	}

	pthread_t stid, ctid;
	int listen_fd = server->listen_fd;

	if(pthread_create(&stid, NULL, server_game_loop, (void *) server) != 0) {
		debug("could not create game loop thread");
		return -1;
	}

	int connfd;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;

	while(server->running) {
		clientlen = sizeof(struct sockaddr_storage);
		if((connfd = accept(listen_fd, (struct sockaddr *) &clientaddr, &clientlen)) == -1) {
			if(!server->running) break;
			debug("could not accept a client");
			return -1;
		}

		print_client_connected(&clientaddr);

		server_clientfd *sc = malloc(sizeof(server_clientfd));
		sc->server = server;
		sc->clientfd = connfd;

		if(pthread_create(&ctid, NULL, server_client_handler, (void *) sc) != 0) {
			debug("could not create the client thread");
			close(connfd);
			free(sc);
			return -1;
		}

		pthread_detach(ctid);
	}

	pthread_join(stid, NULL);

	return 0;
}

void server_cleanup(server_t *server) { 
	if(!server) return;

	server->running = 0;

	for(int i = 0; i < MAX_PLAYERS; i++) {
		if(server->client_fds[i] != -1) {
			close(server->client_fds[i]);
			server->client_fds[i] = -1;
		}
	}

	if(pthread_mutex_lock(&server->board_mutex) == 0) {
		board_free(&server->board);
		pthread_mutex_unlock(&server->board_mutex);
	}
	else {
		debug("could not lock mutex");
	}

	close(server->listen_fd);

	pthread_mutex_destroy(&server->board_mutex);
}

int recv_exact(int fd, uint8_t *buf, size_t len) {
	if(fd < 0 || !buf) {
		debug("null checks");
		return -1;
	}

	size_t total_read = 0lu;

	while(total_read < len) {
		ssize_t read = recv(fd, buf + total_read, len - total_read, 0);
		if(read == -1 || (read == 0 && len != 0)) {
			debug("recv_exact failed");
			return -1;
		}
		total_read += read;
	}
	return 0;
}

int send_all(int fd, const uint8_t *buf, size_t len) {
	if(fd < 0 || !buf) {
		debug("null checks");
		return -1;
	}

	size_t total_sent = 0lu;
    while(total_sent < len) {
        ssize_t sent = send(fd, buf + total_sent, len - total_sent, 0);
        if(sent == -1) {
            debug("send_all failed");
            return -1; 
        }
        total_sent += sent;
    }
    return 0; 
}