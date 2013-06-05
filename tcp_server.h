#ifndef _H_TCP_SERVER
#define _H_TCP_SERVER

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/unistd.h>

#include <map>

#include "stopwatch.h"

const unsigned short DEFAULT_PORT = 2013;
const int BACKLOG = 100; // Connection queue size for accept()
const int SOCKET_DATA_CHUNK_SIZE = 1024 * 8;
const int MAX_FILENAME_LEN = 256;
const int REQUEST_TIMEOUT = 2; // Timeout in seconds for getting a complete request (filename)
const int RESPONSE_TIMEOUT = 2; // Timeout for returning a response in  seconds
const char* const LF = "\n";
const char* const CRLF = "\r\n";

// Server I/O states
enum io_status {UNKNOWN = 0, READ_READY, READING, READ_COMPLETE, WRITE_READY,
  WRITING, WRITE_COMPLETE, ERROR};

// Data structure for holding the state of a client connection
struct conn_info
{
  char data[MAX_FILENAME_LEN+3]; // 2 extra bytes for CRLF, 1 for '\0'
  int size; // size of data
  int file_fd; // fd for the file we are reading data from
  io_status status;
  Stopwatch timer;
  conn_info() : size(0), file_fd(-1), status(UNKNOWN)
  {
  }
};

class TcpServer
{
public:
  // Provide a singleton server instance
  static TcpServer& create_server()
  {
    static TcpServer server; 
    return server;
  }
  int prepare_server_socket(unsigned short port, int type);
  // Method for the main event loop
  int run_main_event_loop();
  // Method for handling read readiness
  void on_read_ready(int fd);
  // Method for handling write readiness
  void on_write_ready(int fd);
  // Method for handling new connections from a client
  int on_new_connection();
  // Do read I/O
  int do_read(int socket);
  // Do rrite I/O
  int do_write(int socket);

private:
  TcpServer() 
  {
    port_ = DEFAULT_PORT;
    FD_ZERO(&readfds_);
    FD_ZERO(&writefds_);
    server_fd_ = max_fd_ = -1;
  }
  //TcpServer(TcpServer const&);
  //void operator=(TcpServer const&);

  int set_nonblocking_mode(int fd);
  int delete_conn(int fd, fd_set* fds);
  bool timed_out(int socket);
  void* get_in_addr(struct sockaddr *sa);

  // Member variables
  unsigned short port_;
  fd_set readfds_, writefds_;
  int max_fd_;
  int server_fd_;
  // All live connection data will live in a map
  std::map<int, conn_info> connections_;
};

#endif /* _H_TCP_SERVER */
