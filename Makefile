# NetChess Root Academic Makefile (IT4062 HUST)
CC = gcc
CFLAGS = -Wall -Wextra -O2

ifeq ($(OS),Windows_NT)
    LIBS = -lws2_32
    SERVER_BIN = server/server.exe
    CLIENT_BIN = client/client_cli/client_cli.exe
else
    LIBS = -lpthread
    SERVER_BIN = server/server
    CLIENT_BIN = client/client_cli/client_cli
endif

.PHONY: all clean test

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): server/src/*.c server/include/*.h
	$(CC) $(CFLAGS) -Iserver/include server/src/*.c -o $(SERVER_BIN) $(LIBS)

$(CLIENT_BIN): client/client_cli/client_cli.c
	$(CC) $(CFLAGS) client/client_cli/client_cli.c -o $(CLIENT_BIN) $(LIBS)

test: all
	$(CC) $(CFLAGS) -Iserver/include server/src/chess_engine.c test/test_engine.c -o test/test_engine
	./test/test_engine
	python3 test/test_protocol.py
	python3 test/test_lobby_sync.py

clean:
	rm -f server/server server/server.exe client/client_cli/client_cli client/client_cli/client_cli.exe test/test_engine test/test_engine.exe *.o
