#include "game.h"
#include "util/util.h"
#include "openrct2.h"

extern int gGzipLevel;

int sv6_unrle(const char **argv, int argc)
{
    if (argc < 2) {
        log_fatal("requires at least two arguments");
        return -1;
    }
    const char *input = argv[0];
    const char *output = argv[1];
    if (!openrct2_initialise()) {
        log_fatal("failed to initialize");
        return -1;
    }

    SDL_RWops* rw = SDL_RWFromFile(input, "rb");
    if (rw == NULL) {
            log_error("unable to open %s", input);
            return -1;
    }

    bool result = game_load_sv6(rw);
    SDL_RWclose(rw);

    gUseRLE = false;


    FILE* temp = tmpfile();
    if (!temp) {
            log_warning("Failed to create temporary file to save map.");
            return -1;
    }
    rw = SDL_RWFromFP(temp, SDL_TRUE);
    scenario_save(rw, 0x80000000);
    int size = (int)SDL_RWtell(rw);
    uint8 *buffer = malloc(size);
    SDL_RWseek(rw, 0, RW_SEEK_SET);
    if (SDL_RWread(rw, &buffer[0], size, 1) == 0) {
            log_warning("Failed to read temporary map file into memory.");
            SDL_RWclose(rw);
            return -1;
    }
    SDL_RWclose(rw);



    rw = SDL_RWFromFile(output, "wb");
    if (rw == NULL) {
            log_error("unable to open %s", output);
            return -1;
    }
    if (SDL_RWwrite(rw, &buffer[0], size, 1) == 0) {
            log_warning("Failed to write sv6 map file into the output file.");
            SDL_RWclose(rw);
            return -1;
    }
    SDL_RWclose(rw);
    return 0;
}
