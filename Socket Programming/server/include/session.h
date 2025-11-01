/**
 * @file session.h
 * @brief FTP session management and state tracking
 * @version 0.1
 * @date 2025-10-31
 */
#ifndef SESSION_H
#define SESSION_H

#include "network.h"
#include "protocol.h"
#include <stdint.h>
#include <pthread.h>

/**
 * @brief Maximum length of username
 */
#define SESSION_MAX_USERNAME 256

/**
 * @brief Maximum length of directory path
 */
#define SESSION_MAX_PATH 1024

/**
 * @brief Session states for authentication flow
 */
typedef enum
{
    SESSION_STATE_CONNECTED,     // Just connected, waiting for USER
    SESSION_STATE_WAIT_PASSWORD, // USER received, waiting for PASS
    SESSION_STATE_AUTHENTICATED, // Fully authenticated
    SESSION_STATE_CLOSING        // Connection closing
} session_state_t;

/**
 * @brief Data connection mode
 */
typedef enum
{
    SESSION_DATA_MODE_NONE,   // No data connection established
    SESSION_DATA_MODE_ACTIVE, // Active mode (PORT)
    SESSION_DATA_MODE_PASSIVE // Passive mode (PASV)
} session_data_mode_t;

/**
 * @brief FTP session structure
 *
 * Contains all state information for a single FTP client connection.
 */
typedef struct
{
    // Connection information
    socket_t control_socket; // Control connection socket
    char client_ip[64];      // Client IP address
    uint16_t client_port;    // Client port number

    // Authentication state
    session_state_t state;               // Current session state
    char username[SESSION_MAX_USERNAME]; // Username (if authenticated)
    int authenticated;                   // 1 if authenticated, 0 otherwise

    // Directory management
    char root_dir[SESSION_MAX_PATH];    // Root directory (chroot)
    char current_dir[SESSION_MAX_PATH]; // Current working directory (relative to root)

    // Transfer parameters
    proto_transfer_type_t transfer_type;   // ASCII or Binary. EBCDIC is rarely used
    proto_transfer_mode_t transfer_mode;   // Stream, Block, or Compressed
    proto_data_structure_t data_structure; // File, Record, or Page

    // Data connection
    session_data_mode_t data_mode; // Active or Passive mode
    socket_t data_socket;          // Data connection socket
    socket_t data_listen_socket;   // Listening socket for passive mode

    // Active mode parameters (PORT command)
    char active_ip[64];   // Client IP for active mode
    uint16_t active_port; // Client port for active mode

    // Passive mode parameters (PASV command)
    uint16_t passive_port; // Server port for passive mode

    // Command state
    char rename_from[SESSION_MAX_PATH]; // Temporary storage for RNFR command
    long long restart_offset;           // File offset for REST command
    int rename_pending;                 // 1 if RNFR was issued, waiting for RNTO

    // Thread safety
    pthread_mutex_t lock; // Mutex for thread-safe access

    // Session management
    time_t connect_time;  // Time when session was established
    time_t last_activity; // Time of last activity
    int should_quit;      // 1 if session should terminate
} session_t;

/**
 * @brief Creates a new FTP session.
 *
 * @param control_socket The control connection socket
 * @param client_ip Client IP address string
 * @param client_port Client port number
 * @param root_dir Root directory for this session (for chroot)
 * @return Pointer to newly created session, or NULL on error
 */
session_t *session_create(socket_t control_socket,
                          const char *client_ip,
                          uint16_t client_port,
                          const char *root_dir);

/**
 * @brief Destroys an FTP session and frees resources.
 *
 * @param session Pointer to session to destroy
 */
void session_destroy(session_t *session);

/**
 * @brief Sets the authenticated user for a session.
 *
 * @param session Pointer to session
 * @param username Username to set
 * @return 0 on success, -1 on error
 */
int session_set_user(session_t *session, const char *username);

/**
 * @brief Marks the session as authenticated.
 *
 * @param session Pointer to session
 * @return 0 on success, -1 on error
 */
int session_authenticate(session_t *session);

/**
 * @brief Changes the current working directory.
 *
 * The path can be absolute (relative to root_dir) or relative to current_dir.
 * Validates that the resulting path is within root_dir (no path traversal).
 *
 * @param session Pointer to session
 * @param path Directory path to change to
 * @return 0 on success, -1 on error
 */
int session_change_directory(session_t *session, const char *path);

/**
 * @brief Gets the absolute filesystem path from a session-relative path.
 *
 * Resolves relative paths against current_dir and prepends root_dir.
 * Validates the path to prevent directory traversal attacks.
 *
 * @param session Pointer to session
 * @param relative_path Path relative to current directory (or absolute within session)
 * @param absolute_path Buffer to store the resolved absolute path
 * @param buffer_size Size of absolute_path buffer
 * @return 0 on success, -1 on error or security violation
 */
int session_resolve_path(session_t *session,
                         const char *relative_path,
                         char *absolute_path,
                         size_t buffer_size);

/**
 * @brief Sets up a data connection in active mode (PORT).
 *
 * @param session Pointer to session
 * @param ip Client IP address
 * @param port Client port number
 * @return 0 on success, -1 on error
 */
int session_set_port(session_t *session, const char *ip, uint16_t port);

/**
 * @brief Sets up a data connection in passive mode (PASV).
 *
 * Creates a listening socket on a dynamic port.
 *
 * @param session Pointer to session
 * @param port_min Minimum port number for passive mode
 * @param port_max Maximum port number for passive mode
 * @param server_ip Server IP address to advertise to client
 * @return 0 on success, -1 on error
 */
int session_set_pasv(session_t *session,
                     uint16_t port_min,
                     uint16_t port_max,
                     const char *server_ip);

/**
 * @brief Establishes the data connection.
 *
 * For active mode, connects to the client.
 * For passive mode, accepts the incoming connection.
 *
 * @param session Pointer to session
 * @param timeout_ms Timeout in milliseconds (-1 for no timeout)
 * @return 0 on success, -1 on error
 */
int session_open_data_connection(session_t *session, int timeout_ms);

/**
 * @brief Closes the data connection.
 *
 * @param session Pointer to session
 */
void session_close_data_connection(session_t *session);

/**
 * @brief Sets the transfer type (ASCII or Binary).
 *
 * @param session Pointer to session
 * @param type Transfer type
 * @return 0 on success, -1 on error
 */
int session_set_type(session_t *session, proto_transfer_type_t type);

/**
 * @brief Sets the transfer mode (Stream, Block, or Compressed).
 *
 * @param session Pointer to session
 * @param mode Transfer mode
 * @return 0 on success, -1 on error
 */
int session_set_mode(session_t *session, proto_transfer_mode_t mode);

/**
 * @brief Sets the data structure (File, Record, or Page).
 *
 * @param session Pointer to session
 * @param structure Data structure
 * @return 0 on success, -1 on error
 */
int session_set_structure(session_t *session, proto_data_structure_t structure);

/**
 * @brief Sets the restart offset for resuming transfers.
 *
 * @param session Pointer to session
 * @param offset Byte offset to restart from
 * @return 0 on success, -1 on error
 */
int session_set_restart_offset(session_t *session, long long offset);

/**
 * @brief Gets and clears the restart offset.
 *
 * @param session Pointer to session
 * @return The current restart offset, then resets it to 0
 */
long long session_pop_restart_offset(session_t *session);

/**
 * @brief Stores the source path for a rename operation (RNFR).
 *
 * @param session Pointer to session
 * @param path Source path for rename
 * @return 0 on success, -1 on error
 */
int session_set_rename_from(session_t *session, const char *path);

/**
 * @brief Gets and clears the rename source path.
 *
 * @param session Pointer to session
 * @param path Buffer to store the rename source path
 * @param buffer_size Size of path buffer
 * @return 0 on success, -1 if no rename is pending
 */
int session_pop_rename_from(session_t *session,
                            char *path,
                            size_t buffer_size);

/**
 * @brief Updates the last activity timestamp.
 *
 * @param session Pointer to session
 */
void session_update_activity(session_t *session);

/**
 * @brief Checks if the session has timed out.
 *
 * @param session Pointer to session
 * @param timeout_seconds Timeout duration in seconds
 * @return 1 if timed out, 0 otherwise
 */
int session_is_timed_out(session_t *session, int timeout_seconds);

/**
 * @brief Sends a response message on the control connection.
 *
 * This is a wrapper of proto_format_response() and network sending.
 *
 * @param session Pointer to session
 * @param code FTP response code
 * @param message Response message
 * @return 0 on success, -1 on error
 */
int session_send_response(session_t *session, int code, const char *message);

/**
 * @brief Sends a multi-line response message on the control connection.
 *
 * This is a wrapper of proto_format_response_multiline() and network sending.
 * It does not automatically send ending message.
 *
 * @param session Pointer to session
 * @param code FTP response code
 * @param message Response message
 * @return 0 on success, -1 on error
 */
int session_send_response_multiline(session_t *session, int code, const char *message);

#endif // SESSION_H
