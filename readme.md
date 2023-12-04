# WebServer
用C++实现的高性能WEB服务器，经过webbenchh压力测试可以实现上万的QPS

## 功能
* 1、实现半同步半反应堆线程池的高并发模型；
* 2、实现get/post两种请求解析，提供文件上传下载功能；
* 3、实现同步/异步日志系统，记录服务器运行状态；
* 4、实现定时器，关闭超时的非活动连接；
* 5、实现数据库连接池，提供登录注册功能；
* 6、实现压力测试，验证服务器承载上万的并发连接数据交换能力；
* 7、实现DNA编码，提供将数据进行DNA编码服务；

## 环境要求
* Linux
* C++14
* MySql

## 运行
make
./bin/server

## 压力测试
![image-webbench](/readme.assest/%E5%8E%8B%E5%8A%9B%E6%B5%8B%E8%AF%95.png)
```bash
./webbench-1.5/webbench -c 100 -t 10 http://ip:port/
./webbench-1.5/webbench -c 1000 -t 10 http://ip:port/
./webbench-1.5/webbench -c 5000 -t 10 http://ip:port/
./webbench-1.5/webbench -c 10000 -t 10 http://ip:port/

./webbench -c 100 -t 10 http://localhost:1999/
```
* QPS 10000+


