#include "Parser.h"

Request *Parser::parse_request(char raw[])
{
    Request *request = new Request(raw);
    request->method = parse_by_deli(request->origin, ' ');
    request->start_line = parse_by_deli(request->origin, '\r');
    request->host_name = parse_by_word(request->origin, "Host: ");
    return request;
}
Request *Parser::parse_request(std::string raw)
{
    Request *request = new Request(raw);
    request->method = parse_by_deli(request->origin, ' ');
    request->start_line = parse_by_deli(request->origin, '\r');
    request->host_name = parse_by_word(request->origin, "Host: ");
    return request;
}
Cache_type Parser::get_cache_type(std::string &cache_control)
{
    if (cache_control.find("no-store") != std::string::npos || cache_control.find("private") != std::string::npos)
    {
        return NOSTORE;
    }
    if (cache_control.find("no-cache") != std::string::npos || cache_control.find("must-revalidate") != std::string::npos)
    {
        return NOCACHE; // 要先判断一下这个，因为 must-revalidate 和 max-age 都出现的话，需要遵守 must-revalidate
    }
    if (cache_control.find("max-age") != std::string::npos)
    {
        return MAXAGE;
    }
    return NOCACHE; // set as NOCACHE in other case
}
Response *Parser::parse_response(const char raw[])
{
    Response *response = new Response(raw);
    response->expires_date = get_expires_date(response->origin);
    response->cache_control = parse_by_word(response->origin, "Cache-Control: ");
    response->cache_type = get_cache_type(response->cache_control);
    response->last_modified = parse_by_word(response->origin, "Last-Modified: ");
    response->etag = parse_by_word(response->origin, "ETag: ");
    response->chunk = parse_by_word(response->origin, "Transfer-Encoding: ");
    // update cache type NOSTORE if there is no etag or last modified, no matter what cache_control contains
    if (response->etag == "" || response->last_modified == "")
    {
        response->cache_type = NOSTORE;
        response->cache_control = "No Etag and no Last-modified";
    }

    // response->print_response();
    return response;
}

Response *Parser::parse_response(const std::string raw)
{
    Response *response = new Response(raw);
    response->expires_date = get_expires_date(response->origin);
    response->cache_control = parse_by_word(response->origin, "Cache-Control: ");
    response->cache_type = get_cache_type(response->cache_control);
    response->last_modified = parse_by_word(response->origin, "Last-Modified: ");
    response->etag = parse_by_word(response->origin, "ETag: ");
    // update cache type NOSTORE if there is no etag or last modified, no matter what cache_control contains
    // if (response->etag == "" && response->last_modified == "")
    // {
    //     response->cache_type = NOSTORE;
    //     response->cache_control = std::string("No Etag and no Last-Modified in response");
    // }
    // response->print_response();
    return response;
}
std::string Parser::parse_status(const std::string &origin)
{
    size_t start_pos = origin.find_first_of(' ') + 1;
    size_t end_pos = origin.find_first_of('\n') - 1;
    return origin.substr(start_pos, end_pos - start_pos);
}

std::string Parser::parse_by_deli(const std::string &origin, char deli)
{
    size_t pos = origin.find_first_of(deli);
    return origin.substr(0, pos);
}
std::string Parser::parse_after_deli(const std::string &origin, char deli)
{
    size_t pos = origin.find_first_of(deli);
    return origin.substr(pos + 1);
}

std::string Parser::parse_by_word(std::string origin, std::string word)
{
    // word = to_lower(word);
    // std::string origin_lower = to_lower(origin);
    // size_t start_pos = origin_lower.find(word);
    size_t start_pos = origin.find(word);
    if (start_pos == std::string::npos) // if there is no such header tag
    {
        return "";
    }
    else
    {
        start_pos += word.length(); // start of the position of substring
        size_t end_pos = origin.find_first_of('\n', start_pos) - 1;
        return origin.substr(start_pos, end_pos - start_pos);
    }
}

std::string Parser::to_lower(std::string &s)
{
    std::for_each(s.begin(), s.end(), [](char &c)
                  { c = ::tolower(c); });
    return s;
}

std::string Parser::get_expires_date(std::string &origin)
{
    int max_age = parse_max_age(origin); // returns 0 if no max-age
    std::string res_time = parse_by_word(origin, "Date: ");

    tm tm_;
    time_t t_;
    char buf[128] = {0};
    strcpy(buf, res_time.c_str());
    strptime(buf, "%a, %d %b %Y %H:%M:%S GMT", &tm_); //将字符串转换为tm时间
    tm_.tm_isdst = -1;
    t_ = timegm(&tm_); //将tm时间转换为秒时间
    t_ += max_age;     //秒数加 max_age
    tm_ = *gmtime(&t_);
    strftime(buf, 64, "%a, %d %b %Y %H:%M:%S GMT", &tm_);
    return buf;
}

int Parser::parse_max_age(std::string &origin)
{
    std::string cache_control = parse_by_word(origin, "Cache-Control: ");
    size_t num_start_pos = cache_control.find("max-age=");
    if (num_start_pos == std::string::npos) // return 0 if there is not a max-age tage
    {
        return 0;
    }
    num_start_pos += 8;
    size_t num_end_pos = cache_control.find_first_of("\n ,") - 1;
    std::string max_age_str = cache_control.substr(num_start_pos, num_end_pos - num_start_pos);
    return atoi(max_age_str.c_str());
}

std::pair<std::string, std::string> Parser::get_connect_host(const std::string &host_name)
{
    return std::pair<std::string, std::string>(parse_by_deli(host_name, ':'), parse_after_deli(host_name, ':'));
}

int Parser::parse_content_length(const std::string &origin)
{
    int header_length = origin.find("\r\n\r\n") + 4;
    int content_length = atoi((parse_by_word(origin, "Content-Length:")).c_str()) + header_length;
    return content_length;
}