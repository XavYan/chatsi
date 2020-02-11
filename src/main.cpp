#include <thread>
#include <pthread.h>
#include <csignal>
#include <cxxabi.h>
#include <atomic>
#include <set>
#include <getopt.h>
#include <cstdlib>    // para std::getenv() y std::setenv()
#include <netdb.h>    //Para gethostbyname()
#include <mutex>
#include "../include/Socket.hpp"
#include "../include/history.hpp"
#include "../include/commands.hpp" //Para los comandos adicionales

#define PORT_BY_DEFAULT 8000

//Declaracion de variables
bool help_mode = false;
bool server_mode = true;
bool client_mode = false;
std::string username = std::getenv("USER");
std::string line;
sockaddr_in local_address;
sockaddr_in dest_address;
std::mutex semaphore;
std::mutex history_semaphore;
std::set<std::pair<uint32_t, in_port_t> > destination_adresses;

std::atomic<bool> quit (false);

void int_signal_handler (int signum) {
  quit = true;
}

void request_cancellation (std::thread& thread) {
    int err = pthread_cancel(thread.native_handle());
    if (err != 0) {
      throw std::system_error(err, std::system_category(), "Problema al cancelar los hilos.");
    }
}

Message create_msg (const std::string text, const int desc = 0, const  int name = 1, const int command = 0) {
  std::time_t time = std::time(nullptr);
  std::string str_time(std::asctime(std::localtime(&time)));
  str_time = str_time.substr(0, str_time.length()-1);
  Message message = create_message(text, desc, username, str_time, local_address.sin_addr.s_addr, local_address.sin_port, name, command);
  return message;
}

void thread_send (Socket& socket, History& history, std::exception_ptr& eptr) {
  //Bloqueamos las señales SIGTERM, SIGINT y SIGHUP
  sigset_t set;
  sigaddset(&set,SIGINT);
  sigaddset(&set,SIGTERM);
  sigaddset(&set,SIGHUP);
  pthread_sigmask(SIG_BLOCK, &set, nullptr);

  try {
    while(line != "/quit") {
      Message message;
      std::getline(std::cin, line);
      int command_temp = 0;
      if (line != "/quit") {
        if (line.substr(0, 4) == "/run") {
          command_temp = 1;
          if (line.length() <= 5) { line += "\nError: /run requiere que se indique un comando a ejecutar."; }
          else { line += "\n" + exec(word_split(line.substr(5))); }
        }
        message = create_msg(line, 0, 1, command_temp);
        std::cout << "\x1b[1A\x1b[2K";
        if (server_mode) {
          {
            std::lock_guard<std::mutex> lock(history_semaphore);
            history.add_message(message);
          }
          for (std::pair<uint32_t, in_port_t> user : destination_adresses) {
            sockaddr_in address {};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = std::get<0>(user);
            address.sin_port = std::get<1>(user);
            socket.send_to(message, address);
          }
          std::cout << "[" << message.time << "] ";
          std::cout << (message.with_name == 1 ? username + ": " : "");
          if (message.command == 1) std::cout << "\n";
          std::cout << message.text << "\n";
        } else if (client_mode) {
          socket.send_to(message, dest_address);
        } else {
          std::cout << "Chatsi: Ha ocurrido un error al enviar el mensaje.\n";
          break;
        }
      }
    }
  } catch (abi::__forced_unwind&) {
    throw;
  } catch (...) {
    eptr = std::current_exception();
  }
  quit = true;
}

void thread_recv (Socket& socket, History& history, std::exception_ptr& eptr) {
  //Bloqueamos las señales SIGTERM, SIGINT y SIGHUP
  sigset_t set;
  sigaddset(&set,SIGINT);
  sigaddset(&set,SIGTERM);
  sigaddset(&set,SIGHUP);
  pthread_sigmask(SIG_BLOCK, &set, nullptr);

  Message message {};

  try {
    while (true) {
      message = socket.receive_from(dest_address);
      { std::lock_guard<std::mutex> lock(history_semaphore);
        history.add_message(message);
      }
      if (server_mode) { //Si estas en modo servidor, aquellos que envien por primera vez el mensaje se deben insertar en el conjunto
        std::pair<uint32_t, in_port_t> user;
        std::get<0>(user) = message.ip;
        std::get<1>(user) = message.port;
        { std::lock_guard<std::mutex> lock(semaphore);
          if ((message.ip != local_address.sin_addr.s_addr || message.port != local_address.sin_port) && (destination_adresses.find(user) == destination_adresses.end())) {
            destination_adresses.insert(user);
          }
        }
        //Enviamos el mensaje a cada usuario
        for (std::pair<uint32_t, in_port_t> pair : destination_adresses) {
          sockaddr_in address {};
          address.sin_family = AF_INET;
          address.sin_addr.s_addr = std::get<0>(pair);
          address.sin_port = std::get<1>(pair);
          socket.send_to(message, address);
        }
        if (message.desc == 1) destination_adresses.erase(user);
      }
      //Imprimiendo el mensaje
      std::cout << "[" << message.time << "] ";
      if (message.with_name == 1) {
        std::cout << message.username << ": ";
        if (message.command == 1) std::cout << "\n";
        std::cout << message.text << "\n";
      } else {
        std::cout << message.text << "\n";
      }
    }
  } catch (abi::__forced_unwind&) {
    throw;
  } catch (...) {
    eptr = std::current_exception();
  }
  quit = true;
}

void usage (std::ostream& os) {
  os << "Uso: [-h/--help] [-c ip/--client ip] [-s/--server] [-p port/--port port] [-u user/--username user]\n";
}

//------------------------------- MAIN -------------------------------//

int main (int argc, char* argv[]) {

  //Manejando señales
  std::signal(SIGINT, &int_signal_handler); //Señal al pulsar Ctrl-C
  std::signal(SIGTERM, &int_signal_handler); //Señal al cerrar la terminal
  std::signal(SIGHUP, &int_signal_handler); //Señal al apagar PC

  try {
    std::exception_ptr eptr1 {};
    std::exception_ptr eptr2 {};

    int port_option = 0;
    std::string client_option = "";

    //Declaramos la estructura de long_option
    static struct option long_options[] = {
      {"help",      no_argument,        0,  'h'},
      {"server",    no_argument,        0,  's'},
      {"client",    required_argument,  0,  'c'},
      {"port",      required_argument,  0,  'p'},
      {"username",  required_argument,  0,  'u'},
      {0, 0, 0, 0}
    };

    int c; //Caracter que se va leyendo a traves de las opciones indicadas en argv
    while ((c = getopt_long(argc, argv, "hsc:p:u:", long_options, nullptr)) != -1) {
      switch (c) {
        case 'h': //Mostrar ayuda INCOMPATIBLE CON LOS DEMAS ARGUMENTOS
          help_mode = true;
          break;
        case 's': //Modo servidor INCOMPATIBLE CON MODO CLIENTE ('c') Y CON AYUDA ('h'). SI NO SE ESPECIFICA PUERTO ('p'), SE LE ASIGNA UNO ALEATORIO
          server_mode = true;
          break;
        case 'c': //Modo cliente INCOMPATIBLE CON MODO SERVIDOR ('s') Y CON AYUDA ('h'). NECESARIO PUERTO ('p') Y ARGUMENTO.
          if (std::string(optarg) != "l") {
            client_option = std::string(optarg);
          }
          client_mode = true;
          server_mode = false;
          break;
        case 'p': //Indica el puerto NECESARIO ARGUMENTO. INCOMPATIBLE CON AYUDA ('h'). SI NO SE INDICA MODO, POR DEFECTO MODO SERVIDOR.
          port_option = stoi(std::string(optarg));
          break;
        case 'u':
          username = std::string(optarg);
          break;
        case '?':
          // No hacemos nada porque optarg se encarga de mostrar un mensaje de error
          return 1;
          break;
        default:
          std::cerr << "?? La funcion getopt devolvio codigo de error " << c << '\n';
          return 1;
      }
    }

    if (help_mode) {
      usage(std::cout);
      std::cout << "\n";
      std::cout << "  -h / --help:\n";
      std::cout << "\tMuestra la ayuda de uso, tal y como ahora esta.\n\tEste comando prioriza sobre todos los demas.\n";
      std::cout << "  -c / --client:\n";
      std::cout << "\tInicia el programa en modo cliente.\n\tSe debe especificar como argumento la IP a la que se quiere conectar o usar \'l\' como argumento si se desea una conexion local.";
      std::cout << "\n\tSi no se especifica puerto de escucha (con la opcion 'p') se usara por defecto el puerto 8000\n";
      std::cout << "  -s / --server:\n";
      std::cout << "\tInicia el programa en modo servidor. Si no se especifica puerto con la opcion '-p', el sistema operativo le asignara un puerto\n\tlibre y se le informara del puerto de escucha asignado.\n";
      std::cout << "  -p / --port:\n";
      std::cout << "\tCon este comando podemos indicar un puerto de conexion.\n\tSi no se especifica ninguna opcion de modo ('-s' o '-c') el programa por defecto iniciara en modo servidor.\n";
      std::cout << "  -u / --username:\n";
      std::cout << "\tCon este comando podemos indicar un nombre de usuario.\n\tSi no se especifica este comando, por defecto se usara el nombre de usuario del sistema.\n";
      std::cout << "\n";
      return 0;
    }

    History history(username);

    if (server_mode) {
      std::cout << "Iniciado en modo servidor.\n";
      if (client_mode) {
        std::cerr << "El modo cliente ('-c') y el modo servidor ('-s') no pueden ser usados al mismo tiempo. Use \"./chatsi -h\" o \"./chatsi --help\" para conocer mas acerca del funcionamiento del programa.\n";
        usage(std::cerr);
        return 2;
      }
      local_address = make_ip_address(getIPAddress(), port_option);
      dest_address = local_address;
    }

    if (client_mode) {
      std::cout << "Iniciado en modo cliente.\n";
      if (port_option == 0) { //Le asignamos el puerto por defecto si el usuario no ha asignado ningun puerto
        port_option = PORT_BY_DEFAULT;
      }
      local_address = make_ip_address(getIPAddress(), 0);
      dest_address = make_ip_address(client_option, port_option);
    }


    Socket socket(local_address); //Creando el socket local

    if (server_mode) { //Si no especifica puerto con '-p' se entiende que el puerto se lo asigna el programa
      std::cout << "Asignado puerto " << ntohs(local_address.sin_port) << "\n";
    }

    if (client_mode) {
      Message client_connection = create_msg(std::string(username + " se ha conectado a la sesion."), 0, 0);
      socket.send_to(client_connection, dest_address); //Mandamos un mensaje para avisar de que se ha conectado un usuario
    }

    std::thread send (&thread_send,std::ref(socket), std::ref(history), std::ref(eptr1));
    std::thread recv (&thread_recv,std::ref(socket), std::ref(history), std::ref(eptr2));

    while(!quit); //Esperar a que algun hilo termine

    //Terminamos todos los hilos
    request_cancellation(send);
    request_cancellation(recv);

    send.join();
    recv.join();

    if (eptr1) {
      std::rethrow_exception(eptr1);
    }
    if (eptr2) {
      std::rethrow_exception(eptr2);
    }

    if (client_mode) {
      Message desconnection = create_msg(std::string(username + " se ha desconectado a la sesion."), 1, 0);
      socket.send_to(desconnection, dest_address); //Mandamos un mensaje para avisar de que se ha desconectado un usuario
      std::cout << "\nTe has desconectado de la sesión.\n";
    }
  } //EXCEPCIONES
  catch (std::bad_alloc& e) {
    std::cerr << "Chatsi: memoria insuficiente.\n";
    return 1;
  }
  catch (std::system_error& e) {
    std::cerr << "Chatsi: " << e.what() << "\n";
    return 2;
  }
  catch(std::invalid_argument& e) {
    std::cerr << "Chatsi: " << e.what() << "\n";
    return 3;
  }
  catch(...) {
    std::cerr << "Chatsi: error desconocido.\n";
    return -1;
  }
  return 0;
}
