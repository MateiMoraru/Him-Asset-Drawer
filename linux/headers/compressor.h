#ifndef COMPRESSOR_H
#define COMRPESSOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_LZ 3
#define MAX_LZ 32

#define KB(v) ((v) * 1024)
#define MB(v) ((v) * 1024 * 1024)

#define SEARCH_WINDOW MB(4)
#define LOOK_AHEAD_WINDOW MB(1)


typedef struct
{
    int length;
    int offset;
} Pair;

void char_to_bits(char c, int bits[8]);

void char_to_binary_string(unsigned char c, char bits[9]);

Pair find_longest_match(char* buffer, int pos, int window_size, int max_look_ahead);
void compress_string(char* buffer, char* compressed_buffer);
void decompress_string(char* compressed, char* decompressed);

#endif