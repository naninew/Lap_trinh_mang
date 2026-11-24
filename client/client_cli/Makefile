CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = client_cli

ifeq ($(OS),Windows_NT)
    LIBS = -lws2_32
    TARGET_BIN = $(TARGET).exe
else
    LIBS =
    TARGET_BIN = $(TARGET)
endif

all: $(TARGET_BIN)

$(TARGET_BIN): client_cli.c
	$(CC) $(CFLAGS) client_cli.c -o $(TARGET_BIN) $(LIBS)

clean:
	rm -f $(TARGET) $(TARGET).exe *.o
