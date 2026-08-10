#include "crypto_utils.h"

void sha256_hex(const char *input, char *output_hex) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)input, strlen(input), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        snprintf(output_hex + 2*i, 3, "%02x", hash[i]);
    output_hex[2*SHA256_DIGEST_LENGTH] = '\0';
}

unsigned long djb2_hash(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}
