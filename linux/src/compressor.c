#include "compressor.h"

void char_to_bits(char c, int bits[8])
{
    for (int i = 0; i < 8; i++)
    {
        bits[7 - i] = (c >> i) & 1;
    }
}

void char_to_binary_string(unsigned char c, char bits[9]) {
    for (int i = 7; i >= 0; i--) {
        bits[7 - i] = ((c >> i) & 1) + '0';
    }
    bits[8] = '\0';
}

Pair find_longest_match(char* buffer, int pos, int window_size, int max_look_ahead)
{
    if (pos == 0)
    {
        return (Pair) {0, 0};
    }
    int best_len = 0;
    int best_offset = 0;
    int buffer_len = strlen(buffer);

    int window_start = (pos > window_size) ? pos - window_size : 0;

    for (int i = window_start; i < pos; i++)
    {
        int len = 0;

        while (pos + len < buffer_len && len < max_look_ahead && buffer[i + len] == buffer[pos + len])
        {
            len++;
        }

        if (len > best_len && len >= MIN_LZ)
        {
            best_len = len;
            best_offset = pos - i;
        }
    }

    return (Pair) {best_len, best_offset};
}

void compress_string(char* buffer, char* compressed_buffer)
{
    int buffer_len = strlen(buffer);

    compressed_buffer[0] = '\0';
    int pos = 0;

    Pair match = {-1, -1};

    char bits[9];
    char temp[50];

    while (pos < buffer_len)
    {

        int look_ahead_len = buffer_len - pos;
        if (look_ahead_len > LOOK_AHEAD_WINDOW)
        {
            look_ahead_len = LOOK_AHEAD_WINDOW;
        }
    
        match = find_longest_match(buffer, pos, SEARCH_WINDOW, LOOK_AHEAD_WINDOW);

        if (match.length >= MIN_LZ)
        {
            sprintf(temp, "1 %d %d ", match.offset, match.length);
            strcat(compressed_buffer, temp);

            pos += match.length;
        }
        else
        {
            char_to_binary_string(buffer[pos], bits);
            strcat(compressed_buffer, bits);
            strcat(compressed_buffer, " ");

            char single_char[2] = {buffer[pos], '\0'};
            
            pos++;
        }
    }
}

void decompress_string(char* compressed, char* decompressed)
{
    decompressed[0] = '\0';
    char* token = strtok(compressed, " ");
    int pos = 0;

    while (token != NULL)
    {
        if (strcmp(token, "1") == 0)
        {
            char* offset_str = strtok(NULL, " ");
            char* length_str = strtok(NULL, " ");

            if (!offset_str || !length_str)
            {
                break;
            }

            int offset = atoi(offset_str);
            int length = atoi(length_str);

            for (int i = 0; i < length; i++)
            {
                decompressed[pos + i] = decompressed[pos - offset + i];
            }
            decompressed[pos + length] = '\0';
            pos += length;
        }
        else
        {
            char c = 0;
            
            for (int i = 0; i < 8; i++)
            {
                c = (c << 1) | (token[i] - '0');
            }

            decompressed[pos] = c;
            pos++;
            decompressed[pos] = '\0';
        }

        token = strtok(NULL, " ");
    }
}

int run()
{
    char input1[] = "ABABABABABABABAB";
    char input2[] = "HELLO HELLO HELLO HELLO";
    char input3[] = "AAAAAAAAAAAAAAAAAAAA";
    char input4[] = "The quick brown fox jumps over the lazy dog";

    char compressed[8192];
    char decompressed[8192];

    char* tests[] = { input1, input2, input3, input4 };
    int num_tests = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < num_tests; i++)
    {
        printf("==== TEST %d ====\n", i + 1);
        printf("Original:     \"%s\"\n", tests[i]);

        int original_bytes = strlen(tests[i]);
        int original_bits = original_bytes * 8;

        compress_string(tests[i], compressed);
        int compressed_bytes = strlen(compressed);

        printf("Compressed:   %s\n", compressed);

        char compressed_copy[8192];
        strcpy(compressed_copy, compressed);

        decompress_string(compressed_copy, decompressed);
        printf("Decompressed: \"%s\"\n", decompressed);

        printf("Result: %s\n",
               strcmp(tests[i], decompressed) == 0 ? "PASS" : "FAIL");

        int total_bits = 0;
        char parse_buf[8192];
        strcpy(parse_buf, compressed);

        char* token = strtok(parse_buf, " ");
        while (token)
        {
            if (strcmp(token, "1") == 0)
            {
                strtok(NULL, " ");
                strtok(NULL, " ");
                total_bits += 18;
            }
            else
            {
                total_bits += 9;
            }
            token = strtok(NULL, " ");
        }

        double theoretical_factor = (double)total_bits / original_bits * 100.0;
        double ascii_factor = (double)compressed_bytes / original_bytes * 100.0;

        printf("Theoretical size: %.2f%% of original\n", theoretical_factor);

        printf("Original size:   %d bytes\n", original_bytes);
        printf("Compressed size: %d bytes\n", compressed_bytes);

        printf("\n");
    }

    return 0;
}
