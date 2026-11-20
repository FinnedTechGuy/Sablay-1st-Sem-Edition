CC     := gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror -g -Iinclude
LIBS   := -lraylib -lopengl32 -lgdi32 -lwinmm -lm

all: sablay_week3.exe

sablay_week3.exe: src/core.c src/main.c include/game.h
	$(CC) $(CFLAGS) src/core.c src/main.c -o $@ $(LIBS)

test: sablay_week3.exe
	./sablay_week3.exe --smoke-test

clean:
	rm -f sablay_week3.exe
