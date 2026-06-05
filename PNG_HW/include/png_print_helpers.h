#ifndef PNG_PRINT_HELPERS
#define PNG_PRINT_HELPERS

#include "png_chunks.h"

#include <stdlib.h>

enum flagsSet {
    F_FLAG = 1 << 0,
    H_FLAG = 1 << 1,
    S_FLAG = 1 << 2,
    P_FLAG = 1 << 3,
    I_FLAG = 1 << 4,
    E_FLAG = 1 << 5,
    D_FLAG = 1 << 6,
    M_FLAG = 1 << 7, 
    W_FLAG = 1 << 8, 
    G_FLAG = 1 << 9, 
    MISSING_FLAG = 1 << 10,
    UNKNOWN_FLAG = 1 << 11,
};

enum fErrorFlags {
    OPEN_FILE_ERROR_FLAG = 1 << 0,
    FILE_REQUIRES_FLAG   = 1 << 1,
};

/* Prints the given chunk summary using the global macros */
void printSummary(const char* filename, png_chunk_t *summary);

/* Prints the given plte colors using the global macros */
void printPlteColors(const char* filename, png_color_t *plteColors, size_t numColors);

#endif 