CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -Isrc
LIBS = -lshell32

SRC = src/cJSON.c src/snippet.c src/storage.c src/main_test.c
TARGET = test_storage.exe

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	del /Q $(TARGET) *.o 2>nul || rm -f $(TARGET) *.o
