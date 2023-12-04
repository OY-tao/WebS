#ifndef SERVER_H
#define SERVER_H

#include <memory>
#include "io_uring.h"
#include "utils.h"
#include <netinet/in.h>
#include <csignal>
#include <map>
#include "task.h"
#include "stream.h"
// #include "http_conn.h"

#include "../http4/httpconn.h"

constexpr size_t ENTRIES = 2048;

class server
{
public:
    server(int port);
    ~server();
    void start();
private:
    std::unique_ptr<io_uring_handler> uring;
    int sock_fd;

    void setup_listening_socket(int port);
};

// int setSocketNonBlocking1(int fd)
// {
//     int flag = fcntl(fd, F_GETFL, 0);
//     if (flag == -1)
//         return -1;

//     flag |= O_NONBLOCK;
//     if (fcntl(fd, F_SETFL, flag) == -1)
//         return -1;
//     return 0;
// }

void server::setup_listening_socket(int port)
{
    struct sockaddr_in srv_addr = {0};

    sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1)
        fatal_error("socket()");

    int enable = 1;
    if (setsockopt(sock_fd,
                   SOL_SOCKET, SO_REUSEADDR,
                   &enable, sizeof(int)) < 0)
        fatal_error("setsockopt(SO_REUSEADDR)");

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* We bind to a port and turn this socket into a listening
     * socket.
     * */
    if (bind(sock_fd,
             (const struct sockaddr *)&srv_addr,
             sizeof(srv_addr)) < 0)
        fatal_error("bind()");

    if (listen(sock_fd, 10) < 0)
        fatal_error("listen()");
    // setSocketNonBlocking1(sock_fd);
}

server::server(int port)
{   
    log("server in port %d", port);
    setup_listening_socket(port);
    uring.reset(new io_uring_handler(ENTRIES, sock_fd));
}

server::~server()
{
}


task handle_http_request(int fd,sockaddr_in client_addr) {

    HttpConn conn;
    conn.init(fd,client_addr);
    int i=0;
    while(true){
        i++;
        // http_conn conn;
        char* read_buffer;
        
        // log("accept request %d", fd);
        size_t read_bytes = co_await read_socket(&read_buffer);
        if (read_bytes <= 0) {
            // co_await read_socket(&read_buffer);
            // log("read_bytes %lu", read_bytes);
            // log("i %d", i);
            //co_await shutdown_socket(fd);
            shutdown(fd, SHUT_RDWR);
            // *this.promise()
            // connections.erase(fd);
            co_return;
            // read_bytes = co_await read_socket(&read_buffer);
        }
        read_buffer[read_bytes] = '\0';
        // conn.readBuff_.Append(read_buffer,read_bytes);
        // conn.process();
        // log("conn_write_buffer %lu %s", const_cast<char*>(conn.writeBuff_.Peek()), conn.writeBuff_.ReadableBytes()); 
        // size_t write_bytes = co_await write_socket(const_cast<char*>(conn.writeBuff_.Peek()), conn.writeBuff_.ReadableBytes());
        // read_buffer[read_bytes] = 0;
        // log("read_buffer %lu %s", read_bytes, read_buffer);
        // // conn.handle_request(read_buffer);
        // read_buffer="1";
        // log("get_response_size%lu",conn.get_response_size());
        size_t write_bytes = co_await write_socket(read_buffer, read_bytes+1);
        //size_t write_bytes = co_await write_socket(read_buffer, conn.get_response_size());
        // log("write_buffer %lu %s", write_bytes, read_buffer); 
        shutdown(fd, SHUT_RDWR);     
        co_return;
    }

}

task handle_http_coon(int fd,HttpConn* client) {

    int i=0;
    while(true){
        i++;
        char* read_buffer;       
        // log("accept request %d", fd);
        size_t read_bytes = co_await read_socket(&read_buffer);
        if (read_bytes <= 0) {
            //co_await shutdown_socket(fd);
            shutdown(fd, SHUT_RDWR);
            co_return;
        }
        read_buffer[read_bytes] = '\0';
        client->readBuff_.Append(read_buffer,read_bytes);
        size_t bytes=co_await http_conn(client,read_bytes);

        log("conn_write_buffer %lu %s", client->writeBuff_.ReadableBytes(), const_cast<char*>(client->writeBuff_.Peek())); 
        size_t write_bytes = co_await write_socket(const_cast<char*>(client->writeBuff_.Peek()), client->writeBuff_.ReadableBytes());
        // shutdown(fd, SHUT_RDWR);     
        // co_return;
    }

}

void server::start() {
    log("server::start()");
    uring->event_loop(handle_http_request);
}

#endif
