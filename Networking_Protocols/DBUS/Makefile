#
# Makefile
#

CC=gcc
all: add-server add-client

%.o: %.c 
	gcc -Wall -c `pkg-config --cflags dbus-1` $< -o $@

add-server: add-server.o
	$(CC) add-server.o -o add-server `pkg-config --libs dbus-1` 

add-client: add-client.o
	$(CC) add-client.o -o add-client `pkg-config --libs dbus-1` 

.PHONY: clean
clean:
	rm *.o add-server add-client

