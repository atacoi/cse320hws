#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

/* Big-endian helpers */
uint32_t read_u32_be(const uint8_t *buf);
/* Assumes that buffer has room for 4 more bytes */
void write_u32_be(uint32_t be, uint8_t *buf);

/* Safe read helpers */
int read_exact(FILE *fp, uint8_t *buf, size_t len);

/* Zlib compression/decompression helpers */
/* Decompress data using zlib inflate */
/* Returns 0 on success, -1 on error. Caller must free *out_data. */
int util_inflate_data(const uint8_t *compressed, size_t compressed_size,
                      uint8_t **out_data, size_t *out_size);

/* Compress data using zlib deflate with default settings */
/* Returns 0 on success, -1 on error. Caller must free *out_data. */
int util_deflate_data(const uint8_t *data, size_t data_size,
                      uint8_t **out_data, size_t *out_size);

/* Compress data using zlib deflate with PNG-compatible settings */
/* PNG requires windowBits = 15 (32KB window) */
/* Returns 0 on success, -1 on error. Caller must free *out_data. */
int util_deflate_data_png(const uint8_t *data, size_t data_size,
                          uint8_t **out_data, size_t *out_size);

/* Convert the given string to a uint32_t */
/* Returns 0 on success, -1 on error. */
int str_to_u32(const char *s, uint32_t *out);

/* Returns the number of samples per pixel depending on the color type */
/* sample = byte for 8 bit images */
/* A spp of 0 indicates an "unknown" color type */
uint8_t _get_samples_per_pixel(uint8_t color_type);

#endif
