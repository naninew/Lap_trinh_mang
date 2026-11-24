CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
SRCS = src/main.c src/network.c src/chess_engine.c src/account.c src/game_room.c src/file_transfer.c src/sha1.c src/base64.c
TARGET = server

ifeq ($(OS),Windows_NT)
    LIBS = -lws2_32
    TARGET_BIN = $(TARGET).exe
else
    LIBS = -lpthread
    TARGET_BIN = $(TARGET)
endif

all: $(TARGET_BIN)

$(TARGET_BIN): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET_BIN) $(LIBS)

clean:
	rm -f $(TARGET) $(TARGET).exe *.o
