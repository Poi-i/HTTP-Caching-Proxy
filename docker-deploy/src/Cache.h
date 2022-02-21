#include <vector>
#include <map>
#include <string>
#include <queue>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "Response.h"

class Cache
{
private:
    std::queue<std::string> key_seq;
    std::map<std::string, Response *> data;
    static pthread_mutex_t mutex;
    size_t capacity;

public:
    Cache() : capacity(10){};
    Response *get_response(std::string &start_line); // return NULL if not find
    Response *get_response_unsafe(std::string &start_line);
    void update_cache(std::string &start_line, Response *response);
    void print_cache();
    virtual ~Cache()
    {
        std::map<std::string, Response *>::iterator it;
        for (it = data.begin(); it != data.end(); ++it)
        {
            delete (it->second);
        }
    }
};