CC = gcc
CFLAGS = -Wall -std=gnu99 -g -lpthread -lcurses -ldl

INC = -I./basetools

C_INCLUDE_PATH += .
COMPILER_PATH += .
INSTALL_PATH = ../bin

DIR_BASETOOLS = ./basetools
DIR_MODULE = ./module
DIR_APP = ./app
DIR_TEST = ./moduletest

SOURCE := $(wildcard *.c) $(wildcard ${DIR_BASETOOLS}/*.c) $(wildcard ${DIR_TEST}/*.c)
OBJS := $(patsubst %.c,%.o,$(SOURCE))
TARGET = test
all:test

objs:$(OBJS)

server:Server.o $(filter-out Client.o,$(OBJS))
	$(CC) $(INC) -o Server $^ $(LDFLAGS) $(CFLAGS) 

client:Client.o $(filter-out Server.o,$(OBJS))
	$(CC) $(INC) -o Client $^ $(LDFLAGS) $(CFLAGS) 
test:$(OBJS)
	$(CC) $(INC) -o test $^ $(LDFLAGS) $(CFLAGS) 
%.o : %.c
	$(CC) $(INC) -c  $^ -o $@ $(CFLAGS)

install:server client 
	$(shell if [ ! -d $(INSTALL_PATH)/server ]; then mkdir -p $(INSTALL_PATH)/server; fi;)
	cp Server $(INSTALL_PATH)/server/
	$(shell if [ ! -d $(INSTALL_PATH)/client ]; then mkdir -p $(INSTALL_PATH)/client; fi;)
	cp Client $(INSTALL_PATH)/client/

.PHONY:clean

clean:
	rm *.o -f
	@rm ${DIR_BASETOOLS}/*.o -f
	@rm ${DIR_MODULE}/*.o -f
	@rm ${DIR_APP}/*.o -f
	@rm ${DIR_TEST}/*.o -f
	rm Server -f
	rm Client -f
	rm *.db3 -f
	rm file/ -rf
