#include "encryption.h"

#include <string.h>
#include <stdlib.h>

// very simple xor stream based on key bytes
// NOTE: this is for demonstration only and is NOT secure crypto

size_t encrypt_message(const char *message,
                       const char *key,
                       uint8_t **encrypted_out) {
    size_t msg_len = strlen(message);
    size_t key_len = strlen(key);

    if (key_len == 0) {
        key = "default-key";
        key_len = strlen(key);
    }

    *encrypted_out = (uint8_t *)malloc(msg_len);
    if (!*encrypted_out) {
        return 0; // allocation failed
    }
    for (size_t i = 0; i < msg_len; i++) {
        uint8_t k = (uint8_t)key[i % key_len];
        (*encrypted_out)[i] = ((const uint8_t *)message)[i] ^ k;
    }

    return msg_len;
}

char *decrypt_message(const uint8_t *encrypted,
                      size_t enc_len,
                      const char *key) {
    size_t key_len = strlen(key);

    if (key_len == 0) {
        key = "default-key";
        key_len = strlen(key);
    }

    char *plaintext = (char *)malloc(enc_len + 1);
    if (!plaintext) {
        return NULL; // allocation failed
    }
    for (size_t i = 0; i < enc_len; i++) {
        uint8_t k = (uint8_t)key[i % key_len];
        plaintext[i] = (char)(encrypted[i] ^ k);
    }
    plaintext[enc_len] = '\0';

    return plaintext;
}

