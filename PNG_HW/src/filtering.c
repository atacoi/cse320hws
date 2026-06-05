#include "filtering.h"

/* Unfilters all bytes of the given idat_buffer in-place */
/* Returns 0 on success, -1 on error */
int _unfilter_idat_buffer_bytes(uint8_t *idat_buffer, size_t idat_buffer_size,
                                uint32_t image_width, uint32_t image_height, uint8_t bpp)
{
    if(!idat_buffer || !bpp) return -1;

    uint8_t *prevScanline = NULL;
    uint8_t *currScanline = idat_buffer;

    /* Include the filter type */
    size_t scanlineSize = (((size_t) image_width) * ((size_t) bpp)) + 1u;

    /* None initially */
    int (*filterFunction)(size_t, uint8_t, uint8_t*, uint8_t*, size_t) = NULL;

    /* Unfilter each scanline */
    for(uint32_t i = 0; i < image_height; i++) {
        for(uint32_t j = 0; j < scanlineSize; j++) {
            /* Filter type found so choose the right filtering function */
            if(j == 0) {
                uint8_t filterType = currScanline[j];
                filterFunction = _get_filtering_function(filterType);
                /* Set filter type to 0 */
                currScanline[j] = 0u;
            } else {
                if(filterFunction) {
                    int filteredByte = filterFunction(j, bpp, currScanline, prevScanline, scanlineSize);
                    if(filteredByte == -1) { 
                        return -1;
                    }
                    currScanline[j] = (uint8_t) (filteredByte & 0xFF);
                } else {
                    return -1; // filtering type not recognized 
                }
            }
        }
        prevScanline = currScanline;
        currScanline += scanlineSize; // go to the next scanline
    }
    return 0;
}

int (*_get_filtering_function(uint8_t filterType))(size_t, uint8_t, uint8_t*, uint8_t*, size_t)
{
    switch(filterType) {
        case 0:
            return _unfilter_0;

        case 1:
            return _unfilter_1;
        
        case 2:
            return _unfilter_2;

        case 3:
            return _unfilter_3;

        case 4:
            return _unfilter_4;

        default:
            return NULL;
    }
    return NULL;
}

/* Unfilter functions excluding type 0 (None) */
/* Note that a scanline includes its filter type */
/* The byte_index is the offset from the current scanline's filter type [0-(scanline_size - 1)]*/
/* Returns the unfiltered byte on success (must & with 0xFF), -1 on error */

/* Type 0: None */
int _unfilter_0(size_t byte_index, uint8_t bpp, 
                uint8_t *curr_scanline, uint8_t *prev_scanline, size_t scanline_size)
{
    /* Null Check + 0 bpp + byte_index out of bounds + byte_index = filter byte */
    if(!curr_scanline || !bpp || byte_index == 0 || byte_index >= scanline_size) return -1;

    return (curr_scanline[byte_index]);
}

/* Type 1: Sub */
int _unfilter_1(size_t byte_index, uint8_t bpp, 
                uint8_t *curr_scanline, uint8_t *prev_scanline, size_t scanline_size)
{
    /* Null Check + 0 bpp + byte_index out of bounds + byte_index = filter byte */
    if(!curr_scanline || !bpp || byte_index == 0 || byte_index >= scanline_size) return -1;

    /* There is no left neighbor */
    if(byte_index <= bpp) return curr_scanline[byte_index];

    uint8_t leftNeighbor = curr_scanline[byte_index - bpp];

    return (curr_scanline[byte_index] + leftNeighbor);
}

/* Type 2: Up */
int _unfilter_2(size_t byte_index, uint8_t bpp,
                uint8_t *curr_scanline, uint8_t *prev_scanline, size_t scanline_size)
{
    /* Null Check + 0 bpp + byte_index out of bounds + byte_index = filter byte */
    if(!curr_scanline || !bpp || byte_index == 0 || byte_index >= scanline_size) return -1;

    /* We have no scanline above the current one so return the current byte */
    if(!prev_scanline) return curr_scanline[byte_index];

    uint8_t aboveNeighbor = prev_scanline[byte_index];

    return (curr_scanline[byte_index] + aboveNeighbor);
}

/* Type 3: Average */
int _unfilter_3(size_t byte_index, uint8_t bpp,
                uint8_t *curr_scanline, uint8_t *prev_scanline, size_t scanline_size)
{
    /* Null Check + 0 bpp + byte_index out of bounds + byte_index = filter byte */
    if(!curr_scanline || !bpp || byte_index == 0 || byte_index >= scanline_size) return -1;

    /* Use 0 if no left neighbor exists */
    uint8_t leftNeighbor = byte_index <= bpp ? 0 : curr_scanline[byte_index - bpp];

    /* Use 0 if no above neighbor exists */
    uint8_t aboveNeighbor = !prev_scanline ? 0 : prev_scanline[byte_index];

    int prediction = (leftNeighbor + aboveNeighbor) / 2;
    return (curr_scanline[byte_index] + prediction);
}

/* Type 4: Paeth */
int _unfilter_4(size_t byte_index, uint8_t bpp,
                uint8_t *curr_scanline, uint8_t *prev_scanline, size_t scanline_size)
{
    /* Null Check + 0 bpp + byte_index out of bounds + byte_index = filter byte */
    if(!curr_scanline || !bpp || byte_index == 0 || byte_index >= scanline_size) return -1;

    /* Use 0 if no left neighbor exists */
    uint8_t a = byte_index <= bpp ? 0 : curr_scanline[byte_index - bpp];

    /* Use 0 if no above neighbor exists */
    uint8_t b = !prev_scanline ? 0 : prev_scanline[byte_index];

    /* Use 0 if no upper-left neighbor exists */
    uint8_t c = byte_index <= bpp || !prev_scanline ? 0 : prev_scanline[byte_index - bpp];

    int p = a + b - c;

    int pa = abs(p - a);
    int pb = abs(p - b);
    int pc = abs(p - c);

    /* Choose the smallest distance among the three */
    int closest = a;
    int smallestDist = pa;

    if(pb < smallestDist) {
        smallestDist = pb;
        closest = b;
    } 
    
    if(pc < smallestDist) {
        smallestDist = pc;
        closest = c;
    }

    return (curr_scanline[byte_index] + closest);
}