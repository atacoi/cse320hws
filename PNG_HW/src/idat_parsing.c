#include "idat_parsing.h"
#include "util.h"
#include "png_reader.h"

#include <string.h>

int _png_decompress_all_idat_chunks(FILE *fptr, uint8_t **output_buffer, size_t *buffer_size,
                                    uint32_t image_width, uint32_t image_height, uint8_t color_type)
{
    if(!fptr || !output_buffer || !buffer_size) return -1;

    size_t totalSize = 0; // initially 0 bytes
    uint8_t *buffer = NULL;

    png_chunk_t currIdat = {};
    size_t currOffset  = 0; // position of the next free byte 
    while(_png_search_type(fptr, &currIdat, "IDAT") == 0) {
        size_t idatDatalen = (size_t) currIdat.length;

        if(idatDatalen > 0) {
            totalSize += idatDatalen;
            uint8_t *tmp = (uint8_t*) realloc(buffer, sizeof(uint8_t) * totalSize);
            if(!tmp) {
                free(buffer);
                return -1;
            }
            buffer = tmp;
        }

        memcpy(buffer+currOffset, currIdat.data, idatDatalen);
        png_free_chunk(&currIdat);
        currOffset = totalSize;
    }

    /* Decompress the idat data */
    uint8_t *idatBuffer   = NULL;
    size_t idatBufferSize = 0u;
    if(util_inflate_data(buffer, totalSize, &idatBuffer, &idatBufferSize) == -1) {
        free(buffer);
        return -1;
    }

    free(buffer);

    /* Validate the idat buffer before writing */
    if(_validate_idat_buffer(idatBuffer, idatBufferSize, image_width, image_height, color_type) == -1) {
        free(idatBuffer);
        return -1;
    }

    *output_buffer = idatBuffer;
    *buffer_size   = idatBufferSize;

    return 0;
}

int _validate_idat_buffer(uint8_t *idat_buffer, size_t idat_buffer_size, 
                          uint32_t image_width, uint32_t image_height, 
                          uint8_t color_type)
{
    if(!idat_buffer) return -1;

    uint8_t bpp = _get_samples_per_pixel(color_type);

    /* Unknown color type */
    if(!bpp) return -1;

    /* The row length in bytes including the filter byte */
    size_t rowLen = ((size_t) image_width) * ((size_t) bpp) + 1;

    /* There are exactly h rows */
    size_t totalBytes = rowLen * (size_t) image_height;

    if(totalBytes != idat_buffer_size) return -1;

    /* Verify that filter bytes are valid filter types */
    uint8_t *currBytePtr = idat_buffer;
    uint8_t *endPtr = idat_buffer + totalBytes;
    while(currBytePtr != endPtr) {
        /* Include the filter byte */
        uint8_t currByte = *currBytePtr;

        /* Filter types are 0-4 */
        if(currByte > 4) return -1;
        currBytePtr += rowLen;
    }

    return 0;
}