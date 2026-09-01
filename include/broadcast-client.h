#ifndef BROADCAST_CLIENT_H
#define BROADCAST_CLIENT_H

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "logger.h"
#include "protocol.h"
#include "utils.h"

int create_broadcast_socket(void);
void setup_broadcast_address(struct sockaddr_in *broadcast_addr,
                             const char *broadcast_ip);
const char *get_broadcast_address(void);
void send_broadcast_message(int sock_fd,
                            const struct sockaddr_in *broadcast_addr);
void listen_for_responses(int sock_fd);

#endif
