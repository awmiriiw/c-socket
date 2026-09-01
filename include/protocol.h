#ifndef PROTOCOL_H
#define PROTOCOL_H

#define TCP_PORT 8080
#define UDP_PORT 8081
#define BUFFER_SIZE 256

#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
  uint32_t user_id;
  uint16_t message_len;
  char message[BUFFER_SIZE];
} Packet;
#pragma pack(pop)

#endif
