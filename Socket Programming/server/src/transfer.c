/**
 * @file transfer.c
 * @brief FTP data transfer operations implementation
 * @version 0.1
 * @date 2025-11-02
 */

#include "transfer.h"
#include "filesys.h"
#include "network.h"
#include "logger.h"
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#endif

transfer_status_t transfer_send_file(session_t *session, const char *filepath, long long offset)
{
    if (!session || !filepath)
    {
        LOG_ERROR("Invalid parameters for transfer_send_file");
        return TRANSFER_STATUS_INTERNAL_ERROR;
    }

    long long file_size = fs_get_file_size(filepath);
    if (file_size < 0)
    {
        LOG_ERROR("Cannot get file size: %s", filepath);
        return TRANSFER_STATUS_IO_ERROR;
    }

    if (offset > file_size)
    {
        LOG_ERROR("Offset %lld exceeds file size %lld", offset, file_size);
        return TRANSFER_STATUS_IO_ERROR;
    }

    char *buffer = malloc(TRANSFER_BUFFER_SIZE);
    if (!buffer)
    {
        LOG_ERROR("Failed to allocate transfer buffer");
        return TRANSFER_STATUS_INTERNAL_ERROR;
    }

    long long remaining = file_size - offset;
    long long current_offset = offset;
    long long total_sent = 0;
    transfer_status_t status = TRANSFER_STATUS_OK;

    LOG_INFO("Starting file transfer: %s (size: %lld, offset: %lld)",
             filepath, file_size, offset);

    while (remaining > 0)
    {
        size_t to_read = (remaining > TRANSFER_BUFFER_SIZE) ? TRANSFER_BUFFER_SIZE : (size_t)remaining;

        long long bytes_read = fs_read_file_chunk(filepath, buffer,
                                                  current_offset, to_read);

        if (bytes_read < 0)
        {
            LOG_ERROR("Failed to read file chunk at offset %lld", current_offset);
            status = TRANSFER_STATUS_IO_ERROR;
            break;
        }

        if (bytes_read == 0)
        {
            LOG_ERROR("Unexpected EOF while reading %s", filepath);
            status = TRANSFER_STATUS_IO_ERROR;
            break;
        }

        if (net_send_all(session->data_socket, buffer, (size_t)bytes_read) != 0)
        {
            LOG_ERROR("Failed to send data to client");
            status = TRANSFER_STATUS_CONN_ERROR;
            break;
        }

        current_offset += bytes_read;
        remaining -= bytes_read;
        total_sent += bytes_read;
    }

    free(buffer);

    if (status == TRANSFER_STATUS_OK)
    {
        LOG_INFO("File transfer completed: %lld bytes sent", total_sent);

        // Update session statistics
        pthread_mutex_lock(&session->lock);
        session->bytes_downloaded += total_sent;
        session->files_downloaded++;
        pthread_mutex_unlock(&session->lock);
    }
    else
    {
        LOG_ERROR("File transfer failed after %lld bytes", total_sent);
    }

    return status;
}

transfer_status_t transfer_receive_file(session_t *session, const char *filepath, long long offset)
{
    if (!session || !filepath)
    {
        LOG_ERROR("Invalid parameters for transfer_receive_file");
        return TRANSFER_STATUS_INTERNAL_ERROR;
    }

    char *buffer = malloc(TRANSFER_BUFFER_SIZE);
    if (!buffer)
    {
        LOG_ERROR("Failed to allocate transfer buffer");
        return TRANSFER_STATUS_INTERNAL_ERROR;
    }

    long long total_received = 0;
    transfer_status_t status = TRANSFER_STATUS_OK;

    LOG_INFO("Starting file reception: %s (offset: %lld)", filepath, offset);

    while (1)
    {
        int bytes_received = net_receive(session->data_socket, buffer,
                                         TRANSFER_BUFFER_SIZE);

        if (bytes_received < 0)
        {
            LOG_ERROR("Failed to receive data from client");
            status = TRANSFER_STATUS_CONN_ERROR;
            break;
        }

        if (bytes_received == 0)
        {
            // Client closed connection or end of transmission
            break;
        }

        long long bytes_written = fs_write_file_chunk(filepath, buffer,
                                                      offset + total_received,
                                                      bytes_received);

        if (bytes_written != bytes_received)
        {
            LOG_ERROR("Failed to write to file at offset %lld",
                      offset + total_received);
            status = TRANSFER_STATUS_IO_ERROR;
            break;
        }

        total_received += bytes_received;
    }

    free(buffer);

    if (status == TRANSFER_STATUS_OK)
    {
        LOG_INFO("File reception completed: %lld bytes received", total_received);

        // Update session statistics
        pthread_mutex_lock(&session->lock);
        session->bytes_uploaded += total_received;
        session->files_uploaded++;
        pthread_mutex_unlock(&session->lock);
    }
    else
    {
        LOG_ERROR("File reception failed after %lld bytes", total_received);
    }

    return status;
}

transfer_status_t transfer_send_file_ascii(session_t *session, const char *filepath, long long offset)
{
    if (!session || !filepath)
    {
        LOG_ERROR("Invalid parameters for transfer_send_file_ascii");
        return TRANSFER_STATUS_INTERNAL_ERROR;
    }

    long long file_size = fs_get_file_size(filepath);
    if (file_size < 0)
    {
        LOG_ERROR("Cannot get file size: %s", filepath);
        return TRANSFER_STATUS_IO_ERROR;
    }

    if (offset > file_size)
    {
        LOG_ERROR("Offset %lld exceeds file size %lld", offset, file_size);
        return TRANSFER_STATUS_IO_ERROR;
    }

    char *read_buffer = malloc(TRANSFER_BUFFER_SIZE);
    char *write_buffer = malloc(TRANSFER_BUFFER_SIZE * 2); // Max 2x for CRLF conversion
    if (!read_buffer || !write_buffer)
    {
        LOG_ERROR("Failed to allocate transfer buffers");
        free(read_buffer);
        free(write_buffer);
        return TRANSFER_STATUS_INTERNAL_ERROR;
    }

    long long remaining = file_size - offset;
    long long current_offset = offset;
    long long total_sent = 0;
    transfer_status_t status = TRANSFER_STATUS_OK;

    LOG_INFO("Starting ASCII file transfer: %s (size: %lld, offset: %lld)",
             filepath, file_size, offset);

    while (remaining > 0)
    {
        size_t to_read = (remaining > TRANSFER_BUFFER_SIZE) ? TRANSFER_BUFFER_SIZE : (size_t)remaining;

        long long bytes_read = fs_read_file_chunk(filepath, read_buffer,
                                                  current_offset, to_read);

        if (bytes_read < 0)
        {
            LOG_ERROR("Failed to read file chunk at offset %lld", current_offset);
            status = TRANSFER_STATUS_IO_ERROR;
            break;
        }

        if (bytes_read == 0)
        {
            LOG_ERROR("Unexpected EOF while reading %s in ASCII mode", filepath);
            status = TRANSFER_STATUS_IO_ERROR;
            break;
        }

        long long converted_bytes = lf_to_crlf(read_buffer, bytes_read, write_buffer, TRANSFER_BUFFER_SIZE * 2);
        if (converted_bytes < 0)
        {
            LOG_ERROR("Failed to convert LF to CRLF for sending");
            status = TRANSFER_STATUS_INTERNAL_ERROR;
            break;
        }

        if (net_send_all(session->data_socket, write_buffer, (size_t)converted_bytes) != 0)
        {
            LOG_ERROR("Failed to send data to client in ASCII mode");
            status = TRANSFER_STATUS_CONN_ERROR;
            break;
        }

        current_offset += bytes_read;
        remaining -= bytes_read;
        total_sent += converted_bytes; // total_sent counts bytes sent over network
    }

    free(read_buffer);
    free(write_buffer);

    if (status == TRANSFER_STATUS_OK)
    {
        LOG_INFO("ASCII file transfer completed: %lld bytes sent", total_sent);

        // Update session statistics
        pthread_mutex_lock(&session->lock);
        session->bytes_downloaded += total_sent;
        session->files_downloaded++;
        pthread_mutex_unlock(&session->lock);
    }
    else
    {
        LOG_ERROR("ASCII file transfer failed after %lld bytes", total_sent);
    }

    return status;
}

transfer_status_t transfer_receive_file_ascii(session_t *session, const char *filepath, long long offset)
{
    if (!session || !filepath)
    {
        LOG_ERROR("Invalid parameters for transfer_receive_file_ascii");
        return TRANSFER_STATUS_INTERNAL_ERROR;
    }

    char *read_buffer = malloc(TRANSFER_BUFFER_SIZE);
    char *write_buffer = malloc(TRANSFER_BUFFER_SIZE); // Output buffer for converted data
    if (!read_buffer || !write_buffer)
    {
        LOG_ERROR("Failed to allocate transfer buffers");
        free(read_buffer);
        free(write_buffer);
        return TRANSFER_STATUS_INTERNAL_ERROR;
    }

    long long total_received = 0;
    long long total_written = 0;
    transfer_status_t status = TRANSFER_STATUS_OK;

    LOG_INFO("Starting ASCII file reception: %s (offset: %lld)", filepath, offset);

    while (1)
    {
        int bytes_received = net_receive(session->data_socket, read_buffer,
                                         TRANSFER_BUFFER_SIZE);

        if (bytes_received < 0)
        {
            LOG_ERROR("Failed to receive data from client in ASCII mode");
            status = TRANSFER_STATUS_CONN_ERROR;
            break;
        }

        if (bytes_received == 0)
        {
            // Client closed connection or end of transmission
            break;
        }

        total_received += bytes_received;

        long long bytes_to_write;
        const char *data_to_write;

#ifdef _WIN32
        // On Windows, No conversion needed for received ASCII.
        bytes_to_write = bytes_received;
        data_to_write = read_buffer;
#else
        // On Unix, convert CRLF to LF.
        long long converted_bytes = crlf_to_lf(read_buffer, bytes_received, write_buffer, TRANSFER_BUFFER_SIZE);
        if (converted_bytes < 0)
        {
            LOG_ERROR("Failed to convert CRLF to LF for receiving");
            status = TRANSFER_STATUS_INTERNAL_ERROR;
            break;
        }
        bytes_to_write = converted_bytes;
        data_to_write = write_buffer;
#endif

        long long bytes_written = fs_write_file_chunk(filepath, data_to_write,
                                                      offset + total_written,
                                                      bytes_to_write);

        if (bytes_written != bytes_to_write)
        {
            LOG_ERROR("Failed to write to file at offset %lld in ASCII mode",
                      offset + total_written);
            status = TRANSFER_STATUS_IO_ERROR;
            break;
        }

        total_written += bytes_written;
    }

    free(read_buffer);
    free(write_buffer);

    if (status == TRANSFER_STATUS_OK)
    {
        LOG_INFO("ASCII file reception completed: %lld bytes written", total_written);

        // Update session statistics (use total_received as it reflects actual network bytes)
        pthread_mutex_lock(&session->lock);
        session->bytes_uploaded += total_received;
        session->files_uploaded++;
        pthread_mutex_unlock(&session->lock);
    }
    else
    {
        LOG_ERROR("ASCII file reception failed after %lld bytes", total_written);
    }

    return status;
}

transfer_status_t transfer_send_list(session_t *session, const char *dirpath)
{
    if (!session || !dirpath)
    {
        LOG_ERROR("Invalid parameters for transfer_send_list");
        return TRANSFER_STATUS_INTERNAL_ERROR;
    }

    fs_file_info_t file_list[1024];
    int count = fs_list_directory(dirpath, file_list, 1024);

    if (count < 0)
    {
        LOG_ERROR("Failed to list directory: %s", dirpath);
        return TRANSFER_STATUS_IO_ERROR;
    }

    char line_buffer[1024];
    char date_str[32];

    for (int i = 0; i < count; i++)
    {
        // Extract file type from mode
        char type_char = '-';
        mode_t mode = file_list[i].mode;

        if (S_ISDIR(mode))
            type_char = 'd';
        else if (S_ISLNK(mode))
            type_char = 'l';
        else if (S_ISREG(mode))
            type_char = '-';
        else if (S_ISCHR(mode))
            type_char = 'c';
        else if (S_ISBLK(mode))
            type_char = 'b';
        else if (S_ISFIFO(mode))
            type_char = 'p';
        else if (S_ISSOCK(mode))
            type_char = 's';

        // Extract permissions from mode
        char perms[10];
        perms[0] = (mode & S_IRUSR) ? 'r' : '-';
        perms[1] = (mode & S_IWUSR) ? 'w' : '-';
        perms[2] = (mode & S_IXUSR) ? 'x' : '-';
        perms[3] = (mode & S_IRGRP) ? 'r' : '-';
        perms[4] = (mode & S_IWGRP) ? 'w' : '-';
        perms[5] = (mode & S_IXGRP) ? 'x' : '-';
        perms[6] = (mode & S_IROTH) ? 'r' : '-';
        perms[7] = (mode & S_IWOTH) ? 'w' : '-';
        perms[8] = (mode & S_IXOTH) ? 'x' : '-';
        perms[9] = '\0';

        // Get user and group names. Default is ftp, ftp
        char user_name[32] = "ftp";
        char group_name[32] = "ftp";

#ifndef _WIN32
        struct passwd *pw = getpwuid(file_list[i].uid);
        if (pw)
            snprintf(user_name, sizeof(user_name), "%s", pw->pw_name);
        else
            snprintf(user_name, sizeof(user_name), "%u", file_list[i].uid);

        struct group *gr = getgrgid(file_list[i].gid);
        if (gr)
            snprintf(group_name, sizeof(group_name), "%s", gr->gr_name);
        else
            snprintf(group_name, sizeof(group_name), "%u", file_list[i].gid);
#endif

        // Format last modification time
        struct tm *tm_info = localtime(&file_list[i].last_modified);
        if (!tm_info)
        {
            LOG_ERROR("Failed to convert modification time for %s", file_list[i].name);
            return TRANSFER_STATUS_INTERNAL_ERROR;
        }
        strftime(date_str, sizeof(date_str), "%b %d %H:%M", tm_info);

        // Unix ls -l style
        // Example: -rw-r--r-- 1 user group 1234 Nov 02 12:34 filename.txt
        // For symlinks: lrwxrwxrwx 1 user group 10 Nov 02 12:34 link -> target
        if (file_list[i].type == FS_TYPE_SYMLINK && file_list[i].link_target[0] != '\0')
        {
            snprintf(line_buffer, sizeof(line_buffer),
                     "%c%s %3u %-8s %-8s %12lld %s %s -> %s\r\n",
                     type_char, perms, (unsigned int)file_list[i].nlink,
                     user_name, group_name, file_list[i].size,
                     date_str, file_list[i].name, file_list[i].link_target);
        }
        else
        {
            snprintf(line_buffer, sizeof(line_buffer),
                     "%c%s %3u %-8s %-8s %12lld %s %s\r\n",
                     type_char, perms, (unsigned int)file_list[i].nlink,
                     user_name, group_name, file_list[i].size,
                     date_str, file_list[i].name);
        }

        if (net_send_all(session->data_socket, line_buffer, strlen(line_buffer)) != 0)
        {
            LOG_ERROR("Failed to send listing line");
            return TRANSFER_STATUS_CONN_ERROR;
        }
    }

    LOG_INFO("Sent directory listing: %d entries", count);
    return TRANSFER_STATUS_OK;
}

transfer_status_t transfer_send_nlst(session_t *session, const char *dirpath)
{
    if (!session || !dirpath)
    {
        LOG_ERROR("Invalid parameters for transfer_send_nlst");
        return TRANSFER_STATUS_INTERNAL_ERROR;
    }

    fs_file_info_t file_list[1024];
    int count = fs_list_directory(dirpath, file_list, 1024);

    if (count < 0)
    {
        LOG_ERROR("Failed to list directory: %s", dirpath);
        return TRANSFER_STATUS_IO_ERROR;
    }

    char line_buffer[512];

    for (int i = 0; i < count; i++)
    {
        // NLST format: just filename with CRLF
        snprintf(line_buffer, sizeof(line_buffer), "%s\r\n", file_list[i].name);

        if (net_send_all(session->data_socket, line_buffer, strlen(line_buffer)) != 0)
        {
            LOG_ERROR("Failed to send name list line");
            return TRANSFER_STATUS_CONN_ERROR;
        }
    }

    LOG_INFO("Sent name list: %d entries", count);
    return TRANSFER_STATUS_OK;
}
