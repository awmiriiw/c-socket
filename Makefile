CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
LDFLAGS = -pthread
LIBS = -lssl -lcrypto

SRC_DIR = .
BIN_DIR = ./bin
OBJ_DIR = ./obj
INCLUDE_DIR = ./include
CERTS_DIR = ./certs
LOGS_DIR = ./logs

CLIENT_DIR = $(SRC_DIR)/client
SERVER_DIR = $(SRC_DIR)/server
COMMON_DIR = $(SRC_DIR)/common

COMMON_SRCS = $(COMMON_DIR)/utils.c $(COMMON_DIR)/logger.c
SERVER_SRCS = $(SERVER_DIR)/server.c
TCP_CLIENT_SRCS = $(CLIENT_DIR)/tcp-client.c
UDP_CLIENT_SRCS = $(CLIENT_DIR)/udp-client.c
BROADCAST_CLIENT_SRCS = $(CLIENT_DIR)/broadcast-client.c

COMMON_OBJS = $(patsubst $(COMMON_DIR)/%.c,$(OBJ_DIR)/common/%.o,$(COMMON_SRCS))
SERVER_OBJS = $(patsubst $(SERVER_DIR)/%.c,$(OBJ_DIR)/server/%.o,$(SERVER_SRCS))
TCP_CLIENT_OBJS = $(patsubst $(CLIENT_DIR)/%.c,$(OBJ_DIR)/client/%.o,$(TCP_CLIENT_SRCS))
UDP_CLIENT_OBJS = $(patsubst $(CLIENT_DIR)/%.c,$(OBJ_DIR)/client/%.o,$(UDP_CLIENT_SRCS))
BROADCAST_CLIENT_OBJS = $(patsubst $(CLIENT_DIR)/%.c,$(OBJ_DIR)/client/%.o,$(BROADCAST_CLIENT_SRCS))

TARGETS = $(BIN_DIR)/server \
          $(BIN_DIR)/tcp-client \
          $(BIN_DIR)/udp-client \
          $(BIN_DIR)/broadcast-client

.PHONY: build clean cleanall setup certs

build: setup $(TARGETS)

setup:
	@mkdir -p $(BIN_DIR) $(OBJ_DIR) $(LOGS_DIR) $(CERTS_DIR)
	@mkdir -p $(OBJ_DIR)/common $(OBJ_DIR)/server $(OBJ_DIR)/client
	@echo "Directories ready."

certs: setup
	@if [ ! -f $(CERTS_DIR)/cert.pem ] || [ ! -f $(CERTS_DIR)/key.pem ]; then \
		echo "Generating self-signed certificate..."; \
		openssl req -x509 -newkey rsa:2048 \
			-keyout $(CERTS_DIR)/key.pem \
			-out $(CERTS_DIR)/cert.pem \
			-days 365 -nodes \
			-subj "/CN=localhost"; \
		echo "Certificate generated in $(CERTS_DIR)/"; \
	else \
		echo "Certificate already exists, skipping."; \
	fi

$(BIN_DIR)/server: $(SERVER_OBJS) $(COMMON_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

$(BIN_DIR)/tcp-client: $(TCP_CLIENT_OBJS) $(COMMON_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

$(BIN_DIR)/udp-client: $(UDP_CLIENT_OBJS) $(COMMON_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

$(BIN_DIR)/broadcast-client: $(BROADCAST_CLIENT_OBJS) $(COMMON_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

$(OBJ_DIR)/common/%.o: $(COMMON_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

$(OBJ_DIR)/server/%.o: $(SERVER_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

$(OBJ_DIR)/client/%.o: $(CLIENT_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Build artifacts removed."

cleanall: clean
	rm -f $(LOGS_DIR)/*.log
	@echo "Logs removed."
