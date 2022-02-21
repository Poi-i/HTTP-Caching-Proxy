#include "Cache.h"
pthread_mutex_t Cache::mutex = PTHREAD_MUTEX_INITIALIZER;

Response *Cache::get_response_unsafe(std::string &start_line)
{
    std::map<std::string, Response *>::iterator it;
    it = data.find(start_line);
    if (it == data.end())
    {
        return NULL;
    }
    return data[start_line];
}
Response *Cache::get_response(std::string &start_line)
{
    pthread_mutex_lock(&mutex);
    Response *response = get_response_unsafe(start_line);
    pthread_mutex_unlock(&mutex);
    return response;
}
/*release cache if needed*/
void Cache::update_cache(std::string &start_line, Response *response)
{
    pthread_mutex_lock(&mutex);
    std::cout << "---------- CACHE UPDATE ----------" << std::endl;

    std::cout << "---------- CACHE STARTLINE ----------"
              << "/n" << start_line << std::endl;
    // updata map and  queue

    if (get_response_unsafe(start_line) == NULL)
    {
        key_seq.push(start_line);
    }
    else
    { // delete old response before add a new one
        delete (data[start_line]);
    }
    data[start_line] = response;
    // relase cache if needed
    if (data.size() > capacity)
    {
        std::string key_todel = key_seq.front();
        key_seq.pop();
        delete (data[key_todel]);
        data.erase(key_todel);
    }

    print_cache();
    pthread_mutex_unlock(&mutex);
}

void Cache::print_cache()
{
    std::cout << "---------- PRINTING CACHE ----------" << std::endl;
    std::map<std::string, Response *>::iterator it;
    for (it = data.begin(); it != data.end(); ++it)
    {
        std::cout << "* START LINE: " << it->first << std::endl;
        std::cout << "* RESPONSE ETAG: <" << it->second->etag << ">" << std::endl;
        std::cout << "* RESPONSE LAST MODIFIED: <" << it->second->last_modified << ">" << std::endl;
        std::cout << "*" << std::endl;
    }
    std::cout << "---------- END PRINTING CACHE ----------"
              << std::endl;
}