#ifndef UTILS_H
#define UTILS_H

#include <time.h>
#include <string.h>

/**
 * @brief Get timestamp string with format %Y-%m-%d %H:%M:%S
 *
 * @param buffer Output buffer
 * @param size Buffer size
 */
void get_timestamp(char *buffer, size_t size);

/**
 * @brief Extract filename from path
 *
 * @param path Absolute path
 * @return filename
 */
const char *extract_filename(const char *path);

#endif