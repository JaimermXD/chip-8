CFLAGS = -std=c99 -Wall -Wextra -pedantic
SDLCONF = `sdl2-config --cflags --libs`

all:
	$(CC) chip8.c -o chip8.out $(CFLAGS) $(SDLCONF)