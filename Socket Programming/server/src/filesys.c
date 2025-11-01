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
#include <stdarg.h>
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
    if (dest == NULL || dir == NULL || name == NULL || dest_size == 0)
        return -1;
#ifdef _WIN32
    size_t dir_len = strlen(dir);

    // Check if we need to add a separator
    // Windows accepts both \ and /
    // Prefer \ here.
    int needs_sep = (dir_len > 0 && dir[dir_len - 1] != '\\' && dir[dir_len - 1] != '/');

    // Skip leading separator in name if present
    const char *actual_name = name;
    while (*actual_name == '\\' || *actual_name == '/')
        actual_name++;

    int n = snprintf(dest, (size_t)dest_size, "%s%s%s",
                     dir, needs_sep ? "\\" : "", actual_name);

    return (n >= 0 && n < dest_size) ? 0 : -1;
#else
    size_t dir_len = strlen(dir);
    int needs_sep = (dir_len > 0 && dir[dir_len - 1] != '/');

    // Skip leading separator in name if present
    const char *actual_name = name;
    while (*actual_name == '/')
        actual_name++;

    int n = snprintf(dest, dest_size, "%s%s%s",
                     dir, needs_sep ? "/" : "", actual_name);

    return (n >= 0 && n < dest_size) ? 0 : -1;
#endif
}

int fs_path_exists(const char *path)
{
    if (path == NULL)
        return 0;
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES) ? 1 : 0;
#else
    struct stat st;
    if (stat(path, &st) == 0)
        return 1;
    return 0;
#endif
}

int fs_is_directory(const char *path)
{
    if (path == NULL)
        return 0;
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return 0;

    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
#else
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;

    return S_ISDIR(st.st_mode) ? 1 : 0;
#endif
}

long long fs_get_file_size(const char *path)
{
    if (path == NULL)
        return -1;
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fileInfo))
        return -1;

    // Check if it's a regular file (not a directory)
    if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return -1;

    // Combine high and low parts to get 64-bit size
    LARGE_INTEGER size;
    size.HighPart = fileInfo.nFileSizeHigh;
    size.LowPart = fileInfo.nFileSizeLow;

    return (long long)size.QuadPart;
#else
    struct stat st;
    if (stat(path, &st) != 0)
        return -1;

    if (!S_ISREG(st.st_mode))
        return -1;

    return (long long)st.st_size;
#endif
}

#ifdef _WIN32
static long long dir_size_recursive_win32(const char *path, int depth)
{
    if (depth > MAX_RECURSION_DEPTH)
        return -1;

    long long total = 0;
    char search_path[PATH_MAX];

    // Create search pattern: path\*
    if (snprintf(search_path, sizeof(search_path), "%s\\*", path) >= sizeof(search_path))
        return -1;

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE)
        return -1;

    do {
        // Skip . and ..
        if (strcmp(find_data.cFileName, ".") == 0 ||
            strcmp(find_data.cFileName, "..") == 0)
            continue;

        char child_path[PATH_MAX];
        if (fs_join_path(child_path, PATH_MAX, path, find_data.cFileName) != 0)
        {
            FindClose(hFind);
            return -1;
        }

        // Skip reparse points (symbolic links, junctions, etc.) to avoid loops
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            continue;

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // Recurse into subdirectory
            long long s = dir_size_recursive_win32(child_path, depth + 1);
            if (s < 0)
            {
                FindClose(hFind);
                return -1;
            }
            total += s;
        }
        else
        {
            // Regular file - add its size
            LARGE_INTEGER file_size;
            file_size.HighPart = find_data.nFileSizeHigh;
            file_size.LowPart = find_data.nFileSizeLow;
            total += file_size.QuadPart;
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    return total;
}
#else
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
        // Use lstat to not follow symbolic links
        if (lstat(childpath, &st) != 0)
        {
            closedir(d);
            return -1;
        }

        // Skip symbolic links to avoid infinite loops
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
#endif

long long fs_get_directory_size(const char *path)
{
    if (path == NULL)
        return -1;
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return -1;

    if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
        return -1;

    return dir_size_recursive_win32(path, 0);
#else
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
    if (path == NULL || file_list == NULL || max_files <= 0)
        return -1;
#ifdef _WIN32
    char search_path[PATH_MAX];
    if (snprintf(search_path, sizeof(search_path), "%s\\*", path) >= sizeof(search_path))
        return -1;

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE)
        return -1;

    int count = 0;
    do {
        // Skip . and ..
        if (strcmp(find_data.cFileName, ".") == 0 ||
            strcmp(find_data.cFileName, "..") == 0)
            continue;

        if (count >= max_files)
            break;

        // Fill file_list[count]
        strncpy(file_list[count].name, find_data.cFileName, MAX_FILENAME_LEN - 1);
        file_list[count].name[MAX_FILENAME_LEN - 1] = '\0';

        // Determine file type
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            file_list[count].type = FS_TYPE_SYMLINK;
        else if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            file_list[count].type = FS_TYPE_DIR;
        else
            file_list[count].type = FS_TYPE_FILE;

        // Get file size (only for regular files)
        if (file_list[count].type == FS_TYPE_FILE)
        {
            LARGE_INTEGER file_size;
            file_size.HighPart = find_data.nFileSizeHigh;
            file_size.LowPart = find_data.nFileSizeLow;
            file_list[count].size = file_size.QuadPart;
        }
        else
        {
            file_list[count].size = 0;
        }

        count++;
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    return count;
#else
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
    if (path == NULL || buffer == NULL || buffer_size <= 0)
        return -1;
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return -1;

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(hFile, &file_size))
    {
        CloseHandle(hFile);
        return -1;
    }

    long long to_read = file_size.QuadPart;
    if (to_read > buffer_size)
        to_read = buffer_size;

    long long total = 0;
    while (total < to_read)
    {
        DWORD bytes_to_read = (DWORD)((to_read - total) > MAXDWORD ? MAXDWORD : (to_read - total));
        DWORD bytes_read = 0;

        if (!ReadFile(hFile, (char *)buffer + total, bytes_to_read, &bytes_read, NULL))
        {
            CloseHandle(hFile);
            return -1;
        }

        if (bytes_read == 0)
            break;

        total += bytes_read;
    }

    CloseHandle(hFile);
    return total;
#else
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
    if (path == NULL || buffer == NULL || size < 0)
        return -1;
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return -1;

    long long total = 0;
    while (total < size)
    {
        DWORD bytes_to_write = (DWORD)((size - total) > MAXDWORD ? MAXDWORD : (size - total));
        DWORD bytes_written = 0;

        if (!WriteFile(hFile, (char *)buffer + total, bytes_to_write, &bytes_written, NULL))
        {
            CloseHandle(hFile);
            return -1;
        }

        total += bytes_written;
    }

    FlushFileBuffers(hFile);
    CloseHandle(hFile);
    return total;
#else
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
    if (path == NULL || buffer == NULL || offset < 0 || length <= 0)
        return -1;
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return -1;

    LARGE_INTEGER li_offset;
    li_offset.QuadPart = offset;

    if (!SetFilePointerEx(hFile, li_offset, NULL, FILE_BEGIN))
    {
        CloseHandle(hFile);
        return -1;
    }

    long long total = 0;
    while (total < length)
    {
        DWORD bytes_to_read = (DWORD)((length - total) > MAXDWORD ? MAXDWORD : (length - total));
        DWORD bytes_read = 0;

        if (!ReadFile(hFile, (char *)buffer + total, bytes_to_read, &bytes_read, NULL))
        {
            CloseHandle(hFile);
            return -1;
        }

        if (bytes_read == 0)
            break;

        total += bytes_read;
    }

    CloseHandle(hFile);
    return total;
#else
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
    if (path == NULL || buffer == NULL || offset < 0 || length < 0)
        return -1;
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return -1;

    LARGE_INTEGER li_offset;
    li_offset.QuadPart = offset;

    if (!SetFilePointerEx(hFile, li_offset, NULL, FILE_BEGIN))
    {
        CloseHandle(hFile);
        return -1;
    }

    long long total = 0;
    while (total < length)
    {
        DWORD bytes_to_write = (DWORD)((length - total) > MAXDWORD ? MAXDWORD : (length - total));
        DWORD bytes_written = 0;

        if (!WriteFile(hFile, (const char *)buffer + total, bytes_to_write, &bytes_written, NULL))
        {
            CloseHandle(hFile);
            return -1;
        }

        total += bytes_written;
    }

    FlushFileBuffers(hFile);
    CloseHandle(hFile);
    return total;
#else
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
    if (path == NULL)
        return -1;
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL))
        return 0;

    DWORD error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS)
    {
        // Check if it's actually a directory
        DWORD attr = GetFileAttributesA(path);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
            return 0;
    }

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
    if (path == NULL)
        return -1;
#ifdef _WIN32
    if (DeleteFileA(path))
        return 0;

    return -1;
#else
    if (unlink(path) == 0)
        return 0;

    return -1;
#endif
}

#ifdef _WIN32
static int remove_directory_recursive_win32(const char *path, int depth)
{
    if (depth > MAX_RECURSION_DEPTH)
        return -1;

    char search_path[PATH_MAX];
    if (snprintf(search_path, sizeof(search_path), "%s\\*", path) >= sizeof(search_path))
        return -1;

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE)
        return -1;

    int ret = 0;
    do {
        // Skip . and ..
        if (strcmp(find_data.cFileName, ".") == 0 ||
            strcmp(find_data.cFileName, "..") == 0)
            continue;

        char child_path[PATH_MAX];
        if (fs_join_path(child_path, PATH_MAX, path, find_data.cFileName) != 0)
        {
            ret = -1;
            break;
        }

        // Handle reparse points (symlinks, junctions) - delete them without following
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        {
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                // It's a directory junction/symlink - use RemoveDirectory
                if (!RemoveDirectoryA(child_path))
                {
                    ret = -1;
                    break;
                }
            }
            else
            {
                // It's a file symlink - use DeleteFile
                if (!DeleteFileA(child_path))
                {
                    ret = -1;
                    break;
                }
            }
        }
        else if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // Regular directory - recurse
            if (remove_directory_recursive_win32(child_path, depth + 1) != 0)
            {
                ret = -1;
                break;
            }
            if (!RemoveDirectoryA(child_path))
            {
                ret = -1;
                break;
            }
        }
        else
        {
            // Regular file
            if (!DeleteFileA(child_path))
            {
                ret = -1;
                break;
            }
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    return ret;
}
#else
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
#endif

int fs_delete_directory(const char *path, int force_delete)
{
    if (path == NULL)
        return -1;
#ifdef _WIN32
    if (!force_delete)
    {
        // Try to remove empty directory
        if (RemoveDirectoryA(path))
            return 0;
        return -1;
    }

    // Force delete recursively
    if (remove_directory_recursive_win32(path, 0) != 0)
        return -1;

    if (!RemoveDirectoryA(path))
        return -1;

    return 0;
#else
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