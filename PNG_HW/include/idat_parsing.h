#ifndef IDAT_PARSING_H
#define IDAT_PARSING_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "png_chunks.h"

/* Extracts all IDAT chunks from the given fptr, */
/* Places them into a single buffer */
/* Then uses zlib to decompress them into output_buffer */
/* Finally, the IDAT buffer is validated with _validate_idat_buffer() before writing to output */
/* Return 0 on success, -1 on error */
/* The caller is responsible for freeing the buffer by calling free() */
int _png_decompress_all_idat_chunks(FILE *fptr, uint8_t **output_buffer, size_t *buffer_size,
                                    uint32_t image_width, uint32_t image_height, uint8_t color_type);

/* Validates that the DECOMPRESSED idat buffer is a multiple of bpp and that each filter byte is a valid filter type */
/* Assumes that the image is 8-bit */
/* Returns 0 on success, -1 on error */
int _validate_idat_buffer(uint8_t *idat_buffer, size_t idat_buffer_size, 
                          uint32_t image_width, uint32_t image_height, 
                          uint8_t color_type);

#endif