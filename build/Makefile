CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g 

TARGET = server
OBJS = ../code/log/*.cc ../code/pool/*.cc ../code/timer/*.cc \
       ../code/http/*.cc ../code/server/*.cc \
       ../code/buffer/*.cc ../code/main.cc

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o ../bin/$(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)
