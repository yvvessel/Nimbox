CC = gcc
RC = windres
CFLAGS = -std=c11 -Wall -Wextra -O2 -Isrc -finput-charset=UTF-8 -fexec-charset=UTF-8
LIBS = -mwindows -lcomctl32 -lshell32 -ladvapi32 -ldwmapi -luxtheme -lgdiplus -lole32

SRC = src/cJSON.c src/snippet.c src/storage.c src/settings.c src/autostart.c src/theme.c src/gui.c src/hook.c src/injector.c src/main.c
HEADERS = src/cJSON.h src/snippet.h src/storage.h src/settings.h src/autostart.h src/theme.h src/gui.h src/hook.h src/injector.h
TEST_SRC = src/cJSON.c src/snippet.c src/storage.c src/settings.c src/autostart.c src/main_test.c
TEST_HEADERS = src/cJSON.h src/snippet.h src/storage.h src/settings.h src/autostart.h
RES = res/resources.o
TARGET = Nimbox.exe
TEST_TARGET = test_storage.exe

.PHONY: all test clean

all: $(TARGET)

$(RES): res/app.rc res/app.manifest res/app.ico assets/branding/NimboxLogo_240.png
	$(RC) -i res/app.rc -o $(RES)

$(TARGET): $(SRC) $(HEADERS) $(RES)
	$(CC) $(CFLAGS) $(SRC) $(RES) -o $(TARGET) $(LIBS)

test: $(TEST_SRC) $(TEST_HEADERS)
	$(CC) $(CFLAGS) $(TEST_SRC) -o $(TEST_TARGET) -lshell32 -ladvapi32
	cmd /c "$(TEST_TARGET)"

clean:
	cmd /c "del /Q $(TARGET) $(TEST_TARGET) *.o 2>nul & del /Q res\resources.o 2>nul" || exit 0
