/**
 * @file handlers.c
 * @brief Implementation of FTP command handlers
 * @version 0.1
 * @date 2025-11-03
 */
#include "command.h"
#include "session.h"
#include "protocol.h"
#include "transfer.h"
#include "filesys.h"
#include "logger.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Access Control Commands

int cmd_handle_user(cmd_handler_context_t context, const proto_command_t *cmd)
{
    session_t *session = (session_t *)context;

    if (!cmd->has_argument)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "Syntax error in parameters");
    }

    const char *username = cmd->argument;

    // Check if user exists or if anonymous login is enabled
    int user_exists = auth_user_exists(username);
    int is_anonymous = (strcmp(username, "anonymous") == 0);
    int anonymous_enabled = auth_is_anonymous_enabled();

    // If not anonymous and user doesn't exist, reject
    if (!is_anonymous && !user_exists)
    {
        LOG_WARN("User '%s' not found from %s:%u", username,
                 session->client_ip, session->client_port);
        return session_send_response(session, PROTO_RESP_NOT_LOGGED_IN,
                                     "User not found");
    }

    // If anonymous but anonymous login is disabled, reject
    if (is_anonymous && !anonymous_enabled)
    {
        LOG_WARN("Anonymous login disabled, rejected from %s:%u",
                 session->client_ip, session->client_port);
        return session_send_response(session, PROTO_RESP_NOT_LOGGED_IN,
                                     "Anonymous login not allowed");
    }

    // Set the username in session and update state
    session_set_user(session, username);

    LOG_INFO("User '%s' from %s:%u", username,
             session->client_ip, session->client_port);

    // For anonymous users, send appropriate message
    if (is_anonymous)
    {
        return session_send_response(session, PROTO_RESP_NEED_PASSWORD,
                                     "Anonymous login OK, send your email as password");
    }
    else
    {
        return session_send_response(session, PROTO_RESP_NEED_PASSWORD,
                                     "Username OK, need password");
    }
}

int cmd_handle_pass(cmd_handler_context_t context, const proto_command_t *cmd)
{
    session_t *session = (session_t *)context;

    if (session->state != SESSION_STATE_WAIT_PASSWORD)
    {
        return session_send_response(session, PROTO_RESP_BAD_COMMAND_SEQUENCE,
                                     "Login with USER first");
    }

    // Get password from command argument
    const char *password = cmd->has_argument ? cmd->argument : "";

    // Authenticate
    if (session_authenticate(session, password) != 0)
    {
        LOG_WARN("Authentication failed for user '%s' from %s:%u",
                 session->username, session->client_ip, session->client_port);
        return session_send_response(session, PROTO_RESP_NOT_LOGGED_IN,
                                     "Login incorrect");
    }

    LOG_INFO("User '%s' logged in from %s:%u",
             session->username, session->client_ip, session->client_port);

    return session_send_response(session, PROTO_RESP_USER_LOGGED_IN,
                                 "User logged in, proceed");
}

int cmd_handle_acct(cmd_handler_context_t context, const proto_command_t *cmd)
{
    (void)cmd;  // Unused parameter
    session_t *session = (session_t *)context;
    return session_send_response(session, PROTO_RESP_COMMAND_NOT_IMPL,
                                 "ACCT not implemented");
}

int cmd_handle_cwd(cmd_handler_context_t context, const proto_command_t *cmd)
{
    session_t *session = (session_t *)context;

    if (!session->authenticated)
    {
        return session_send_response(session, PROTO_RESP_NOT_LOGGED_IN,
                                     "Please login with USER and PASS");
    }

    if (!cmd->has_argument)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "Syntax error in parameters");
    }

    if (session_change_directory(session, cmd->argument) != 0)
    {
        return session_send_response(session, PROTO_RESP_FILE_UNAVAILABLE,
                                     "Failed to change directory");
    }

    return session_send_response(session, PROTO_RESP_FILE_ACTION_OK,
                                 "Directory successfully changed");
}

int cmd_handle_cdup(cmd_handler_context_t context, const proto_command_t *cmd)
{
    session_t *session = (session_t *)context;

    if (!session->authenticated)
    {
        return session_send_response(session, PROTO_RESP_NOT_LOGGED_IN,
                                     "Please login with USER and PASS");
    }

    if (cmd->has_argument)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "CDUP does not take parameters");
    }

    if (session_change_directory(session, "..") != 0)
    {
        return session_send_response(session, PROTO_RESP_FILE_UNAVAILABLE,
                                     "Failed to change to parent directory");
    }

    return session_send_response(session, PROTO_RESP_FILE_ACTION_OK,
                                 "Directory successfully changed");
}

int cmd_handle_smnt(cmd_handler_context_t context, const proto_command_t *cmd)
{
    (void)cmd;  // Unused parameter
    session_t *session = (session_t *)context;
    return session_send_response(session, PROTO_RESP_COMMAND_NOT_IMPL,
                                 "SMNT not implemented");
}

int cmd_handle_quit(cmd_handler_context_t context, const proto_command_t *cmd)
{
    session_t *session = (session_t *)context;

    if (cmd->has_argument)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "QUIT does not take parameters");
    }

    // Calculate session duration
    time_t session_duration = time(NULL) - session->connect_time;

    // Log the logout with statistics
    if (session->authenticated && session->username[0] != '\0')
    {
        LOG_INFO("User '%s' logging out from %s:%u - Stats: %llu bytes uploaded, %llu bytes downloaded, %u files up, %u files down, %u commands, %ld seconds",
                 session->username, session->client_ip, session->client_port,
                 session->bytes_uploaded, session->bytes_downloaded,
                 session->files_uploaded, session->files_downloaded,
                 session->commands_received, (long)session_duration);
    }
    else
    {
        LOG_INFO("Client %s:%u disconnecting (not logged in) - %u commands, %ld seconds",
                 session->client_ip, session->client_port,
                 session->commands_received, (long)session_duration);
    }

    session->should_quit = 1;

    // Send goodbye response with statistics (221 Service closing control connection)
    if (session->authenticated)
    {
        // Send multi-line response with statistics for authenticated users
        session_send_response_multiline(session, PROTO_RESP_CLOSING_CONTROL,
                                        "Goodbye! Session statistics:");

        char stat_line[256];
        snprintf(stat_line, sizeof(stat_line),
                 "  Data uploaded: %llu bytes", session->bytes_uploaded);
        session_send_response_multiline(session, PROTO_RESP_CLOSING_CONTROL, stat_line);

        snprintf(stat_line, sizeof(stat_line),
                 "  Data downloaded: %llu bytes", session->bytes_downloaded);
        session_send_response_multiline(session, PROTO_RESP_CLOSING_CONTROL, stat_line);

        snprintf(stat_line, sizeof(stat_line),
                 "  Files uploaded: %u", session->files_uploaded);
        session_send_response_multiline(session, PROTO_RESP_CLOSING_CONTROL, stat_line);

        snprintf(stat_line, sizeof(stat_line),
                 "  Files downloaded: %u", session->files_downloaded);
        session_send_response_multiline(session, PROTO_RESP_CLOSING_CONTROL, stat_line);

        snprintf(stat_line, sizeof(stat_line),
                 "  Commands received: %u", session->commands_received);
        session_send_response_multiline(session, PROTO_RESP_CLOSING_CONTROL, stat_line);

        snprintf(stat_line, sizeof(stat_line),
                 "  Session duration: %ld seconds", (long)session_duration);
        session_send_response_multiline(session, PROTO_RESP_CLOSING_CONTROL, stat_line);

        return session_send_response(session, PROTO_RESP_CLOSING_CONTROL,
                                     "Closing connection");
    }
    else
    {
        // Simple goodbye for non-authenticated users
        char goodbye_msg[256];
        snprintf(goodbye_msg, sizeof(goodbye_msg),
                 "Goodbye. Session duration: %ld seconds",
                 (long)session_duration);
        return session_send_response(session, PROTO_RESP_CLOSING_CONTROL, goodbye_msg);
    }
}

int cmd_handle_rein(cmd_handler_context_t context, const proto_command_t *cmd)
{
    session_t *session = (session_t *)context;

    if (cmd->has_argument)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "REIN does not take parameters");
    }

    LOG_INFO("Reinitializing session for %s:%u", session->client_ip, session->client_port);

    // Close any existing data connections
    session_close_data_connection(session);

    pthread_mutex_lock(&session->lock);

    // Reset authentication state
    session->authenticated = 0;
    session->state = SESSION_STATE_CONNECTED;
    memset(session->username, 0, sizeof(session->username));
    session->permissions = AUTH_PERM_NONE;

    // Reset directory state
    strcpy(session->current_dir, "/");
    memset(session->user_home_dir, 0, sizeof(session->user_home_dir));

    // Reset transfer parameters to defaults
    session->transfer_type = PROTO_TYPE_ASCII;
    session->transfer_mode = PROTO_MODE_STREAM;
    session->data_structure = PROTO_STRU_FILE;

    // Reset data connection mode
    session->data_mode = SESSION_DATA_MODE_NONE;
    memset(session->active_ip, 0, sizeof(session->active_ip));
    session->active_port = 0;
    session->passive_port = 0;

    // Clear command state
    session->restart_offset = 0;
    session->rename_pending = 0;
    memset(session->rename_from, 0, sizeof(session->rename_from));

    // Do not reset statistics on REIN

    pthread_mutex_unlock(&session->lock);

    return session_send_response(session, PROTO_RESP_SERVICE_READY,
                                 "Service ready for new user");
}

// Transfer Parameter Commands

int cmd_handle_port(cmd_handler_context_t context, const proto_command_t *cmd)
{
    session_t *session = (session_t *)context;

    if (!session->authenticated)
    {
        return session_send_response(session, PROTO_RESP_NOT_LOGGED_IN,
                                     "Please login with USER and PASS");
    }

    if (!cmd->has_argument)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "Syntax error in parameters");
    }

    proto_port_params_t port_params;
    if (proto_parse_port(cmd->argument, &port_params) != 0)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "Invalid PORT parameters");
    }

    char client_ip[64];
    uint16_t client_port;

    if (proto_port_to_address(&port_params, client_ip, sizeof(client_ip),
                              &client_port) != 0)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "Invalid PORT address");
    }

    if (session_set_port(session, client_ip, client_port) != 0)
    {
        return session_send_response(session, PROTO_RESP_LOCAL_ERROR,
                                     "Failed to set PORT mode");
    }

    LOG_DEBUG("PORT mode set: %s:%u", client_ip, client_port);

    return session_send_response(session, PROTO_RESP_OK,
                                 "PORT command successful");
}

int cmd_handle_pasv(cmd_handler_context_t context, const proto_command_t *cmd)
{
    session_t *session = (session_t *)context;

    if (!session->authenticated)
    {
        return session_send_response(session, PROTO_RESP_NOT_LOGGED_IN,
                                     "Please login with USER and PASS");
    }

    if (cmd->has_argument)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "PASV does not take parameters");
    }

    // Get server IP (simplified - use control connection IP)
    char server_ip[64];
    uint16_t dummy_port;

    if (net_get_socket_info(session->control_socket, server_ip,
                            sizeof(server_ip), &dummy_port) != 0)
    {
        return session_send_response(session, PROTO_RESP_LOCAL_ERROR,
                                     "Failed to get server address");
    }

    // Set passive mode (ports 20000-65535)
    if (session_set_pasv(session, 20000, 65535, server_ip) != 0)
    {
        return session_send_response(session, PROTO_RESP_LOCAL_ERROR,
                                     "Failed to enter passive mode");
    }

    // Build PASV response
    proto_pasv_params_t pasv_params;
    if (proto_address_to_pasv(server_ip, session->passive_port, &pasv_params) != 0)
    {
        session_close_data_connection(session); // Clean up on error
        return session_send_response(session, PROTO_RESP_LOCAL_ERROR,
                                     "Failed to format PASV response");
    }

    char response[256];
    if (proto_format_pasv_response(response, sizeof(response), &pasv_params) < 0)
    {
        session_close_data_connection(session); // Clean up on error
        return session_send_response(session, PROTO_RESP_LOCAL_ERROR,
                                     "Failed to format PASV response");
    }

    LOG_DEBUG("PASV mode: %s:%u", server_ip, session->passive_port);

    // Send the pre-formatted response directly
    int result = net_send_all(session->control_socket, response, strlen(response));
    if (result != 0)
    {
        LOG_ERROR("Failed to send PASV response");
        session_close_data_connection(session);
        return -1;
    }

    return 0;
}

int cmd_handle_type(cmd_handler_context_t context, const proto_command_t *cmd)
{
    session_t *session = (session_t *)context;

    if (!session->authenticated)
    {
        return session_send_response(session, PROTO_RESP_NOT_LOGGED_IN,
                                     "Please login with USER and PASS");
    }

    if (!cmd->has_argument)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "Syntax error in parameters");
    }

    proto_transfer_type_t type;
    if (proto_parse_type(cmd->argument, &type) != 0)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "Invalid type parameter");
    }

    if (type == PROTO_TYPE_EBCDIC)
    {
        return session_send_response(session, PROTO_RESP_COMMAND_NOT_IMPL_PARAM,
                                     "Type not supported (EBCDIC not supported)");
    }

    session_set_type(session, type);

    const char *type_name = (type == PROTO_TYPE_ASCII)    ? "A"
                            : (type == PROTO_TYPE_BINARY) ? "I"
                                                          : "E";

    char msg[128];
    snprintf(msg, sizeof(msg), "Type set to %s", type_name);

    return session_send_response(session, PROTO_RESP_OK, msg);
}

int cmd_handle_stru(cmd_handler_context_t context, const proto_command_t *cmd)
{
    session_t *session = (session_t *)context;

    if (!session->authenticated)
    {
        return session_send_response(session, PROTO_RESP_NOT_LOGGED_IN,
                                     "Please login with USER and PASS");
    }

    if (!cmd->has_argument)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "Syntax error in parameters");
    }

    proto_data_structure_t structure;
    if (proto_parse_stru(cmd->argument, &structure) != 0)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "Invalid structure parameter");
    }

    // Check if the requested structure is supported
    if (structure != PROTO_STRU_FILE)
    {
        // Server only supports File structure
        const char *stru_name = (structure == PROTO_STRU_RECORD) ? "Record"
                                : (structure == PROTO_STRU_PAGE) ? "Page"
                                                                 : "Unknown";
        LOG_WARN("Unsupported structure type requested: %s", stru_name);

        return session_send_response(session, PROTO_RESP_COMMAND_NOT_IMPL_PARAM,
                                     "Structure not supported (only File structure)");
    }

    // Set the structure (File is the default and only supported)
    session_set_structure(session, structure);

    LOG_DEBUG("Structure set to File for user '%s'", session->username);

    return session_send_response(session, PROTO_RESP_OK,
                                 "Structure set to File");
}

int cmd_handle_mode(cmd_handler_context_t context, const proto_command_t *cmd)
{
    session_t *session = (session_t *)context;

    if (!session->authenticated)
    {
        return session_send_response(session, PROTO_RESP_NOT_LOGGED_IN,
                                     "Please login with USER and PASS");
    }

    if (!cmd->has_argument)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "Syntax error in parameters");
    }

    proto_transfer_mode_t mode;
    if (proto_parse_mode(cmd->argument, &mode) != 0)
    {
        return session_send_response(session, PROTO_RESP_SYNTAX_ERROR_PARAM,
                                     "Invalid mode parameter");
    }

    if (mode != PROTO_MODE_STREAM)
    {
        return session_send_response(session, PROTO_RESP_COMMAND_NOT_IMPL_PARAM,
                                     "Mode not supported (only Stream mode supported)");
    }

    session_set_mode(session, mode);

    return session_send_response(session, PROTO_RESP_OK,
                                 "Mode set to Stream");
}
