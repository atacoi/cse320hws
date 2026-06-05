#include "png_overlay.h"
#include "png_reader.h"
#include "png_chunks.h"
#include "png_crc.h"
#include "util.h"
#include "debug.h"
#include "filtering.h"
#include "idat_parsing.h"
#include "png_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int png_overlay_paste(const char *large_path, const char *small_path,
                      const char *output_path, uint32_t x_offset, uint32_t y_offset)
{
    if(!large_path || !small_path || !output_path) return -1;

    /* File paths are the same as output path */
    if(strcmp(large_path, output_path) == 0 || strcmp(small_path, output_path) == 0) return -1;

    /* Open both pngs */

    FILE *lfptr = png_open(large_path);

    if(!lfptr) return -1;

    FILE *sfptr = png_open(small_path);

    if(!sfptr) {
        fclose(lfptr);
        return -1;
    }

    /* Check it exists here and close immediately just to save us from doing all of that if its invalid */
    FILE *ofptr = fopen(output_path, "wb");

    if(!ofptr) {
        fclose(lfptr);
        fclose(sfptr);
        return -1;
    }

    fclose(ofptr);
    ofptr = NULL;

    /* Save the position after the signature for both files */
    long lStartPos = ftell(lfptr);
    long sStartPos = ftell(sfptr);

    /* Extract the IHDR from both files */
    png_ihdr_t lihdr, sihdr;

    if(png_extract_ihdr(lfptr, &lihdr) == -1) {
        fclose(lfptr);
        fclose(sfptr);
        return -1;
    }

    if(png_extract_ihdr(sfptr, &sihdr) == -1) {
        fclose(lfptr);
        fclose(sfptr);
        return -1;
    }

    /* Verify equal bit depths and that both are 8 bit */
    if(lihdr.bit_depth != sihdr.bit_depth || lihdr.bit_depth != 8) {
        fclose(lfptr);
        fclose(sfptr);
        return -1;
    }

    /* Verify that they have the same color_type and that color_type is valid (invalid when bpp == 0) */
    uint8_t bpp = 0u;
    if(lihdr.color_type != sihdr.color_type || !(bpp = _get_samples_per_pixel(lihdr.color_type))) {
        fclose(lfptr);
        fclose(sfptr);
        return -1;
    }

    /* Go back to the signature */
    fseek(lfptr, lStartPos, SEEK_SET);
    fseek(sfptr, sStartPos, SEEK_SET);

    /* Decompress the idat chunks into two separate buffers */

    uint8_t *lIdatBuffer = NULL;
    size_t lIdatBufferSize = 0u;

    if(_png_decompress_all_idat_chunks(lfptr, &lIdatBuffer, &lIdatBufferSize, lihdr.width, lihdr.height, lihdr.color_type) == -1) {
        fclose(lfptr);
        fclose(sfptr);
        return -1;
    }

    uint8_t *sIdatBuffer = NULL;
    size_t sIdatBufferSize = 0u;

    if(_png_decompress_all_idat_chunks(sfptr, &sIdatBuffer, &sIdatBufferSize, sihdr.width, sihdr.height, sihdr.color_type) == -1) {
        free(lIdatBuffer);
        fclose(lfptr);
        fclose(sfptr);
        return -1;
    }

    /* Unfilter buffers (applies 0 filter type to each scanline) */

    if(_unfilter_idat_buffer_bytes(lIdatBuffer, lIdatBufferSize, lihdr.width, lihdr.height, bpp) == -1) {
        free(lIdatBuffer);
        free(sIdatBuffer);
        fclose(lfptr);
        fclose(sfptr);
        return -1;
    }

    if(_unfilter_idat_buffer_bytes(sIdatBuffer, sIdatBufferSize, sihdr.width, sihdr.height, bpp) == -1) {
        free(lIdatBuffer);
        free(sIdatBuffer);
        fclose(lfptr);
        fclose(sfptr);
        return -1;
    }

    /* Go back to the signature */
    fseek(lfptr, lStartPos, SEEK_SET);
    fseek(sfptr, sStartPos, SEEK_SET);

    /* Create the merged color palette */
    /* And get rid of duplicates in both idat chunks */

    png_color_t *mergedColors = NULL;
    size_t mergedColorSize = 0u;

    if(lihdr.color_type == 3) {
        if(_create_merged_colors(&mergedColors, &mergedColorSize, 
                                 lIdatBuffer, lihdr.width, lihdr.height, 
                                 sIdatBuffer, sihdr.width, sihdr.height,
                                 bpp, lfptr, sfptr) == -1) {
            fclose(lfptr);
            fclose(sfptr);
            return -1;
        }
    }

    /* Paste smaller image onto larger one in-place */
    if(_paste_operation(lIdatBuffer, lihdr.width, lihdr.height, 
                        sIdatBuffer, sihdr.width, sihdr.height, 
                        bpp, x_offset, y_offset) == -1) {
        if(lihdr.color_type == 3) {
            free(mergedColors);
        }
        free(lIdatBuffer);
        free(sIdatBuffer);
        fclose(lfptr);
        fclose(sfptr);
        return -1;
    }

    /* We no longer need the smaller image data */
    free(sIdatBuffer);

    /* Finally write it to the output file */
    ofptr = fopen(output_path, "wb");

    if(!ofptr) {
        if(lihdr.color_type == 3) free(mergedColors);
        free(lIdatBuffer);
        fclose(lfptr);
        fclose(sfptr);
        return -1;
    }

    /* Go back to the signature */
    fseek(lfptr, lStartPos, SEEK_SET);
    fseek(sfptr, sStartPos, SEEK_SET);

    /* Write the final output file while merging the chunks from both source files */
    if(_png_write_chunks_to_file(lfptr, sfptr, ofptr, 
                                 mergedColors, mergedColorSize, 
                                 lIdatBuffer, lIdatBufferSize) == -1) 
    {
        if(lihdr.color_type == 3) free(mergedColors);
        free(lIdatBuffer);
        fclose(lfptr);
        fclose(sfptr);
        fclose(ofptr);
        return -1;
    }

    /* Merged PLTE colors no longer needed */
    if(lihdr.color_type == 3) free(mergedColors);
        
    /* Free the merged IDAT buffer */
    free(lIdatBuffer);

    /* Close all files */
    fclose(lfptr);
    fclose(sfptr);
    fclose(ofptr);

    return 0;
}

int _paste_operation(uint8_t *lidat_buffer, uint32_t lwidth, uint32_t lheight, 
                     uint8_t *sidat_buffer, uint32_t swidth, uint32_t sheight, 
                     uint8_t bpp, uint32_t x_offset, uint32_t y_offset)
{
    if(!lidat_buffer || !sidat_buffer) return -1;

    /* We exceed the width of the large image; this is fine simply quit */
    if(x_offset >= lwidth) return 0;

    /* We exceed the height of the large image; this is fine simply quit */
    if (y_offset >= lheight) return 0;

    /* Include the filter byte */
    size_t lscanlineSize = (lwidth * bpp) + 1u;
    size_t sscanlineSize = (swidth * bpp) + 1u;

    /* x offset in terms of bytes including the filter byte */
    size_t xbyteOffset = x_offset * bpp + 1u;

    uint8_t *mergedPtr = lidat_buffer + xbyteOffset + y_offset * lscanlineSize;
    size_t mergedScanline = y_offset; // the scanline the large image starts from

    // start from the top-most scanline after the filter byte
    uint8_t *currsscanline = sidat_buffer + 1u;

    /* Prevent overflow */
    size_t numBytesToCopy = (swidth * bpp) > (lscanlineSize - xbyteOffset) ? (lscanlineSize - xbyteOffset) : (swidth * bpp);

    for(size_t j = 0; j < sheight && mergedScanline < lheight; j++) {
        memcpy(mergedPtr, currsscanline, numBytesToCopy);
        mergedPtr += lscanlineSize; // go to the next scanline
        currsscanline += sscanlineSize;
        mergedScanline++;
    }
    return 0;
}

int _create_merged_colors(png_color_t **merged_colors, size_t *merged_color_size, 
                          uint8_t *lidat_buffer, uint32_t lwidth, uint32_t lheight, 
                          uint8_t *sidat_buffer, uint32_t swidth, uint32_t sheight,
                          uint8_t bpp, FILE *lfptr, FILE *sfptr)
{
    if(!merged_colors || !merged_color_size || !lidat_buffer || !sidat_buffer || !lfptr || !sfptr) return -1;

    /* Extract the palettes from both image files */
    png_color_t *lcolors = NULL, *scolors = NULL;
    size_t lcount = 0u, scount = 0u; 

    if(png_extract_plte(lfptr, &lcolors, &lcount) == -1) return -1;

    if(png_extract_plte(sfptr, &scolors, &scount) == -1) {
        free(lcolors);
        return -1;
    }

    /* Remove and remap duplicate colors */
    png_color_t *nlcolors = NULL, *nscolors = NULL;
    size_t nlcount = 0u, nscount = 0u; 

    if(_remove_duplicate_colors_and_remap(lcolors, lcount, 
                                          lidat_buffer, lwidth, lheight, 
                                          bpp,  &nlcolors, &nlcount) == -1) {
        free(lcolors);
        free(scolors);
        return -1;
    }

    free(lcolors);

    if(_remove_duplicate_colors_and_remap(scolors, scount, 
                                          sidat_buffer, swidth, sheight, 
                                          bpp, &nscolors, &nscount) == -1) {
        free(nlcolors);
        free(scolors);
        return -1;
    }

    free(scolors);

    /* Now, merge palettes */

    size_t initMergedSize = nlcount + nscount;
    png_color_t *initMergedColors = (png_color_t*) malloc(sizeof(png_color_t) * initMergedSize);

    if(!initMergedColors) {
        free(nlcolors);
        free(nscolors);
        return -1;
    }

    memcpy(initMergedColors, nlcolors, sizeof(png_color_t) * nlcount);

    free(nlcolors);

    /* Create mapping */
    size_t indexMap[nscount];
    size_t currMergedSize = nlcount;
    for(size_t i = 0; i < nscount; i++) {
        png_color_t *currColor = nscolors+i;
        uint8_t duplicateFound = 0u;
        size_t duplicateIndex = 0u;
        for(size_t j = 0; j < nlcount; j++) {
            png_color_t *innerColor = initMergedColors+j;
            if(   currColor->r == innerColor->r 
               && currColor->g == innerColor->g 
               && currColor->b == innerColor->b) {
                duplicateFound = 1u;
                duplicateIndex = j;
                break;
            } 
        }

        if(!duplicateFound) {
            indexMap[i] = currMergedSize;
            memcpy(initMergedColors+currMergedSize, currColor, sizeof(png_color_t));
            currMergedSize++;
        } else {
            indexMap[i] = duplicateIndex;
        }
    }

    free(nscolors);

    /* Exit if palette is greater than 256 */
    if(currMergedSize > 256) {
        free(initMergedColors);
        return -1;
    }

    /* Now, remap in sidat */
    size_t scanlineSize = (swidth * bpp) + 1u;
    for(size_t i = 0; i < (size_t) sheight; i++) {
        size_t currScanline = i * scanlineSize;
        for(size_t j = 1; j < scanlineSize; j++) {
            uint8_t byte = sidat_buffer[currScanline + j];

            if(byte >= scount) {
                free(initMergedColors);
                return -1;
            }

            size_t index = indexMap[byte];  
            sidat_buffer[currScanline + j] = index;
        }
    }

    *merged_colors = initMergedColors; // no need to resize 
    *merged_color_size = currMergedSize;

    return 0;
}

int _remove_duplicate_colors_and_remap(png_color_t *init_colors, size_t init_colors_size, 
                                       uint8_t *idat_buffer, uint32_t width, uint32_t height, 
                                       uint8_t bpp, png_color_t **out_colors, size_t *out_colors_size)
{
    if(!init_colors || !idat_buffer || !out_colors || !out_colors_size) return -1;

    /* Palette cannot exceed more than 256 colors */
    if(init_colors_size > 256) return -1;

    /* Remove duplicates and save the mapping */
    int indexMap[init_colors_size];
    png_color_t *mergedColors = (png_color_t*) malloc(sizeof(png_color_t) * init_colors_size); // will reshape later
    size_t currentMergedCount = 0u;

    if(!mergedColors) return -1;

    for(size_t i = 0; i < init_colors_size; i++) {
        png_color_t *currColor = init_colors+i;
        uint8_t duplicateFound = 0u;
        for(size_t j = 0; j < currentMergedCount; j++) {
            png_color_t *innerColor = mergedColors+j;
            if(   currColor->r == innerColor->r 
               && currColor->g == innerColor->g 
               && currColor->b == innerColor->b) {
                indexMap[i] = j;
                duplicateFound = 1u;
                break;
            } 
        }

        if(!duplicateFound) {
            indexMap[i] = i;
            memcpy(mergedColors+currentMergedCount, currColor, sizeof(png_color_t));
            currentMergedCount++;
        }
    }

    /* Palette cannot exceed more than 256 colors */
    if(currentMergedCount > 256) {
        free(mergedColors);
        return -1;
    }

    png_color_t *tmp = (png_color_t*) realloc(mergedColors, currentMergedCount * sizeof(png_color_t));

    if(!tmp) return -1;

    /* Now, remap in idat */
    size_t scanlineSize = (width * bpp) + 1u;
    for(size_t i = 0; i < (size_t) height; i++) {
        size_t currScanline = i * scanlineSize;
        for(size_t j = 1; j < scanlineSize; j++) {
            uint8_t byte = idat_buffer[currScanline + j];

            if(byte >= init_colors_size) {
                free(tmp);
                return -1;
            }

            size_t index = indexMap[byte];  
            idat_buffer[currScanline + j] = index;
        }
    }

    *out_colors = tmp;
    *out_colors_size = currentMergedCount;

    return 0;
}
