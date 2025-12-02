#ifndef EMBEDDING_H
#define EMBEDDING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// embed encrypted message into image using LSBs in low-contrast mask regions
void embed_message(uint8_t *image,
                   int width,
                   int height,
                   int channels,
                   const uint8_t *encrypted,
                   size_t enc_len,
                   const bool *mask);

// extract encrypted message using the SAME mask pattern;
// returns encrypted length and allocates *encrypted_out
size_t extract_message(uint8_t *image,
                       int width,
                       int height,
                       int channels,
                       const bool *mask,
                       uint8_t **encrypted_out);

#endif


