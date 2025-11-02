#include "utils.h"
#include <ctype.h>

void get_timestamp(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

const char *extract_filename(const char *path)
{
    const char *filename = strrchr(path, '/');
    if (!filename)
    {
        filename = strrchr(path, '\\'); // Windows PATH
    }
    return filename ? filename + 1 : path;
}

void trim_whitespace(char *str)
{
    if (!str || !*str)
        return;

    // Trim leading whitespace
    char *start = str;
    while (*start && isspace((unsigned char)*start))
        start++;

    // Trim trailing whitespace
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end))
        end--;

    // Move trimmed string to beginning and null-terminate
    size_t len = end - start + 1;
    memmove(str, start, len);
    str[len] = '\0';
}

void to_uppercase(char *str)
{
    if (!str)
        return;

    while (*str)
    {
        *str = (char)toupper((unsigned char)*str);
        str++;
    }
}

long long crlf_to_lf(const char *input_buffer, long long input_len, char *output_buffer, long long output_size)
{
    if (!input_buffer || !output_buffer || input_len < 0 || output_size <= 0)
        return -1;

    long long write_idx = 0;
    for (long long i = 0; i < input_len; i++)
    {
        if (input_buffer[i] == '\r')
        {
            // Check for CRLF sequence
            if (i + 1 < input_len && input_buffer[i+1] == '\n')
            {
                if (write_idx + 1 > output_size) return -1; // Output buffer too small
                output_buffer[write_idx++] = '\n';
                i++; // Skip next character (LF)
            }
            else
            {
                // Lone CR, treat as regular character
                if (write_idx + 1 > output_size) return -1; // Output buffer too small
                output_buffer[write_idx++] = input_buffer[i];
            }
        }
        else
        {
            if (write_idx + 1 > output_size) return -1; // Output buffer too small
            output_buffer[write_idx++] = input_buffer[i];
        }
    }
    return write_idx;
}

long long lf_to_crlf(const char *input_buffer, long long input_len, char *output_buffer, long long output_size)
{
    if (!input_buffer || !output_buffer || input_len < 0 || output_size <= 0)
        return -1;

    long long write_idx = 0;
    for (long long i = 0; i < input_len; i++)
    {
        if (input_buffer[i] == '\n')
        {
            if (write_idx + 2 > output_size) return -1; // Output buffer too small
            output_buffer[write_idx++] = '\r';
            output_buffer[write_idx++] = '\n';
        }
        else
        {
            if (write_idx + 1 > output_size) return -1; // Output buffer too small
            output_buffer[write_idx++] = input_buffer[i];
        }
    }
    return write_idx;
}