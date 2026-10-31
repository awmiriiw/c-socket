# C-Socket - Secure TCP & UDP Socket Programming

A lightweight, modular, and security-focused Linux networking project built with **C**, **POSIX Sockets**, **OpenSSL**, and **POSIX Threads**.

C-Socket is designed to demonstrate secure network communication using TCP/TLS, UDP/DTLS, and UDP broadcasting through a modular client-server architecture.

It provides encrypted communication, concurrent TCP client handling, DTLS session management, structured packet transmission, network byte-order conversion, connection timeout handling, and detailed logging.

---

## Key Features

### 🔒 Secure TCP Communication (TLS)

- **Encrypted TCP Communication:** Establishes secure client-server connections using OpenSSL and TLS.
- **TLS Handshake:** Performs secure handshakes between TCP clients and the server.
- **Concurrent Client Handling:** Supports multiple TCP clients using POSIX threads.
- **Structured Packet Transmission:** Exchanges packets containing user IDs, message lengths, and message data.
- **Network Byte Order:** Uses standardized byte-order conversion for consistent communication.
- **Echo Communication:** Receives client messages and sends them back through encrypted connections.
- **Connection Management:** Handles client connections, disconnections, and communication errors.

### 🌐 Secure UDP Communication (DTLS)

- **Encrypted UDP Communication:** Provides secure datagram communication using OpenSSL and DTLS.
- **DTLS Handshake:** Establishes encrypted communication sessions over UDP.
- **Session Management:** Maintains DTLS sessions for individual clients.
- **Client Identification:** Identifies UDP clients using their IP addresses and ports.
- **Session Timeout:** Tracks inactive sessions and removes expired connections.
- **Response Retry Handling:** Implements retry logic for UDP client responses.
- **Datagram Processing:** Processes structured messages through encrypted UDP communication.

### 📡 UDP Broadcasting

- **Network Broadcasting:** Sends UDP broadcast messages to devices on the local network.
- **Broadcast Socket Configuration:** Enables UDP broadcasting using `SO_BROADCAST`.
- **Broadcast Address Support:** Uses `255.255.255.255` for IPv4 limited broadcasting.
- **Response Listening:** Waits for responses from devices receiving broadcast messages.
- **Receive Timeout:** Uses configurable socket timeouts to prevent indefinite waiting.
- **Sender Identification:** Displays the IP addresses of responding devices.
- **Broadcast Logging:** Records broadcast transmission and response information.

### ⚙️ Socket & Network Management

- **POSIX Socket API:** Uses standard Linux socket interfaces for TCP and UDP communication.
- **IPv4 Networking:** Supports IPv4 socket addressing and communication.
- **Socket Configuration:** Configures socket options for broadcasting, address reuse, and receive timeouts.
- **Connection Handling:** Manages socket creation, binding, listening, accepting, and connecting.
- **Data Transmission Utilities:** Provides reusable functions for sending and receiving complete buffers.
- **Error Handling:** Includes centralized error reporting and fatal error handling.
- **Resource Cleanup:** Releases sockets, SSL objects, and allocated resources during shutdown.

### 🛡️ Security & Encryption

- **OpenSSL Integration:** Uses OpenSSL for TLS and DTLS communication.
- **TLS Encryption:** Protects TCP communication through encrypted TLS sessions.
- **DTLS Encryption:** Protects UDP communication through encrypted DTLS sessions.
- **Certificate Management:** Supports X.509 certificates and private keys for server authentication.
- **Certificate Generation:** Provides automated self-signed certificate generation using OpenSSL.
- **Cipher Negotiation:** Negotiates encryption algorithms during TLS and DTLS handshakes.
- **SSL Context Management:** Creates and manages separate TLS and DTLS contexts.

**Security Note:** The current clients do not verify the server's certificate or hostname. The UDP client explicitly disables certificate verification. TLS and DTLS provide encryption, but server authentication must be implemented before using this project in security-sensitive environments.

### 🔄 Concurrent Connections & Session Lifecycle

- **Multithreaded TCP Server:** Handles TCP client connections through dedicated POSIX threads.
- **Thread Management:** Creates detached threads for concurrent TCP communication.
- **DTLS Session Tracking:** Maintains a linked list of active DTLS client sessions.
- **Mutex Synchronization:** Protects shared DTLS session structures using pthread mutexes.
- **Session Activity Monitoring:** Tracks the last activity time of DTLS clients.
- **Inactive Session Cleanup:** Removes inactive DTLS sessions after a configured timeout.
- **Graceful Shutdown:** Supports server shutdown through `SIGINT`.
- **Resource Management:** Cleans up SSL contexts, sockets, mutexes, and session resources.

### 📦 Packet Protocol

- **Custom Packet Structure:** Defines a shared packet format for TCP and UDP communication.
- **User Identification:** Includes a 32-bit user identifier in each packet.
- **Message Length:** Uses a 16-bit field to represent message length.
- **Fixed Buffer Capacity:** Supports message buffers up to 256 bytes.
- **Packed Structures:** Uses packed C structures to control packet memory layout.
- **Byte Order Conversion:** Converts packet fields between host and network byte order.
- **Shared Protocol Definitions:** Maintains common packet definitions in `include/protocol.h`.

### 📝 Logging & Diagnostics

- **Structured Logging:** Provides timestamped logs with DEBUG, INFO, WARN, ERROR, and FATAL severity levels.
- **Console Logging:** Displays log messages through standard error output.
- **Persistent Log Files:** Stores TCP client, UDP client, and server logs in the `logs/` directory.
- **Source Location Tracking:** Includes source filenames and line numbers in log messages.
- **Configurable Log Levels:** Allows different logging verbosity levels.
- **Connection Diagnostics:** Reports connection establishment, handshake results, and disconnections.
- **OpenSSL Error Reporting:** Displays OpenSSL diagnostic information when cryptographic operations fail.

### 🛠️ Modular Architecture & Build System

- **Modular Source Structure:** Separates client, server, shared utilities, and header files.
- **Shared Utilities:** Provides reusable logging, error handling, and socket communication functions.
- **Makefile Build System:** Automates compilation and linking using GNU Make.
- **Separate Executables:** Builds independent TCP, UDP, broadcast, and server binaries.
- **Automatic Directory Creation:** Creates required build, certificate, and logging directories.
- **Certificate Generation:** Generates development certificates using OpenSSL.
- **Build Cleanup:** Provides commands for removing compiled binaries, object files, and logs.

---

## Requirements

C-Socket is designed for **Linux systems** supporting POSIX sockets and POSIX threads.

### Required Dependencies

- GCC
- GNU Make
- OpenSSL
- OpenSSL development headers
- POSIX Threads (pthread)
- Standard C Library
- Linux networking headers

### Ubuntu / Debian

```bash
sudo apt update

sudo apt install build-essential libssl-dev openssl
```

### Arch Linux

```bash
sudo pacman -S base-devel openssl
```

### Fedora

```bash
sudo dnf install gcc make openssl openssl-devel
```

**Important:** The project uses ports `8080/TCP` and `8081/UDP` by default. Make sure these ports are available before starting the server.

---

## Get Start:

```bash
git clone https://github.com/awmiriiw/c-socket.git

cd c-socket

# Create the required directories
make setup

# Generate self-signed TLS/DTLS certificates
make certs

# Build the entire project
make build

# Remove compiled binaries and object files
make clean

# Remove build artifacts and log files
make cleanall


# Compiled binaries will be available at:
# bin/server
# bin/tcp-client
# bin/udp-client
# bin/broadcast-client

# Generated certificates will be stored in:
# certs/cert.pem
# certs/key.pem

# Application logs will be stored in:
# logs/server.log
# logs/tcp-client.log
# logs/udp-client.log
```

---

## Available Commands

| Command         | Description                                                    |
| :-------------- | :------------------------------------------------------------- |
| `make setup`    | Creates the required project directories.                      |
| `make certs`    | Generates self-signed SSL/TLS certificates and private keys.   |
| `make build`    | Compiles the server and all client applications.               |
| `make clean`    | Removes compiled binaries and object files.                    |
| `make cleanall` | Removes compiled binaries, object files, and application logs. |

---

## Running the Project

### Start the Server

Start the hybrid TCP/TLS and UDP/DTLS server:

```bash
./bin/server
```

The server listens on:

- **TCP Port 8080:** Secure TCP communication using TLS.
- **UDP Port 8081:** Secure UDP communication using DTLS.

The server handles TCP connections using POSIX threads and manages DTLS sessions through a shared session list.

### TCP Client (TLS)

Open another terminal and execute:

```bash
./bin/tcp-client
```

The TCP client establishes a TLS connection to:

```text
127.0.0.1:8080
```

After connecting, enter a message:

```text
> Hello from TCP client!

Echo from User #12345: Hello from TCP client!
```

Type `exit` to disconnect.

### UDP Client (DTLS)

Open another terminal and execute:

```bash
./bin/udp-client
```

The UDP client attempts to establish a DTLS connection to:

```text
127.0.0.1:8081
```

After a successful handshake, enter a message:

```text
> Hello from UDP client!

Echo from User #67890: Hello from UDP client!
```

Type `exit` to disconnect.

**Note:** The current DTLS server implementation is experimental. Its shared UDP socket and session handling may cause handshake or communication failures, particularly with multiple clients.

### UDP Broadcast Client

Execute:

```bash
./bin/broadcast-client
```

The broadcast client sends a UDP message to:

```text
255.255.255.255:8081
```

Example output:

```text
Broadcast message sent to 255.255.255.255:8081
Listening for responses (2 second timeout)...
No more responses received.
```

**Note:** The broadcast client uses plain UDP, while the server expects DTLS packets on port `8081`. Therefore, the current server does not provide a compatible broadcast response service.

A separate UDP broadcast receiver is required to respond to broadcast messages.

---

## Network Configuration

The default network configuration is defined in `include/protocol.h`.

```c
#define TCP_PORT 8080
#define UDP_PORT 8081
#define BUFFER_SIZE 256
```

The TCP and UDP clients connect to `127.0.0.1` by default.

To connect to another machine, modify the server IP address in the corresponding client source file.

The server binds to `INADDR_ANY`, allowing connections through available IPv4 network interfaces.

**Important:** When exposing the server to other devices, configure firewall rules appropriately and implement certificate verification before relying on encrypted connections for security.

---

## Known Limitations

- **Certificate Verification:** Clients currently do not authenticate the server certificate.
- **TCP Message Framing:** TLS operates over a byte stream, and the current implementation assumes a complete packet is returned by a single `SSL_read()` call.
- **Packet Validation:** Received packet lengths are not fully validated against the actual number of bytes received.
- **DTLS Session Handling:** Multiple DTLS sessions share the same UDP listening socket, which can cause datagrams to be processed by the wrong session.
- **Broadcast Compatibility:** The broadcast client cannot communicate directly with the DTLS echo server using plain UDP.
- **Shutdown Management:** Detached TCP worker threads are not explicitly joined before server resources are released.
- **IPv4 Only:** The current implementation does not support IPv6.

These limitations should be addressed before using the project in production environments.

---

## License

This project is under the **GNU Affero General Public License v3.0 (AGPL-3.0)**.

See the [LICENSE](LICENSE) file for the complete license text.
