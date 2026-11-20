CC     := gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror -g -Iinclude
LIBS   := -lraylib -lopengl32 -lgdi32 -lwinmm -lm

all: sablay_week3.exe

app.res: assets/app.rc assets/icon.ico
	windres assets/app.rc -O coff -o $@

sablay_week3.exe: src/core.c src/main.c include/game.h app.res
	$(CC) $(CFLAGS) src/core.c src/main.c app.res -o $@ $(LIBS)

test: sablay_week3.exe
	./sablay_week3.exe --smoke-test

clean:
	if exist sablay_week3.exe del /f /q sablay_week3.exe
	if exist app.res del /f /q app.res
