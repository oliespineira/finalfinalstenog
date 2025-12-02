#include "embedding.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void embed_message(uint8_t *image,
                   int width,
                   int height,
                   int channels,
                   const uint8_t *encrypted,
                   size_t enc_len,
                   const bool *mask) {
    // prepare data: length (4 bytes big-endian) + encrypted message
    size_t total_size = 4 + enc_len;
    uint8_t *full_data = (uint8_t *)malloc(total_size);
    if (!full_data) {
        printf("❌ memory allocation failed in embed_message\n");
        return;
    }

    full_data[0] = (uint8_t)((enc_len >> 24) & 0xFF);
    full_data[1] = (uint8_t)((enc_len >> 16) & 0xFF);
    full_data[2] = (uint8_t)((enc_len >> 8) & 0xFF);
    full_data[3] = (uint8_t)(enc_len & 0xFF);
    memcpy(full_data + 4, encrypted, enc_len);

    int total_bits = (int)(total_size * 8);
    int bit_index = 0;

    printf("embedding %d bits into low-contrast regions...\n", total_bits);

    for (int y = 0; y < height && bit_index < total_bits; y++) {
        for (int x = 0; x < width && bit_index < total_bits; x++) {
            int pixel_idx = y * width + x;
            if (!mask[pixel_idx]) {
                continue;
            }

            for (int c = 0; c < channels && bit_index < total_bits; c++) {
                int img_idx = pixel_idx * channels + c;

                int byte_idx = bit_index / 8;
                int bit_pos = 7 - (bit_index % 8);
                int bit = (full_data[byte_idx] >> bit_pos) & 1;

                image[img_idx] = (uint8_t)((image[img_idx] & 0xFE) | bit);

                bit_index++;
            }
        }
    }

    printf("✓ embedded %d bits\n", bit_index);
    free(full_data);
}

size_t extract_message(uint8_t *image,
                       int width,
                       int height,
                       int channels,
                       const bool *mask,
                       uint8_t **encrypted_out) {
    // phase 1: read 4-byte length header
    uint8_t header[4];
    int header_bits = 0;
    int bits_in_byte = 0;
    int byte_idx = 0;
    uint8_t current_byte = 0;

    for (int y = 0; y < height && header_bits < 32; y++) {
        for (int x = 0; x < width && header_bits < 32; x++) {
            int pixel_idx = y * width + x;
            if (!mask[pixel_idx]) {
                continue;
            }

            for (int c = 0; c < channels && header_bits < 32; c++) {
                int img_idx = pixel_idx * channels + c;
                int bit = image[img_idx] & 1;

                current_byte = (uint8_t)((current_byte << 1) | bit);
                bits_in_byte++;
                header_bits++;

                if (bits_in_byte == 8) {
                    header[byte_idx++] = current_byte;
                    current_byte = 0;
                    bits_in_byte = 0;
                }
            }
        }
    }

    size_t msg_len = ((size_t)header[0] << 24) |
                     ((size_t)header[1] << 16) |
                     ((size_t)header[2] << 8) |
                     (size_t)header[3];

    *encrypted_out = (uint8_t *)malloc(msg_len);
    if (!*encrypted_out) {
        printf("❌ memory allocation failed in extract_message\n");
        return 0;
    }

    // phase 2: walk the same masked pixels again to read the message bits
    size_t bits_needed = msg_len * 8;
    size_t bits_collected = 0;
    size_t bits_seen = 0; // including header bits

    bits_in_byte = 0;
    byte_idx = 0;
    current_byte = 0;

    for (int y = 0; y < height && bits_collected < bits_needed; y++) {
        for (int x = 0; x < width && bits_collected < bits_needed; x++) {
            int pixel_idx = y * width + x;
            if (!mask[pixel_idx]) {
                continue;
            }

            for (int c = 0; c < channels && bits_collected < bits_needed; c++) {
                int img_idx = pixel_idx * channels + c;
                int bit = image[img_idx] & 1;

                if (bits_seen < 32) {
                    bits_seen++;
                    continue; // skip header bits
                }

                current_byte = (uint8_t)((current_byte << 1) | bit);
                bits_in_byte++;
                bits_collected++;
                bits_seen++;

                if (bits_in_byte == 8) {
                    (*encrypted_out)[byte_idx++] = current_byte;
                    current_byte = 0;
                    bits_in_byte = 0;
                }
            }
        }
    }

    printf("✓ extracted %zu encrypted bytes\n", msg_len);
    return msg_len;
}

