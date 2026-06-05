#ifndef PNG_READER_H
#define PNG_READER_H

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include "png_chunks.h"

enum read_flags {
    validate_max_length = 1, 
    validate_ascii_type = 2, 
    validate_crc        = 4, // set the crc to boolean
    dont_check_crc      = 8, // ignore crc checks 
    free_data           = 16,
};

/* Read all chunks from a PNG file and return summary */
/* Returns 0 on success, -1 on error */
/* The caller is responsible for freeing the summary array with free() */
int png_summary(const char *filename, png_chunk_t **out_summary);

/* Opens a PNG file and validates signature */
FILE *png_open(const char *path);

/* Reads the next chunk from the file */
int png_read_chunk(FILE *fp, png_chunk_t *out);

/* Reads the next summary chunk from the file */
int _png_read_chunk_summary(FILE *fp, png_chunk_t *out);

/* Helper for png_read_chunk that can skip certain stages based on flags above */
int _png_read_chunk_helper(FILE *fp, png_chunk_t *out, uint8_t flags);

/* Frees memory allocated inside png_chunk_t */
void png_free_chunk(png_chunk_t *chunk);

/* Extract IHDR chunk and parse it */
/* Returns 0 on success, -1 on error/not found. */
int png_extract_ihdr(FILE *fp, png_ihdr_t *out);

/* Extract (first) PLTE chunk and parse it into an array of colors */
/* Returns 0 on success, -1 on error/not found. */
int png_extract_plte(FILE *fp, png_color_t **out_colors, size_t *out_count);

/* Finds and extracts the desired chunk type based on the "read_chunk" function */
/* Returns 0 on success, -1 on error/not found. */
/* The caller is responsible for freeing the chunk with png_free_chunk() */
int _png_search_type(FILE *fp, png_chunk_t *out, const char *chunk_type);

#endif
