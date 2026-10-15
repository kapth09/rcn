CC = gcc
CFLAGS = -Wall -Wextra -std=gnu2x
SRCS = *.c
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
