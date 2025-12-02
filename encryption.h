#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include <stdint.h>
#include <stddef.h>

// simple xor-based "encryption" (NOT secure, demo only)
// returns encrypted data size and allocates *encrypted_out
size_t encrypt_message(const char *message,
                       const char *key,
                       uint8_t **encrypted_out);

// decrypt message, returns heap-allocated null-terminated string
char *decrypt_message(const uint8_t *encrypted,
                      size_t enc_len,
                      const char *key);

#endif


