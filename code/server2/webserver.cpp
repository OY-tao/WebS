#include "webserver.h"

using namespace std;

WebServer::WebServer(
            unsigned entries, int sock_listen_fd,
            int port, int trigMode, int timeoutMS, bool OptLinger,
            int sqlPort, const char* sqlUser, const  char* sqlPwd,
            const char* dbName, int connPoolNum, int threadNum,
            bool openLog, int logLevel, int logQueSize):
            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum))
    {
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    // InitEventMode_(trigMode);
    if(!InitSocket_()) { isClose_ = true;}

    if(openLog) {
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            // LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
            //                 (listenEvent_ & EPOLLET ? "ET": "LT"),
            //                 (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }

    struct io_uring_params params;
    memset(&params, 0, sizeof(params));
    this->sock_listen_fd = sock_listen_fd;

    if (io_uring_queue_init_params(entries, &ring, &params) < 0)
    {
        perror("io_uring_init_failed...\n");
        exit(1);
    }

    // check if IORING_FEAT_FAST_POLL is supported
    if (!(params.features & IORING_FEAT_FAST_POLL))
    {
        printf("IORING_FEAT_FAST_POLL not available in the kernel, quiting...\n");
        exit(0);
    }

    struct io_uring_probe *probe;
    probe = io_uring_get_probe_ring(&ring);
    if (!probe || !io_uring_opcode_supported(probe, IORING_OP_PROVIDE_BUFFERS))
    {
        printf("Buffer select not supported, skipping...\n");
        exit(0);
    }
    //free(probe);

    setup_first_buffer();

}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
    io_uring_queue_exit(&ring);
}

// void WebServer::InitEventMode_(int trigMode) {
//     listenEvent_ = EPOLLRDHUP;
//     connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
//     switch (trigMode)
//     {
//     case 0:
//         break;
//     case 1:
//         connEvent_ |= EPOLLET;
//         break;
//     case 2:
//         listenEvent_ |= EPOLLET;
//         break;
//     case 3:
//         listenEvent_ |= EPOLLET;
//         connEvent_ |= EPOLLET;
//         break;
//     default:
//         listenEvent_ |= EPOLLET;
//         connEvent_ |= EPOLLET;
//         break;
//     }
//     HttpConn::isET = (connEvent_ & EPOLLET);
// }

// void WebServer::Start2() {
//     int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
//     if(!isClose_) { LOG_INFO("========== Server start =========="); }
//     while(!isClose_) {
//         if(timeoutMS_ > 0) {
//             timeMS = timer_->GetNextTick();
//         }
//         int eventCnt = epoller_->Wait(timeMS);
//         for(int i = 0; i < eventCnt; i++) {
//             /* 处理事件 */
//             int fd = epoller_->GetEventFd(i);
//             uint32_t events = epoller_->GetEvents(i);
//             if(fd == listenFd_) {
//                 DealListen_();
//             }
//             else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
//                 assert(users_.count(fd) > 0);
//                 CloseConn_(&users_[fd]);
//             }
//             else if(events & EPOLLIN) {
//                 assert(users_.count(fd) > 0);
//                 DealRead_(&users_[fd]);
//             }
//             else if(events & EPOLLOUT) {
//                 assert(users_.count(fd) > 0);
//                 DealWrite_(&users_[fd]);
//             } else {
//                 LOG_ERROR("Unexpected event");
//             }
//         }
//     }
// }


// task WebServer::handle_http_request(int fd,HttpConn* client) {
//     HttpConn* conn=client;
//     int i=0;
//     while(true){
//         i++;
//         // http_conn conn;
//         char* read_buffer;
//         log("accept request %d", fd);
//         size_t read_bytes = co_await read_socket(&read_buffer);
//         if (read_bytes <= 0) {
//             // co_await read_socket(&read_buffer);
//             log("read_bytes %lu", read_bytes);
//             log("i %d", i);
//             //co_await shutdown_socket(fd);
//             shutdown(fd, SHUT_RDWR);
//             // *this.promise()
//             // connections.erase(fd);
//             co_return;
//             // read_bytes = co_await read_socket(&read_buffer);
//         }
//         read_buffer[read_bytes] = '\0';
//         // read_buffer[read_bytes] = 0;
//         log("read_buffer %lu %s", read_bytes, read_buffer);
//         // // conn.handle_request(read_buffer);
//         // read_buffer="1";
//         // log("get_response_size%lu",conn.get_response_size());
//         size_t write_bytes = co_await write_socket(read_buffer, read_bytes+1);
//         //size_t write_bytes = co_await write_socket(read_buffer, conn.get_response_size());
//         log("write_buffer %lu %s", write_bytes, read_buffer); 
//         shutdown(fd, SHUT_RDWR);     
//         co_return;
//     }
//     // log("accept request %d", fd);
//     // size_t read_bytes = co_await read_socket(&read_buffer);
//     // read_buffer[read_bytes] = 0;
//     // log("read_buffer %lu %s", read_bytes, read_buffer);
//     // conn.handle_request(read_buffer);
//     // size_t write_bytes = co_await write_socket(read_buffer, conn.get_response_size());
//     // log("write_buffer %lu %s", write_bytes, read_buffer);
//     // co_await shutdown_socket(fd);
//     // // free(read_buffer);
//     // co_return;
// }

void WebServer::Start(){
    LOG_INFO("server::start()");
    event_loop(handle_http_request); 
}

void WebServer::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn_(int fd) {
    // assert(client);
    // LOG_INFO("Client[%d] quit!", client->GetFd());
    // //epoller_->DelFd(client->GetFd());

    // auto &h = connections.at(fd).handler;
    // h.destroy();
    shutdown(fd, SHUT_RDWR);
    connections.erase(fd);
}

void WebServer::AddClient_(int sock_conn_fd, task handle_event(int)) {
    // assert(fd > 0);
    // // users_[fd].init(fd, addr);
    // if(timeoutMS_ > 0) {
    //     timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, fd));
    // }
    // // epoller_->AddFd(fd, EPOLLIN | connEvent_);
    // SetFdNonblock(fd);
    // LOG_INFO("Client[%d] in!", users_[fd].GetFd());
    if (sock_conn_fd >= 0&&!connections.count(sock_conn_fd))
    {
        connections.emplace(sock_conn_fd, handle_event(sock_conn_fd));
        if(timeoutMS_ > 0) {
            timer_->add(sock_conn_fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, sock_conn_fd));
        }
        auto &h = connections.at(sock_conn_fd).handler;
        auto &p = h.promise();
        p.request_info.client_socket = sock_conn_fd;
        // setSocketNonBlocking1(sock_conn_fd);
        p.uring = this;
        h.resume();
    }

}

void WebServer::DealListen_() {
    // struct sockaddr_in addr;
    // socklen_t len = sizeof(addr);
    // do {
    //     int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
    //     if(fd <= 0) { return;}
    //     else if(HttpConn::userCount >= MAX_FD) {
    //         SendError_(fd, "Server busy!");
    //         LOG_WARN("Clients is full!");
    //         return;
    //     }
    //     AddClient_(fd, addr);
    // } while(listenEvent_ & EPOLLET);
}

void WebServer::DealRead_(HttpConn* client) {
    // assert(client);
    // ExtentTime_(client);
    // threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

void WebServer::DealWrite_(HttpConn* client) {
    // assert(client);
    // ExtentTime_(client);
    // threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

void WebServer::OnRead_(HttpConn* client) {
    // assert(client);
    // int ret = -1;
    // int readErrno = 0;
    // ret = client->read(&readErrno);
    // if(ret <= 0 && readErrno != EAGAIN) {
    //     CloseConn_(client);
    //     return;
    // }
    // OnProcess(client);
}

void WebServer::OnProcess(HttpConn* client) {
    // if(client->process()) {
    //     epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    // } else {
    //     epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    // }
}

void WebServer::OnWrite_(HttpConn* client) {
    // assert(client);
    // int ret = -1;
    // int writeErrno = 0;
    // ret = client->write(&writeErrno);
    // if(client->ToWriteBytes() == 0) {
    //     /* 传输完成 */
    //     if(client->IsKeepAlive()) {
    //         OnProcess(client);
    //         return;
    //     }
    // }
    // else if(ret < 0) {
    //     if(writeErrno == EAGAIN) {
    //         /* 继续传输 */
    //         epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    //         return;
    //     }
    // }
    // CloseConn_(client);
}

/* Create listenFd */
bool WebServer::InitSocket_() {
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    struct linger optLinger = { 0 };
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    // ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);
    // if(ret == 0) {
    //     LOG_ERROR("Add listen error!");
    //     close(listenFd_);
    //     return false;
    // }
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}


task WebServer::handle_http_request(int fd) {
    HttpConn* conn=new HttpConn();
    conn->init(fd,saddr[fd]);
    int i=0;
    while(true){
        i++;
        // http_conn conn;
        char* read_buffer;
        log("accept request %d", fd);
        size_t read_bytes = co_await read_socket(&read_buffer);
        if (read_bytes <= 0) {
            // co_await read_socket(&read_buffer);
            log("read_bytes %lu", read_bytes);
            log("i %d", i);
            //co_await shutdown_socket(fd);
            shutdown(fd, SHUT_RDWR);
            // *this.promise()
            // connections.erase(fd);
            co_return;
            // read_bytes = co_await read_socket(&read_buffer);
        }
        read_buffer[read_bytes] = '\0';
        // read_buffer[read_bytes] = 0;
        log("read_buffer %lu %s", read_bytes, read_buffer);
        // // conn.handle_request(read_buffer);
        // read_buffer="1";
        // log("get_response_size%lu",conn.get_response_size());
        size_t write_bytes = co_await write_socket(read_buffer, read_bytes+1);
        //size_t write_bytes = co_await write_socket(read_buffer, conn.get_response_size());
        log("write_buffer %lu %s", write_bytes, read_buffer); 
        shutdown(fd, SHUT_RDWR);     
        co_return;
    }
    // log("accept request %d", fd);
    // size_t read_bytes = co_await read_socket(&read_buffer);
    // read_buffer[read_bytes] = 0;
    // log("read_buffer %lu %s", read_bytes, read_buffer);
    // conn.handle_request(read_buffer);
    // size_t write_bytes = co_await write_socket(read_buffer, conn.get_response_size());
    // log("write_buffer %lu %s", write_bytes, read_buffer);
    // co_await shutdown_socket(fd);
    // // free(read_buffer);
    // co_return;
}

void WebServer::event_loop(task handle_event(int))
{
    // start event loop
    log("start event loop");
    add_accept_request(sock_listen_fd, (struct sockaddr *)&client_addr, &client_len, 0);
    while (1)
    {
        io_uring_submit_and_wait(&ring, 1);
        struct io_uring_cqe *cqe;
        unsigned head;
        unsigned count = 0;

        // go through all CQEs
        io_uring_for_each_cqe(&ring, head, cqe)
        {
            ++count;
            request conn_i;
            memcpy(&conn_i, &cqe->user_data, sizeof(conn_i));

            int type = conn_i.event_type;
            if (cqe->res == -ENOBUFS)
            {
                fprintf(stdout, "bufs in automatic buffer selection empty, this should not happen...\n");
                fflush(stdout);
                exit(1);
            }
            else if (type == PROV_BUF)
            {
                if (cqe->res < 0)
                {
                    printf("cqe->res = %d\n", cqe->res);
                    exit(1);
                }
            }
            else if (type == ACCEPT)
            {
                int sock_conn_fd = cqe->res;
                // only read when there is no error, >= 0
                log("accept in io_uring_for_each_cqe");
                if (sock_conn_fd >= 0&&!connections.count(sock_conn_fd))
                {
                    connections.emplace(sock_conn_fd, handle_event(sock_conn_fd));
                    if(timeoutMS_ > 0) {
                        timer_->add(sock_conn_fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, sock_conn_fd));
                    }
                    auto &h = connections.at(sock_conn_fd).handler;
                    auto &p = h.promise();
                    p.request_info.client_socket = sock_conn_fd;
                    // setSocketNonBlocking1(sock_conn_fd);
                    p.uring = this;
                    h.resume();
                }

                // new connected client; read data from socket and re-add accept to monitor for new connections
                add_accept_request(sock_listen_fd, (struct sockaddr *)&client_addr, &client_len, 0);
            }
            else if (type == READ)
            {
                auto &h = connections.at(conn_i.client_socket).handler;
                auto &p = h.promise();
                p.request_info.bid = cqe->flags >> 16;
                p.res = cqe->res;
                int t=p.res;
                h.resume();
                // if(t==0){
                //     add_close_request(fd);
                // }
                if(t==0){
                    // h.resume();
                    connections.erase(conn_i.client_socket);
                    add_accept_request(sock_listen_fd, (struct sockaddr *)&client_addr, &client_len, 0);
                }
                log("fdnum %lu",connections.size());
            }
            else if (type == WRITE)
            {
                // add_accept_request(sock_listen_fd, (struct sockaddr *)&client_addr, &client_len, 0);
                auto &h = connections.at(conn_i.client_socket).handler;
                h.promise().res = cqe->res;
                h.resume();
                connections.erase(conn_i.client_socket);
                
                
            }
        }

        io_uring_cq_advance(&ring, count);
    }
}

void WebServer::setup_first_buffer()
{
    buffer.reset(new char[BUFFERS_COUNT][MAX_MESSAGE_LEN]);

    // register buffers for buffer selection
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;

    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_provide_buffers(sqe, buffer.get(), MAX_MESSAGE_LEN, BUFFERS_COUNT, group_id, 0);

    io_uring_submit(&ring);
    io_uring_wait_cqe(&ring, &cqe);
    if (cqe->res < 0)
    {
        printf("cqe->res = %d\n", cqe->res);
        exit(1);
    }
    io_uring_cqe_seen(&ring, cqe);
}


void WebServer::add_read_request(int fd, request &req)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_recv(sqe, fd, NULL, MAX_MESSAGE_LEN, 0);
    io_uring_sqe_set_flags(sqe, IOSQE_BUFFER_SELECT);
    sqe->buf_group = group_id;
    req.event_type = READ;
    sqe->user_data = req.uring_data;
}

void WebServer::add_close_request(int fd)
{
    // connections.erase(fd);
    shutdown(fd, SHUT_RDWR);
    // connections.erase(fd);
}


void WebServer::add_write_request(int fd, size_t message_size, request &req)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    // io_uring_prep_send(sqe, fd, &buffer[req.bid], message_size, 0);
    io_uring_prep_write(sqe, fd, &buffer[req.bid], message_size, 0);
    io_uring_sqe_set_flags(sqe, 0);
    req.event_type = WRITE;
    sqe->user_data = req.uring_data;
    log("add_write_request %lu", message_size);
}

void WebServer::add_accept_request(int fd, struct sockaddr *client_addr, socklen_t *client_len, unsigned flags)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_accept(sqe, fd, client_addr, client_len, 0);
    io_uring_sqe_set_flags(sqe, flags);

    request conn_i;
    conn_i.event_type = ACCEPT;
    conn_i.bid = 0;
    conn_i.client_socket = fd;

    sqe->user_data = conn_i.uring_data;
}

void WebServer::add_buffer_request(request &req)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_provide_buffers(sqe, buffer[req.bid], MAX_MESSAGE_LEN, 1, group_id, req.bid);
    req.event_type = PROV_BUF;
    sqe->user_data = req.uring_data;
}

void WebServer::add_open_request()
{
}