#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <iostream>
#include <string>

// --------
#include "mem_manager.h"    // must be the final #include header
// --------

const char* const PORT = "7000"; // the port client will be connecting to
const int MAXDATASIZE = 1024 * 16; // max number of bytes we can get at once
const bool READ_SLOW = true;
const bool SEND_SLOW = true;

using namespace std;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int do_read(int socket, bool sleep_us)
{
  int read_size = 1024;
  char* buf = new char[read_size+1];
  int numbytes = -1;
  while ((numbytes = read(socket, buf, read_size)) > 0) {
    buf[numbytes] = '\0';
    cout << buf;
    usleep(sleep_us); // sleep to simulate slow read
  }
  delete [] buf;

  cout << endl;

  if (numbytes == 0) {
    cout << "ALl data received, disconnecting..." << endl;
  }
  else if (numbytes == -1) {
    cout << "Error reading from the socket: " << strerror(errno) << endl;
    return -1;
  }
  
  return 0;
}

void slow_send(int socket, char* fname)
{
  std::string filename = fname;
  filename += "\r\n";
  int numbytes = -1;
  int send_size = 1;
  int bytes_remaining = filename.length();
  int bytes_sent = 0;
  int bytes_to_send = bytes_remaining > send_size ? send_size : bytes_remaining;
  while ((numbytes = send(socket, filename.c_str()+bytes_sent, bytes_to_send, 0)) > 0) {
    cout << "Sent: " << *(filename.c_str()+bytes_sent) << endl;
    bytes_remaining -= numbytes;
    bytes_sent += numbytes;
    bytes_to_send = bytes_remaining > send_size ? send_size : bytes_remaining;
    sleep(1); // sleep for some time
  }

  if (numbytes == -1) {
    cerr << "Write error: " << strerror(errno) << endl;
  }
}

int main(int argc, char *argv[])
{
  int sockfd, numbytes;
  char buf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];

  if (argc != 4) {
    fprintf(stderr,"usage: client servername port filename\n");
    exit(1);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and connect to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
                         p->ai_protocol)) == -1) {
      perror("client: socket");
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("client: connect");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);

  printf("client: connecting to %s\n", s);

  freeaddrinfo(servinfo); // all done with this structure

  if (SEND_SLOW) {
    slow_send(sockfd, argv[3]);
  }
  else {
    std::string filename = argv[3];
    filename += "\r\n";
    if ((numbytes = send(sockfd, filename.c_str(), filename.length(), 0)) == -1) {
      perror("send error");
      exit(1);
    }
  }

  int sleep_us = READ_SLOW ? 500000 : 0;

  do_read(sockfd, sleep_us);
  
  close(sockfd);
  return 0;
}
