#include <iostream>
#include <string>
#include <thread>
#include "threadpool2.h"


void func1() {
    for(int i = 0; i < 100000;i++) {
        if(i%1000==0)
         std::cout << "FUN 1:" << i << std::endl;
    }
}

void func2() {
    for(int i = 0; i < 100000;i++) {
        if(i%1000==0)
         std::cout << "FUN 2:" << i << std::endl;
    }
}

void func3() {
    for(int i = 0; i < 100000;i++) {
        if(i%1000==0)
         std::cout << "FUN 3:" << i << std::endl;
    }
}

void func4() {
    for(int i = 0; i < 100000;i++) {
        if(i%1000==0)
         std::cout << "FUN 4:" << i << std::endl;
    }
}



int main() {

    ThreadPool2 threadpool_(8);
    int i=4;
    while(i){
        if(i%4==0)
        threadpool_.AddTask(std::bind(&func1));
        else if(i%4==1)
        threadpool_.AddTask(std::bind(&func2));
        else if(i%4==2)
        threadpool_.AddTask(std::bind(&func3));
        else if(i%4==3)
        threadpool_.AddTask(std::bind(&func4));
        i--;
    }
    while(1){

    }
    return 0;
}