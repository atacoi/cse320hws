#include "png_writer.h"
#include "png_reader.h"
#include "util.h"
#include "png_crc.h"

#include <string.h>

int _png_write_chunks_to_file(FILE *ifptr, FILE *iifptr, FILE *ofptr, 
                              png_color_t *out_colors, size_t out_count, 
                              uint8_t *idatBuffer,     size_t idatBufferSize)
{
    if(!ifptr || !ofptr || !idatBuffer) return -1;

    /* Write the png signature */

    const uint8_t PNG_SIGNATURE[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    if(fwrite(PNG_SIGNATURE, 1, 8, ofptr) != 8) return -1;

    /* First create the PLTE chunk (if def.) and the IDAT chunk(s) (if we reach max len in first chunk) */

    /* Collect the out_colors and out_count into a plte chunk */

    png_chunk_t plte = { .data = NULL }; 
    if(out_colors) {
        /* Copy all colors into a single buffer */

        uint8_t *actualColorData = (uint8_t*) malloc(sizeof(uint8_t) * out_count * 3);
        uint8_t *ptr = actualColorData;

        if(!actualColorData) return -1;

        for(size_t i = 0; i < out_count; i++) {
            ptr[0] = out_colors[i].r;
            ptr[1] = out_colors[i].g;
            ptr[2] = out_colors[i].b;
            ptr += 3;
        }        

        /* Write the fields to plte */

        if(_png_write_chunk_from_fields(out_count * 3, "PLTE", actualColorData, &plte) == -1) {
            free(actualColorData);
            return -1;
        }
    }

    /* Compress the idat buffer */
    uint8_t *compressedIdatBuffer   = NULL;
    size_t compressedIdatBufferSize = 0u;
    if(util_deflate_data_png(idatBuffer, idatBufferSize, &compressedIdatBuffer, &compressedIdatBufferSize) == -1) {
        png_free_chunk(&plte);
        return -1;
    }

    /* Collect idat buffer into idat chunks */

    png_chunk_t *idats = NULL;
    const uint32_t maxChunkLen = (uint32_t) ((1u << 31) - 1u);

    size_t totalIdatChunks = (compressedIdatBufferSize + maxChunkLen - 1) / maxChunkLen;
    if (totalIdatChunks == 0) totalIdatChunks = 1;
    
    idats = (png_chunk_t*) malloc(sizeof(png_chunk_t) * totalIdatChunks);
    if(!idats) {
        png_free_chunk(&plte);
        free(compressedIdatBuffer);
        return -1;
    }

    size_t idatBufferOffset = 0;
    size_t remainingBytes = compressedIdatBufferSize;

    size_t builtIdatChunks = 0;
    for(size_t i = 0; i < totalIdatChunks; i++) {
        size_t bytesToWrite = remainingBytes <= maxChunkLen ? remainingBytes : maxChunkLen;
        if(_png_write_chunk_from_fields(bytesToWrite, "IDAT", compressedIdatBuffer+idatBufferOffset, &(idats[i])) == -1) {
            png_free_chunk(&plte);
            free(idats);
            idats = NULL;
            free(compressedIdatBuffer);
            compressedIdatBuffer = NULL;
            return -1;
        }

        builtIdatChunks++;

        idatBufferOffset += bytesToWrite;
        remainingBytes -= bytesToWrite; 
    }
    /* Compiler warning thinks this variable is unused */
    (void)builtIdatChunks;

    /* Generate the iend chunk */
    png_chunk_t iend;
    if(_png_write_chunk_from_fields(0u, "IEND", NULL, &iend) == -1) {
        png_free_chunk(&plte);
        free(compressedIdatBuffer);
        return -1;
    }

    /* Now write each chunk to the output file */

    /* Write the rest of the chunks until IEND is found */

    /* Just a way to shorten the free stuff */
    #define FREE_ALL do { \
        png_free_chunk(&plte); \
        if(idats) { \
            /* IDAT chunk data points into compressedIdatBuffer: do NOT free per-chunk data */ \
            free(idats); \
            idats = NULL; \
        } \
        free(compressedIdatBuffer); \
        compressedIdatBuffer = NULL; \
    } while(0)


    char stoppedChunkType[5] = { '\0' };

    /* Skip the ihdr chunk of the second file */
    if(iifptr) {
        png_ihdr_t tmp;
        if(png_extract_ihdr(iifptr, &tmp) == -1) {
            FREE_ALL;
            return -1;
        }
    }

    /* Copy everything up to PLTE if type 3 */

    /* For the first input file */
    if(out_colors && _png_write_chunks_to_file_up_to(ifptr, ofptr, "PLTE", stoppedChunkType) == -1) {
        FREE_ALL;
        return -1;
    }

    /* For the second input file */
    if(iifptr && out_colors && _png_write_chunks_to_file_up_to(iifptr, ofptr, "PLTE", stoppedChunkType) == -1) {
        FREE_ALL;
        return -1;
    }

    /* Copy PLTE to output */
    if(out_colors && memcmp(stoppedChunkType, "PLTE", 4) == 0) {
        if(_png_write_chunk_to_file(ofptr, &plte) == -1) {
            FREE_ALL;
            return -1;
        }
    }

    /* For type 3, this will copy everything after PLTE and up to IDAT */
    /* For other types, this will copy everything after IHDR */ 
    /* (except for the first file since it will copy its IHDR )*/
 
    /* For the first input file */
    if(_png_write_chunks_to_file_up_to(ifptr, ofptr, "IDAT", stoppedChunkType) == -1) {
        FREE_ALL;
        return -1;
    }

    /* First input file has no IDAT chunk */
    if(memcmp(stoppedChunkType, "IDAT", 4) != 0) {
        FREE_ALL;
        return -1;
    }

    /* For the second input file */
    if(iifptr && _png_write_chunks_to_file_up_to(iifptr, ofptr, "IDAT", stoppedChunkType) == -1) {
        FREE_ALL;
        return -1;
    }

    /* Second input file has no IDAT chunk */
    if(memcmp(stoppedChunkType, "IDAT", 4) != 0) {
        FREE_ALL;
        return -1;
    } else {
        for(size_t i = 0; i < totalIdatChunks; i++) {
            if(_png_write_chunk_to_file(ofptr, &(idats[i])) == -1) {
                FREE_ALL;
                return -1;
            }
        }
    }

    /* Skip other idats and copy the rest of the chunks */

    /* For the first file */
    while(_png_write_chunks_to_file_up_to(ifptr, ofptr, "IDAT", stoppedChunkType) == 0
          && memcmp(stoppedChunkType, "IEND", 4) != 0) {}

    /* For the second file */
    if(iifptr) {
        while(_png_write_chunks_to_file_up_to(iifptr, ofptr, "IDAT", stoppedChunkType) == 0
            && memcmp(stoppedChunkType, "IEND", 4) != 0) {}
    }

    /* Write IEND chunk */
    if(memcmp(stoppedChunkType, "IEND", 4) == 0) {
        if(_png_write_chunk_to_file(ofptr, &iend) == -1) {
            FREE_ALL;
            return -1;
        }
    }

    FREE_ALL;

    #undef FREE_ALL

    return 0;
}

int _png_write_chunks_to_file_up_to(FILE *ifptr, FILE *ofptr, const char *chunk_type, char *out_chunk_type) 
{
    if(!ifptr || !ofptr || !chunk_type || !out_chunk_type) return -1;

    png_chunk_t curr = {0};
    while(   _png_read_chunk_helper(ifptr, &curr, dont_check_crc) == 0 
          && memcmp(curr.type, chunk_type, 4) != 0 
          && memcmp(curr.type, "IEND", 4) != 0) {
        if(_png_write_chunk_to_file(ofptr, &curr) == -1) {
            png_free_chunk(&curr);
            return -1;
        }
        png_free_chunk(&curr);
    }

    memcpy(out_chunk_type, curr.type, 4);
    png_free_chunk(&curr);
    return 0;
}

int _png_write_chunk_to_file(FILE *ofptr, png_chunk_t *chunk)
{
    if(!ofptr || !chunk) return -1;

    /* 4 bytes for length */
    /* 4 bytes for type */
    /* length bytes for data */
    /* 4 bytes for crc */
    size_t bytesToWrite = 12u + (size_t) chunk->length;  

    uint8_t *buffer = (uint8_t*) malloc(sizeof(uint8_t) * bytesToWrite);

    if(!buffer) return -1;

    /* Write length */
    write_u32_be(chunk->length, buffer);

    /* Write type */
    memcpy(buffer+4, chunk->type, 4);

    /* Write data */
    memcpy(buffer+8, chunk->data, chunk->length);

    /* Write crc */
    write_u32_be(chunk->crc, buffer+8+chunk->length);

    /* Finally write to the file */
     if(fwrite(buffer, 1, bytesToWrite, ofptr) != bytesToWrite) {
        free(buffer);
        return -1;
    }
    free(buffer);

    return bytesToWrite;
}

int _png_write_chunk_from_fields(uint32_t length, const char *type, uint8_t *data, png_chunk_t *out_chunk) 
{
    if(!type || !out_chunk) return -1;

    if(strlen(type) != 4) return -1;

    /* tmpbuffer = type + data */
    size_t tmpBufferSize = length + 4;
    uint8_t *tmpBuffer = malloc(sizeof(uint8_t) * tmpBufferSize);

    if(!tmpBuffer) return -1;

    memcpy(tmpBuffer, type, 4);
    memcpy(tmpBuffer+4, data, length);

    uint32_t crc = png_crc(tmpBuffer, tmpBufferSize);

    free(tmpBuffer);

    out_chunk->length = length;
    memcpy(out_chunk->type, type, 4);
    out_chunk->data = data;
    out_chunk->crc = crc;

    return 0;
}