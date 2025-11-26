# UDP File Transfer System

## Description
This project implements a simple client-server file transfer system using UDP sockets in C. The server supports multiple operations like uploading (put), downloading (get), deleting files, listing directory contents (ls), and exiting the session. To handle UDP's unreliability, the system uses chunked file transfers with acknowledgment (ACK) mechanisms and timeouts to manage packet loss and ensure data integrity.

## Features
- **Operations Supported**: get [filename], put [filename], delete [filename], ls, exit.
- **Chunked Transfer**: Files are divided into chunks (up to 16KB) for transmission.
- **Reliability**: ACK-based retransmission with timeouts (2 seconds) and max retries (5) for lost packets.
- **Error Handling**: Graceful handling of file not found, invalid commands, and network errors.
- **Timing**: Measures time taken for get/put operations on the client side.

## Assumptions
- The server runs indefinitely until interrupted (e.g., Ctrl+C).
- For `ls`, the server uses a temporary file (`../ls_output.txt`) to store directory listing, which is deleted after sending.
- File names are limited to 256 characters; commands to 16 characters.
- No authentication or encryption; assumes trusted network.
- Tested with small to medium files; large files may require tuning chunk size.

## Compilation
Compile the server and client separately:
```bash
gcc uftp_server.c -o uftp_server
gcc uftp_client.c -o uftp_client
```

## Usage
- **Server**: Run the server on a specified port.
  ```bash
  ./uftp_server <port>
  ```
  Example:
  ```bash
  ./uftp_server 8080
  ```

- **Client**: Connect to the server and perform operations interactively.
  ```bash
  ./uftp_client <hostname> <port>
  ```
  Example:
  ```bash
  ./uftp_client localhost 8080
  ```
  Once connected, enter commands like `put example.txt`, `get example.txt`, etc.

## Example
- Upload a file: `put test.txt` (client sends file in chunks; server saves it).
- Download: `get test.txt` (server sends chunks; client reconstructs).
- List files: `ls` (server sends directory listing).
- Delete: `delete test.txt`.
- Exit: `exit`.

## Dependencies
- Standard C libraries (no external dependencies).

## Notes
- The system uses `setsockopt` for timeouts to simulate reliability over UDP.
- For debugging, set `#define DEBUG 1` in the code to enable print statements.
