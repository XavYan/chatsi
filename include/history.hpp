#pragma once

#include <iostream>
#include <dirent.h>   //Para opendir()
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <ctime>
#include "Socket.hpp"

#define HISTORY_SIZE 1048576

class History {

private:
  int fd_;
  void* mmapArea_;
  void* pointer_;
  void* map_end_;
public:
  History(std::string username);
  ~History(void);

  void add_message (Message& message);
};
