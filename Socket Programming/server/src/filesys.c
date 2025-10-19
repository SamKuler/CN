/**
 * @file filesys.c
 * @brief Implementations for filesystem helper functions.
 *
 * @version 0.1
 * @date 2025-10-19
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
#include <stdio.h>
#include <string.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Maximum recursion depth to prevent stack overflow and infinite loops */
#define MAX_RECURSION_DEPTH 256

int fs_join_path(char *dest, long long dest_size, const char *dir, const char *name)
{
#ifdef _WIN32
    (void)dest;
    (void)dest_size;
    (void)dir;
    (void)name;
    return -1;
#else
    if (dest == NULL || dir == NULL || name == NULL || dest_size == 0)
        return -1;

    size_t dir_len = strlen(dir);
    int needs_sep = (dir_len > 0 && dir[dir_len - 1] != '/');

    int n = snprintf(dest, dest_size, "%s%s%s",
                     dir, needs_sep ? "/" : "", name);

    return (n >= 0 && n < dest_size) ? 0 : -1;
#endif
}

int fs_path_exists(const char *path)
{
#ifdef _WIN32
    /* Windows implementation to be added later */
    (void)path;
    return 0;
#else
    if (path == NULL)
        return 0;

    struct stat st;
    if (stat(path, &st) == 0)
        return 1;
    return 0;
#endif
}

int fs_is_directory(const char *path)
{
#ifdef _WIN32
    (void)path;
    return 0;
#else
    if (path == NULL)
        return 0;

    struct stat st;
    if (stat(path, &st) != 0)
        return 0;

    return S_ISDIR(st.st_mode) ? 1 : 0;
#endif
}

long long fs_get_file_size(const char *path)
{
#ifdef _WIN32
    (void)path;
    return -1;
#else
    if (path == NULL)
        return -1;

    struct stat st;
    if (stat(path, &st) != 0)
        return -1;

    if (!S_ISREG(st.st_mode))
        return -1;

    return (long long)st.st_size;
#endif
}

static long long dir_size_recursive(const char *path, int depth)
{
    if (depth > MAX_RECURSION_DEPTH)
        return -1;

    long long total = 0;
    DIR *d = opendir(path);
    if (!d)
        return -1;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char childpath[PATH_MAX];
        if (fs_join_path(childpath, PATH_MAX, path, entry->d_name) != 0)
        {
            closedir(d);
            return -1;
        }

        struct stat st;
        /* Use lstat to not follow symbolic links */
        if (lstat(childpath, &st) != 0)
        {
            closedir(d);
            return -1;
        }

        /* Skip symbolic links to avoid infinite loops */
        if (S_ISLNK(st.st_mode))
            continue;

        if (S_ISDIR(st.st_mode))
        {
            long long s = dir_size_recursive(childpath, depth + 1);
            if (s < 0)
            {
                closedir(d);
                return -1;
            }
            total += s;
        }
        else if (S_ISREG(st.st_mode))
        {
            total += (long long)st.st_size;
        }
    }

    closedir(d);
    return total;
}

long long fs_get_directory_size(const char *path)
{
#ifdef _WIN32
    (void)path;
    return -1;
#else
    if (path == NULL)
        return -1;

    struct stat st;
    /* Use lstat to check if path itself is a symlink */
    if (lstat(path, &st) != 0)
        return -1;

    if (!S_ISDIR(st.st_mode))
        return -1;

    return dir_size_recursive(path, 0);
#endif
}

int fs_list_directory(const char *path, fs_file_info_t *file_list, int max_files)
{
#ifdef _WIN32
    (void)path;
    (void)file_list;
    (void)max_files;
    return -1;
#else
    if (path == NULL || file_list == NULL || max_files <= 0)
        return -1;

    DIR *d = opendir(path);
    if (!d)
        return -1;

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL && count < max_files)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char childpath[PATH_MAX];
        if (fs_join_path(childpath, PATH_MAX, path, entry->d_name) != 0)
            continue; // skip overly long names

        struct stat st;
        /* Use lstat to get info about symlinks themselves */
        if (lstat(childpath, &st) != 0)
            continue; // skip entries we can't stat

        // fill file_list[count]
        strncpy(file_list[count].name, entry->d_name, MAX_FILENAME_LEN - 1);
        file_list[count].name[MAX_FILENAME_LEN - 1] = '\0';

        if (S_ISLNK(st.st_mode))
            file_list[count].type = FS_TYPE_SYMLINK;
        else if (S_ISDIR(st.st_mode))
            file_list[count].type = FS_TYPE_DIR;
        else if (S_ISREG(st.st_mode))
            file_list[count].type = FS_TYPE_FILE;
        else
            file_list[count].type = FS_TYPE_UNKNOWN;

        file_list[count].size = S_ISREG(st.st_mode) ? (long long)st.st_size : 0;

        count++;
    }

    closedir(d);
    return count;
#endif
}

long long fs_read_file_all(const char *path, void *buffer, long long buffer_size)
{
#ifdef _WIN32
    (void)path;
    (void)buffer;
    (void)buffer_size;
    return -1;
#else
    if (path == NULL || buffer == NULL || buffer_size <= 0)
        return -1;

    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    struct stat st;
    if (fstat(fd, &st) != 0)
    {
        close(fd);
        return -1;
    }

    long long to_read = (long long)st.st_size;
    if (to_read > buffer_size)
        to_read = buffer_size;

    long long total = 0;
    while (total < to_read)
    {
        ssize_t r = read(fd, (char *)buffer + total, (size_t)(to_read - total));
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            total = -1;
            break;
        }
        if (r == 0)
            break;
        total += r;
    }

    close(fd);
    return total;
#endif
}

long long fs_write_file_all(const char *path, void *buffer, long long size)
{
#ifdef _WIN32
    (void)path;
    (void)buffer;
    (void)size;
    return -1;
#else
    if (path == NULL || buffer == NULL || size < 0)
        return -1;

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return -1;

    long long total = 0;
    while (total < size)
    {
        ssize_t w = write(fd, (char *)buffer + total, (size_t)(size - total));
        if (w < 0)
        {
            if (errno == EINTR)
                continue;
            total = -1;
            break;
        }
        total += w;
    }

    fsync(fd);
    close(fd);
    return total;
#endif
}

long long fs_read_file_chunk(const char *path, void *buffer, long long offset, long long length)
{
#ifdef _WIN32
    (void)path;
    (void)buffer;
    (void)offset;
    (void)length;
    return -1;
#else
    if (path == NULL || buffer == NULL || offset < 0 || length <= 0)
        return -1;

    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    if (lseek(fd, offset, SEEK_SET) == (off_t)-1)
    {
        close(fd);
        return -1;
    }

    long long total = 0;
    while (total < length)
    {
        ssize_t r = read(fd, (char *)buffer + total, (size_t)(length - total));
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            total = -1;
            break;
        }
        if (r == 0)
            break;
        total += r;
    }

    close(fd);
    return total;
#endif
}

long long fs_write_file_chunk(const char *path, const void *buffer, long long offset, long long length)
{
#ifdef _WIN32
    (void)path;
    (void)buffer;
    (void)offset;
    (void)length;
    return -1;
#else
    if (path == NULL || buffer == NULL || offset < 0 || length < 0)
        return -1;

    int fd = open(path, O_WRONLY | O_CREAT, 0644);
    if (fd < 0)
        return -1;

    if (lseek(fd, offset, SEEK_SET) == (off_t)-1)
    {
        close(fd);
        return -1;
    }

    long long total = 0;
    while (total < length)
    {
        ssize_t w = write(fd, (const char *)buffer + total, (size_t)(length - total));
        if (w < 0)
        {
            if (errno == EINTR)
                continue;
            total = -1;
            break;
        }
        total += w;
    }

    fsync(fd);
    close(fd);
    return total;
#endif
}

int fs_create_directory(const char *path)
{
#ifdef _WIN32
    (void)path;
    return -1;
#else
    if (path == NULL)
        return -1;

    if (mkdir(path, 0755) == 0)
        return 0;

    if (errno == EEXIST) // already exists
    {
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
            return 0;
    }

    return -1;
#endif
}

int fs_delete_file(const char *path)
{
#ifdef _WIN32
    (void)path;
    return -1;
#else
    if (path == NULL)
        return -1;

    if (unlink(path) == 0)
        return 0;

    return -1;
#endif
}

static int remove_directory_recursive(const char *path, int depth)
{
    if (depth > MAX_RECURSION_DEPTH)
        return -1;

    DIR *d = opendir(path);
    if (!d)
        return -1;

    struct dirent *entry;
    int ret = 0;
    while ((entry = readdir(d)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char childpath[PATH_MAX];
        if (fs_join_path(childpath, PATH_MAX, path, entry->d_name) != 0)
        {
            ret = -1;
            break;
        }

        struct stat st;
        /* Use lstat to not follow symbolic links */
        if (lstat(childpath, &st) != 0)
        {
            ret = -1;
            break;
        }

        if (S_ISLNK(st.st_mode))
        {
            /* Remove symlink itself, don't follow it */
            if (unlink(childpath) != 0)
            {
                ret = -1;
                break;
            }
        }
        else if (S_ISDIR(st.st_mode))
        {
            if (remove_directory_recursive(childpath, depth + 1) != 0)
            {
                ret = -1;
                break;
            }
            if (rmdir(childpath) != 0)
            {
                ret = -1;
                break;
            }
        }
        else
        {
            if (unlink(childpath) != 0)
            {
                ret = -1;
                break;
            }
        }
    }

    closedir(d);
    return ret;
}

int fs_delete_directory(const char *path, int force_delete)
{
#ifdef _WIN32
    (void)path;
    (void)force_delete;
    return -1;
#else
    if (path == NULL)
        return -1;

    if (!force_delete)
    {
        if (rmdir(path) == 0)
            return 0;
        return -1;
    }

    // force delete recursively
    if (remove_directory_recursive(path, 0) != 0)
        return -1;

    if (rmdir(path) != 0)
        return -1;

    return 0;
#endif
}