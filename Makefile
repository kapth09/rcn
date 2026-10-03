CC = gcc
CFLAGS = -Wall -Wextra -std=gnu2x
SRCS = ./rcn.c ./rcn_daemon.c ./rcn_epoll.c ./rcn_relay.c ./rcn_util.c ./rcn_arg.c ./rcn_device.c ./rcn_peer.c ./rcn_stream.c
TARGET = rcn

default: CFLAGS += -g
default:
	$(CC) $(SRCS) $(CFLAGS) -o $(TARGET)

standard:
	$(CC) $(SRCS) $(CFLAGS) -o $(TARGET)

install: standard
	install -Dm 0755 rcn /usr/local/bin/$(TARGET)
	install -Dm 0755 README.md /usr/local/share/doc/$(TARGET)/README.md

uninstall:
	rm /usr/local/bin/$(TARGET)
	rm -rf /usr/local/share/doc/$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: default clean
