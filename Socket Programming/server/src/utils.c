#include "utils.h"

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