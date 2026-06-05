#include "png_chunks.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

/* Parse IHDR data from chunk */
/* Chunk must be an IHDR chunk with length 13 */
int png_parse_ihdr(const png_chunk_t *chunk, png_ihdr_t *out)
{
    /* null pointers */
    if(!chunk || !out) return -1;

    /* invalid chunk type */
    if(memcmp(chunk->type, "IHDR", 4) != 0) return -1;

    /* invalid chunk length */
    if(chunk->length != 13) return -1;

    /* invalid chunk data */
    uint8_t *data = chunk->data;
    if(!data) {
        return -1;
    }

    uint32_t width, height;
    uint8_t bit_depth, color_type, compression, filter, interlace;

    width = read_u32_be(data);
    data += 4;

    height = read_u32_be(data);
    data += 4;

    /* Zero is an invalid value */
    if(!width || !height) return -1;

    /* Maximum is 2^31 - 1 */
    uint32_t maxwh = (uint32_t) ((1u << 31) - 1u);
    if(width > maxwh || height > maxwh) return -1;

    bit_depth = *data;
    data += 1;

    color_type = *data;
    data += 1;

    int correctBitDepth = 0; // 1 if the right bit_depth is set for given color_type 

    /* Some bit depths cannot appear for certain color types; refer to https://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html */
    switch(color_type) {
        case 0:
            /* must be powers of 2 from 1 to 16 */
            for(int i = 1; i <= 16; i <<= 1) {
                if(bit_depth == i) {
                    correctBitDepth = 1;
                    break;
                }
            }
            break;

        case 2:
            /* must be powers of 2 from 8 to 16 */
            for(int i = 8; i <= 16; i <<= 1) {
                if(bit_depth == i) {
                    correctBitDepth = 1;
                    break;
                }
            }
            break;

        case 3:
            /* must be powers of 2 from 1 to 8 */
            for(int i = 1; i <= 8; i <<= 1) {
                if(bit_depth == i) {
                    correctBitDepth = 1;
                    break;
                }
            }
            break;

        case 4:
            /* must be powers of 2 from 1 to 8 */
            for(int i = 8; i <= 16; i <<= 1) {
                if(bit_depth == i) {
                    correctBitDepth = 1;
                    break;
                }
            }
            break;

        case 6:
            /* must be powers of 2 from 1 to 8 */
            for(int i = 8; i <= 16; i <<= 1) {
                if(bit_depth == i) {
                    correctBitDepth = 1;
                    break;
                }
            }
            break;

        default:    
            return -1; // not allowed 
    }

    if(!correctBitDepth) return -1;

    compression = *data;
    data += 1;

    /* Compression must be 0 */
    if(compression != 0) return -1;

    filter = *data;
    data += 1;

    /* Filter must be 0 */
    if(filter != 0) return -1;

    interlace = *data;

    /* Interlace must be 1 or 0 */
    if(interlace != 0 && interlace != 1) return -1;

    /* Now its safe to write */

    out->width       = width;
    out->height      = height;
    out->bit_depth   = bit_depth;
    out->color_type  = color_type;
    out->compression = compression;
    out->filter      = filter;
    out->interlace   = interlace;

    return 0;
}

/* Parse PLTE data from chunk into an allocated array of colors */
/* Chunk must be a PLTE chunk with length multiple of 3 */
int png_parse_plte(const png_chunk_t *chunk, png_color_t **out_colors, size_t *out_count)
{
    /* NULL Pointer Check */
    if(!chunk || !out_colors || !out_count) return -1;

    uint8_t *data = chunk->data;
    if(!data) return -1;

    /* Is it a PLTE chunk? */ 
    if(memcmp(chunk->type, "PLTE", 4) != 0) return -1;

    /* Length is not divisible by 3 */
    if(chunk->length % 3 != 0) return -1;

    /* There are between 1 and 256 palette entires */
    int numOfEntires = chunk->length / 3;
    if(numOfEntires < 1 || numOfEntires > 256) return -1;

    png_color_t *colors = (png_color_t*) malloc(sizeof(png_color_t) * numOfEntires);

    /* malloc failed */
    if(!colors) return -1;

    for(int i = 0; i < numOfEntires; i++) {
        colors[i].r = *data;
        colors[i].g = *(data+1);
        colors[i].b = *(data+2);
        data += 3;
    }

    *out_colors = colors;
    *out_count = numOfEntires;

    return 0;
}