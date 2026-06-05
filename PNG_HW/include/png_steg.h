#ifndef PNG_STEG_H
#define PNG_STEG_H

#include "png_chunks.h"

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

typedef struct {
    uint32_t color;
    uint8_t originalIndex;
} ColorIndex;

/* Encode a secret string into the LSBs of PNG image data */
/* Returns 0 on success, -1 on error */
int png_encode_lsb(const char *input_path, const char *output_path, const char *secret);

/* Extract a secret string from the LSBs of PNG image data */
/* Returns length of extracted string on success, -1 on error */
/* The extracted string is written to 'out', which must be at least 'max_len' bytes */
int png_extract_lsb(const char *input_path, char *out, size_t max_len);

/* Fills in the pairs array based on the given transformed colors */
/* Pairs array must have a size of < 256 */
/* Returns the number of colors we need to duplicate on success and -1 on error */
int _generate_pairs_from_current_colors(ColorIndex *transformed_colors, size_t original_size, uint8_t *pairs);

/* Extracts the PLTE, duplicates any singular colors and creates the pair mapping */
/* Returns 0 on success, -1 on error */
/* Pairs array must have a size of < 256 */
/* Color must free out_colors */
int _generate_pairs(FILE *fptr, uint8_t *pairs, png_color_t **out_colors, size_t *out_count);

/* Comparators for generate pairs */

int _colorindex_color_comparator(const void *ptr1, const void *ptr2);

int _colorindex_ogindex_comparator(const void *ptr1, const void *ptr2);


#endif

