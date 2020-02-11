#pragma once

#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <cstring> //Para std::strerror()
#include <ctime>

//Librerias para errores
#include <exception>
#include <cerrno> //Para errno
#include <system_error>

struct Message {
  int with_name;
  int command;
  int desc;
  char username[1024];
  char time[30];
  uint32_t ip;
  in_port_t port;
  char text[32768];
};

Message create_message (const std::string text, const int desc, const std::string username, const std::string time, const uint32_t ip, const in_port_t port, const  int name = 1, const int command = 0);

std::string getIPAddress (void);

sockaddr_in make_ip_address(const std::string& ip, int port);

class Socket {
private:
  int fd_;

public:

  Socket(sockaddr_in& addr);
  ~Socket();

  void send_to(const Message& message,const sockaddr_in& address);
  Message receive_from(sockaddr_in& address);
};
