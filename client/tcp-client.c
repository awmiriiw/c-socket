#include "../include/tcp-client.h"

void initialize_openssl() {
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
}

SSL_CTX *create_client_context() {
  const SSL_METHOD *method = TLS_client_method();
  SSL_CTX *ctx = SSL_CTX_new(method);
  if (!ctx) {
    ERR_print_errors_fp(stderr);
    log_fatal("Unable to create SSL context");
  }
  return ctx;
}

int create_and_connect_socket(struct sockaddr_in *server_addr) {
  int sock_fd;
  struct timeval tv = {5, 0};

  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    error_exit("socket() failed");
  }

  if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    log_warn("setsockopt SO_RCVTIMEO failed: %s", strerror(errno));
  }

  memset(server_addr, 0, sizeof(*server_addr));
  server_addr->sin_family = AF_INET;
  server_addr->sin_port = htons(TCP_PORT);

  if (inet_pton(AF_INET, "127.0.0.1", &server_addr->sin_addr) <= 0) {
    error_exit("inet_pton() failed");
  }

  if (connect(sock_fd, (struct sockaddr *)server_addr, sizeof(*server_addr)) <
      0) {
    close(sock_fd);
    error_exit("connect() failed");
  }

  log_info("Connected to server at 127.0.0.1:%d", TCP_PORT);
  return sock_fd;
}

void communication_loop(SSL *ssl) {
  Packet send_packet, recv_packet;
  memset(&send_packet, 0, sizeof(send_packet));
  send_packet.user_id = 12345;

  log_info("--- Secure TCP Client Connected ---");
  log_info("Enter 'exit' to quit.");

  while (1) {
    printf("> ");
    fflush(stdout);

    if (fgets(send_packet.message, sizeof(send_packet.message), stdin) ==
        NULL) {
      log_info("EOF received, exiting.");
      break;
    }

    size_t msg_len = strlen(send_packet.message);
    if (msg_len > 0 && send_packet.message[msg_len - 1] == '\n') {
      send_packet.message[msg_len - 1] = '\0';
      msg_len--;
    }

    if (strcmp(send_packet.message, "exit") == 0)
      break;

    if (msg_len == 0) {
      log_warn("Empty message, skipping.");
      continue;
    }

    send_packet.message_len = (uint16_t)msg_len;

    Packet network_packet;
    memset(&network_packet, 0, sizeof(network_packet));
    network_packet.user_id = htonl(send_packet.user_id);
    network_packet.message_len = htons(send_packet.message_len);
    memcpy(network_packet.message, send_packet.message, msg_len);

    size_t send_size = offsetof(Packet, message) + msg_len;

    int written = SSL_write(ssl, &network_packet, (int)send_size);
    if (written <= 0) {
      int err = SSL_get_error(ssl, written);
      log_error("SSL_write failed (ssl_err=%d)", err);
      ERR_print_errors_fp(stderr);
      break;
    }

    memset(&recv_packet, 0, sizeof(recv_packet));
    int n = SSL_read(ssl, &recv_packet, sizeof(recv_packet));
    if (n > 0) {
      recv_packet.user_id = ntohl(recv_packet.user_id);
      recv_packet.message_len = ntohs(recv_packet.message_len);

      uint16_t safe_len = (recv_packet.message_len < BUFFER_SIZE - 1)
                              ? recv_packet.message_len
                              : BUFFER_SIZE - 1;
      recv_packet.message[safe_len] = '\0';

      printf("Echo from User #%u: %s\n", recv_packet.user_id,
             recv_packet.message);
    } else {
      int err = SSL_get_error(ssl, n);
      if (err == SSL_ERROR_ZERO_RETURN) {
        log_info("Server closed the connection.");
      } else {
        log_warn("SSL_read failed (ssl_err=%d)", err);
        ERR_print_errors_fp(stderr);
      }
      break;
    }
  }
}

void cleanup(int sock_fd, SSL_CTX *ctx, SSL *ssl) {
  if (ssl) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
  }
  if (sock_fd >= 0) {
    close(sock_fd);
  }
  if (ctx) {
    SSL_CTX_free(ctx);
  }
  EVP_cleanup();
  log_info("--- Client Shutting Down ---");
}

int main(void) {
  logger_init(LOG_LEVEL_DEBUG, "logs/tcp-client.log");

  int sock_fd = -1;
  struct sockaddr_in server_addr;
  SSL_CTX *ctx = NULL;
  SSL *ssl = NULL;

  initialize_openssl();
  ctx = create_client_context();
  sock_fd = create_and_connect_socket(&server_addr);

  ssl = SSL_new(ctx);
  if (!ssl) {
    log_fatal("SSL_new failed");
  }

  SSL_set_fd(ssl, sock_fd);

  if (SSL_connect(ssl) <= 0) {
    log_error("SSL connection failed");
    ERR_print_errors_fp(stderr);
    cleanup(sock_fd, ctx, ssl);
    return 1;
  }

  log_info("TLS handshake successful. Cipher: %s", SSL_get_cipher(ssl));

  communication_loop(ssl);
  cleanup(sock_fd, ctx, ssl);

  return 0;
}
