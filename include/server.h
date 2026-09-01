#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "logger.h"
#include "protocol.h"

#define DTLS_SESSION_TIMEOUT 30

typedef struct {
  int conn_fd;
  struct sockaddr_in client_addr;
  SSL_CTX *ctx;
} tcp_thread_args_t;

typedef struct dtls_session {
  struct sockaddr_in client_addr;
  SSL *ssl;
  time_t last_activity;
  int ref_count;
  struct dtls_session *next;
} dtls_session_t;

SSL_CTX *create_context(int is_dtls);
int create_listening_socket(int type, uint16_t port);
void *handle_tcp_client(void *args);
dtls_session_t *
find_or_create_dtls_session(const struct sockaddr_in *client_addr, SSL_CTX *ctx,
                            int listen_fd);
void remove_dtls_session(const struct sockaddr_in *client_addr);
void cleanup_stale_dtls_sessions(void);
void server_loop(int tcp_fd, int udp_fd, SSL_CTX *tls_ctx, SSL_CTX *dtls_ctx);
void cleanup_server(int tcp_fd, int udp_fd, SSL_CTX *tls_ctx,
                    SSL_CTX *dtls_ctx);
void int_handler(int sig);

#endif
