#include "tcp_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <fstream>
#include <iostream>

using namespace std;

// Read a file name from the socket
// The file name is delimitted by a CR or CRLF characters
int TcpServer::do_read(int socket)
{
  ssize_t bytes_read = -1;

  // Create a connection state object if one does not exist for this socket
  if (connections_.find(socket) == connections_.end()) {
    conn_info conn;
    Stopwatch w;
    conn.timer = w;
    conn.timer.start(); // timer starts when we create a conn object for first time
    connections_[socket] = conn;
  }

  while (true) {
    bytes_read = read(socket, connections_[socket].data + connections_[socket].size,
      MAX_FILENAME_LEN+2 - connections_[socket].size);
    if (bytes_read == -1) {
      if (errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN) {
        cout << "Error reading from socket: " << strerror(errno) << endl;
        connections_[socket].status = ERROR;
        return -1;
      }
    }
    else if (bytes_read == 0) {
      cout << "Error reading from socket: sockets closed already." << endl;
      connections_[socket].status = ERROR;
      return -1;
    }
    break; // we only read as much data as we can without blocking
  }

  // There was no error in reading data, go check if we received all 
  // bytes we need
  connections_[socket].size += bytes_read; // update data read so far
  int total_bytes = connections_[socket].size;
  connections_[socket].data[total_bytes] = '\0';
  std::string fname(connections_[socket].data);
  size_t id = fname.find(CRLF);
  if (id == string::npos) {
    id = fname.find(LF);
  }

  if (id == string::npos) {
    connections_[socket].status = READING;
  }
  else if (id > MAX_FILENAME_LEN) {
    connections_[socket].status = ERROR;
    return -1;
  }
  else {
    connections_[socket].status = READ_COMPLETE;
    connections_[socket].size = id; // prune the LF/CRLF chars
    connections_[socket].timer.stop();
  }

  return 0;
}

// Read the file content from disk 
// and write it to the socket
int TcpServer::do_write(int socket)
{
  std::map<int, conn_info>::iterator it = connections_.find(socket);
  if (it == connections_.end()) {
    cerr << "Error: filename not found in connection state object!" << endl;
    return -1;
  }

  connections_[socket].data[connections_[socket].size] = '\0'; // for using in cerr/cout
  if (connections_[socket].status == WRITE_READY) {
    // Open the file to read
    connections_[socket].file_fd = open(connections_[socket].data, O_RDONLY);
    if (connections_[socket].file_fd == -1) {
      cerr << "open() failed to open file " << connections_[socket].data << ": "
        << strerror(errno) << endl;
      connections_[socket].status = ERROR; // FIXME: do we need read_error and write_error?
      return -1;
    }
    connections_[socket].timer.reset(); // reset timer for file reads/writes
    connections_[socket].timer.start();
  }

  int file_fd = connections_[socket].file_fd;
  char data[SOCKET_DATA_CHUNK_SIZE+1];

  while (true) {
    ssize_t bytes_read = read(file_fd, data, SOCKET_DATA_CHUNK_SIZE);
    if (bytes_read == -1) {
      if (errno == EINTR) {
        continue;
      }
      cerr << "error reading data from file " << connections_[socket].data << endl;
      connections_[socket].status = ERROR;
      close(file_fd);
      return -1;
    }

    if (bytes_read == 0) {
      // We reached end of file
      connections_[socket].status = WRITE_COMPLETE;
      close(file_fd);
      break;
    }

    int bytes_written = write(socket, data, bytes_read);
    if ((bytes_written == -1) || (bytes_written == 0)){
      if (errno == EINTR) {
        continue;
      }
      cerr << "error writing data from file " << connections_[socket].data <<
        " to socket." <<  endl;
      connections_[socket].status = ERROR;
      close(file_fd);
      return -1;
    }
    connections_[socket].status = WRITING;
    break; // write only SOCKET_DATA_CHUNK_SIZE bytes at a time
  }

  return 0;
}

int TcpServer::prepare_server_socket(unsigned short port, int type)
{
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY); // Bind to all interfaces
  addr.sin_port = htons(port);

  int sock_fd = socket(PF_INET, type, 0);
  if (sock_fd == -1) {
    cerr << "socket() failed: " << strerror(errno) << endl;
    return -1;
  }

  int optval = 1;
  int ret = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  if (ret == -1) {
    cerr << "setsockopt() failed: " << strerror(errno) << endl;
    close(sock_fd);
    return -1;
  }

  ret = bind(sock_fd, (const struct sockaddr*)&addr, sizeof(addr));
  if (ret == -1) {
    cerr << "bind() failed: " << strerror(errno) << endl;
    close(sock_fd);
    return -1;
  }

  ret = listen(sock_fd, BACKLOG);
  if (ret == -1) {
    cerr << "listen() failed: " << strerror(errno) << endl;
    close(sock_fd);
    return -1;
  }

  server_fd_ = max_fd_ = sock_fd;

  return sock_fd;
}

int TcpServer::set_nonblocking_mode(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
        flags = 0;
  }

  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int TcpServer::delete_conn(int fd, fd_set* fds)
{
  FD_CLR(fd, fds);
  close(fd);
  connections_.erase(fd);
  // TODO: handle errors from above ops
  return 0;
}

void* TcpServer::get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*) sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

bool TcpServer::timed_out(int socket)
{
  std::map<int, conn_info>::iterator it = connections_.find(socket);
  if (it != connections_.end()) {
    double elapsed_time = (it->second).timer.elapsed_time();
    if (((it->second).status == READING) && (elapsed_time >= REQUEST_TIMEOUT)) {
      return true;
    }
    if (((it->second).status == WRITING) && (elapsed_time >= RESPONSE_TIMEOUT)) {
      return true;
    }
  }

  return false;
}

void TcpServer::on_read_ready(int fd)
{
  int ret = do_read(fd);
  if (ret == -1) {
    cout << "A filename could not be read successfully from a client" << endl;
    delete_conn(fd, &readfds_);
  }
  else if (connections_[fd].status == READ_COMPLETE) {
    cout << "File read complete, now sending the content back to client" << endl;
    FD_SET(fd, &writefds_); // this socket is ready to be monitored for write-readyness
    FD_CLR(fd, &readfds_); // read is complete, no need to monitor for read-readyness
    connections_[fd].status = WRITE_READY;
  }  
}

void TcpServer::on_write_ready(int fd)
{
  int ret = do_write(fd);
  if (ret == -1) {
    cout << "A file could not be sent successfully to a client" << endl;
  }
  else if (connections_[fd].status == WRITE_COMPLETE) {
    cout << "File successfully sent to client" << endl;
  }
  
  if (connections_[fd].status != WRITING) {
    // We are not in WRITING state, no longer need this socket
    delete_conn(fd, &writefds_);
  }
}

int TcpServer::on_new_connection()
{  
  // Got a new connection, accept() it
  struct sockaddr_storage client_addr;
  socklen_t sin_size = sizeof(client_addr);
  int client_fd = accept(server_fd_, (struct sockaddr*) &client_addr, &sin_size);
  if (client_fd == -1) {
    if (errno == EINTR) {
      cout << "accept() interrupted by a signal." << endl;
    }
    cerr << "accept() failed: " << strerror(errno) << endl;
    return -1; // Lose this connection
  }
  
  char temp[INET6_ADDRSTRLEN];
  inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr*) &client_addr),
    temp, sizeof(temp));
  cout << "Connection established with " << temp << endl;
  
  if (client_fd > max_fd_) {
    max_fd_ = client_fd;
  }
  
  FD_SET(client_fd, &readfds_); // will be monitored for read

  int ret = set_nonblocking_mode(client_fd);
  if (ret == -1) {
    cerr << "Error setting non-blocking mode with O_NONBLOCK: " << strerror(errno) << endl;
    return -1;
  }

  return 0;
}

int TcpServer::run_main_event_loop()
{
  char hostname[128];
  gethostname(hostname, sizeof hostname);
  cerr << "Server is listening on host " << hostname << " and port " << port_ << endl;

  FD_SET(server_fd_, &readfds_); // server fd for listening for incoming connections

  fd_set rfds; // read fd set to be passed to select()
  fd_set wfds; // write fd set to be passed to select

  int ret = -1;
  // Run an infinite loop to accept connections from client.
  for (;;) {
    FD_ZERO(&rfds);
    rfds = readfds_;
    FD_ZERO(&wfds);
    wfds = writefds_;

    // select() system call for notifications for read and write readiness
    ret = select(max_fd_ + 1, &rfds, &wfds, NULL, NULL);
    if (ret == -1) {
      if (errno == EINTR) {
        cout << "select() interrupted by a signal." << endl;
        continue;
      }
      cerr << "select() error: " << strerror(errno) << endl;
      return -1;
    }

    // We have at least one descriptor ready for I/O or accept(), find it/them
    int temp_max_fd = max_fd_; // copying since max_fd may be modified inside the loop
    for (int fd = 0; fd <= temp_max_fd; fd++) {
      if (FD_ISSET(fd, &rfds)) {
        if (fd == server_fd_) {
          // Handle new connection from client
          ret = on_new_connection();
          if (ret == -1) {
            cerr << "Error accepting a new connection from client." << endl;
            continue;
          }
        }
        else {
          // read I/O is possible on this socket, perform I/O
          if (timed_out(fd)) {
            cout << "Read timeout, closing connection!" << endl;
            delete_conn(fd, &readfds_);
          }
          else {
            on_read_ready(fd);
          }
        }
      } // end if FD_ISSET(fd, &rfds)

      if (FD_ISSET(fd, &wfds)) {
        if (timed_out(fd)) {
          cout << "Write timeout, closing connection!" << endl;
          delete_conn(fd, &writefds_);
        }
        else {
          on_write_ready(fd);
        }
      } // end if (FD_ISSET(fd, &wfds))
    } // end for loop over sockets in select()
  } // end infinite for loop for listening for clients

  return 0;
}
