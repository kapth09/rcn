CC = gcc
CFLAGS = -Wall -Wextra -g
SRCS = ./rcn.c ./rcn_daemon.c ./rcn_epoll.c ./rcn_server.c ./rcn_client.c ./rcn_relay.c ./rcn_util.c ./rcn_arg.c ./rcn_device.c ./rcn_peer.c ./rcn_stream.c
TARGET = rcn

all:
	$(CC) $(SRCS) $(CFLAGS) -o $(TARGET)

debug:
	$(CC) $(SRCS) $(CFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all debug clean
