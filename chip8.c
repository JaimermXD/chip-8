/* -------------------------------------------------------------------------- */
/*                                  INCLUDES                                  */
/* -------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

/* -------------------------------------------------------------------------- */
/*                                   MACROS                                   */
/* -------------------------------------------------------------------------- */

#ifdef DEBUG
#define debug_print(...) do { printf(__VA_ARGS__); } while (false)
#else
#define debug_print(...) do {} while (false)
#endif

/* -------------------------------------------------------------------------- */
/*                                    DATA                                    */
/* -------------------------------------------------------------------------- */

// Types
typedef enum {
    RUNNING,
    PAUSED,
    QUIT
} state_t;

// Config
uint32_t width;
uint32_t height;
uint32_t scale;

// SDL
SDL_Window *window;
SDL_Renderer *renderer;

// Emulator
state_t state;
uint8_t memory[4096];
uint8_t V[16];
uint16_t stack[16];
uint8_t sp;
uint16_t PC;
uint16_t I;
uint8_t DT;
uint8_t ST;
bool display[64 * 32];
bool keypad[16];
char *rom;

// Constants
const uint16_t entry_point = 0x200;
const uint8_t font[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

/* -------------------------------------------------------------------------- */
/*                                   CONFIG                                   */
/* -------------------------------------------------------------------------- */

/**
 * Set config defaults and override from args
 * @param argc Number of args
 * @param argv Args list
 * @return Whether setup was successful
*/
bool set_config(int argc, char **argv) {
    // Set defaults
    width = 64;
    height = 32;
    scale = 15;

    // TODO: Override from args
    for (int i = 0; i < argc; i++) {
        (void)argv[i];
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*                                     SDL                                    */
/* -------------------------------------------------------------------------- */

/**
 * Initialize SDL susbsytems and components
 * @return Whether initialization was successful
*/
bool init_sdl() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow(
        "CHIP-8 Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width * scale,
        height * scale,
        0
    );
    if (!window) {
        fprintf(stderr, "Unable to create window: %s\n", SDL_GetError());
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Unable to create renderer: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

/**
 * SDL event handler
*/
void handle_events() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                // Quit emulator
                state = QUIT;
                break;
            
            case SDL_KEYDOWN:
                switch (event.key.keysym.scancode) {
                    case SDL_SCANCODE_ESCAPE:
                        // Quit emulator (ESC key)
                        state = QUIT;
                        break;
                    
                    case SDL_SCANCODE_SPACE:
                        // Toggle pause
                        if (state == PAUSED) {
                            state = RUNNING;
                            printf("[INFO] Unpaused\n");
                        } else {
                            state = PAUSED;
                            printf("[INFO] Paused\n");
                        }
                        break;

                    default:
                        break;
                }
                break;
            
            default:
                break;
        }
    }
}

/**
 * Cap framerate to 60 FPS
 * @param diff Main loop execution time difference
*/
void cap_framerate(uint64_t diff) {
    double elapsed = diff / (double)SDL_GetPerformanceFrequency() * 1000.0;
    SDL_Delay(SDL_floor(16.6667 - elapsed));
}

/**
 * Final SDL cleanup
*/
void clean_sdl() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

/* -------------------------------------------------------------------------- */
/*                                  EMULATOR                                  */
/* -------------------------------------------------------------------------- */

/**
 * Load ROM file into memory
 * @param rom_name ROM file name
 * @return Whether loading was successful
*/
bool load_rom(char *rom_name) {
    // Open ROM file
    FILE *f = fopen(rom_name, "rb");
    if (!f) {
        fprintf(stderr, "Unable to open ROM file '%s'\n", rom_name);
        return false;
    }

    // Get size
    fseek(f, 0, SEEK_END);
    size_t rom_size = ftell(f);
    size_t max_size = sizeof(memory) - entry_point;
    rewind(f);

    // Check size
    if (rom_size > max_size) {
        fprintf(stderr, "ROM file '%s' is too large\n", rom_name);
        return false;
    }

    // Load ROM
    if (fread(&memory[entry_point], rom_size, 1, f) != 1) {
        fprintf(stderr, "Unable to read ROM file '%s' into memory\n", rom_name);
        return false;
    }
    rom = rom_name;

    // Close ROM file
    fclose(f);

    return true;
}

/**
 * Initialize CHIP-8 emulator and set up initial state
 * @return Whether initialization was successful
*/
bool init_emulator(char *rom_name) {
    // Set defaults
    state = RUNNING;
    PC = entry_point;

    // Load font
    memcpy(&memory[0], font, sizeof(font));

    // Load ROM file
    return load_rom(rom_name);
}

/**
 * Execute current instruction
*/
void execute_instruction() {
    // Fetch current opcode and increment PC for next one
    uint16_t opcode = (memory[PC] << 8) | memory[PC + 1];
    PC += 2;

    // Decode instruction
    uint16_t NNN = opcode & 0x0FFF;
    uint8_t NN = opcode & 0x00FF;
    uint8_t N = opcode & 0x000F;
    uint8_t X = (opcode & 0x0F00) >> 8;
    uint8_t Y = (opcode & 0x00F0) >> 4;

    // Execute instruction
    debug_print("[DEBUG] Opcode=0x%04X @ PC=0x%04X - ", opcode, PC - 2);
    switch (opcode >> 12) {
        default:
            debug_print("Unimplemented opcode\n");
            break;
    }
}

/* -------------------------------------------------------------------------- */
/*                                    MAIN                                    */
/* -------------------------------------------------------------------------- */

/**
 * Application entry point
 * @param argc Number of args
 * @param argv Args list
 * @return Exit code
*/
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s ROM_NAME\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!set_config(argc, argv)) return EXIT_FAILURE;
    if (!init_emulator(argv[1])) return EXIT_FAILURE;
    if (!init_sdl()) return EXIT_FAILURE;

    // Main loop
    while (state != QUIT) {
        handle_events();

        if (state == PAUSED) continue;

        uint64_t start = SDL_GetPerformanceCounter();
        execute_instruction();
        uint64_t end = SDL_GetPerformanceCounter();

        cap_framerate(end - start);        
    }

    clean_sdl();

    return EXIT_SUCCESS;
}