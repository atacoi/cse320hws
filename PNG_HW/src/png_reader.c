#include "png_reader.h"
#include "png_crc.h"
#include "util.h"
#include "global.h"
#include <stdlib.h>
#include <string.h>

/* Opens a PNG file and validates signature */
FILE *png_open(const char *path)
{
    if(!path) return NULL;

    FILE* fptr = NULL;
    fptr = fopen(path, "rb");

    if(!fptr) return NULL;

    uint8_t signature[8];

    if(read_exact(fptr, signature, 8) == -1) return NULL;

    const uint8_t PNG_SIGNATURE[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

    /* Validate signature */
    for(int i = 0; i < 8; i++) {
        if(signature[i] != PNG_SIGNATURE[i]) return NULL;
    }

    return fptr;
}

/* Reads the next chunk from the file */
int png_read_chunk(FILE *fp, png_chunk_t *out)
{
    return _png_read_chunk_helper(fp, out, validate_max_length | validate_ascii_type);
}

/* Reads the next summary chunk from the file */
int _png_read_chunk_summary(FILE *fp, png_chunk_t *out) 
{
    return _png_read_chunk_helper(fp, out, validate_crc | free_data);
}

/* Helper for png_read_chunk that can skip validation (e.g., length, ascii type, crc) */
int _png_read_chunk_helper(FILE *fp, png_chunk_t *out, uint8_t flags) 
{
    if(!fp || !out) {
        PRINT_ERROR_READ_CHUNKS();
        return -1;
    }

    uint32_t length;
    char type[5] = { '\0' };
    uint8_t *data = NULL;
    uint32_t crc;

    /* Read length */
    uint8_t lenBuffer[4];
    if(read_exact(fp, lenBuffer, 4) == -1) {
        PRINT_ERROR_READ_CHUNKS();
        return -1;
    }
    length = read_u32_be(lenBuffer);

    /* Max length is 2^31 - 1 */
    if((flags & validate_max_length) && length > (uint32_t) ((1u << 31) - 1u)) {
        PRINT_ERROR_READ_CHUNKS();
        return -1;
    }

    /* Read type */
    if(read_exact(fp, (uint8_t*) type, 4) == -1) {
        PRINT_ERROR_READ_CHUNKS();
        return -1;
    }

    /* Type must be made up of ascii letters */
    if((flags & validate_ascii_type)) {
        for(int i = 0; i < 4; i++) {
            char t = type[i];
            if(!((t >= 'A' && t <= 'Z') || (t >= 'a' && t <= 'z'))) {
                PRINT_ERROR_READ_CHUNKS();
                return -1;
            }
        }
    }

    /* Read data */
    data = (uint8_t*) malloc(sizeof(uint8_t) * length);

    /* Malloc failed */
    if(!data) {
        PRINT_ERROR_READ_CHUNKS();
        return -1;
    }

    if(read_exact(fp, data, length) == -1) {
        free(data);
        PRINT_ERROR_READ_CHUNKS();
        return -1;
    }

    /* Read crc */
    uint8_t crcBuffer[4];
    if(read_exact(fp, crcBuffer, 4) == -1) {
        free(data);
        PRINT_ERROR_READ_CHUNKS();
        return -1;
    }
    crc = read_u32_be(crcBuffer);

    /* tmpbuffer = type + data */
    size_t tmpBufferSize = length + 4;
    uint8_t *tmpBuffer = malloc(sizeof(uint8_t) * tmpBufferSize);

    memcpy(tmpBuffer, type, 4);
    memcpy(tmpBuffer+4, data, length);

    uint32_t actualCrc = png_crc(tmpBuffer, tmpBufferSize);

    free(tmpBuffer);

    if(!(dont_check_crc & flags)) {
        if(actualCrc != crc) {
            if((flags & validate_crc)) {
                crc = 0u; // invalid crc
            } else {
                free(data);
                PRINT_ERROR_READ_CHUNKS();
                return -1;
            }
        } else {
            if((flags & validate_crc)) {
                crc = 1u;
            }
        }
    }

    if(flags & free_data) {
        free(data);
        data = NULL;
    }

    /* Finally write to out */
    out->length = length;
    strcpy(out->type, type);
    out->data = data;
    out->crc = crc;
    
    return 0;
}

/* Frees memory allocated inside png_chunk_t */
void png_free_chunk(png_chunk_t *chunk)
{
	if(chunk) { 
        free(chunk->data);
        chunk->data = NULL; 
    }
}

int png_extract_ihdr(FILE *fp, png_ihdr_t *out)
{
    if(!fp || !out) return -1;

    png_chunk_t chunk = {};
    if(png_read_chunk(fp, &chunk) == -1) {
        PRINT_ERROR_READ_IHDR();
        return -1;
    }

    if(png_parse_ihdr(&chunk, out) == -1) {
        PRINT_ERROR_PARSE_IHDR();
        png_free_chunk(&chunk);
        return -1;
    }

    png_free_chunk(&chunk);

    return 0;
}

int png_extract_plte(FILE *fp, png_color_t **out_colors, size_t *out_count)
{
    if(!fp || !out_colors || !out_count) return -1;

    /* Continue while we have a chunk and the chunk is not PLTE, IDAT or IEND */
    /* Be sure to free the data after passing an invalid chunk */
    png_chunk_t curr = {};
    while(   _png_read_chunk_helper(fp, &curr, 0u) == 0
          && memcmp(curr.type, "PLTE", 4) != 0 
          && memcmp(curr.type, "IDAT", 4) != 0 
          && memcmp(curr.type, "IEND", 4) != 0) { png_free_chunk(&curr); }

    if(png_parse_plte(&curr, out_colors, out_count) == -1) {
        png_free_chunk(&curr);
        return -1;
    }
    
    png_free_chunk(&curr);

    /* Save the position after the PLTE */
    long pos = ftell(fp);

    /* Verify there are no other PLTE chunks */
    if(_png_search_type(fp, &curr, "PLTE") == 0) {
        png_free_chunk(&curr);
        return -1;
    }

    fseek(fp, pos, SEEK_SET);

    png_free_chunk(&curr);
    return 0;
}

int png_summary(const char *filename, png_chunk_t **out_summary)
{
    if(!filename || !out_summary) return -1;

    FILE* fptr = png_open(filename);

    if(!fptr) return -1;

    size_t size      = 10; 
    png_chunk_t *currSummary = (png_chunk_t*) malloc(size * sizeof(png_chunk_t));

    if(!currSummary) {
        fclose(fptr);
        return -1;
    }

    png_chunk_t curr;
    size_t numChunks = 0;
    while(   _png_read_chunk_summary(fptr, &curr) == 0
          && memcmp(curr.type, "IEND", 4) != 0) 
    {
        if(numChunks >= size) {
            png_chunk_t *temp = (png_chunk_t*) realloc(currSummary, sizeof(png_chunk_t) * (size + size));
            if(!temp) {
                fclose(fptr);
                free(currSummary);
                return -1;
            }
            currSummary = temp;
            size += size;
        }
        currSummary[numChunks] = curr;
        numChunks += 1;
    }

    if(memcmp(curr.type, "IEND", 4) == 0) {
        if(numChunks >= size) {
            png_chunk_t *temp = (png_chunk_t*) realloc(currSummary, sizeof(png_chunk_t) * (size + size));
            if(!temp) {
                fclose(fptr);
                free(currSummary);
                return -1;
            }
            currSummary = temp;
            size += size;
        }
        currSummary[numChunks] = curr;
        numChunks += 1;
    }

    *out_summary = currSummary;

    fclose(fptr);

    return 0;
}

int _png_search_type(FILE *fp, png_chunk_t *out, const char *chunk_type) {
    png_chunk_t curr;
    while(   _png_read_chunk_helper(fp, &curr, validate_crc) == 0
          && memcmp(curr.type, chunk_type, 4) != 0 
          && memcmp(curr.type, "IEND", 4) != 0) { png_free_chunk(&curr); }

    /* We reach the end of the file and we don't find the chunk we were looking for */
    if(memcmp(curr.type, chunk_type, 4) != 0) { 
        png_free_chunk(&curr);
        return -1;
    }

    /* Verify that the checksum is correct */
    if(!curr.crc) {
        png_free_chunk(&curr);
        return -1;
    }

    *out = curr;
    return 0;
}