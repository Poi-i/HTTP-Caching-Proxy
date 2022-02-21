#ifndef RESPONSE_H
#define RESPONSE_H
#include <string>
enum Cache_type
{
    NOSTORE = 0,
    NOCACHE = 1,
    MAXAGE = 2
};
class Response
{
public:
    std::string cache_control; // no-store no-cache/must-revalidate max-age=31536000 private/public
    Cache_type cache_type;     // 1: no-store, 2: no-cache, 3: max-age
    std::string etag;          // for etag validation with "If-None-Match: " header
    std::string last_modified; // for last modified validation with "If-Modified-Since:" header
    std::string expires_date;  // expires date for max-age cache control
    std::string chunk;
    std::string origin;

    Response(const char raw[]) : origin(raw){};
    Response(std::string raw) : origin(raw){};
    void print_response()
    {
        std::cout << "---------- PRINTING RESPONSE ----------" << std::endl;
        std::cout << "* cache_control: <" << cache_control << ">" << std::endl;
        std::cout << "* cache_type: <" << cache_type << ">" << std::endl;
        std::cout << "* etag: <" << etag << ">" << std::endl;
        std::cout << "* last_modified: <" << last_modified << ">" << std::endl;
        std::cout << "* expires_date: <" << expires_date << ">" << std::endl;
        std::cout << "* origin: \n<" << origin << ">" << std::endl;
        std::cout << "* recv buf size: <" << origin.length() << ">" << std::endl;

        std::cout << "---------- END PRINTING RESPONSE ----------" << std::endl;
    }
};
#endif