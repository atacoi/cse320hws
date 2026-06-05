#include "png_print_helpers.h"
#include "png_chunks.h"
#include "global.h"
#include "util.h"

#include <stdio.h> 
#include <string.h>

void printSummary(const char* filename, png_chunk_t *summary) {
    PRINT_CHUNK_SUMMARY_HEADER(filename);

    /* Printing each chunk */
    png_chunk_t *curr = summary;
    int i = 0;
    while(curr && memcmp(curr->type, "IEND", 4) != 0) {
        PRINT_CHUNK_INFO(i, *curr);
        curr += 1;
        i += 1;
    }

    // cleanup
    if(curr && memcmp(curr->type, "IEND", 4) == 0) {
        PRINT_CHUNK_INFO(i, *curr);
    }
}

void printPlteColors(const char* filename, png_color_t *plteColors, size_t numColors) {
    PRINT_PALETTE_HEADER(filename);
    PRINT_PALETTE_COUNT(numColors);

    for(size_t i = 0; i < numColors; i++) {
        PRINT_PALETTE_COLOR(i, plteColors[i].r, plteColors[i].g, plteColors[i].b);
    }
}