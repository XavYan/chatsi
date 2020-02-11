#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <sys/mman.h>

/*Los comandos estaran formados por / y no mas de cinco letras consecutivas que indican el comando a ejecutar*/
std::vector<std::string> word_split (const std::string& line) {

  std::vector<std::string> str;
  std::stringstream ss(line);
  while(!ss.eof()) {
    std::string word;
    ss >> word;
    str.push_back(word);
  }
  return str;
}

std::string exec (const std::vector<std::string>& v) {

  std::string result = "";

  std::array<int, 2> fds;
  int error_pipe = pipe(fds.data()); //fds[1] para entrada y fds[0] para salida
  if (error_pipe < 0) {
      result += "/run: Fallo en el pipe(): " + std::string(std::strerror(errno)) + "\n";
      result += "/run: No se pudo ejecutar la operacion.\n";
      return result;
  }

  pid_t pid = fork();
  if (pid == 0) { //Proceso hijo
    //Creamos el array de cadenas de caracteres
    char** aux = new char* [v.size()+1];
    for (int i = 0; i < v.size(); i++) {
      aux[i] = new char [v[i].size()+1];
      strcpy(aux[i],v[i].c_str());
      aux[i][v[i].size()] = '\0';
    }
    aux[v.size()] = nullptr;

    //Redireccionamos la E/S
    dup2(fds[1], STDOUT_FILENO);

    //Ejecutamos la operacion
    close(fds[0]);
    int error_execvp = execvp(v[0].c_str(), aux);
    if (error_execvp < 0) {  throw std::system_error(errno, std::system_category(), "Fallo al ejecutar el execvp().\n"); }
    close(fds[1]);
    exit(0);

  } else if (pid > 0) { //Proceso padre
    close(fds[1]);
    char buffer[255];
    int len = 0;
    while ((len = read(fds[0], buffer, 255)) > 0) {
      result += std::string(buffer, len);
    }
    wait(NULL);
    close(fds[0]);
  } else { //Error
    throw std::system_error(errno, std::system_category(), "Fallo en el fork().\n");
  }
  return result;
}
