#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include "common.h"

void sha256_hex(const char *input, char *output_hex);
unsigned long djb2_hash(const char *str);

#endif
