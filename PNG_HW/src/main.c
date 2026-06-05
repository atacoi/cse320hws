#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "util.h"
#include "png_reader.h"
#include "png_chunks.h"
#include "png_steg.h"
#include "png_overlay.h"
#include "png_print_helpers.h"

int main(int argc, char **argv)
{
    int opt;
    char *filename = NULL;
    FILE *pngFile = NULL;
    optind = 1; // reset getopt just in case it was set beforehand

    /* Denotes which flags have already been processed */
    uint16_t flagsSet = 0u;

    /* All error flags currently set (used in first pass) */
    uint8_t errorFlagsSet = 0u;

    /* First pass */
    while((opt = getopt(argc, argv, "+:hf:")) != -1) { 
        switch(opt) { 
            case 'h':
                PRINT_USAGE(argv[0]); // first argument is the program name
                if(filename) free(filename);
                if(pngFile) fclose(pngFile);
                return EXIT_SUCCESS;

            case 'f':
                if(flagsSet & F_FLAG) break;
                
                // print usage even if -h is an argument to -f
                if(opt == 'f' && strcmp(optarg, "-h") == 0) {
                    PRINT_USAGE(argv[0]); // first argument is the program name
                    if(filename) free(filename);
                    if(pngFile) fclose(pngFile);
                    return EXIT_SUCCESS;
                }

                filename = strdup(optarg);
                if(!(pngFile = png_open(filename))) {
                    errorFlagsSet |= OPEN_FILE_ERROR_FLAG;
                }
                
                flagsSet |= F_FLAG;
                break;

            case ':': 
                // -f option has no filepath
                if(optopt == 'f') {
                    flagsSet |= F_FLAG;
                    errorFlagsSet |= FILE_REQUIRES_FLAG;
                }
            case '?': 
                break; 
        } 
    } 

    if(!(flagsSet & F_FLAG)) {
        PRINT_ERROR_MISSING_F_FLAG();
        if(filename) free(filename);
        if(pngFile) fclose(pngFile);
        return EXIT_FAILURE;
    }

    if(errorFlagsSet & OPEN_FILE_ERROR_FLAG) {
        PRINT_ERROR_OPEN_FILE(filename);
        if(filename) free(filename);
        if(pngFile) fclose(pngFile);
        return EXIT_FAILURE;
    }

    if(errorFlagsSet & FILE_REQUIRES_FLAG) {
        PRINT_ERROR_F_REQUIRES_FILENAME();
        if(filename) free(filename);
        if(pngFile) fclose(pngFile);
        return EXIT_FAILURE;
    }

    optind = 1; // reset getopt

    png_chunk_t *summary = NULL;

    png_color_t *plteColors = NULL;
    size_t numColors = 0;

    png_ihdr_t ihdr;

    char *message = NULL; // used for 'e'
    char *encodingOutputFile = NULL; // used for 'o' when 'e' is used

    char *smallFilename = NULL; // used for 'm'
    char *overlayOutputFile = NULL; // used for 'o' when 'm' is used
    uint32_t xOffset = 0u;
    uint32_t yOffset = 0u;

    /* The file position right after the signature */
    long startPos = ftell(pngFile);

    /* Denotes the order opterations should be printed from left to right */
    char parseOrder[25]  = { '\0' };
    int parseOrderIndex = 0;

    // defines the last and second to last operation string filled in during the loop 
    int lastOpt = 0;
    int sndLastOpt = 0;

    // 0 - none
    // 1 - overlay requires
    // 2 - width requires
    int widthError = 0;

    // 0 - none
    // 1 - overlay requires
    // 2 - height requires
    int heightError = 0;

    // 0 - none
    // 1 - overlay
    // 2 - encode
    // 3 - w 
    // 4 - g
    int missingError = 0;

    int unknownOption = 0;

    /* Second pass */
    while((opt = getopt(argc, argv, ":f:spie:dm:o:w:g:")) != -1) { 
        switch(opt) {
            case 's':
                if(flagsSet & S_FLAG) break;

                parseOrder[parseOrderIndex++] = 's';

                flagsSet |= S_FLAG;
                break;

            case 'p':
                if(flagsSet & P_FLAG) break;

                parseOrder[parseOrderIndex++] = 'p';

                flagsSet |= P_FLAG;
                break;

            case 'i':
                if(flagsSet & I_FLAG) break;

                parseOrder[parseOrderIndex++] = 'i';

                flagsSet |= I_FLAG;
                break;

            case 'e':
                if(flagsSet & E_FLAG) break;

                parseOrder[parseOrderIndex++] = 'e';

                message = strdup(optarg);

                flagsSet |= E_FLAG;
                break;

            case 'd':
                if(flagsSet & D_FLAG) break;

                parseOrder[parseOrderIndex++] = 'd';

                flagsSet |= D_FLAG;
                break;

            case 'm':
                if(flagsSet & M_FLAG) break;

                parseOrder[parseOrderIndex++] = 'm';

                smallFilename = strdup(optarg);

                flagsSet |= M_FLAG;
                break;

            case 'f':
                break;

            case 'o':
                if(lastOpt == 'e') {
                    encodingOutputFile = strdup(optarg);
                } 
                else if(lastOpt == 'm') {
                    overlayOutputFile = strdup(optarg);
                } 
                break;

            case 'w':
                if(flagsSet & W_FLAG) break;

                // we have one case:
                // -m -o
                if(!(lastOpt == 'o' && sndLastOpt == 'm')) { 
                    widthError = 1;
                }

                if(str_to_u32(optarg, &xOffset) == -1) {
                    widthError = 2;
                }

                if(widthError) {
                    parseOrder[parseOrderIndex++] = 'w';
                }
 
                flagsSet |= W_FLAG;
                break;

            case 'g':
                if(flagsSet & G_FLAG) break;

                // The last true operation processed
                char headOperation = parseOrderIndex ? parseOrder[parseOrderIndex - 1] : '\0';

                // we have two cases:
                // -m -o -w
                // -m -o 
                if(!(lastOpt == 'w' && sndLastOpt == 'o' && headOperation == 'm') && !(lastOpt == 'o' && headOperation == 'm')) { 
                    heightError = 1;
                }

                if(str_to_u32(optarg, &yOffset) == -1) {
                    heightError = 2;
                }

                if(heightError) {
                    parseOrder[parseOrderIndex++] = 'g';
                }

                flagsSet |= G_FLAG;
                break;

            case ':':
                if(flagsSet & MISSING_FLAG) break;

                // -e option has no message
                if(optopt == 'e') {
                    missingError = 2;
                }

                // -m option has no small file to overlay
                if(optopt == 'm') {
                    missingError = 1;
                }

                if(optopt == 'w') {
                    missingError = 3;
                }

                if(optopt == 'g') {
                    missingError = 4;
                }

                // -o option has no output file
                if(optopt == 'o') {
                    if(lastOpt == 'e') {
                        missingError = 2;
                    } else if(lastOpt == 'm') {
                        missingError = 1;
                    }
                }

                if(missingError) {
                    parseOrder[parseOrderIndex++] = ':';
                }
                
                flagsSet |= MISSING_FLAG;
                break;

            case '?':
                if(flagsSet & UNKNOWN_FLAG) break; 

                parseOrder[parseOrderIndex++] = '?';
                unknownOption = optopt;
                flagsSet |= UNKNOWN_FLAG;
                break;
        }
        sndLastOpt = lastOpt;
        lastOpt = opt;
    }

    #define __MAIN_FREE_ALL \
        /* Free the allocated strings */ \
        free(filename); \
        free(message);  \
        free(encodingOutputFile);  \
        free(smallFilename); \
        free(overlayOutputFile); \
        /* Close the png file*/ \
        fclose(pngFile); \

    fseek(pngFile, startPos, SEEK_SET);

    /* Now print the outputs for each operation */
    const char *ptr = parseOrder;
    while(ptr && *ptr != '\0') {
        switch(*ptr) {
            case 's':
                if(png_summary(filename, &summary) == 0) {
                    printSummary(filename, summary);
                    free(summary);
                } else {
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                }
                break;

            case 'p':
                if(png_extract_plte(pngFile, &plteColors, &numColors) == -1) {
                    PRINT_ERROR_PLTE_NOT_FOUND();
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                } else {
                    printPlteColors(filename, plteColors, numColors);
                    free(plteColors);
                }
                break;

            case 'i':
                if(png_extract_ihdr(pngFile, &ihdr) == 0) {
                    PRINT_IHDR(filename, ihdr);
                } else {
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                }

                break;

            case 'e':
                if(!encodingOutputFile) {
                    PRINT_ERROR_ENCODE_REQUIRES();
                    __MAIN_FREE_ALL;
                    return EXIT_FAILURE;
                }

                if(png_encode_lsb(filename, encodingOutputFile, message) == -1) {
                    PRINT_ERROR_ENCODE_FAILED();
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                }
                PRINT_ENCODE_SUCCESS(encodingOutputFile);
                break;

            case 'd': 
                size_t maxLen = 1024;

                if(png_extract_ihdr(pngFile, &ihdr) == 0) {
                    // each pixel encodes a single bit
                    maxLen = ihdr.width * ihdr.height;
                }

                char *decodeBuffer = (char*) malloc(sizeof(char) * maxLen);

                if(!decodeBuffer) {
                    PRINT_ERROR_EXTRACT_FAILED();
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                }

                if(png_extract_lsb(filename, decodeBuffer, maxLen) == -1) {
                    PRINT_ERROR_EXTRACT_FAILED();
                    free(decodeBuffer);
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                }
                PRINT_HIDDEN_MESSAGE(decodeBuffer);

                free(decodeBuffer);
                break;

            case 'm':
                if(widthError || heightError || missingError) break;
                
                if(!smallFilename || !overlayOutputFile) {
                    PRINT_ERROR_OVERLAY_REQUIRES();
                    __MAIN_FREE_ALL;
                    return EXIT_FAILURE;
                }

                if(png_overlay_paste(filename, smallFilename, overlayOutputFile, xOffset, yOffset) == -1) {
                    PRINT_ERROR_OVERLAY_FAILED();
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                }
                PRINT_OVERLAY_SUCCESS(overlayOutputFile);
                break;

            case 'w':
                if(widthError == 1) { 
                    PRINT_ERROR_OVERLAY_REQUIRES();
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                }

                if(widthError == 2) {
                    PRINT_ERROR_WIDTH_REQUIRES();
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                }
                break;

            case 'g':
                if(heightError == 1) { 
                    PRINT_ERROR_OVERLAY_REQUIRES();
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                }

                if(heightError == 2) {
                    PRINT_ERROR_HEIGHT_REQUIRES();
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                }
                break;

            case ':':
                if(missingError == 1) {
                    PRINT_ERROR_OVERLAY_REQUIRES();
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                }

                if(missingError == 2) {
                    PRINT_ERROR_ENCODE_REQUIRES();
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                }

                if(missingError == 3) {
                    PRINT_ERROR_WIDTH_REQUIRES();
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                }

                if(missingError == 4) {
                    PRINT_ERROR_HEIGHT_REQUIRES();
                    __MAIN_FREE_ALL
                    return EXIT_FAILURE;
                }

                break;

            case '?': 
                char unknownOptionBuffer[] = { '-', unknownOption, '\0' };
                PRINT_ERROR_UNKNOWN_OPTION(unknownOptionBuffer);
                __MAIN_FREE_ALL
                return EXIT_FAILURE;
                break;

            default: 
                break;
        }
        ptr++;
        fseek(pngFile, startPos, SEEK_SET); // go to the start of the file for each operation
    }

    // getopt exited early
    if (opt == -1 && optind < argc && argv[optind][0] != '-') {
        PRINT_ERROR_UNKNOWN_OPTION(argv[optind]);
        __MAIN_FREE_ALL
        return EXIT_FAILURE;
    }

    optind = 1; // reset getopt

    __MAIN_FREE_ALL
    
    return EXIT_SUCCESS; 
}
