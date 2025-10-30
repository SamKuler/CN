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

/**
 * @brief Trims leading and trailing whitespace from a string.
 *
 * @param str The string to trim (modified in place).
 */
void trim_whitespace(char *str);

/**
 * @brief Converts a string to uppercase.
 *
 * @param str The string to convert (modified in place).
 */
void to_uppercase(char *str);

#endif