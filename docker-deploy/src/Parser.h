#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>
#include <string>
#include <iostream>
#include <iostream>
#include <algorithm>
#include "Request.h"
#include "Response.h"

class Parser
{
public:
    Request *parse_request(char raw[]);
    Request *parse_request(std::string raw);
    Response *parse_response(const char raw[]);
    Cache_type get_cache_type(std::string &cache_control);
    std::string parse_status(const std::string &origin); // parse response status, 200/304/etc
    std::string parse_by_deli(const std::string &origin, char deli);
    std::string parse_after_deli(const std::string &origin, char deli);
    std::string parse_by_word(std::string origin, std::string word);
    std::string to_lower(std::string &s);
    std::string time_to_string();
    std::string get_expires_date(std::string &origin);
    int parse_content_length(const std::string &origin);
    Response *parse_response(const std::string raw);
    std::pair<std::string, std::string> get_connect_host(const std::string &start_line); // get host name:port of CONNECT
    int parse_max_age(std::string &origin);
};