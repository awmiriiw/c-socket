#include "../include/broadcast-client.h"

int create_broadcast_socket() {
  int sock_fd;
  int broadcast_enable = 1;

  sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_fd < 0) {
    error_exit("socket() failed");
  }

  if (setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable,
                 sizeof(broadcast_enable)) < 0) {
    close(sock_fd);
    error_exit("setsockopt(SO_BROADCAST) failed");
  }

  struct timeval tv = {2, 0};
  if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    log_warn("setsockopt SO_RCVTIMEO failed: %s", strerror(errno));
  }

  return sock_fd;
}

void setup_broadcast_address(struct sockaddr_in *broadcast_addr,
                             const char *broadcast_ip) {
  memset(broadcast_addr, 0, sizeof(*broadcast_addr));
  broadcast_addr->sin_family = AF_INET;
  broadcast_addr->sin_port = htons(UDP_PORT);

  if (inet_pton(AF_INET, broadcast_ip, &broadcast_addr->sin_addr) <= 0) {
    error_exit("inet_pton() failed for broadcast address");
  }
}

const char *get_broadcast_address() { return "255.255.255.255"; }

void send_broadcast_message(int sock_fd,
                            const struct sockaddr_in *broadcast_addr) {
  const char *message = "This is a broadcast message for everyone!";
  size_t msg_len = strlen(message);
  char addr_str[INET_ADDRSTRLEN];

  inet_ntop(AF_INET, &broadcast_addr->sin_addr, addr_str, INET_ADDRSTRLEN);

  ssize_t sent =
      sendto(sock_fd, message, msg_len, 0, (struct sockaddr *)broadcast_addr,
             sizeof(*broadcast_addr));
  if (sent < 0) {
    error_exit("sendto() failed");
  }

  log_info("Broadcast message sent to %s:%d (%zd bytes)", addr_str, UDP_PORT,
           sent);
}

void listen_for_responses(int sock_fd) {
  char recv_buf[BUFFER_SIZE];
  struct sockaddr_in sender_addr;
  socklen_t sender_len = sizeof(sender_addr);
  char sender_ip[INET_ADDRSTRLEN];

  log_info("Listening for responses (2 second timeout)...");

  while (1) {
    memset(recv_buf, 0, sizeof(recv_buf));
    ssize_t n = recvfrom(sock_fd, recv_buf, sizeof(recv_buf) - 1, 0,
                         (struct sockaddr *)&sender_addr, &sender_len);
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        log_info("No more responses received.");
        break;
      }
      log_warn("recvfrom() failed: %s", strerror(errno));
      break;
    }

    recv_buf[n] = '\0';
    inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
    log_info("Response from %s: %s", sender_ip, recv_buf);
  }
}

int main() {
  logger_init(LOG_LEVEL_INFO, NULL);
  log_info("--- UDP Broadcast Client ---");

  struct sockaddr_in broadcast_addr;
  int sock_fd = create_broadcast_socket();

  const char *broadcast_ip = get_broadcast_address();
  log_info("Using broadcast address: %s", broadcast_ip);

  setup_broadcast_address(&broadcast_addr, broadcast_ip);
  send_broadcast_message(sock_fd, &broadcast_addr);
  listen_for_responses(sock_fd);

  close(sock_fd);
  log_info("--- Client Shutting Down ---");

  return 0;
}
