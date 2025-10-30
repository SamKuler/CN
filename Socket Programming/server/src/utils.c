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