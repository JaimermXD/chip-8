CFLAGS = -std=c99 -Wall -Wextra -pedantic
SDLCONF = `sdl2-config --cflags --libs`

all: executable

debug: CFLAGS += -DDEBUG
debug: executable

executable:
	$(CC) chip8.c -o chip8.out $(CFLAGS) $(SDLCONF)

clean:
	rm chip8.out