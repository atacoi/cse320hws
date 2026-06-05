#ifndef PNG_OVERLAY_H
#define PNG_OVERLAY_H

#include <stdint.h>

#include "png_chunks.h"

/* Overlay a smaller image onto a larger one starting at (x_offset, y_offset),
 * replacing the larger image's pixels wherever the smaller image lies. */
int png_overlay_paste(const char *large_path, const char *small_path,
                      const char *output_path, uint32_t x_offset, uint32_t y_offset);


/* Copies the pixels from the small image's idat buffer (sidat_buffer) */
/* to the larger image's idat buffer (lidat_buffer)                    */
/* Returns 0 on success, -1 on error                                   */                  
int _paste_operation(uint8_t *lidat_buffer, uint32_t lwidth, uint32_t lheight, 
                     uint8_t *sidat_buffer, uint32_t swidth, uint32_t sheight, 
                     uint8_t bpp, uint32_t x_offset, uint32_t y_offset);

/* Creates a palette of merged colors by retrieving the PLTE from both files, */
/* Removing and remapping all duplicate colors from the both idat buffers,  */
/* And finally appending colors from the small image palette to the large ones */
/* The input files are assumed to be placed right after the signature */
/* Returns 0 on success, -1 on error */
/* The caller is responsible for freeing merged_colors by calling free() */
int _create_merged_colors(png_color_t **merged_colors, size_t *merged_color_size, 
                          uint8_t *lidat_buffer, uint32_t lwidth, uint32_t lheight, 
                          uint8_t *sidat_buffer, uint32_t swidth, uint32_t sheight,
                          uint8_t bpp, FILE *lfptr, FILE *sfptr);


/* Removes all duplicate colors from the initial colors, */
/* Remaps all indexed colors in the idat to their new colors in the palette */
/* And writes that palette to out_colors */
/* Returns 0 on success, -1 on error */
/* The caller is responsible for freeing out_colors by calling free() */
int _remove_duplicate_colors_and_remap(png_color_t *init_colors, size_t init_colors_size, 
                                       uint8_t *idat_buffer, uint32_t width, uint32_t height, 
                                       uint8_t bpp, png_color_t **out_colors, size_t *out_colors_size);
#endif

