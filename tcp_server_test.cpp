#include "tcp_server.h"

#include <iostream>
#include <stdlib.h>

int main(int argc, char* argv[])
{
  // Create an instance of the server
  TcpServer server = TcpServer::create_server();
  
  // Prepare the server for listening for client connections
  char* port_str = (char*) "2013"; // default port 2013
  if (argc == 2) {
    port_str = (char*) argv[1];
  }
  unsigned short port = (unsigned short) atoi(port_str);

  int server_fd = server.prepare_server_socket(port, SOCK_STREAM);
  if (server_fd  == -1) {
    cout << "Server preparation failed. Exiting application..." << endl;
    exit(1);
  }
  
  // Run the main loop
  // This method run indefinitely
  int ret = server.run_main_event_loop();
  if (ret != 0) {
    cout << "Error running the server main event loop, exiting the server process..." << endl;
  }

  // At this point we are exiting the server
  return 0;
}
