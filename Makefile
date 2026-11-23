CC = gcc
RC = windres
CFLAGS = -std=c11 -Wall -Wextra -O2 -Isrc
LIBS = -mwindows -lcomctl32 -lshell32

SRC = src/cJSON.c src/snippet.c src/storage.c src/gui.c src/main.c
RES = res/resources.o
TARGET = Nimbox.exe

all: $(TARGET)

$(RES): res/app.rc res/app.manifest
	$(RC) -i res/app.rc -o $(RES)

$(TARGET): $(SRC) $(RES)
	$(CC) $(CFLAGS) $(SRC) $(RES) -o $(TARGET) $(LIBS)

test:
	$(CC) $(CFLAGS) src/cJSON.c src/snippet.c src/storage.c src/main_test.c -o test_storage.exe -lshell32

clean:
	cmd /c "del /Q $(TARGET) $(RES) test_storage.exe *.o 2>nul" || exit 0
