/**
 * @file session.c
 * @brief FTP session management implementation
 * @version 0.2
 * @date 2025-11-03
 */
#include "session.h"
#include "auth.h"
#include "logger.h"
#include "filesys.h"
#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Forward declaration of helper functions
static int normalize_and_validate_path(const char *base, const char *path,
                                       char *result, size_t result_size);

session_t *session_create(socket_t control_socket,
                          const char *client_ip,
                          uint16_t client_port,
                          const char *root_dir)
{
    if (control_socket == INVALID_SOCKET_T || !client_ip || !root_dir)
    {
        LOG_ERROR("Invalid parameters for session creation");
        return NULL;
    }

    session_t *session = (session_t *)malloc(sizeof(session_t));
    if (!session)
    {
        LOG_ERROR("Failed to allocate memory for session");
        return NULL;
    }

    // Initialize to zero
    memset(session, 0, sizeof(session_t));

    // Set connection info
    session->control_socket = control_socket;
    strncpy(session->client_ip, client_ip, sizeof(session->client_ip) - 1);
    session->client_ip[sizeof(session->client_ip) - 1] = '\0';
    session->client_port = client_port;

    // Set initial state
    session->state = SESSION_STATE_CONNECTED;
    session->authenticated = 0;
    session->permissions = AUTH_PERM_NONE;
    

    // Set directory information
    strncpy(session->root_dir, root_dir, sizeof(session->root_dir) - 1);
    session->root_dir[sizeof(session->root_dir) - 1] = '\0';
    if (!fs_is_directory(session->root_dir))
    {
        LOG_ERROR("Root directory does not exist or is not a directory: %s", root_dir);
        free(session);
        return NULL;
    }

    strcpy(session->current_dir, "/"); // Start at root
    memset(session->user_home_dir, 0, sizeof(session->user_home_dir)); // No home dir yet

    // Set default transfer parameters
    session->transfer_type = PROTO_TYPE_ASCII;
    session->transfer_mode = PROTO_MODE_STREAM;
    session->data_structure = PROTO_STRU_FILE;

    // Initialize data connection state
    session->data_mode = SESSION_DATA_MODE_NONE;
    session->data_socket = INVALID_SOCKET_T;
    session->data_listen_socket = INVALID_SOCKET_T;

    // Initialize command state
    session->restart_offset = 0;
    session->rename_pending = 0;

    // Initialize timestamps
    session->connect_time = time(NULL);
    session->last_activity = time(NULL);
    session->should_quit = 0;

    // Initialize statistics
    session->bytes_uploaded = 0;
    session->bytes_downloaded = 0;
    session->files_uploaded = 0;
    session->files_downloaded = 0;
    session->commands_received = 0;

    // Initialize mutex
    pthread_mutex_init(&session->lock, NULL);

    LOG_INFO("Session created for client %s:%u", client_ip, client_port);

    return session;
}

void session_destroy(session_t *session)
{
    if (!session)
    {
        return;
    }

    LOG_INFO("Destroying session for client %s:%u",
             session->client_ip, session->client_port);

    // Close data connections
    session_close_data_connection(session);

    // Close control socket
    if (session->control_socket != INVALID_SOCKET_T)
    {
        net_close_socket(session->control_socket);
        session->control_socket = INVALID_SOCKET_T;
    }

    // Destroy mutex
    pthread_mutex_destroy(&session->lock);

    // Free memory
    free(session);
}

int session_set_user(session_t *session, const char *username)
{
    if (!session || !username)
    {
        return -1;
    }

    pthread_mutex_lock(&session->lock);

    strncpy(session->username, username, sizeof(session->username) - 1);
    session->username[sizeof(session->username) - 1] = '\0';
    session->state = SESSION_STATE_WAIT_PASSWORD;

    pthread_mutex_unlock(&session->lock);

    LOG_DEBUG("User set to '%s' for session %s:%u",
              username, session->client_ip, session->client_port);

    return 0;
}

int session_authenticate(session_t *session, const char *password)
{
    if (!session || !password)
    {
        return -1;
    }

    pthread_mutex_lock(&session->lock);

    // Verify credentials
    if (!auth_authenticate(session->username, password))
    {
        pthread_mutex_unlock(&session->lock);
        LOG_WARN("Authentication failed for user '%s' from %s:%u",
                 session->username, session->client_ip, session->client_port);
        return -1;
    }

    // Get user information
    const auth_user_t *user = auth_get_user(session->username);
    if (!user)
    {
        pthread_mutex_unlock(&session->lock);
        LOG_ERROR("Failed to get user info after successful authentication for '%s'",
                  session->username);
        return -1;
    }

    // Set user permissions and home directory
    session->permissions = user->permissions;
    strncpy(session->user_home_dir, user->home_dir, sizeof(session->user_home_dir) - 1);
    session->user_home_dir[sizeof(session->user_home_dir) - 1] = '\0';

    // Mark as authenticated
    session->authenticated = 1;
    session->state = SESSION_STATE_AUTHENTICATED;

    // Try to change to user's home directory if specified
    if (session->user_home_dir[0] != '\0')
    {
        char absolute_home[SESSION_MAX_PATH];
        const char *home_rel = session->user_home_dir;
        
        // Skip leading '/' if present, as fs_join_path will add separators
        if (home_rel[0] == '/')
        {
            home_rel++;
        }
        
        // Build absolute path to home directory
        if (fs_join_path(absolute_home, sizeof(absolute_home),
                        session->root_dir, home_rel) == 0)
        {
            // Check if home directory exists
            if (fs_is_directory(absolute_home))
            {
                // Set current directory to home directory (with leading /)
                strncpy(session->current_dir, session->user_home_dir, sizeof(session->current_dir) - 1);
                session->current_dir[sizeof(session->current_dir) - 1] = '\0';
                LOG_DEBUG("Changed to home directory: %s", session->current_dir);
            }
            else
            {
                LOG_WARN("User home directory does not exist: %s", absolute_home);
            }
        }
    }

    pthread_mutex_unlock(&session->lock);

    LOG_INFO("Session authenticated for user '%s' from %s:%u (permissions: 0x%02X)",
             session->username, session->client_ip, session->client_port, session->permissions);

    return 0;
}

int session_has_permission(session_t *session, auth_permission_t permission)
{
    if (!session)
    {
        return 0;
    }

    if (!session->authenticated)
    {
        return 0;
    }

    pthread_mutex_lock(&session->lock);
    int has_perm = (session->permissions & permission) == permission;
    pthread_mutex_unlock(&session->lock);

    return has_perm;
}

int session_check_path_access(session_t *session, const char *path, auth_permission_t required_permission)
{
    if (!session || !path)
    {
        return 0;
    }

    if (!session->authenticated)
    {
        return 0;
    }

    pthread_mutex_lock(&session->lock);

    // Admin users can access any path
    if (session->permissions & AUTH_PERM_ADMIN)
    {
        pthread_mutex_unlock(&session->lock);
        return 1;
    }

    // Check if user has the required permission
    if ((session->permissions & required_permission) != required_permission)
    {
        pthread_mutex_unlock(&session->lock);
        LOG_DEBUG("User '%s' lacks permission 0x%02X for path '%s'",
                  session->username, required_permission, path);
        return 0;
    }

    // Normalize the target path
    char normalized_path[SESSION_MAX_PATH];
    if (normalize_and_validate_path(session->current_dir, path,
                                    normalized_path, sizeof(normalized_path)) != 0)
    {
        pthread_mutex_unlock(&session->lock);
        LOG_WARN("Invalid path in access check: %s", path);
        return 0;
    }

    // Check if path is within user's home directory
    // Both paths should start with '/'
    if (session->user_home_dir[0] == '\0')
    {
        // No home directory restriction
        pthread_mutex_unlock(&session->lock);
        return 1;
    }

    size_t home_len = strlen(session->user_home_dir);
    
    // Check if normalized_path starts with user_home_dir
    if (strncmp(normalized_path, session->user_home_dir, home_len) == 0)
    {
        // Path must either be exactly the home dir, or start with home_dir/
        if (normalized_path[home_len] == '\0' || normalized_path[home_len] == '/')
        {
            pthread_mutex_unlock(&session->lock);
            return 1;
        }
    }

    pthread_mutex_unlock(&session->lock);
    LOG_WARN("User '%s' attempted to access path outside home directory: %s (home: %s)",
             session->username, normalized_path, session->user_home_dir);
    return 0;
}

int session_change_directory(session_t *session, const char *path)
{
    if (!session || !path)
    {
        return -1;
    }

    char new_path[SESSION_MAX_PATH];
    char absolute_path[SESSION_MAX_PATH];

    pthread_mutex_lock(&session->lock);

    // Normalize and validate the path
    if (normalize_and_validate_path(session->current_dir, path,
                                    new_path, sizeof(new_path)) != 0)
    {
        pthread_mutex_unlock(&session->lock);
        LOG_WARN("Invalid path in change_directory: %s", path);
        return -1;
    }

    // Check path access permission (READ permission needed to list/enter directory)
    // Temporarily unlock to call session_check_path_access
    pthread_mutex_unlock(&session->lock);
    if (!session_check_path_access(session, new_path, AUTH_PERM_READ))
    {
        LOG_WARN("Access denied for user '%s' to directory: %s", session->username, new_path);
        return -1;
    }
    pthread_mutex_lock(&session->lock);

    // Build absolute filesystem path
    // when joining with root_dir, letting fs_join_path handle platform-specific separators
    if (fs_join_path(absolute_path, sizeof(absolute_path),
                     session->root_dir, new_path + 1) != 0)
    { // +1 to skip leading '/'
        pthread_mutex_unlock(&session->lock);
        LOG_WARN("Failed to join path in change_directory");
        return -1;
    }

    // Check if directory exists
    if (!fs_is_directory(absolute_path))
    {
        pthread_mutex_unlock(&session->lock);
        LOG_DEBUG("Directory does not exist: %s", absolute_path);
        return -1;
    }

    // Update current directory
    strncpy(session->current_dir, new_path, sizeof(session->current_dir) - 1);
    session->current_dir[sizeof(session->current_dir) - 1] = '\0';

    pthread_mutex_unlock(&session->lock);

    LOG_DEBUG("Changed directory to '%s' (absolute: %s)", new_path, absolute_path);

    return 0;
}

int session_get_current_directory(session_t *session, char *buffer, size_t buffer_size)
{
    if (!session || !buffer || buffer_size == 0)
    {
        return -1;
    }

    pthread_mutex_lock(&session->lock);
    strncpy(buffer, session->current_dir, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    pthread_mutex_unlock(&session->lock);

    return 0;
}

int session_resolve_path(session_t *session,
                         const char *relative_path,
                         char *absolute_path,
                         size_t buffer_size)
{
    if (!session || !relative_path || !absolute_path || buffer_size == 0)
    {
        return -1;
    }

    char normalized_path[SESSION_MAX_PATH];

    pthread_mutex_lock(&session->lock);

    // Normalize and validate the path
    if (normalize_and_validate_path(session->current_dir, relative_path,
                                    normalized_path, sizeof(normalized_path)) != 0)
    {
        pthread_mutex_unlock(&session->lock);
        LOG_WARN("Invalid path in resolve_path: %s", relative_path);
        return -1;
    }

    // Build absolute filesystem path
    // normalized_path starts with '/', skip it when joining with root_dir
    // when joining with root_dir, as fs_join_path handles the separator
    if (fs_join_path(absolute_path, buffer_size,
                     session->root_dir, normalized_path + 1) != 0)
    {
        pthread_mutex_unlock(&session->lock);
        LOG_WARN("Failed to join path in resolve_path");
        return -1;
    }

    pthread_mutex_unlock(&session->lock);

    return 0;
}

int session_set_port(session_t *session, const char *ip, uint16_t port)
{
    if (!session || !ip)
    {
        return -1;
    }

    pthread_mutex_lock(&session->lock);

    // Close any existing data connection
    if (session->data_socket != INVALID_SOCKET_T)
    {
        net_close_socket(session->data_socket);
        session->data_socket = INVALID_SOCKET_T;
    }

    if (session->data_listen_socket != INVALID_SOCKET_T)
    {
        net_close_socket(session->data_listen_socket);
        session->data_listen_socket = INVALID_SOCKET_T;
    }

    // Store active mode parameters
    strncpy(session->active_ip, ip, sizeof(session->active_ip) - 1);
    session->active_ip[sizeof(session->active_ip) - 1] = '\0';
    session->active_port = port;
    session->data_mode = SESSION_DATA_MODE_ACTIVE;

    pthread_mutex_unlock(&session->lock);

    LOG_DEBUG("Set active mode: %s:%u", ip, port);

    return 0;
}

int session_set_pasv(session_t *session,
                     uint16_t port_min,
                     uint16_t port_max,
                     const char *server_ip)
{
    if (!session || !server_ip)
    {
        return -1;
    }

    pthread_mutex_lock(&session->lock);

    // Close any existing data connection
    if (session->data_socket != INVALID_SOCKET_T)
    {
        net_close_socket(session->data_socket);
        session->data_socket = INVALID_SOCKET_T;
    }

    if (session->data_listen_socket != INVALID_SOCKET_T)
    {
        net_close_socket(session->data_listen_socket);
        session->data_listen_socket = INVALID_SOCKET_T;
    }

    // Create listening socket on dynamic port
    uint16_t assigned_port = 0;
    session->data_listen_socket = net_create_listening_socket_range(
        NET_AF_UNSPEC, port_min, port_max, 1, &assigned_port);

    if (session->data_listen_socket == INVALID_SOCKET_T)
    {
        pthread_mutex_unlock(&session->lock);
        LOG_ERROR("Failed to create passive mode listening socket");
        return -1;
    }

    session->passive_port = assigned_port;
    session->data_mode = SESSION_DATA_MODE_PASSIVE;

    pthread_mutex_unlock(&session->lock);

    LOG_DEBUG("Set passive mode: listening on port %u", assigned_port);

    return 0;
}

int session_open_data_connection(session_t *session, int timeout_ms)
{
    if (!session)
    {
        return -1;
    }

    pthread_mutex_lock(&session->lock);

    // Check if data connection already open
    if (session->data_socket != INVALID_SOCKET_T)
    {
        pthread_mutex_unlock(&session->lock);
        return 0; // Already open
    }

    if (session->data_mode == SESSION_DATA_MODE_ACTIVE)
    {
        // Active mode: connect to client
        session->data_socket = net_connect(session->active_ip, session->active_port);

        if (session->data_socket == INVALID_SOCKET_T)
        {
            pthread_mutex_unlock(&session->lock);
            LOG_ERROR("Failed to connect to client in active mode: %s:%u",
                      session->active_ip, session->active_port);
            return -1;
        }

        LOG_DEBUG("Data connection established in active mode");
    }
    else if (session->data_mode == SESSION_DATA_MODE_PASSIVE)
    {
        // Passive mode: accept incoming connection
        if (session->data_listen_socket == INVALID_SOCKET_T)
        {
            pthread_mutex_unlock(&session->lock);
            LOG_ERROR("No listening socket for passive mode");
            return -1;
        }

        // Wait for connection with timeout
        if (timeout_ms >= 0)
        {
            int ready = net_wait_readable(session->data_listen_socket, timeout_ms);
            if (ready <= 0)
            {
                pthread_mutex_unlock(&session->lock);
                LOG_ERROR("Timeout waiting for passive mode connection");
                return -1;
            }
        }

        session->data_socket = net_accept(session->data_listen_socket, NULL, 0, NULL);

        if (session->data_socket == INVALID_SOCKET_T)
        {
            pthread_mutex_unlock(&session->lock);
            LOG_ERROR("Failed to accept connection in passive mode");
            return -1;
        }

        // Close listening socket after accepting
        net_close_socket(session->data_listen_socket);
        session->data_listen_socket = INVALID_SOCKET_T;

        LOG_DEBUG("Data connection accepted in passive mode");
    }
    else
    {
        pthread_mutex_unlock(&session->lock);
        LOG_ERROR("No data mode set (neither active nor passive)");
        return -1;
    }

    pthread_mutex_unlock(&session->lock);

    return 0;
}

void session_close_data_connection(session_t *session)
{
    if (!session)
    {
        return;
    }

    pthread_mutex_lock(&session->lock);

    if (session->data_socket != INVALID_SOCKET_T)
    {
        net_close_socket(session->data_socket);
        session->data_socket = INVALID_SOCKET_T;
        LOG_DEBUG("Data connection closed");
    }

    if (session->data_listen_socket != INVALID_SOCKET_T)
    {
        net_close_socket(session->data_listen_socket);
        session->data_listen_socket = INVALID_SOCKET_T;
        LOG_DEBUG("Data listening socket closed");
    }

    session->data_mode = SESSION_DATA_MODE_NONE;

    pthread_mutex_unlock(&session->lock);
}

int session_set_type(session_t *session, proto_transfer_type_t type)
{
    if (!session)
    {
        return -1;
    }

    pthread_mutex_lock(&session->lock);
    session->transfer_type = type;
    pthread_mutex_unlock(&session->lock);

    return 0;
}

int session_set_mode(session_t *session, proto_transfer_mode_t mode)
{
    if (!session)
    {
        return -1;
    }

    pthread_mutex_lock(&session->lock);
    session->transfer_mode = mode;
    pthread_mutex_unlock(&session->lock);

    return 0;
}

int session_set_structure(session_t *session, proto_data_structure_t structure)
{
    if (!session)
    {
        return -1;
    }

    pthread_mutex_lock(&session->lock);
    session->data_structure = structure;
    pthread_mutex_unlock(&session->lock);

    return 0;
}

int session_set_restart_offset(session_t *session, long long offset)
{
    if (!session || offset < 0)
    {
        return -1;
    }

    pthread_mutex_lock(&session->lock);
    session->restart_offset = offset;
    pthread_mutex_unlock(&session->lock);

    LOG_DEBUG("Restart offset set to %lld", offset);

    return 0;
}

long long session_get_restart_offset(session_t *session)
{
    if (!session)
    {
        return 0;
    }

    pthread_mutex_lock(&session->lock);
    long long offset = session->restart_offset;
    pthread_mutex_unlock(&session->lock);

    return offset;
}

void session_clear_restart_offset(session_t *session)
{
    if (!session)
    {
        return;
    }

    pthread_mutex_lock(&session->lock);
    session->restart_offset = 0;
    pthread_mutex_unlock(&session->lock);
}

int session_set_rename_from(session_t *session, const char *path)
{
    if (!session || !path)
    {
        return -1;
    }

    pthread_mutex_lock(&session->lock);

    strncpy(session->rename_from, path, sizeof(session->rename_from) - 1);
    session->rename_from[sizeof(session->rename_from) - 1] = '\0';
    session->rename_pending = 1;

    pthread_mutex_unlock(&session->lock);

    LOG_DEBUG("Rename from: %s", path);

    return 0;
}

int session_get_rename_from(session_t *session,
                            char *path,
                            size_t buffer_size)
{
    if (!session || !path || buffer_size == 0)
    {
        return -1;
    }

    pthread_mutex_lock(&session->lock);

    if (!session->rename_pending)
    {
        pthread_mutex_unlock(&session->lock);
        return -1;
    }

    strncpy(path, session->rename_from, buffer_size - 1);
    path[buffer_size - 1] = '\0';

    pthread_mutex_unlock(&session->lock);

    return 0;
}

void session_clear_rename_state(session_t *session)
{
    if (!session)
    {
        return;
    }

    pthread_mutex_lock(&session->lock);
    session->rename_pending = 0;
    memset(session->rename_from, 0, sizeof(session->rename_from));
    pthread_mutex_unlock(&session->lock);
}

void session_update_activity(session_t *session)
{
    if (!session)
    {
        return;
    }

    pthread_mutex_lock(&session->lock);
    session->last_activity = time(NULL);
    pthread_mutex_unlock(&session->lock);
}

int session_is_timed_out(session_t *session, int timeout_seconds)
{
    if (!session || timeout_seconds <= 0)
    {
        return 0;
    }

    pthread_mutex_lock(&session->lock);
    time_t now = time(NULL);
    int timed_out = (now - session->last_activity) > timeout_seconds;
    pthread_mutex_unlock(&session->lock);

    return timed_out;
}

int session_send_response(session_t *session, int code, const char *message)
{
    if (!session || !message)
    {
        return -1;
    }

    char buffer[PROTO_MAX_RESPONSE_LINE];

    if (proto_format_response(buffer, sizeof(buffer), code, message) < 0)
    {
        LOG_ERROR("Failed to format response");
        return -1;
    }

    pthread_mutex_lock(&session->lock);

    int result = net_send_all(session->control_socket, buffer, strlen(buffer));

    pthread_mutex_unlock(&session->lock);

    if (result < 0)
    {
        LOG_ERROR("Failed to send response to client");
        return -1;
    }

    LOG_DEBUG("Sent: %d %s", code, message);

    return 0;
}

int session_send_response_multiline(session_t *session, int code, const char *message)
{
    if (!session || !message)
    {
        return -1;
    }

    char buffer[PROTO_MAX_RESPONSE_LINE];

    if (proto_format_response_multiline(buffer, sizeof(buffer), code, message) < 0)
    {
        LOG_ERROR("Failed to format multiline response");
        return -1;
    }

    pthread_mutex_lock(&session->lock);

    int result = net_send_all(session->control_socket, buffer, strlen(buffer));

    pthread_mutex_unlock(&session->lock);

    if (result < 0)
    {
        LOG_ERROR("Failed to send multiline response to client");
        return -1;
    }

    LOG_DEBUG("Sent (multiline): %d-%s", code, message);

    return 0;
}

/**
 * @brief Normalizes and validates a path within the FTP virtual filesystem.
 *
 * Handles both absolute paths (starting with '/') and relative paths.
 * Resolves '..' and '.' components.
 * Ensures the result is within the root directory.
 *
 * @param base Base directory (current working directory)
 * @param path Input path (relative or absolute)
 * @param result Buffer to store normalized path
 * @param result_size Size of result buffer
 * @return 0 on success, -1 on error
 */
static int normalize_and_validate_path(const char *base, const char *path,
                                       char *result, size_t result_size)
{
    if (!base || !path || !result || result_size == 0)
    {
        return -1;
    }

    char temp[SESSION_MAX_PATH];

    // If path is absolute, use it directly; otherwise join with base
    if (path[0] == '/')
    {
        strncpy(temp, path, sizeof(temp) - 1);
    }
    else
    {
        // Join base and path
        if (strcmp(base, "/") == 0)
        {
            snprintf(temp, sizeof(temp), "/%s", path);
        }
        else
        {
            snprintf(temp, sizeof(temp), "%s/%s", base, path);
        }
    }
    temp[sizeof(temp) - 1] = '\0';

    // Normalize the path (remove redundant slashes, resolve . and ..)

    if (proto_normalize_path(temp, sizeof(temp)) != 0)
    { // Handle slashes
        return -1;
    }

    // Split into components and resolve
    char *components[256];
    int component_count = 0;

    char *token = strtok(temp, "/");
    while (token != NULL && component_count < 256)
    {
        if (strcmp(token, ".") == 0)
        {
            // Skip current directory
        }
        else if (strcmp(token, "..") == 0)
        {
            // Go up one level
            if (component_count > 0)
            {
                component_count--;
            }
        }
        else
        {
            components[component_count++] = token;
        }
        token = strtok(NULL, "/");
    }

    // Build the normalized path
    if (component_count == 0)
    {
        strncpy(result, "/", result_size);
    }
    else
    {
        result[0] = '\0';
        for (int i = 0; i < component_count; i++)
        {
            strncat(result, "/", result_size - strlen(result) - 1);
            strncat(result, components[i], result_size - strlen(result) - 1);
        }
    }

    // Validate path (no dangerous patterns)
    if (!proto_validate_path(result + 1))
    { // Skip leading '/' for validation
        return -1;
    }

    return 0;
}