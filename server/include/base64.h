#ifndef BASE64_H
#define BASE64_H

#include <stddef.h>

int base64_encode(const unsigned char *src, size_t len, char *out, size_t out_size);

#endif
