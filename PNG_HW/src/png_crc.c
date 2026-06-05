#include "png_crc.h"

uint32_t table[256] = { 0u };
uint8_t tableIsComputed = 0u;

uint32_t crcPolynomial = 0xEDB88320;

void _init_table() 
{       
    if(tableIsComputed) return;

    for(int n = 0; n < 256; n++) {
        uint32_t val = (uint32_t) n;
        for(int i = 0; i < 8; i++) {
            if(val & 1) {
                val = crcPolynomial ^ (val >> 1);
            } else {
                val >>= 1;
            }
        }
        table[n] = val;
    }
    tableIsComputed = 1u;
}

uint32_t png_crc(const uint8_t *buf, size_t len)
{
    if(!buf) return 0;

    if(!tableIsComputed) {
        _init_table();
    }

    uint32_t crcReg = (uint32_t) ~0u;
    
    for(int i = 0; i < len; i++) {
        uint8_t byte = buf[i];

        crcReg = table[(crcReg ^ byte) & 0xFF] ^ (crcReg >> 8);
    }

    return crcReg ^ ((uint32_t) ~0u);
}

