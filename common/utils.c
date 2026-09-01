#include "../include/utils.h"
#include "../include/logger.h"

void error_exit(const char *msg) { log_fatal("%s: %s", msg, strerror(errno)); }

ssize_t send_all(int sockfd, const void *buf, size_t len) {
  size_t total_sent = 0;
  const char *p = (const char *)buf;

  while (total_sent < len) {
    ssize_t sent = send(sockfd, p + total_sent, len - total_sent, 0);
    if (sent < 0) {
      return -1;
    }
    total_sent += sent;
  }
  return total_sent;
}

ssize_t recv_all(int sockfd, void *buf, size_t len) {
  size_t total_received = 0;
  char *p = (char *)buf;

  while (total_received < len) {
    ssize_t received =
        recv(sockfd, p + total_received, len - total_received, 0);
    if (received < 0) {
      return -1;
    }
    if (received == 0) {
      return total_received;
    }
    total_received += received;
  }
  return total_received;
}
