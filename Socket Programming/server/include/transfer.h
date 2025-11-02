/**
 * @file transfer.h
 * @brief FTP data transfer operations
 * @version 0.1
 * @date 2025-11-02
 */

#ifndef TRANSFER_H
#define TRANSFER_H

#include "session.h"

/**
 * @brief Transfer buffer size for file operations
 */
#define TRANSFER_BUFFER_SIZE 65536 // 64KB

/**
 * @brief Sends a file to the client through the data connection
 *
 * Binary mode transmission.
 *
 * @param session The FTP session
 * @param filepath Absolute filesystem path to the file
 * @param offset Starting byte offset (for REST command support)
 * @return 0 on success, -1 on error
 */
int transfer_send_file(session_t *session, const char *filepath, long long offset);

/**
 * @brief Receives a file from the client through the data connection
 *
 * @param session The FTP session
 * @param filepath Absolute filesystem path to save the file
 * @param offset Starting byte offset (for APPE/REST command support)
 * @return 0 on success, -1 on error
 */
int transfer_receive_file(session_t *session, const char *filepath, long long offset);

/**
 * @brief Sends a file to the client through the data connection
 *
 * With ASCII Mode. Converts LF to CRLF for transmission
 *
 * @param session The FTP session
 * @param filepath Absolute filesystem path to the file
 * @param offset Starting byte offset
 * @return 0 on success, -1 on error
 */
int transfer_send_file_ascii(session_t *session, const char *filepath, long long offset);

/**
 * @brief Receives a file from the client through the data connection
 *
 * With ASCII Mode. Converts CRLF to local type.
 *
 * @param session The FTP session
 * @param filepath Absolute filesystem path to save the file
 * @param offset Starting byte offset
 * @return 0 on success, -1 on error
 */
int transfer_receive_file_ascii(session_t *session, const char *filepath, long long offset);

/**
 * @brief Sends directory listing to the client (LIST command)
 *
 * Binary mode transmission.
 *
 * @param session The FTP session
 * @param dirpath Absolute filesystem path to the directory
 * @return 0 on success, -1 on error
 */
int transfer_send_list(session_t *session, const char *dirpath);

/**
 * @brief Sends name list to the client (NLST command)
 *
 * @param session The FTP session
 * @param dirpath Absolute filesystem path to the directory
 * @return 0 on success, -1 on error
 */
int transfer_send_nlst(session_t *session, const char *dirpath);

#endif // TRANSFER_H