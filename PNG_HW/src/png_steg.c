#include "png_steg.h"
#include "png_reader.h"
#include "png_crc.h"
#include "png_print_helpers.h"
#include "util.h"
#include "png_writer.h"
#include "idat_parsing.h"
#include <stdlib.h>
#include <string.h>

/* Encode secret string into LSBs of image data */
int png_encode_lsb(const char *input_path, const char *output_path, const char *secret)
{
    if(!input_path || !output_path || !secret) return -1;

    if(strcmp(input_path, output_path) == 0) return -1;

    FILE *fptr = NULL;
    if(!(fptr = png_open(input_path))) return -1;

    long startPos = ftell(fptr);

    png_ihdr_t ihdr;
    if(png_extract_ihdr(fptr, &ihdr) == -1) { 
        fclose(fptr);
        return -1; 
    }

    /* Support only 8 bit sample images */
    if(ihdr.bit_depth != 8) {
        fclose(fptr);
        return -1;
    }

    uint8_t *idatBuffer   = NULL;
    size_t idatBufferSize = 0u;

    uint32_t encodingCapacity = ihdr.width * ihdr.height; // total pixels = total bits

    uint32_t secretLen = (uint32_t) strlen(secret);

    /* Each pixel encodes one bit */
    if((secretLen + 1) * 8 > encodingCapacity) {
        fclose(fptr);
        return -1;
    }

    if(_png_decompress_all_idat_chunks(fptr, &idatBuffer, &idatBufferSize, ihdr.width, ihdr.height, ihdr.color_type) == -1) {
        fclose(fptr);
        return -1;
    }

    fseek(fptr, startPos, SEEK_SET);

    png_color_t *out_colors = NULL;
    size_t out_count = 0;
    uint8_t pairs[256];
    if(ihdr.color_type == 3) {
        if(_generate_pairs(fptr, pairs, &out_colors, &out_count) == -1) {
            free(idatBuffer);
            fclose(fptr);
            return -1;
        }
    }

    /* Skip filter type */
    uint8_t *currBytePtr = idatBuffer;
    uint8_t bpp = _get_samples_per_pixel(ihdr.color_type);

    size_t pixelsProcessed = 0u;

    for(int i = 0; i < secretLen+1; i++) {
        char c = i == secretLen ? '\0' : secret[i]; // Don't forget the null terminator!
        for(int b = 1; b < (1 << 8); b <<= 1) {
            /* Skip filter type for each scanline */
            if(pixelsProcessed % ihdr.width == 0) {
                currBytePtr += 1;
            }

            uint8_t bit = c & b; // if > 0 => 1 else 0
            uint8_t currByte = *currBytePtr;

            if(ihdr.color_type == 3) {
                /* Verify that the index is not out of bounds */
                if(currByte >= out_count) {
                    free(idatBuffer);
                    free(out_colors);
                    fclose(fptr);
                    return -1;
                }

                size_t p = pairs[currByte];

                size_t lowIndex  = p >= currByte ? currByte : p;
                size_t highIndex = p >= currByte ? p : currByte;

                /* high index = 1, low index = 0 */
                currByte = bit ? highIndex : lowIndex;
            } else {
                currByte >>= 1; // get rid of lsb
                currByte <<= 1; // lsb is 0
                if(bit) currByte |= 1;  // lsb is always 1
            }

            *currBytePtr = currByte;
            currBytePtr += bpp;
            pixelsProcessed += 1;
        }
    }  

    fseek(fptr, startPos, SEEK_SET);

    /* Encapsulate all of this into a single function */

    FILE *ofptr = fopen(output_path, "wb");
    if(!ofptr) return -1;

    if(_png_write_chunks_to_file(fptr, NULL, ofptr, out_colors, out_count, idatBuffer, idatBufferSize) == -1) {
        fclose(fptr);
        fclose(ofptr);
        free(idatBuffer);
        if(ihdr.color_type == 3) {
            free(out_colors);
        }
        return -1;
    }

    free(idatBuffer);

    if(ihdr.color_type == 3) {
        free(out_colors);
    }

    fclose(fptr);
    fclose(ofptr);

    return 0;
}

/* Extract secret string from LSBs of image data */
int png_extract_lsb(const char *input_path, char *out, size_t max_len)
{
    if(!input_path || !out || !max_len) return -1;

    FILE *fptr = NULL;
    if(!(fptr = png_open(input_path))) return -1;

    long startPos = ftell(fptr);

    png_ihdr_t ihdr;
    if(png_extract_ihdr(fptr, &ihdr) == -1) { 
        fclose(fptr);
        return -1; 
    }

    /* Support only 8 bit sample images */
    if(ihdr.bit_depth != 8) {
        fclose(fptr);
        return -1;
    }

    /* Build pairs table */

    png_color_t *out_colors = NULL;
    size_t out_count = 0;

    uint8_t pairs[256];

    if(ihdr.color_type == 3) {
        if(png_extract_plte(fptr, &out_colors, &out_count) == -1) {
            fclose(fptr);
            return -1;
        }

        ColorIndex transformedColors[out_count];

        for(int i = 0; i < out_count; i++) {
            transformedColors[i].color =  ((uint32_t) out_colors[i].r << 16) 
                                        | ((uint32_t) out_colors[i].g << 8) 
                                        | ((uint32_t) out_colors[i].b);

            transformedColors[i].originalIndex = i;
        }

        if(_generate_pairs_from_current_colors(transformedColors, out_count, pairs) == -1) {
            free(out_colors);
            fclose(fptr);
            return -1;
        }

        free(out_colors);
        out_colors = NULL;
    }

    uint8_t *idatBuffer   = NULL;
    size_t idatBufferSize = 0u;

    fseek(fptr, startPos, SEEK_SET);

    if(_png_decompress_all_idat_chunks(fptr, &idatBuffer, &idatBufferSize, ihdr.width, ihdr.height, ihdr.color_type) == -1) {
        fclose(fptr);
        return -1;
    }

    size_t capacity = ihdr.width * ihdr.height;

    uint8_t bpp = _get_samples_per_pixel(ihdr.color_type);

    uint8_t *currBytePtr = idatBuffer;
    size_t pixelsProcessed = 0u;
    size_t currCharIndex = 0u;
    uint16_t currBit = 1u;
    uint8_t tmpByte = 0u; // holds the bits for the processing char

    while(currCharIndex < max_len && pixelsProcessed < capacity) {

        /* Skip filter type for each scanline */
        if(pixelsProcessed % ihdr.width == 0) {
            currBytePtr += 1;
        }

        /* Exit if we encounter null-terminator */
        if(currBit == 256) {
            out[currCharIndex] = tmpByte;
            if(!tmpByte) {
                break;
            }
            tmpByte = 0u;
            currCharIndex++;
            currBit = 1u;
        }

        uint8_t currByte = *currBytePtr;

        if(ihdr.color_type == 3) {
            /* Verify that the index is not out of bounds */
            if(currByte >= out_count) {
                free(idatBuffer);
                free(out_colors);
                fclose(fptr);
                return -1;
            }

            uint8_t p = pairs[currByte];

            uint8_t highIndex = p >= currByte ? p : currByte;

            /* high index = 1, low index = 0 */
            if(currByte == highIndex) tmpByte |= currBit; 
        } else {
            uint8_t bit = (uint8_t) currByte & 0x01;
            if(bit) tmpByte |= currBit;            
        }

        currBit <<= 1;
        pixelsProcessed++;
        currBytePtr += bpp;
    }    

    free(idatBuffer);

    fclose(fptr);

    return pixelsProcessed / 8;
}

int _generate_pairs_from_current_colors(ColorIndex *transformed_colors, size_t original_size, uint8_t *pairs) 
{
    if(original_size > 256 || !transformed_colors) return -1;

    qsort(transformed_colors, original_size, sizeof(ColorIndex), _colorindex_color_comparator);

    size_t duplicateCnt = 0u; // number of duplicates we have to append

    int i = 0;
    while(i < original_size - 1) {
        ColorIndex *fst = &(transformed_colors[i]);
        ColorIndex *snd = &(transformed_colors[i+1]);

        if(fst->color == snd->color) {
            pairs[fst->originalIndex] = snd->originalIndex;
            pairs[snd->originalIndex] = fst->originalIndex;
            i += 2;
        } else {
            pairs[fst->originalIndex] = fst->originalIndex; // pair with itself
            duplicateCnt++;
            i++;
        }
    }

    /* Add the last color if we did not skip it */
    if(i == original_size - 1) {
        ColorIndex *ci = &(transformed_colors[i]);
        pairs[ci->originalIndex] = ci->originalIndex; // pair with itself
        duplicateCnt++;
    }

    return duplicateCnt;
}

int _generate_pairs(FILE *fptr, uint8_t *pairs, png_color_t **out_colors, size_t *out_count)
{
    if(!fptr || !out_colors || !out_count || !pairs) return -1;

    png_color_t *tmpOutColors = NULL;
    size_t originalCnt = 0;

    if(png_extract_plte(fptr, &tmpOutColors, &originalCnt) == -1) return -1;

    ColorIndex transformedColors[originalCnt];

    for(int i = 0; i < originalCnt; i++) {
        transformedColors[i].color =  ((uint32_t) tmpOutColors[i].r << 16) 
                                    | ((uint32_t) tmpOutColors[i].g << 8) 
                                    | ((uint32_t) tmpOutColors[i].b);

        transformedColors[i].originalIndex = i;
    }

    /* The number of duplicates we have to append */
    size_t duplicateCnt = _generate_pairs_from_current_colors(transformedColors, originalCnt, pairs);

    if(duplicateCnt == -1) {
        free(tmpOutColors);
        return -1;
    }

    size_t fnlOutCount = originalCnt + duplicateCnt;

    /* If we have to add a total of more than 256 colors, escape */
    if(fnlOutCount > 256) {
        free(tmpOutColors);
        return -1;
    }

    png_color_t *fnlOutColors = (png_color_t*) realloc(tmpOutColors, sizeof(png_color_t) * fnlOutCount);
    
    if(!fnlOutColors) {
        free(tmpOutColors);
        return -1;
    }

    /* Sort in original order */
    qsort(transformedColors, originalCnt, sizeof(ColorIndex), _colorindex_ogindex_comparator);

    /* Offset from pair array */
    int startingPair = originalCnt;
    for(int i = 0; i < originalCnt; i++) {
        ColorIndex *a = &(transformedColors[i]);
        
        // a pair paired with itself needs to be duplicated 
        if(a->originalIndex == pairs[a->originalIndex]) {
            pairs[a->originalIndex] = startingPair+i;
            pairs[startingPair+i] = a->originalIndex;

            png_color_t tmp = { .r = (uint8_t) (a->color >> 16), 
                                .g = (uint8_t) (a->color >> 8 ),
                                .b = (uint8_t) (a->color)};

            memcpy(&(fnlOutColors[startingPair+i]), &tmp, sizeof(png_color_t));  
        }
    }

    *out_colors = fnlOutColors;
    *out_count  = fnlOutCount;

    return 0;
}

int _colorindex_color_comparator(const void *ptr1, const void *ptr2)
{
    if(!ptr1 || !ptr2) return 0;

    ColorIndex colorindex1 = *((const ColorIndex*) ptr1);
    ColorIndex colorindex2 = *((const ColorIndex*) ptr2);

    if (colorindex1.color < colorindex2.color) return -1;
    if (colorindex1.color > colorindex2.color) return 1;

    return 0;
}

int _colorindex_ogindex_comparator(const void *ptr1, const void *ptr2)
{
    if(!ptr1 || !ptr2) return 0;

    ColorIndex colorindex1 = *((const ColorIndex*) ptr1);
    ColorIndex colorindex2 = *((const ColorIndex*) ptr2);

    if (colorindex1.originalIndex < colorindex2.originalIndex) return -1;
    if (colorindex1.originalIndex > colorindex2.originalIndex) return 1;
    
    return 0;
}
