#include "Proxy.h"

// init static field
Cache *Proxy::cache = new Cache();
Parser *Proxy::parser = new Parser();
std::ofstream Proxy::log("/var/log/erss/proxy.log");
// std::ofstream Proxy::log("/home/yz566/HTTP-Caching-Proxy/docker-deploy/src/proxy.log");
pthread_mutex_t Proxy::mutex = PTHREAD_MUTEX_INITIALIZER;

void Proxy::run_proxy()
{
    activate_server();

    int user_id = 1;
    while (1)
    {
        pthread_t thread;
        std::pair<int, std::string> uid_uip = accept_user(); // build connect to user
        Socketer *socketer = new Socketer();
        socketer->user_fd = uid_uip.first;
        socketer->user_ip = uid_uip.second;
        socketer->user_id = user_id++;

        // std::cout << "\n---------- USER FD ----------\n"
        //   << socketer->user_fd << std::endl;
        // std::cout << "\n---------- USER ID ----------\n"
        //           << socketer->user_id << std::endl;
        pthread_create(&thread, NULL, handle_request, socketer);
        pthread_detach(thread);
    }
}

Response *Proxy::commute_webserver(Socketer *socketer, Request *request)
{
    if (send(socketer->web_fd, request->origin.c_str(), request->origin.length(), 0) == -1)
    {
        perror("send");
        throw "send request failed in commute_webserver";
    }
    char buf[BUFSIZE];
    int numbytes;
    if ((numbytes = recv(socketer->web_fd, buf, BUFSIZE, 0)) == -1)
    {
        perror("recv");
        throw "recv response failed in commute_webserver";
        // std::terminate();
    }
    Response *response = parser->parse_response(buf);
    if (check_bad_response(response, socketer))
    {
        return NULL;
    }
    if (response->chunk != "")
    {
        pthread_mutex_lock(&mutex);
        log << socketer->user_id << ": not cacheable because response is CHUNCKED" << std::endl;
        log << socketer->user_id << ": Responding \"" << parser->parse_by_deli(response->origin, '\r') << "\"" << std::endl;
        pthread_mutex_unlock(&mutex);
        delete (response);
        if (send(socketer->user_fd, buf, numbytes, 0) == -1)
        {
            perror("send");
            throw "send response failed in commute_webserver";
        }
        while (true)
        {
            char chunk[BUFSIZE];
            int numbytes = recv(socketer->web_fd, chunk, BUFSIZE, 0);
            if (numbytes == 0)
            {
                perror("recv");
                break;
            }
            else if (numbytes < 0)
            {
                perror("recv");
                // std::terminate();
                throw "recv chunk response failed in commute_webserver";
            }
            else
            {
                if (send(socketer->user_fd, chunk, numbytes, 0) == -1)
                {
                    perror("send");
                    throw "send chunk response failed in commute_webserver";
                }
            }
        }
        return NULL;
    }
    std::string res(buf, numbytes);
    int content_length = parser->parse_content_length(response->origin);
    // std::cout << "content length: " << content_length << std::endl;
    int received_length = numbytes;
    // std::cout << "received length: " << received_length << std::endl;
    while (content_length > received_length)
    {
        numbytes = recv(socketer->web_fd, buf, BUFSIZE, 0);
        if (numbytes == 0)
        {
            send_502(socketer);
            return NULL;
        }
        if (numbytes == -1)
        {
            perror("recv");
            // std::terminate();
            throw "recv full response failed in commute_webserver";
        }

        res += std::string(buf, numbytes);
        received_length += numbytes;
        std::cout << "received length: " << received_length << std::endl;
    }
    response = parser->parse_response(res);
    if (check_bad_response(response, socketer))
    {
        return NULL;
    }
    return response;
}

void Proxy::resolve_GET(Request *request, Socketer *socketer)
{
    Response *response = cache->get_response(request->start_line);
    if (response == NULL)
    {
        // make log
        pthread_mutex_lock(&mutex);
        log << socketer->user_id << ": not in cache" << std::endl;
        pthread_mutex_unlock(&mutex);
        socketer->activate_client(request->host_name.c_str(), "80");
        pthread_mutex_lock(&mutex);
        log << socketer->user_id << ": Requesting \"" << request->start_line << "\" from " << request->host_name << std::endl;
        pthread_mutex_unlock(&mutex);
        response = commute_webserver(socketer, request);
        if (response == NULL)
        {
            return; // response has been sent in commute_webserver()
        }

        pthread_mutex_lock(&mutex);
        log << socketer->user_id << ": Received \"" << parser->parse_by_deli(response->origin, '\r') << "\" from " << request->host_name << std::endl;
        pthread_mutex_unlock(&mutex);

        std::string res_status = parser->parse_status(response->origin);

        if (res_status == "200 OK") // check cache control
        {
            switch (response->cache_type)
            {
            case NOSTORE:
                // make log
                // ID: not cacheable because REASON
                pthread_mutex_lock(&mutex);
                log << socketer->user_id << ": not cacheable because response is NOSTORE: " << response->cache_control << std::endl;
                pthread_mutex_unlock(&mutex);
                break;
            case NOCACHE:
                // make log
                // cached, but requires re-validation
                pthread_mutex_lock(&mutex);
                log << socketer->user_id << ": cached, but requires re-validation" << std::endl;
                pthread_mutex_unlock(&mutex);
                cache->update_cache(request->start_line, response);
                break;
            case MAXAGE:
                // make log
                // ID: cached, expires at EXPIRES
                pthread_mutex_lock(&mutex);
                log << socketer->user_id << ": cached, expires at " << response->expires_date << std::endl;
                pthread_mutex_unlock(&mutex);
                cache->update_cache(request->start_line, response);
                break;
            }
        }
    }
    else
    { // cache exist
        // response get from cache
        std::cout << "--------------Cached response-----------------" << std::endl;
        // response->print_response();
        std::cout << "--------------End Cached response-----------------" << std::endl;
        switch (response->cache_type)
        {
        case NOSTORE:
            break; // this would never happen if cache exist
        case NOCACHE:
            // make log
            // ID: in cache, requires validation
            pthread_mutex_lock(&mutex);
            log << socketer->user_id << ": in cache, requires validation" << std::endl;
            pthread_mutex_unlock(&mutex);
            revalidate(&response, request, socketer);
            break;
        case MAXAGE:
            // make log
            if (is_stale(response))
            {
                // ID: in cache, but expired at EXPIREDTIME
                pthread_mutex_lock(&mutex);
                log << socketer->user_id << ": in cache, but expired at " << response->expires_date << std::endl;
                pthread_mutex_unlock(&mutex);
                revalidate(&response, request, socketer);
            }
            else
            {
                // ID: in cache, valid
                pthread_mutex_lock(&mutex);
                log << socketer->user_id << ": in cache, valid" << std::endl;
                pthread_mutex_unlock(&mutex);
                break;
            }
        }
    }
    // std::cout << "_________revalidate 200 OK response_________" << std::endl;
    response->print_response();
    // std::cout << "_________END__________" << std::endl;
    // send response to user

    // send_response(response, socketer);
    if (send(socketer->user_fd, response->origin.c_str(), response->origin.length(), 0) == -1)
    {
        perror("send");
        throw "send get response failed in resolve_GET";
    }
    // make log: Responding "RESPONSE"
    pthread_mutex_lock(&mutex);
    log << socketer->user_id << ": Responding \"" << parser->parse_by_deli(response->origin, '\r') << "\"" << std::endl;
    pthread_mutex_unlock(&mutex);
    if (response->cache_type == NOSTORE)
    {
        // don't need to delete response if it is in cache/updated by cahce
        delete (response);
    }
}

void Proxy::send_response(Response *response, Socketer *socketer)
{
    std::cout << "--- From send_response: " << std::endl;
    std::string to_send(response->origin.c_str());
    int response_length = response->origin.length();
    std::cout << "--- From send_response: length to send: " << response_length << std::endl;
    int sent_length = 0;
    while (sent_length < response_length)
    {
        int numbytes = send(socketer->user_fd, to_send.substr(sent_length).c_str(), response_length - sent_length, 0);
        if (numbytes == -1)
        {
            perror("send");
            throw "send get response failed in resolve_GET";
        }
        sent_length += numbytes;
        std::cout << "--- From send_response: sent_length: " << sent_length << std::endl;
    }
}

void Proxy::resolve_CONNECT(Request *request, Socketer *socketer)
{

    // send 200 OK to user to indicate the successful connect
    std::string msg = "HTTP/1.1 200 OK\r\n\r\n";
    if (send(socketer->user_fd, msg.c_str(), msg.length(), 0) == -1)
    {
        perror("send");
        throw "send connect response failed in resolve_CONNECT";
    }
    pthread_mutex_lock(&mutex);
    log << socketer->user_id << ": Responding \"HTTP/1.1 200 OK\"" << std::endl;
    pthread_mutex_unlock(&mutex);
    std::pair<std::string, std::string> host_port = parser->get_connect_host(request->host_name);
    // std::cout << "host: <" << host_port.first << ">, port: <" << host_port.second << ">" << std::endl;
    socketer->activate_client(host_port.first.c_str(), host_port.second.c_str());

    int fd_list[2] = {socketer->user_fd, socketer->web_fd};
    fd_set readfds;
    int n = std::max(fd_list[0], fd_list[1]);

    // std::cout << "socketer->user_fd: <" << socketer->user_fd << ">, socketer->web_fd: <" << socketer->web_fd << ">, n: <" << n << ">" << std::endl;

    while (1)
    {
        // printf("-----------------------------------------------\n");
        FD_ZERO(&readfds);
        for (int i = 0; i < 2; i++)
        {
            FD_SET(fd_list[i], &readfds);
        }
        int rv = select(n + 1, &readfds, NULL, NULL, NULL);
        if (rv == -1)
        {
            perror("select"); // select() error
            return;
        }
        else
        {
            for (int i = 0; i < 2; i++) //至少一個 descriptor(s) 有資料
            {
                int fd = fd_list[i];
                int other_fd = fd_list[(i + 1) % 2];
                // char buf[MAXDATASIZE];
                std::vector<char> buf(MAXDATASIZE);
                if (FD_ISSET(fd, &readfds))
                {

                    int numbytes = recv(fd, &buf.data()[0], MAXDATASIZE - 1, 0);
                    if (numbytes == -1)
                    {
                        perror("recv");
                        return;
                    }
                    if (numbytes == 0)
                    {
                        // std::cout << "---------- CONNECT END ----------" << std::endl;
                        socketer->close_sockets();
                        return;
                    }
                    buf[numbytes] = '\0';
                    // std::cout << "\n---------- RESPONSE FROM CONNECT ----------" << buf << std::endl;

                    int numbytes_sent = send(other_fd, buf.data(), numbytes, 0);
                    if (numbytes_sent == -1)
                    {
                        perror("send");
                        return;
                    }
                    // std::cout << "\n---------- REQUEST FROM CONNECT ----------" << buf << std::endl;
                }
            }
        }
    }
}

void Proxy::resolve_POST(Request *request, Socketer *socketer)
{

    char http_port[] = "80";
    // build socket as client, send request/recv response to/from web server
    socketer->activate_client(request->host_name.c_str(), http_port);
    pthread_mutex_lock(&mutex);
    log << socketer->user_id << ": Requesting \"" << request->start_line << "\" from " << request->host_name << std::endl;
    pthread_mutex_unlock(&mutex);

    Response *response = commute_webserver(socketer, request); // parser->parse_response(buf.data());
    pthread_mutex_lock(&mutex);
    log << socketer->user_id << ": Received \"" << parser->parse_by_deli(response->origin, '\r') << "\" from " << request->host_name << std::endl;
    pthread_mutex_unlock(&mutex);

    if (send(socketer->user_fd, response->origin.c_str(), response->origin.length(), 0) == -1)
    {
        perror("send");
        return;
    }

    // make responding log
    pthread_mutex_lock(&mutex);
    log << socketer->user_id << ": Responding \"" << parser->parse_by_deli(response->origin, '\r') << "\"" << std::endl;
    pthread_mutex_unlock(&mutex);
    delete (response);
}

void Proxy::resolve_other(Socketer *socketer)
{
    std::string msg("HTTP/1.1 400 Bad Request");
    if (send(socketer->user_fd, msg.c_str(), msg.size(), 0) == -1)
    {
        perror("send");
        return;
    }
    pthread_mutex_lock(&mutex);
    log << socketer->user_id << ": Responding \"" << msg << "\"" << std::endl;
    pthread_mutex_unlock(&mutex);
}

bool Proxy::check_bad_response(Response *response, Socketer *socketer)
{
    bool bad_response = false;
    if (response->origin.find("\r\n\r\n") == std::string::npos)
    {
        send_502(socketer);
        delete (response);
        bad_response = true;
    }
    return bad_response;
}

void Proxy::send_502(Socketer *socketer)
{
    std::string msg("HTTP/1.1 502 502 Bad Gateway");
    if (send(socketer->user_fd, msg.c_str(), msg.size(), 0) == -1)
    {
        perror("send");
        throw "send 502 response failed in send_502";
    }
    pthread_mutex_lock(&mutex);
    log << socketer->user_id << ": Responding \"" << msg << "\"" << std::endl;
    pthread_mutex_unlock(&mutex);
}

void *Proxy::handle_request(void *input)
{

    Socketer *socketer = (Socketer *)input;
    // receive user request to buf
    int numbytes;
    std::vector<char> buf(MAXDATASIZE);
    if ((numbytes = recv(socketer->user_fd, &buf.data()[0], MAXDATASIZE - 1, 0)) == -1)
    {
        perror("recv");
        return NULL;
    }
    buf[numbytes] = '\0';
    std::cout << "\n---------- NEW REQUEST FROM USER ----------\n"
              << buf.data() << std::endl;

    // parse request
    Request *request = parser->parse_request(buf.data());
    pthread_mutex_lock(&mutex);
    log << socketer->user_id << ": \"" << request->start_line << "\" from "
        << socketer->user_ip << " @ " << get_currtime() << std::endl;
    pthread_mutex_unlock(&mutex);
    try
    {
        if (request->method == "GET")
        {
            resolve_GET(request, socketer);
        }
        else if (request->method == "POST")
        {
            resolve_POST(request, socketer);
        }
        else if (request->method == "CONNECT")
        {
            resolve_CONNECT(request, socketer);
            pthread_mutex_lock(&mutex);
            log << socketer->user_id << ": Tunnel closed" << std::endl;
            pthread_mutex_unlock(&mutex);
        }
        else
        {
            resolve_other(socketer);
        }
        delete (request);
        delete (socketer);
        return NULL;
    }
    catch (const char *msg)
    {
        std::cerr << "Proxy::handle_request() caught exception: " << msg << std::endl;
        delete (request);
        delete (socketer);
        return NULL;
    }
    catch (std::exception &e)
    {
        std::cerr << "Proxy::handle_request() caught exception: " << e.what() << std::endl;
        delete (request);
        delete (socketer);
        return NULL;
    }
}

bool Proxy::is_stale(Response *response)
{
    time_t curr_time = time(NULL);
    // convert the expries date string to time_t expires time
    tm tm_;
    char buf[128] = {0};
    strcpy(buf, response->expires_date.c_str());
    strptime(buf, "%a, %d %b %Y %H:%M:%S GMT", &tm_); //将字符串转换为tm时间
    tm_.tm_isdst = -1;
    time_t expires_time = timegm(&tm_); //将tm时间转换为GMT秒时间
    return difftime(expires_time, curr_time) < 0.0;
}

void Proxy::revalidate(Response **response, Request *request, Socketer *socketer)
{
    socketer->activate_client(request->host_name.c_str(), "80");
    std::string my_request_str = make_revalidate_req(*response, request);
    Request *my_request = parser->parse_request(my_request_str);
    pthread_mutex_lock(&mutex);
    log << socketer->user_id << ": Requesting \"" << request->start_line << "\" from " << request->host_name << std::endl;
    pthread_mutex_unlock(&mutex);
    *response = commute_webserver(socketer, my_request);
    /* TESTING CODE */
    // std::string our_str = std::string("HTTP/1.1 200 OK\r\nDate: Wed, 16 Feb 2022 21:43:25 GMT\r\nServer: Apache\r\nETag: \"286-4f1aadb3105c1\"\r\nAccept-Ranges: bytes\r\nContent-Length: 646\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html><head></head><body><header><title>http://info.cern.ch</title></header><h1>home of the first website</h1></body></html>\r\n");
    // *response = parser->parse_response(our_str);
    /* TESTING CODE */
    if (*response == NULL)
    {
        return;
    }

    pthread_mutex_lock(&mutex);
    log << socketer->user_id << ": Received \"" << parser->parse_by_deli((*response)->origin, '\r') << "\" from " << request->host_name << std::endl;
    pthread_mutex_unlock(&mutex);
    std::string status = parser->parse_status((*response)->origin);
    std::cout << "STATUS CODE: <" << status << ">" << std::endl;
    if (status == "200 OK")
    {
        // Response *new_response = parser->parse_response(buf.data());
        cache->update_cache(request->start_line, *response);
        *response = cache->get_response(request->start_line);
    }
    else if (status == "304 Not Modified")
    {
        // make log
        // ID: in cache, valid
        pthread_mutex_lock(&mutex);
        log << socketer->user_id << ": in cache, valid" << std::endl;
        pthread_mutex_unlock(&mutex);
        std::cout << "304 respons" << std::endl;
        (*response)->print_response();
        *response = cache->get_response(request->start_line);
    }
    else
    {
        std::cerr << "Revalidation response is neither 200 nor 304!" << std::endl;
        resolve_other(socketer);
        return;
    }
    delete (my_request);
}

std::string Proxy::make_revalidate_req(Response *response, Request *request)
{
    std::string my_request = request->origin;
    if (response->etag != "")
    {
        std::string my_etag = std::string("If-None-Match: ").append(response->etag).append("\r\n");
        my_request.insert(my_request.length() - 2, my_etag);
    }
    if (response->last_modified != "")
    {
        std::string my_modified = std::string("If-Modified-Since: ").append(response->last_modified).append("\r\n");
        my_request.insert(my_request.length() - 2, my_modified);
    }
    return my_request;
}

/*make a socket as server to users*/
void Proxy::activate_server()
{
    int status;
    struct addrinfo hostinfo;
    struct addrinfo *servinfo;                   // will point to the results
    std::memset(&hostinfo, 0, sizeof(hostinfo)); // make sure the struct is empty
    hostinfo.ai_family = AF_UNSPEC;              // don't care IPv4 or IPv6
    hostinfo.ai_socktype = SOCK_STREAM;          // TCP stream sockets
    hostinfo.ai_flags = AI_PASSIVE;              // fill in my IP for me
    // get all addr info into servinfo
    if ((status = getaddrinfo(NULL, "12345", &hostinfo, &servinfo)) != 0)
    {
        std::cerr << "getaddrinfo error:" << gai_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }

    my_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (my_fd == -1)
    {
        fprintf(stderr, "Fail to create socket!\n");
        exit(EXIT_FAILURE);
    }
    // lose the pesky "Address already in use" error message
    int yes = 1;
    if (setsockopt(my_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
    {
        perror("setsockopt failed!\n");
        exit(EXIT_FAILURE);
    }
    // bind socket with a port on local machine(ai_addr)
    if ((status = bind(my_fd, servinfo->ai_addr, servinfo->ai_addrlen)) == -1)
    {
        fprintf(stderr, "bind failed!\n");
        exit(EXIT_FAILURE);
    }
    // listen
    if (listen(my_fd, BACKLOG) == -1)
    {
        fprintf(stderr, "listen failed!\n");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(servinfo);
}

/*accept user's connection and get client's ip*/
std::pair<int, std::string> Proxy::accept_user()
{
    struct sockaddr_storage socket_addr;
    socklen_t socket_addr_len = sizeof(socket_addr);
    int user_fd = accept(my_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
    if (user_fd == -1)
    {
        std::cerr << "Error: cannot accept connection on socket" << std::endl;
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in *client_addr = (struct sockaddr_in *)&socket_addr;
    std::string ip = inet_ntoa(client_addr->sin_addr);
    return std::pair<int, std::string>(user_fd, ip);
}

std::string Proxy::get_currtime()
{
    time_t curr_time = time(NULL);
    char buf[128] = {0};
    tm tm_ = *gmtime(&curr_time);
    strftime(buf, 64, "%a, %d %b %Y %H:%M:%S GMT", &tm_);
    return std::string(buf);
}
