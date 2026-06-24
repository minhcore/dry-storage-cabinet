#ifndef FONT_8X12_H_
#define FONT_8X12_H_

#include <stdint.h>

#define FONT_8X12_COUNT    ($(out_images_count))

typedef struct
{
    uint8_t ascii_code;
    uint8_t bitmap[16];
} font_t;

extern const font_t font_8x12[FONT_8X12_COUNT];

#endif