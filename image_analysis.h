#ifndef IMAGE_ANALYSIS_H
#define IMAGE_ANALYSIS_H

#include <stdint.h>
#include <stdbool.h>

// find low contrast regions in image
// image: width * height * channels (RGB or grayscale)
// returns mask (width * height, true = can embed here), caller must free
bool *find_low_contrast_regions(uint8_t *image,
                                int width,
                                int height,
                                int channels);

#endif


