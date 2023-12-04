CXX = g++
# CFLAGS = -std=c++14 -O2 -Wall -g 
CXXFLAGS ?= -Wall -O2 -D_GNU_SOURCE -luring -std=c++2a -fcoroutines

TARGET = server4
OBJS = ../code/log/*.cpp ../code/pool/*.cpp ../code/timer/*.cpp \
       ../code/http4/*.cpp  \
       ../code/buffer/*.cpp ../code/main.cpp

all: $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o ../bin/$(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)




