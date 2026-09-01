#include "../include/server.h"
#include "../include/utils.h"

volatile sig_atomic_t keep_running = 1;
static dtls_session_t *dtls_sessions_head = NULL;
static pthread_mutex_t dtls_sessions_mutex = PTHREAD_MUTEX_INITIALIZER;

void int_handler(int sig) {
  (void)sig;
  keep_running = 0;
  log_info("Shutdown signal received, server will stop...");
}

dtls_session_t *
find_or_create_dtls_session(const struct sockaddr_in *client_addr, SSL_CTX *ctx,
                            int listen_fd) {
  pthread_mutex_lock(&dtls_sessions_mutex);

  for (dtls_session_t *curr = dtls_sessions_head; curr != NULL;
       curr = curr->next) {
    if (curr->client_addr.sin_addr.s_addr == client_addr->sin_addr.s_addr &&
        curr->client_addr.sin_port == client_addr->sin_port) {
      curr->last_activity = time(NULL);
      curr->ref_count++;
      pthread_mutex_unlock(&dtls_sessions_mutex);
      return curr;
    }
  }

  dtls_session_t *new_session = malloc(sizeof(dtls_session_t));
  if (!new_session) {
    pthread_mutex_unlock(&dtls_sessions_mutex);
    log_error("malloc failed for new DTLS session");
    return NULL;
  }

  new_session->client_addr = *client_addr;
  new_session->last_activity = time(NULL);
  new_session->next = NULL;
  new_session->ref_count = 1;

  new_session->ssl = SSL_new(ctx);
  if (!new_session->ssl) {
    free(new_session);
    pthread_mutex_unlock(&dtls_sessions_mutex);
    log_error("SSL_new failed for DTLS session");
    return NULL;
  }

  BIO *bio = BIO_new_dgram(listen_fd, BIO_NOCLOSE);
  if (!bio) {
    SSL_free(new_session->ssl);
    free(new_session);
    pthread_mutex_unlock(&dtls_sessions_mutex);
    log_error("BIO_new_dgram failed");
    return NULL;
  }

  struct sockaddr_storage peer_addr;
  memset(&peer_addr, 0, sizeof(peer_addr));
  memcpy(&peer_addr, client_addr, sizeof(*client_addr));
  BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_PEER, 0, &peer_addr);

  SSL_set_bio(new_session->ssl, bio, bio);
  SSL_set_accept_state(new_session->ssl);

  new_session->next = dtls_sessions_head;
  dtls_sessions_head = new_session;

  pthread_mutex_unlock(&dtls_sessions_mutex);
  return new_session;
}

static void release_dtls_session(dtls_session_t *session) {
  pthread_mutex_lock(&dtls_sessions_mutex);
  session->ref_count--;
  pthread_mutex_unlock(&dtls_sessions_mutex);
}

void remove_dtls_session(const struct sockaddr_in *client_addr) {
  pthread_mutex_lock(&dtls_sessions_mutex);
  dtls_session_t *current = dtls_sessions_head;
  dtls_session_t *prev = NULL;

  while (current != NULL) {
    if (current->client_addr.sin_addr.s_addr == client_addr->sin_addr.s_addr &&
        current->client_addr.sin_port == client_addr->sin_port) {

      if (current->ref_count > 1) {
        current->ref_count--;
        pthread_mutex_unlock(&dtls_sessions_mutex);
        return;
      }

      if (prev == NULL)
        dtls_sessions_head = current->next;
      else
        prev->next = current->next;

      SSL_free(current->ssl);
      free(current);
      break;
    }
    prev = current;
    current = current->next;
  }
  pthread_mutex_unlock(&dtls_sessions_mutex);
}

void cleanup_stale_dtls_sessions() {
  pthread_mutex_lock(&dtls_sessions_mutex);
  dtls_session_t *current = dtls_sessions_head;
  dtls_session_t *prev = NULL;
  time_t now = time(NULL);

  while (current != NULL) {
    if (current->ref_count == 0 &&
        difftime(now, current->last_activity) > DTLS_SESSION_TIMEOUT) {
      log_info("DTLS session timed out. Cleaning up.");
      dtls_session_t *to_remove = current;

      if (prev == NULL) {
        dtls_sessions_head = current->next;
        current = dtls_sessions_head;
      } else {
        prev->next = current->next;
        current = prev->next;
      }

      SSL_free(to_remove->ssl);
      free(to_remove);
    } else {
      prev = current;
      current = current->next;
    }
  }
  pthread_mutex_unlock(&dtls_sessions_mutex);
}

void *handle_tcp_client(void *args) {
  tcp_thread_args_t *thread_args = (tcp_thread_args_t *)args;
  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &thread_args->client_addr.sin_addr, client_ip,
            INET_ADDRSTRLEN);

  SSL *ssl = SSL_new(thread_args->ctx);
  if (!ssl) {
    log_error("SSL_new failed for TCP client %s", client_ip);
    close(thread_args->conn_fd);
    free(thread_args);
    return NULL;
  }

  SSL_set_fd(ssl, thread_args->conn_fd);

  if (SSL_accept(ssl) <= 0) {
    log_error("SSL handshake failed for %s", client_ip);
    ERR_print_errors_fp(stderr);
  } else {
    log_info("Secure TCP connection from %s established", client_ip);
    Packet packet;

    while (SSL_read(ssl, &packet, sizeof(packet)) > 0) {
      packet.user_id = ntohl(packet.user_id);
      packet.message_len = ntohs(packet.message_len);

      uint16_t safe_len = (packet.message_len < BUFFER_SIZE - 1)
                              ? packet.message_len
                              : BUFFER_SIZE - 1;
      packet.message[safe_len] = '\0';

      log_debug("Received from TLS User #%u (%zu bytes): %s", packet.user_id,
                (size_t)safe_len, packet.message);

      Packet reply = packet;
      reply.user_id = htonl(packet.user_id);
      reply.message_len = htons(safe_len);

      size_t send_size = offsetof(Packet, message) + safe_len;
      if (SSL_write(ssl, &reply, send_size) <= 0) {
        log_error("SSL_write failed for client %s", client_ip);
        ERR_print_errors_fp(stderr);
        break;
      }
    }
    log_info("Client %s disconnected.", client_ip);
  }

  SSL_shutdown(ssl);
  SSL_free(ssl);
  close(thread_args->conn_fd);
  free(thread_args);
  return NULL;
}

void handle_dtls_datagram(int udp_listen_fd, SSL_CTX *dtls_ctx) {
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  char peek_buf[1];

  ssize_t ret = recvfrom(udp_listen_fd, peek_buf, sizeof(peek_buf), MSG_PEEK,
                         (struct sockaddr *)&client_addr, &client_len);
  if (ret <= 0) {
    if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      log_error("recvfrom (peek) failed: %s", strerror(errno));
    }
    return;
  }

  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

  dtls_session_t *session =
      find_or_create_dtls_session(&client_addr, dtls_ctx, udp_listen_fd);
  if (!session) {
    log_error("Could not find or create DTLS session for %s", client_ip);
    char discard[1500];
    recvfrom(udp_listen_fd, discard, sizeof(discard), 0, NULL, NULL);
    return;
  }

  if (!SSL_is_init_finished(session->ssl)) {
    int accept_ret = SSL_accept(session->ssl);
    if (accept_ret > 0) {
      log_info("DTLS Handshake successful for %s", client_ip);
    } else {
      int ssl_err = SSL_get_error(session->ssl, accept_ret);
      if (ssl_err != SSL_ERROR_WANT_READ && ssl_err != SSL_ERROR_WANT_WRITE) {
        log_error("DTLS Handshake failed for %s (ssl_err=%d)", client_ip,
                  ssl_err);
        ERR_print_errors_fp(stderr);
        release_dtls_session(session);
        remove_dtls_session(&client_addr);
        return;
      }
    }
  } else {
    Packet packet;
    int read_len = SSL_read(session->ssl, &packet, sizeof(packet));
    if (read_len > 0) {
      session->last_activity = time(NULL);

      packet.user_id = ntohl(packet.user_id);
      packet.message_len = ntohs(packet.message_len);

      uint16_t safe_len = (packet.message_len < BUFFER_SIZE - 1)
                              ? packet.message_len
                              : BUFFER_SIZE - 1;
      packet.message[safe_len] = '\0';

      log_debug("Received from DTLS User #%u (%zu bytes): %s", packet.user_id,
                (size_t)safe_len, packet.message);

      Packet reply = packet;
      reply.user_id = htonl(packet.user_id);
      reply.message_len = htons(safe_len);

      size_t send_size = offsetof(Packet, message) + safe_len;
      if (SSL_write(session->ssl, &reply, send_size) <= 0) {
        log_error("SSL_write failed for DTLS client %s", client_ip);
        ERR_print_errors_fp(stderr);
      }
    } else {
      int ssl_err = SSL_get_error(session->ssl, read_len);
      if (ssl_err != SSL_ERROR_WANT_READ && ssl_err != SSL_ERROR_WANT_WRITE) {
        log_info("DTLS client %s disconnected (ssl_err=%d).", client_ip,
                 ssl_err);
        release_dtls_session(session);
        remove_dtls_session(&client_addr);
        return;
      }
    }
  }

  release_dtls_session(session);
}

int create_listening_socket(int type, uint16_t port) {
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));

  int fd = socket(AF_INET, type, 0);
  if (fd < 0)
    error_exit("socket() failed");

  int optval = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    error_exit("bind() failed");

  if (type == SOCK_STREAM && listen(fd, 10) < 0)
    error_exit("listen() failed");

  return fd;
}

SSL_CTX *create_context(int is_dtls) {
  const SSL_METHOD *method =
      is_dtls ? DTLS_server_method() : TLS_server_method();
  SSL_CTX *ctx = SSL_CTX_new(method);
  if (!ctx) {
    ERR_print_errors_fp(stderr);
    log_fatal("Unable to create SSL context");
  }

  if (SSL_CTX_use_certificate_file(ctx, "certs/cert.pem", SSL_FILETYPE_PEM) <=
          0 ||
      SSL_CTX_use_PrivateKey_file(ctx, "certs/key.pem", SSL_FILETYPE_PEM) <=
          0) {
    ERR_print_errors_fp(stderr);
    log_fatal("Failed to load certificate/key files");
  }

  if (!SSL_CTX_check_private_key(ctx)) {
    log_fatal("Private key does not match certificate");
  }

  return ctx;
}

void server_loop(int tcp_fd, int udp_fd, SSL_CTX *tls_ctx, SSL_CTX *dtls_ctx) {
  fd_set read_fds;
  int max_fd = (tcp_fd > udp_fd) ? tcp_fd : udp_fd;

  log_info("--- Hybrid Server Started ---");
  log_info("Listening for TCP+TLS on port %d", TCP_PORT);
  log_info("Listening for UDP+DTLS on port %d", UDP_PORT);

  while (keep_running) {
    FD_ZERO(&read_fds);
    FD_SET(tcp_fd, &read_fds);
    FD_SET(udp_fd, &read_fds);

    struct timeval tv = {5, 0};
    int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);

    if (activity < 0) {
      if (errno == EINTR)
        continue;
      log_error("select() error: %s", strerror(errno));
      break;
    }

    if (!keep_running)
      break;

    if (activity == 0) {
      cleanup_stale_dtls_sessions();
      continue;
    }

    if (FD_ISSET(tcp_fd, &read_fds)) {
      struct sockaddr_in client_addr;
      socklen_t client_len = sizeof(client_addr);
      int conn_fd =
          accept(tcp_fd, (struct sockaddr *)&client_addr, &client_len);

      if (conn_fd < 0) {
        if (errno != EINTR && errno != EAGAIN) {
          log_error("accept() failed: %s", strerror(errno));
        }
      } else {
        tcp_thread_args_t *args = malloc(sizeof(tcp_thread_args_t));
        if (!args) {
          log_error("malloc failed for tcp_thread_args");
          close(conn_fd);
        } else {
          args->conn_fd = conn_fd;
          args->client_addr = client_addr;
          args->ctx = tls_ctx;

          pthread_t tid;
          if (pthread_create(&tid, NULL, handle_tcp_client, args) != 0) {
            log_error("pthread_create failed: %s", strerror(errno));
            close(conn_fd);
            free(args);
          } else {
            pthread_detach(tid);
          }
        }
      }
    }

    if (FD_ISSET(udp_fd, &read_fds)) {
      handle_dtls_datagram(udp_fd, dtls_ctx);
    }

    cleanup_stale_dtls_sessions();
  }
}

void cleanup_server(int tcp_fd, int udp_fd, SSL_CTX *tls_ctx,
                    SSL_CTX *dtls_ctx) {
  log_info("--- Server Shutting Down ---");

  pthread_mutex_lock(&dtls_sessions_mutex);
  dtls_session_t *current = dtls_sessions_head;
  while (current != NULL) {
    dtls_session_t *next = current->next;
    SSL_free(current->ssl);
    free(current);
    current = next;
  }
  dtls_sessions_head = NULL;
  pthread_mutex_unlock(&dtls_sessions_mutex);

  close(tcp_fd);
  close(udp_fd);
  SSL_CTX_free(tls_ctx);
  SSL_CTX_free(dtls_ctx);
  pthread_mutex_destroy(&dtls_sessions_mutex);
}

int main(void) {
  logger_init(LOG_LEVEL_DEBUG, "logs/server.log");
  signal(SIGINT, int_handler);
  signal(SIGPIPE, SIG_IGN);

  OPENSSL_init_ssl(0, NULL);

  SSL_CTX *tls_ctx = create_context(0);
  SSL_CTX *dtls_ctx = create_context(1);

  int tcp_listen_fd = create_listening_socket(SOCK_STREAM, TCP_PORT);
  int udp_listen_fd = create_listening_socket(SOCK_DGRAM, UDP_PORT);

  server_loop(tcp_listen_fd, udp_listen_fd, tls_ctx, dtls_ctx);

  cleanup_server(tcp_listen_fd, udp_listen_fd, tls_ctx, dtls_ctx);

  return 0;
}
