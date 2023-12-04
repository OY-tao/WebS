#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <memory>
#include <csignal>
#include <map>
#include "task.h"
// #include "stream.h"
#include "utils.h"
// #include "io_uring.h"

//#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../http/httpconn.h"

constexpr size_t MAX_MESSAGE_LEN = 4096;
constexpr size_t BUFFERS_COUNT = 4096;

struct stream_base
{
    stream_base(task::promise_type *promise, size_t message_size)
        : promise(promise),
          message_size(message_size) {}
    task::promise_type *promise = NULL;
    size_t message_size;
};

struct read_awaitable : public stream_base
{
    read_awaitable(task::promise_type *promise, size_t message_size, char **buffer_pointer)
        : stream_base(promise, message_size),
          buffer_pointer(buffer_pointer) {}
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<task::promise_type> h)
    {
        auto &promise = h.promise();
        this->promise = &promise;
        promise.uring->add_read_request(promise.request_info.client_socket, promise.request_info);
    }
    size_t await_resume()
    {
        *buffer_pointer = promise->uring->get_buffer_pointer(promise->request_info.bid);
        return promise->res;
    }
    char **buffer_pointer;
};

struct write_awaitable : public stream_base
{
    write_awaitable(task::promise_type *promise, size_t message_size, char *buffer)
        : stream_base(promise, message_size),
          buffer(buffer) {}
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<task::promise_type> h)
    {
        auto &promise = h.promise();
        this->promise = &promise;
        promise.request_info.bid = promise.uring->get_buffer_id(buffer);
        promise.uring->add_write_request(promise.request_info.client_socket, message_size, promise.request_info);
        log("write await_suspend %lu", message_size);
    }
    size_t await_resume()
    {
        promise->uring->add_buffer_request(promise->request_info);
        return promise->res;
    }
    char *buffer;
};

struct read_file_awaitable : public read_awaitable
{   
    read_file_awaitable(task::promise_type *promise, size_t message_size, char **buffer_pointer, int read_fd)
        : read_awaitable(promise, message_size, buffer_pointer),
          read_fd(read_fd) {}
    void await_suspend(std::coroutine_handle<task::promise_type> h)
    {
        auto &promise = h.promise();
        this->promise = &promise;
        promise.uring->add_read_request(read_fd, promise.request_info);
        // pool->append
    }
    int read_fd;
};

struct write_file_awaitable : public write_awaitable
{
    write_file_awaitable(task::promise_type *promise, size_t message_size, char *buffer, int write_fd)
        : write_awaitable(promise, message_size, buffer),
          write_fd(write_fd) {}
    void await_suspend(std::coroutine_handle<task::promise_type> h)
    {
        auto &promise = h.promise();
        this->promise = &promise;
        promise.request_info.bid = promise.uring->get_buffer_id(buffer);
        promise.uring->add_write_request(write_fd, message_size, promise.request_info);
    }
    int write_fd;
};

struct close_awaitable
{   
    close_awaitable(int fd): fd(fd) {};
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<task::promise_type> h)
    {
        auto &promise = h.promise();
        promise.uring->add_close_request(fd);
    }
    void await_resume()
    {
    }
    int fd;
};

auto read_socket(char **buffer_pointer)
{
    return read_awaitable(nullptr, 0, buffer_pointer);
}

auto read_fd(int fd, char **buffer_pointer)
{
    return read_file_awaitable(nullptr, 0, buffer_pointer, fd);
}

auto write_fd(int fd, char *buffer, size_t message_size)
{
    return write_file_awaitable(nullptr, message_size, buffer, fd);
}

auto write_socket(char *buffer, size_t message_size)
{   
    log("write write_socket %lu", message_size);
    return write_awaitable(nullptr, message_size, buffer);
}

auto shutdown_socket(int fd)
{
    return close_awaitable(fd);
}


class WebServer {
public:
    WebServer(
        unsigned entries, int sock_listen_fd,
        int port, int trigMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);

    ~WebServer();
    void Start();

    void event_loop(task func(int));
    void setup_first_buffer();
    void add_read_request(int fd, request &req);
    void add_write_request(int fd, size_t message_size, request &req);
    void add_accept_request(int fd, struct sockaddr *client_addr, socklen_t *client_len, unsigned flags);
    void add_buffer_request(request &req);
    void add_open_request();
    void add_close_request(int fd);

    char* get_buffer_pointer(int bid) {
        return buffer[bid];
    }

    int get_buffer_id(char* buffer) {
        return (buffer - (char*)this->buffer.get()) / MAX_MESSAGE_LEN;
    }

private:
    bool InitSocket_(); 
    void InitEventMode_(int trigMode);
    void AddClient_(int sock_conn_fd, task handle_event(int));
  
    void DealListen_();
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);

    void SendError_(int fd, const char*info);
    void ExtentTime_(HttpConn* client);
    void CloseConn_(int fd);

    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess(HttpConn* client);
    task handle_http_request(int fd);

    static const int MAX_FD = 65536;

    static int SetFdNonblock(int fd);

    int port_;
    bool openLinger_;
    int timeoutMS_;  /* 毫秒MS */
    bool isClose_;
    int listenFd_;
    char* srcDir_;
    
    // uint32_t listenEvent_;
    // uint32_t connEvent_;
    struct io_uring ring;
    std::unique_ptr<char[][4096]> buffer;
    std::map<int, task> connections;
    std::map<int, struct sockaddr_in> saddr;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int sock_listen_fd;

    //std::unique_ptr<io_uring_handler> uring; 
    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool> threadpool_;
    // std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConn> users_;
};


#endif //WEBSERVER_H