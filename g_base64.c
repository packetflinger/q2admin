#include "g_local.h"

/**
 *
 */
char* base64_encode(const unsigned char *data, size_t ilen, size_t *olen) {
    int i;
    int j;
    *olen = 4 * ((ilen + 2) / 3);

    char *encoded_data = malloc(*olen);
    if (encoded_data == NULL) {
        return NULL;
    }

    for (i = 0, j = 0; i < ilen;) {
        uint32_t octet_a = i < ilen ? (unsigned char) data[i++] : 0;
        uint32_t octet_b = i < ilen ? (unsigned char) data[i++] : 0;
        uint32_t octet_c = i < ilen ? (unsigned char) data[i++] : 0;
        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (i = 0; i < mod_table[ilen % 3]; i++) {
        encoded_data[*olen - 1 - i] = '=';
    }

    return encoded_data;
}

/**
 *
 */
unsigned char* base64_decode(const char *data, size_t ilen, size_t *olen) {
    int i;
    int j;
    unsigned char *decoded_data;

    if (decoding_table == NULL) {
        decoding_table = malloc(256);

        for (i = 0; i < 64; i++) {
            decoding_table[(unsigned char) encoding_table[i]] = i;
        }
    }

    if (ilen % 4 != 0) {
        return NULL;
    }

    *olen = ilen / 4 * 3;
    if (data[ilen - 1] == '=') {
        (*olen)--;
    }
    if (data[ilen - 2] == '=') {
        (*olen)--;
    }

    decoded_data = malloc(*olen);
    if (decoded_data == NULL) {
        return NULL;
    }

    for (i = 0, j = 0; i < ilen;) {
        uint32_t sextet_a =
                data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_b =
                data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_c =
                data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_d =
                data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];

        uint32_t triple = (sextet_a << 3 * 6) + (sextet_b << 2 * 6)
                + (sextet_c << 1 * 6) + (sextet_d << 0 * 6);

        if (j < *olen) {
            decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        }
        if (j < *olen) {
            decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        }
        if (j < *olen) {
            decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
        }
    }
    free(decoding_table);
    return decoded_data;
}
