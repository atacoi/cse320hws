#ifndef PNG_WRITER_H
#define PNG_WRITER_H

#include <stdio.h>
#include <stdlib.h>
#include "png_chunks.h"

/* Write the new PLTE chunk and IDAT chunk to the output file while preserving the ordering of the other chunks */
/* We assume that both input files start right after the signature */
/* The second input file (iifptr) may be null in the case of steg. */
/* We assume that the idat buffer is decompressed and we must compress it */
/* Returns 0 on success, -1 on error */
int _png_write_chunks_to_file(FILE *ifptr, FILE *iifptr, FILE *ofptr, 
                              png_color_t *out_colors, size_t out_count, 
                              uint8_t *idatBuffer,     size_t idatBufferSize);

/* Writes the chunks from the initial position of ifptr to the ofptr until the desired chunk type is reached */
/* Or IEND is found */
/* The function will fill in the last encountered chunk type into the out_chunk_type (assumed to be allocated somewhere else) */
/* Returns 0 on success, -1 on error */
int _png_write_chunks_to_file_up_to(FILE *ifptr, FILE *ofptr, const char *chunk_type, char *out_chunk_type);

/* Writes the given chunk to the input file */
/* Returns the number of bytes written on success, -1 on error */
int _png_write_chunk_to_file(FILE *ofptr, png_chunk_t *chunk);

/* Writes the given fields to the out_chunk */
/* Returns 0 on success and -1 on error */
int _png_write_chunk_from_fields(uint32_t length, const char *type, uint8_t *data, png_chunk_t *out_chunk);

#endif