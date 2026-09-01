#include "../include/udp-client.h"

void initialize_openssl() {
  SSL_library_init();
  OpenSSL_add_ssl_algorithms();
  SSL_load_error_strings();
}

SSL_CTX *create_dtls_context() {
  const SSL_METHOD *method = DTLS_client_method();
  SSL_CTX *ctx = SSL_CTX_new(method);
  if (!ctx) {
    ERR_print_errors_fp(stderr);
    log_fatal("Failed to create DTLS context");
  }

  SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

  return ctx;
}

int create_and_connect_udp_socket(struct sockaddr_in *server_addr) {
  int sock_fd;
  struct timeval tv = {3, 0};

  sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_fd < 0) {
    error_exit("socket() failed");
  }

  if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    log_warn("setsockopt SO_RCVTIMEO failed: %s", strerror(errno));
  }

  memset(server_addr, 0, sizeof(*server_addr));
  server_addr->sin_family = AF_INET;
  server_addr->sin_port = htons(UDP_PORT);

  if (inet_pton(AF_INET, "127.0.0.1", &server_addr->sin_addr) <= 0) {
    close(sock_fd);
    error_exit("inet_pton() failed");
  }

  if (connect(sock_fd, (struct sockaddr *)server_addr, sizeof(*server_addr)) <
      0) {
    close(sock_fd);
    error_exit("connect() failed");
  }

  log_info("UDP socket connected to 127.0.0.1:%d", UDP_PORT);
  return sock_fd;
}

SSL *perform_dtls_handshake(int sock_fd, SSL_CTX *ctx,
                            const struct sockaddr_in *server_addr) {
  SSL *ssl = SSL_new(ctx);
  if (!ssl) {
    ERR_print_errors_fp(stderr);
    log_fatal("SSL_new failed");
  }

  BIO *bio = BIO_new_dgram(sock_fd, BIO_NOCLOSE);
  if (!bio) {
    ERR_print_errors_fp(stderr);
    SSL_free(ssl);
    log_fatal("BIO_new_dgram failed");
  }

  struct sockaddr_storage peer_storage;
  memset(&peer_storage, 0, sizeof(peer_storage));
  memcpy(&peer_storage, server_addr, sizeof(*server_addr));
  BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_PEER, 0, &peer_storage);

  struct timeval timeout = {3, 0};
  BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

  SSL_set_bio(ssl, bio, bio);
  SSL_set_mtu(ssl, 1400);

  int ret = SSL_connect(ssl);
  if (ret <= 0) {
    int err = SSL_get_error(ssl, ret);
    log_error("SSL_connect returned %d, SSL_get_error = %d", ret, err);
    ERR_print_errors_fp(stderr);
    SSL_free(ssl);
    return NULL;
  }

  log_info("DTLS handshake successful. Cipher: %s", SSL_get_cipher(ssl));
  return ssl;
}

void communication_loop(SSL *ssl) {
  Packet send_packet, recv_packet;
  memset(&send_packet, 0, sizeof(send_packet));
  send_packet.user_id = 67890;

  log_info("--- Secure UDP (DTLS) Client Connected ---");
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

    if (SSL_write(ssl, &network_packet, (int)send_size) <= 0) {
      int err = SSL_get_error(ssl, -1);
      log_error("SSL_write failed (ssl_err=%d)", err);
      ERR_print_errors_fp(stderr);
      break;
    }

    int received = 0;
    int retry_count = 0;
    const int MAX_RETRIES = 3;

    while (!received && retry_count < MAX_RETRIES) {
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
        received = 1;
      } else {
        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
          retry_count++;
          log_debug("SSL_read WANT_READ/WRITE, retry %d/%d", retry_count,
                    MAX_RETRIES);
          continue;
        } else if (err == SSL_ERROR_ZERO_RETURN) {
          log_info("Server closed the DTLS connection.");
          return;
        } else {
          log_error("SSL_read failed: n=%d err=%d", n, err);
          ERR_print_errors_fp(stderr);
          return;
        }
      }
    }

    if (!received) {
      log_warn("No response after %d retries.", MAX_RETRIES);
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

int main() {
  logger_init(LOG_LEVEL_DEBUG, "logs/udp-client.log");

  int sock_fd = -1;
  struct sockaddr_in server_addr;
  SSL_CTX *ctx = NULL;
  SSL *ssl = NULL;

  initialize_openssl();
  ctx = create_dtls_context();
  sock_fd = create_and_connect_udp_socket(&server_addr);
  ssl = perform_dtls_handshake(sock_fd, ctx, &server_addr);

  if (ssl) {
    communication_loop(ssl);
  } else {
    log_error("DTLS handshake failed, cannot communicate.");
  }

  cleanup(sock_fd, ctx, ssl);

  return ssl ? 0 : 1;
}
