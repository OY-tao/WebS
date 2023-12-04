#include <unistd.h>
#include "server/webserver.h"
#include <string>
#include <cstring>


int main() {
    int srcFd = open("/home/siat/yt/WS/WebServer-master/resources/login.html", O_RDONLY);
    if(srcFd < 0) { 
        std::cout<<100;
        return 1; 
    }
    int iovCnt_;
    struct iovec iov_;
    struct stat mmFileStat_= { 0 };
    char* mmFile_; 
    stat("/home/siat/yt/WS/WebServer-master/resources/login.html", &mmFileStat_);
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {
        std::cout<<300;
        return 1; 
    }
    mmFile_ = (char*)mmRet;
    std::cout<<mmFile_[100];
    close(srcFd);
    std::string s(mmFile_);
    std::string t="username";
    int i = s.find(t); 
    std::string s2(mmFile_+i);
    std::cout<<s2<<std::endl;
    //2479
    return 1; 
} 