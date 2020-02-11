#include "../include/Socket.hpp"

Message create_message (const std::string text, const int desc, const std::string username, const std::string time, const uint32_t ip, const in_port_t port, const int name, const int command) {
  Message message{};
  message.with_name = name;
  message.command = command;
  message.ip = ip;
  message.port = port;
  message.desc = desc;
  strcpy(message.time, time.c_str());
  strcpy(message.username, username.c_str());
  strcpy(message.text, text.c_str());
  return message;
}

std::string getIPAddress (void) {
    std::string ipAddress="Unable to get IP Address";
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    int success = 0;
    // retrieve the current interfaces - returns 0 on success
    success = getifaddrs(&interfaces);
    if (success == 0) {
        // Loop through linked list of interfaces
        temp_addr = interfaces;
        while(temp_addr != NULL) {
            if(temp_addr->ifa_addr->sa_family == AF_INET) {
                // Check if interface is en0 which is the wifi connection on the iPhone
                if(strcmp(temp_addr->ifa_name, "en0")){
                    ipAddress=inet_ntoa(((struct sockaddr_in*)temp_addr->ifa_addr)->sin_addr);
                }
            }
            temp_addr = temp_addr->ifa_next;
        }
    }
    // Free memory
    freeifaddrs(interfaces);
    return ipAddress;
}

sockaddr_in make_ip_address(const std::string& ip, int port) {
  sockaddr_in local_address{};

  local_address.sin_family = AF_INET;
  if (ip.empty()) {
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
  } else {
    int e_inet_aton = inet_aton(ip.data(),&local_address.sin_addr);
    if (e_inet_aton == 0) {
      throw std::invalid_argument("La dirección indicada es inválida.");
    }
  }
  local_address.sin_port = htons(port);

  return local_address;
}

//AF_INET porque queremos conectarnos a internet; SOCK_DGRAM porque no queremos solicitar conexion, sino mandarsela porque si (UDP)
Socket::Socket (sockaddr_in& addr) {
  fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_ < 0) {
    throw std::system_error(errno, std::system_category(), "no se pudo crear el socket.");
  }

  int result = bind(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
  if (result < 0) {
    throw std::system_error(errno, std::system_category(), "no se pudo establecer conexión con el socket.");
  }

  //Reasignamos addr los valores oficiales (en caso de que se hayan asignado IP y puerto no controlados)
  socklen_t len = sizeof(addr);
  result = getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &len);
  if (result < 0) {
    throw std::system_error(errno, std::system_category(), "error al reasignar el socket.");
  }
}

Socket::~Socket (void) {
  close(fd_);
}

void Socket::send_to(const Message& message, const sockaddr_in& address) {
  int result = sendto(fd_, &message, sizeof(message), 0,
    reinterpret_cast<const sockaddr*>(&address), sizeof(address));
  if (result < 0) {
    throw std::system_error(errno, std::system_category(), "no se pudo enviar el mensaje.");
  }
}

Message Socket::receive_from(sockaddr_in& address) {
  Message message;
  socklen_t src_len = sizeof(address);
  int result = recvfrom(fd_, &message, sizeof(message), 0,
  reinterpret_cast<sockaddr*>(&address), &src_len);
  if (result < 0) {
    throw std::system_error(errno, std::system_category());
  }
  return message;
}
