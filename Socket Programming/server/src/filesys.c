/**
 * @file filesys.c
 * @brief Implementations for filesystem helper functions.
 *
 * @version 0.1
 * @date 2025-10-15
 *
 */
#include "filesys.h"

#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <io.h>
#else
#include <limits.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif
