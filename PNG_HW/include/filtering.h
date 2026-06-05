#ifndef FILTERING_H
#define FILTERING_H

#include <stdlib.h>
#include <stdint.h>

/* Unfilters all bytes of the given idat_buffer in-place */
/* Returns 0 on success, -1 on error */
int _unfilter_idat_buffer_bytes(uint8_t *idat_buffer, size_t idat_buffer_size,
                                uint32_t image_width, uint32_t image_height, uint8_t bpp);

/* Returns a function pointer to one of the filtering types on success */
/* Else, returns a nullptr */
int (*_get_filtering_function(uint8_t filterType))(size_t, uint8_t, uint8_t*, uint8_t*, size_t);

/* Unfilter functions excluding type 0 (None) */
/* Note that a scanline includes its filter type */
/* The byte_index is the offset from the current scanline's filter type [0-(scanline_size - 1)]*/
/* Returns the unfiltered byte on success (must & with 0xFF), -1 on error */

/* Type 0: None */
int _unfilter_0(size_t byte_index, uint8_t bpp, 
                uint8_t *curr_scanline, uint8_t *prev_scanline, size_t scanline_size);

/* Type 1: Sub */
/* prev_scanline unused */
int _unfilter_1(size_t byte_index, uint8_t bpp, 
                uint8_t *curr_scanline, uint8_t *prev_scanline, size_t scanline_size);

/* Type 2: Up */
int _unfilter_2(size_t byte_index, uint8_t bpp,
                uint8_t *curr_scanline, uint8_t *prev_scanline, size_t scanline_size);

/* Type 3: Average */
int _unfilter_3(size_t byte_index, uint8_t bpp,
                uint8_t *curr_scanline, uint8_t *prev_scanline, size_t scanline_size);

/* Type 4: Paeth */
int _unfilter_4(size_t byte_index, uint8_t bpp,
                uint8_t *curr_scanline, uint8_t *prev_scanline, size_t scanline_size);

#endif 