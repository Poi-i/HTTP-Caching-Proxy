#include <cstdio>
#include <cstdlib>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#define BACKLOG 1024
#define BUFSIZE 1024

class Socketer
{
public:
    int web_fd;  // me as client to web server
    int user_fd; // user's fd
    int user_id;
    std::string user_ip;

    void activate_client(const char *host_name, const char *port_num);
    void close_sockets();

    virtual ~Socketer()
    {
        close_sockets();
    }
};