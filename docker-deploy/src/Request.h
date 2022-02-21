#ifndef REQUEST_H
#define REQUEST_H
#include <string>
class Request
{
public:
    std::string method;
    std::string start_line;
    std::string host_name;
    std::string origin;

    Request(char raw[]) : origin(raw) {}
    Request(std::string raw) : origin(raw) {}
};
#endif