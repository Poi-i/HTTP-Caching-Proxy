#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include "Socketer.h"
#include "Parser.h"
#include "Cache.h"

#define MAXDATASIZE 1024 * 1024 * 2

class Proxy
{
private:
    static Parser *parser;
    static Cache *cache;
    static pthread_mutex_t mutex;
    static std::ofstream log;
    int my_fd; // me as server to user;
public:
    // Proxy()
    // {
    //     parser = new Parser();
    //     cache = new Cache();
    // }
    void run_proxy();
    static void *handle_request(void *input); // recv, parse, resolve
    static Response *commute_webserver(Socketer *socketer, Request *request);
    static void resolve_GET(Request *request, Socketer *socketer);
    static void resolve_POST(Request *request, Socketer *socketer);
    static void resolve_CONNECT(Request *request, Socketer *socketer);
    static void resolve_other(Socketer *socketer);
    static bool check_bad_response(Response *response, Socketer *socketer);
    static void send_502(Socketer *socketer);
    static bool is_stale(Response *response);
    static void revalidate(Response **response, Request *request, Socketer *socketer);
    static std::string make_revalidate_req(Response *response, Request *request);
    void activate_server();
    std::pair<int, std::string> accept_user(); // return the user id and user ip
    static std::string get_currtime();

    ~Proxy()
    {
        close(my_fd);
    }
};