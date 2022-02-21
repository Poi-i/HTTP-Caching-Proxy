#include <cstdlib>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "Proxy.h"

void run_daemon(void)
{
  pid_t pid;
  if (pid = fork()) // fork()
  {
    exit(EXIT_SUCCESS); // exit if in parent process
  }
  else if (pid < 0)
  {
    exit(EXIT_FAILURE); // fork error
  }

  // child process as group process leader and session leader
  pid_t sid = setsid(); // dissociate from controlling tty
  if (sid < 0)
  {
    exit(EXIT_FAILURE); // setsid error
  }
  // fork() again, make child child process no longer session leader
  if (pid = fork())
  {
    exit(EXIT_SUCCESS);
  }
  else if (pid < 0)
  {
    exit(EXIT_FAILURE);
  }
  // change working directory to "/"
  if (chdir("/") < 0)
  {
    exit(EXIT_FAILURE);
  }

  // clear umask
  umask(0);
  // close stdin/stderr/stdout, open them to /dev/null
  int fd = open("/dev/null", O_RDWR);
  if (fd < 0)
  {
    exit(EXIT_FAILURE);
  }
  if (dup2(fd, STDIN_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0 || dup2(fd, STDOUT_FILENO < 0))
  {
    exit(EXIT_FAILURE);
  }
  close(STDIN_FILENO);
  close(STDERR_FILENO);
  close(STDOUT_FILENO);
  if (fd > STDERR_FILENO && close(fd) < 0)
  {
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char **argv)
{
  if (argc != 1)
  {
    std::cerr << "Usage: /server" << std::endl;
  }

  // run_daemon();
  Proxy *proxy = new Proxy();
  proxy->run_proxy();
  delete (proxy);
  return EXIT_SUCCESS;
}