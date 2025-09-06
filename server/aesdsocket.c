#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <syslog.h>
#include <stdbool.h>
#include <fcntl.h>

#define PORT "9000"
#define BACKLOG 1

bool g_sigterm = false;
bool g_sigint = false;
const char* g_filename = "/var/tmp/aesdsocketdata";

// void sigchld_handler(int s) {
//   (void)s;
//   int saved_errno = errno;
//   while(waitpid(-1, NULL, WNOHANG) > 0);
//   errno = saved_errno;
// }

void sigterm_handler(int s) {
  if(s == SIGTERM) {
    g_sigterm = true;
  } else if(s == SIGINT) {
    g_sigint = true;
  }
}

int setup_sigaction() {
  struct sigaction action;
  // memset(&action, 0, sizeof(action));
  // action.sa_handler = sigchld_handler;

  // sigemptyset(&action.sa_mask);
  // action.sa_flags = SA_RESTART | SA_NOCLDSTOP;

  // if(sigaction(SIGCHLD, &action, NULL) == -1) {
  //   syslog(LOG_ERR, "signal action failed for SIGCHLD: %m\n");
  //   return EXIT_FAILURE;
  // }

  memset(&action, 0, sizeof(action));
  action.sa_handler = sigterm_handler;

  sigemptyset(&action.sa_mask);
  if(sigaction(SIGTERM, &action, NULL) == -1) {
    syslog(LOG_ERR, "signal action failed for SIGTERM: %m\n");
    return EXIT_FAILURE;
  }

  if(sigaction(SIGINT, &action, NULL) == -1) {
    syslog(LOG_ERR, "signal action failed for SIGINT: %m\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

void* get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  } else {
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
  }
}

int server_socket_bind() {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *addr_info;
  int rv;
  if((rv = getaddrinfo(NULL, PORT, &hints, &addr_info)) != 0) {
    syslog(LOG_ERR, "getaddrinfo: %s, %m\n", gai_strerror(rv));
    return -1;
  }

  struct addrinfo *p_addr_info;
  int socket_fd;
  for(p_addr_info = addr_info; p_addr_info != NULL; p_addr_info = p_addr_info->ai_next) {
    if((socket_fd = socket(p_addr_info->ai_family, p_addr_info->ai_socktype, p_addr_info->ai_protocol)) == -1) {
      syslog(LOG_WARNING, "socket: %m\n");
      continue;
    }

    int val = 1;
    if(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int)) == -1) {
      syslog(LOG_ERR, "setsockopt: %m\n");
      close(socket_fd);
      p_addr_info = NULL;
      break;
    }

    if(bind(socket_fd, p_addr_info->ai_addr, p_addr_info->ai_addrlen) == -1) {
      close(socket_fd);
      syslog(LOG_WARNING, "bind: %m\n");
      continue;
    }

    break;
  }

  freeaddrinfo(addr_info);

  if(p_addr_info == NULL)  {
    syslog(LOG_ERR, "failed to bind\n");
    return -1;
  }

  if(listen(socket_fd, BACKLOG) == -1) {
    syslog(LOG_ERR, "failed to listen: %m\n");
    close(socket_fd);
    return -1;
  }

  return socket_fd;
}

int read_message_from_file(char* message, size_t message_size, int pos) {
  if(message_size == 0 || message == NULL) {
    syslog(LOG_WARNING, "0 buffer length provided\n");
    return -1;
  }

  FILE* fd = fopen(g_filename, "r");
  if(fd == NULL) {
    syslog(LOG_ERR, "Error openning file %s: %m\n", g_filename);
    return -1;
  }

  if(fseek(fd, pos, SEEK_SET) != 0) {
    syslog(LOG_ERR, "Error seek file %s: %m\n", g_filename);
    fclose(fd);
    return -1;
  }

  size_t read = fread(message, 1, message_size, fd);
  if(read != message_size && !feof(fd)) {
    syslog(LOG_ERR, "Error reading file %s: %m\n", g_filename);
    fclose(fd);
    return -1;
  }

  fclose(fd);
  return (int)read;
}

int write_message_to_file(const char* message, size_t message_size) {
  if(message_size == 0) {
    return EXIT_SUCCESS;
  }

  syslog(LOG_DEBUG, "Writing message %ld bytes to %s\n", message_size, g_filename);

  FILE* fd = fopen(g_filename, "a");
  if(fd == NULL) {
    syslog(LOG_ERR, "Error openning file %s: %m\n", g_filename);
    return EXIT_FAILURE;
  }

  size_t wr = fwrite(message, 1, message_size, fd);
  if(wr != message_size) {
    syslog(
      LOG_ERR, "Error writting to file %s message %ld bytes while only %ld bytes written: %m\n", g_filename, message_size, wr
    );
    fclose(fd);
    return EXIT_FAILURE;
  }

  fclose(fd);
  return EXIT_SUCCESS;
}

int sendall(int socket, const char* buf, int len) {
  int total = 0;
  int bytesleft = len;
  int n;

  while(total < len) {
    n = send(socket, buf+total, bytesleft, 0);
    if(n == -1) {
      syslog(LOG_ERR, "sendall failed to send %d bytes: %m\n", bytesleft);
      break;
    }

    total += n;
    bytesleft -= n;
  }

  syslog(LOG_DEBUG, "sendall %d bytes complete\n", total);
  return n == -1 ? -1 : 0;
}

int daemonization(int socket_fd) {
  pid_t pid = fork();
  if(pid > 0) {
    return pid;
  } else if(pid < 0) {
    syslog(LOG_ERR, "daemon fork failed: %m\n");
    return pid;
  }

  if(setsid() == -1) {
    syslog(LOG_ERR, "daemon setsid failed: %m\n");
    return -1;
  }

  if(chdir("/") == -1) {
    syslog(LOG_ERR, "daemon setsid failed: %m\n");
    return -1;
  }

  long maxfd = sysconf(_SC_OPEN_MAX);
  if(maxfd < 0) {
    maxfd = 1024;
  }

  for(long fd_cnt = 0; fd_cnt < maxfd; fd_cnt++) {
    if(socket_fd != (int)fd_cnt) {
      close((int)fd_cnt);
    }
  }

  int fd = open("/dev/null", O_RDWR);
  if(fd < 0) {
    syslog(LOG_ERR, "Error openning file /dev/null: %m\n");
    return -1;
  }

  if(dup2(fd, STDIN_FILENO) < 0) {
    syslog(LOG_ERR, "dup2 stdin failed: %m");
    close(fd);
    return -1;
  }

  if(dup2(fd, STDOUT_FILENO) < 0) {
    syslog(LOG_ERR, "dup2 stdout failed: %m");
    close(fd);
    return -1;
  }

  if(dup2(fd, STDERR_FILENO) < 0) {
    syslog(LOG_ERR, "dup2 stderr failed: %m");
    close(fd);
    return -1;
  }

  if(fd > 2) {
    close(fd);
  }

  return pid;
}

int run_server(int socket_fd) {
  while(!(g_sigint || g_sigterm)) {
    struct sockaddr_storage client_addr;
    socklen_t sin_size = sizeof(client_addr);

    int connection_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &sin_size);
    if(connection_fd == -1) {
      syslog(LOG_WARNING, "accept: %m\n");
      continue;
    }

    char client_addr_str[INET6_ADDRSTRLEN];
    inet_ntop(
      client_addr.ss_family, get_in_addr((struct sockaddr*)&client_addr), client_addr_str, sizeof(client_addr_str)
    );

    syslog(LOG_DEBUG, "Accepted connection from %s\n", client_addr_str);

    char* msg = (char*)malloc(1024);
    if(msg == NULL) {
      syslog(LOG_ERR, "no memory: %m\n");
      close(connection_fd);
      return EXIT_FAILURE;
    }

    ssize_t recv_len;
    while((recv_len = recv(connection_fd, msg, 1024, 0)) > 0) {
      char* eol = memchr(msg, '\n', recv_len);
      if(eol) {
        size_t first_package_len = eol - msg + 1;
        syslog(LOG_DEBUG, "Found package delimiter at %ld\n", first_package_len);

        if(write_message_to_file(msg, first_package_len) != EXIT_SUCCESS) {
          recv_len = -1;
          break;
        }


        size_t second_package_len = recv_len - first_package_len;
        char* second_msg = malloc(second_package_len);
        if(msg == NULL) {
          syslog(LOG_ERR, "no memory: %m\n");
          close(connection_fd);
          free(msg);
          return EXIT_FAILURE;
        }

        memcpy(second_msg, eol+1, second_package_len);

        int size;
        int pos = 0;
        while((size = read_message_from_file(msg, 1024, pos)) >= 0) {
          if(sendall(connection_fd, msg, size) == -1) {
            syslog(LOG_ERR, "failed to send %d message to client %s\n", size, client_addr_str);
            close(connection_fd);
            free(msg);
            free(second_msg);
            return EXIT_FAILURE;
          }

          if(size != 1024) {
            syslog(LOG_DEBUG, "End of file %d\n", size + pos);
            break;
          }

          pos += size;
        }

        if(size < 0) {
          close(connection_fd);
          free(msg);
          free(second_msg);
          return EXIT_FAILURE;
        }

        if(write_message_to_file(second_msg, second_package_len) != EXIT_SUCCESS) {
          recv_len = -1;
          free(second_msg);
          break;
        }
        free(second_msg);
      } else if(write_message_to_file(msg, recv_len) != EXIT_SUCCESS) {
          recv_len = -1;
          break;
      }
    }

    if(recv_len < 0) {
      syslog(LOG_ERR, "recv failed: %m\n");
      close(connection_fd);
      free(msg);
      return EXIT_FAILURE;
    }

    syslog(LOG_DEBUG, "Closed connection from %s\n", client_addr_str);
    close(connection_fd);
    free(msg);
  }

  return EXIT_SUCCESS;
}

int start_server(bool daemon) {
  syslog(LOG_DEBUG, "Start server%s\n", daemon ? " (daemon)" : "");

  int socket_fd = server_socket_bind();
  if(socket_fd == -1) {
    return EXIT_FAILURE;
  }

  if(daemon) {
    pid_t pid = daemonization(socket_fd);
    if(pid > 0) {
      syslog(LOG_DEBUG, "Shutdown daemon parent process. Child process pid %d\n", pid);
      return EXIT_SUCCESS;
    } else if(pid < 0) {
      close(socket_fd);
      return EXIT_FAILURE;
    }

    syslog(LOG_DEBUG, "Started daemon in child process\n");
  }

  int rv;
  if((rv = setup_sigaction()) != EXIT_SUCCESS) {
    return rv;
  }

  syslog(LOG_DEBUG, "Waiting for connections...\n");

  if(run_server(socket_fd) != EXIT_SUCCESS) {
    close(socket_fd);
    return EXIT_FAILURE;
  }

  if(unlink(g_filename) != 0) {
    syslog(LOG_WARNING, "%s delete failed: %m", g_filename);
  }

  close(socket_fd);
  syslog(LOG_DEBUG, "Caught signal, exiting\n");
  return EXIT_SUCCESS;
}

int main(int argc, char** argv) {
  if(argc == 1) {
    exit(start_server(false));
  } else if(argc == 2 && strcmp(argv[1], "-d") == 0) {
    exit(start_server(true));
  } else {
    syslog(LOG_ERR, "Invalid usage\n");
    return EXIT_FAILURE;
  }
}
