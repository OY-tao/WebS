#include "webserver copy.h"

using namespace std;

WebServer::WebServer(
            int port, int trigMode, int timeoutMS, bool OptLinger,
            int sqlPort, const char* sqlUser, const  char* sqlPwd,
            const char* dbName, int connPoolNum, int threadNum,
            bool openLog, int logLevel, int logQueSize):
            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), iorws_(new iorws())
    {
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    InitEventMode_(trigMode);
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
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

void WebServer::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

void WebServer::Start() {
    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */

	int ret = 0;
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	add_accept(&ring, listenFd_, (struct sockaddr*)&client_addr, &client_len, 0);
	signal(SIGINT, sig_handler);      
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    while(!isClose_) {
        if(timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick();
        }

		int cqe_count;
		struct io_uring_cqe *cqes[BACKLOG];

		ret = io_uring_submit_and_wait(&ring, 1); //提交sq的entry，阻塞等到其完成，最小完成1个时返回
		if (ret < 0) {
			printf("Returned from io is %d\n", errno);
			perror("Error io_uring_submit_and_wait\n");
			LOG_ERROR("%s", "io_uring failure");
			exit(1);
		}

		//将准备好的队列填充到cqes中，并返回已准备好的数目，收割cqe
		cqe_count = io_uring_peek_batch_cqe(&ring, cqes, sizeof(cqes) / sizeof(cqes[0]));
		assert(cqe_count >= 0);
		// if (debug) {
		// 	printf("Returned from cqe_count is %d\n", cqe_count);
		// }

		for (int i = 0; i < cqe_count; ++i) {
			struct io_uring_cqe* cqe = cqes[i];
			conn_info* user_data = (conn_info*)io_uring_cqe_get_data(cqe);
			int type = user_data->type;
            unsigned fd= user_data->fd;

			if (type == ACCEPT) {
				int sock_conn_fd = cqe->res;
				//cqe向前移动避免当前请求避免被二次处理，Must be called after io_uring_{peek,wait}_cqe() after the cqe has been processed by the application.
				io_uring_cqe_seen(&ring, cqe);
				// if (debug) {
				// 	printf("Returned from ACCEPT is %d\n", sock_conn_fd);
				// }
				if (sock_conn_fd <= 0) {
					continue;
				}

				if (registerfiles && registered_files[sock_conn_fd] == -1) { //寄存文件中并没有注册该连接套接字
					ret = io_uring_register_files_update(&ring, sock_conn_fd, &sock_conn_fd, 1); //重新将该套接字加入到寄存器文件中，减少反复读取
					if (ret < 0) {
						fprintf(stderr, "io_uring_register_files_update "
							"failed: %d %d\n", sock_conn_fd, ret);
						exit(1);
					}
					registered_files[sock_conn_fd] = sock_conn_fd;
				}              
                users_[sock_conn_fd].init(sock_conn_fd, client_addr);
                if(timeoutMS_ > 0) {
                    timer_->add(sock_conn_fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[sock_conn_fd]));
                }       
                SetFdNonblock(sock_conn_fd); 
				add_socket_recv(&ring, sock_conn_fd, 0); //对该连接套接字添加读取
				add_accept(&ring, listenFd_, (struct sockaddr*)&client_addr, &client_len, 0); 
				//再继续对监听套接字添加监听
			}
			else if (type == READ) {
				int bytes_have_read = cqe->res;
				io_uring_cqe_seen(&ring, cqe);
				// if (debug) {
				// 	printf("Returned from READ is %d\n", bytes_have_read);
				// }

                ExtentTime_(&user_[fd]);

                if(timeoutMS_ > 0) { timer_->adjust(user_data->fd, timeoutMS_); }

                user_[fd].readBuff_.readPos_+=bytes_have_read;
				//(*users)[user_data->fd].m_read_idx += bytes_have_read;

				if(bytes_have_read > 0) {
					// LOG_INFO("deal with the client(%s)", 
					// 	inet_ntoa((*users)[user_data->fd].get_address()->sin_addr));

					//若监测到读事件，将该事件放入请求队列
					// server->m_pool->append_p(*users + user_data->fd);

                    threadpool_->AddTask(std::bind(&WebServer::OnProcess, this, user_[fd]));

					// while (!(*users)[user_data->fd].available_to_write) {}
					// (*users)[user_data->fd].available_to_write = false;

					// add_socket_writev(&ring, fd, 0);

					// if ((*users_timer)[user_data->fd].timer_exist)
					// {
					// 	server->adjust_timer(timer);
					// }
				}
				else {
					// if (debug) {
					// 	printf("closing by READ: fd%d...\n", user_data->fd);
					// }
					// if ((*users_timer)[user_data->fd].timer_exist) {
					// 	server->deal_timer(timer, user_data->fd);
					// }
					// else close(user_data->fd);
                    close(fd);
					add_accept(&ring, listenFd_, (struct sockaddr*)&client_addr, &client_len, 0);
				}
			}
			else if (type == WRITE) {
				int	ret = cqe->res;
				// io_uring_cqe_seen(&ring, cqe);
				// if (debug) {
				// 	printf("Returned from WRITE is %d\n", ret);
				// 	printf("Bytes to send is %d\n",(*users)[user_data->fd].bytes_to_send);
				// 	printf("Bytes have send is %d\n",(*users)[user_data->fd].bytes_have_send);
				// }
				// util_timer* timer = (*users_timer)[user_data->fd].timer;
                ExtentTime_(&user_[fd]);

				if (ret < 0) {
					if(ret == -11) add_socket_writev(&ring, fd, 0);
					continue;
				}

				if (user_[fd].ToWriteBytes() == 0) {
					// LOG_INFO("send data to the client(%s)", 
					// 	inet_ntoa((*users)[user_data->fd].get_address()->sin_addr));

                    if(user_[fd].IsKeepAlive()) {
                        OnProcess(user_[fd]);
                    }
					// if (user_[fd].m_linger) {
					// 	(*users)[user_data->fd].init();
					// 	if ((*users_timer)[user_data->fd].timer_exist)
					// 	{
					// 		server->adjust_timer(timer);
					// 	}
					// 	add_socket_recv(&ring, user_data->fd, 0);
					// }
					else
					{
						// if (debug) {
						// 	printf("closing by WRITE: fd%d...\n", user_data->fd);
						// }
						// if ((*users_timer)[user_data->fd].timer_exist) {
						// 	server->deal_timer(timer, user_data->fd);
						// }
						// else close(user_data->fd);
                        // CloseConn_(client);
                        //     assert(client);
                        // LOG_INFO("Client[%d] quit!", client->GetFd());
                        user_[fd].Close();
                        user_[fd].unmap();
					}
					// user_[fd].unmap();
					add_accept(&ring, listenFd_, (struct sockaddr*)&client_addr, &client_len, 0);
					//如果关闭后不添加accept会导致重用处在CLOSE_WAIT状态的文件描述符导致无法读写
				}
				else
				{
					deal_with_write(*users, fd, ret);
					add_socket_writev(&ring, fd, 0);
				}
			}
		}
            // /* 处理事件 */
            // int fd = epoller_->GetEventFd(i);
            // uint32_t events = epoller_->GetEvents(i);
            // if(fd == listenFd_) {
            //     DealListen_();
            // }
            // else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
            //     assert(users_.count(fd) > 0);
            //     CloseConn_(&users_[fd]);
            // }
            // else if(events & EPOLLIN) {
            //     assert(users_.count(fd) > 0);
            //     DealRead_(&users_[fd]);
            // }
            // else if(events & EPOLLOUT) {
            //     assert(users_.count(fd) > 0);
            //     DealWrite_(&users_[fd]);
            // } else {
            //     LOG_ERROR("Unexpected event");
            // }
        }
    }
}

void WebServer::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if(timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

void WebServer::DealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd <= 0) { return;}
        else if(HttpConn::userCount >= MAX_FD) {
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);
    } while(listenEvent_ & EPOLLET);
}

void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if(ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    OnProcess(client);
}

void WebServer::OnProcess(HttpConn* client) {
    if(client->process()) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
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
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}


