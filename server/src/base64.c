#include "base64.h"

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64_encode(const unsigned char *src, size_t len, char *out, size_t out_size) {
    size_t out_len = 4 * ((len + 2) / 3);
    if (out_size < out_len + 1) return -1;

    size_t i, j = 0;
    for (i = 0; i < len; i += 3) {
        unsigned int val = (src[i] << 16);
        if (i + 1 < len) val |= (src[i + 1] << 8);
        if (i + 2 < len) val |= src[i + 2];

        out[j++] = b64_table[(val >> 18) & 0x3F];
        out[j++] = b64_table[(val >> 12) & 0x3F];
        out[j++] = (i + 1 < len) ? b64_table[(val >> 6) & 0x3F] : '=';
        out[j++] = (i + 2 < len) ? b64_table[val & 0x3F] : '=';
    }
    out[j] = '\0';
    return (int)j;
}
