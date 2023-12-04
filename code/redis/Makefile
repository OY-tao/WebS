CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g 

TARGET = redis
OBJS = redis.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o ../bin/$(TARGET)  -lhiredis

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)