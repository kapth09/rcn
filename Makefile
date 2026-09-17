CC = gcc
CFLAGS = -Wall -Wextra
SRCS = ./rcn.c ./rcn_daemon.c ./rcn_server.c ./rcn_client.c ./rcn_relay.c ./rcn_util.c ./rcn_arg.c ./rcn_evdev.c ./rcn_peer.c
TARGET = rcn

all:
	$(CC) $(SRCS) $(CFLAGS) -o $(TARGET)

debug: CFLAGS += -g
debug:
	$(CC) $(SRCS) $(CFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all debug clean
