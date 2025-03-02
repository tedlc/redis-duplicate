#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>

// wait indefinitely
#define SERVER_FD_POLL_TIMEOUT -1
#define BUFFER_SIZE 1096

const nfds_t SERVER_FD_POLL_NUMBER = 256;

void read_handler(int client_fd, int read_size, struct pollfd fds[], int fds_length) {
    char buffer[BUFFER_SIZE];
    int err = read(client_fd, buffer, 15);
    // client has been disconnected
    if (err <= 0) {
        close(client_fd);
        // deregister the client fd from the subsciption list
        for (int i = 0; i < fds_length; i++) {
            if (fds[i].fd == client_fd) {
                fds[i].fd = -1;
                break;
            }
        } 
    }
    write(client_fd, "+PONG\r\n", 7);
    return;
}

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	
	printf("Logs from your program will appear here!\n");

	socklen_t client_addr_len;
    int server_fd;
	struct sockaddr_in client_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
    printf("Server socket is created, waiting...\n");

	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}

	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(6379),
									 .sin_addr = { htonl(INADDR_ANY) },
									};

	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

    printf("Server socket bind to port 6379. \n");

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return 1;
    }

    // fds to track, fds[0] should be the listening socket for accepting new TCP connection
    struct pollfd fds[256];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    int fds_length = sizeof(fds) / sizeof(fds[0]);

    for (int i = 1; i < fds_length; i++) {
        fds[i].fd = -1;
    }
    
    // Main event loop
    while (1) {
        // listen for new client connection request
        int ret = poll(fds, SERVER_FD_POLL_NUMBER, SERVER_FD_POLL_TIMEOUT);
        if (ret < 0) {
            printf("Poll failed.\n");
            close(server_fd);
            return 0;
        }
        printf("Calling poll...\n");

        // ready to accept new connections
        if (ret > 0) {
            if ((fds[0].revents & POLLIN) == POLLIN) {
                client_addr_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
                printf("Client connected.\n");
                for (int i = 1; i < fds_length; i++) {
                    if (fds[i].fd == -1) {
                        fds[i].fd = client_fd;
                        fds[i].events = POLLIN;
                        break;
                    }
                }
            }

            // Read event
            for (int i = 1; i < fds_length; i++) {
                if ((fds[i].revents & POLLIN) == POLLIN) {
                    // handover to the handler 
                    read_handler(fds[i].fd, 15, fds, fds_length);
                }
            }
        }
    }

	close(server_fd);

	return 0;
}
