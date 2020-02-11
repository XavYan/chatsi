#include "../include/history.hpp"

History::History(std::string username) : fd_(0), mmapArea_(), pointer_(0) {
  //Establecemos como 'home' el directorio donde guardar los archivos del programa
  std::string home = std::string(std::getenv("HOME"))+"/.chatsi";
  //Si el directorio no existe se debe crear
  DIR* dir = opendir(home.c_str());
  if (dir != NULL) { //El directorio existe
    closedir(dir);
  } else if (ENOENT == errno) { //El directorio no existe
    int error_mkdir = mkdir(home.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (error_mkdir < 0) {
      throw std::system_error(errno, std::system_category(), "mkdir(): no se pudo crear la carpeta '.chatsi'\n");
    }
  }
  std::string path = home+"/"+username+".log";
  fd_ = open (path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH);
  if (fd_ < 0) {
    throw std::system_error(errno, std::system_category(), "no se pudo abrir el archivo del historial.\n");
  }
  //Truncamos el archivo con ftruncate()
  int error_ftruncate = ftruncate(fd_, HISTORY_SIZE);
  if (error_ftruncate < 0) {
    throw std::system_error(errno, std::system_category(), "ftruncate(): Error al truncar el archivo.\n");
  }

  //Mapeamos en memorira con mmap()
  int error_lockf = lockf(fd_, F_TLOCK, 0); //Bloqueamos el archivo
  if (error_lockf < 0) {
    if (errno == EACCES || errno == EAGAIN) throw std::system_error(errno, std::system_category(), "El nombre de usuario ya se esta usando en este equipo.\n");
    throw std::system_error(errno, std::system_category(), "Fallo al bloquear el archivo.\n");
  }
  mmapArea_ = mmap(NULL, HISTORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  pointer_ = mmapArea_;
  map_end_ = (uint8_t*)mmapArea_ + HISTORY_SIZE;
  if (mmapArea_ == MAP_FAILED) {
    throw std::system_error(errno, std::system_category(), "Fallo al mapear en memoria.\n");
  }
}

History::~History (void) {


  int error_mmap = munmap(mmapArea_, HISTORY_SIZE);
  if (error_mmap < 0) {
    std::cerr << "Error al desmapear el archivo.\n";
  }

  int error_lockf = lockf(fd_, F_ULOCK, 0);
  if (error_lockf < 0) {
    std::cerr << "Error al desbloquear el archivo.\n";
  }
  close(fd_);
}

void History::add_message (Message& message) {
  std::string line = "[" + std::string(message.time) + "] " + std::string(message.username) + " dijo: " + std::string(message.text) + "\n";
  if ((uint8_t*)pointer_+line.size()+1 >= map_end_) {
    pointer_ = mmapArea_;
    std::cout << "Ha llegado al final del archivo\n";
  }
  memcpy(pointer_, line.c_str(), line.size()+1);
  msync(pointer_, line.size()+1, MS_SYNC);
  pointer_ = (uint8_t*)pointer_ + line.size();
}
