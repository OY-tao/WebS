CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g  

TARGET = server_lfq
OBJS = ../code/log/*.cpp ../code/pool2/*.cpp ../code/timer/*.cpp \
       ../code/http2/*.cpp ../code/server_lfq/*.cpp \
       ../code/buffer/*.cpp  main.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o ../bin/$(TARGET)  -pthread -lmysqlclient 

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)




