#include "Socketer.h"

/*make a socket as client to web servers*/
void Socketer::activate_client(const char *host_name, const char *port_num)
{
    int status;
    struct addrinfo hostinfo;
    struct addrinfo *servinfo;
    std::memset(&hostinfo, 0, sizeof(hostinfo)); // make sure the struct is empty
    hostinfo.ai_family = AF_UNSPEC;              // don't care IPv4 or IPv6
    hostinfo.ai_socktype = SOCK_STREAM;          // TCP stream sockets
    hostinfo.ai_flags = AI_PASSIVE;              // fill in my IP for me
    // get all addr info into servinfo
    if ((status = getaddrinfo(host_name, port_num, &hostinfo, &servinfo)) != 0)
    {
        std::cerr << "getaddrinfo error:" << gai_strerror(status) << std::endl;
        throw "getaddrinfo failed in activate_client";
    }

    web_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (web_fd == -1)
    {
        std::cerr << "Fail to create socket!\n"
                  << std::endl;
        throw "create socket failed in activate_client";
    }
    status = connect(web_fd, servinfo->ai_addr, servinfo->ai_addrlen);
    if (status == -1)
    {
        std::cerr << "Error: cannot connect to socket" << std::endl;
        throw "connect failed in activate_client";
    }
    freeaddrinfo(servinfo);
}

void Socketer::close_sockets()
{
    close(web_fd);
    close(user_fd);
}