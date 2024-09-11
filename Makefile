CC = gcc
CFLAGS = -Wall -Wextra
TARGETS = tcp_client tcp_server

all: $(TARGETS)

tcp_client: tcp_client.c
	$(CC) $(CFLAGS) -o $@ $<

tcp_server: tcp_server.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean
