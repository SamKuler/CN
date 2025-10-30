/**
 * @file command.c
 * @brief FTP command handler registration and dispatch implementation
 * @version 0.1
 * @date 2025-10-30
 *
 */
#include "command.h"
#include "utils.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/**
 * @brief Structure representing a registered command handler.
 */
typedef struct {
    char command[PROTO_MAX_CMD_NAME];
    cmd_handler_t handler;
    int in_use; //
} cmd_handler_entry_t;

/**
 * @brief Global handler table.
 */
static cmd_handler_entry_t g_handlers[CMD_MAX_HANDLERS];

/**
 * @brief Flag indicating whether the module has been initialized.
 */
static int g_initialized = 0;

int cmd_init(void) {
    if (g_initialized)
        return 0;

    // Clear handler table
    memset(g_handlers, 0, sizeof(g_handlers));

    g_initialized = 1;
    return 0;
}

void cmd_cleanup(void) {
    if (!g_initialized)
        return;

    // Clear all handlers
    memset(g_handlers, 0, sizeof(g_handlers));

    g_initialized = 0;
}

int cmd_register_handler(const char *command, cmd_handler_t handler) {
    if (!g_initialized)
        return -1;

    if (!command || !handler)
        return -1;

    // Convert command to uppercase for comparison
    char cmd_upper[PROTO_MAX_CMD_NAME];
    strncpy(cmd_upper, command, PROTO_MAX_CMD_NAME - 1);
    cmd_upper[PROTO_MAX_CMD_NAME - 1] = '\0';
    to_uppercase(cmd_upper);

    // Check if command is already registered
    for (int i = 0; i < CMD_MAX_HANDLERS; i++) {
        if (g_handlers[i].in_use && strcmp(g_handlers[i].command, cmd_upper) == 0) {
            // Update existing handler
            g_handlers[i].handler = handler;
            return 0;
        }
    }

    // Find empty slot
    for (int i = 0; i < CMD_MAX_HANDLERS; i++) {
        if (!g_handlers[i].in_use) {
            strncpy(g_handlers[i].command, cmd_upper, PROTO_MAX_CMD_NAME - 1);
            g_handlers[i].command[PROTO_MAX_CMD_NAME - 1] = '\0';
            g_handlers[i].handler = handler;
            g_handlers[i].in_use = 1;
            return 0;
        }
    }

    // No space available
    return -1;
}

int cmd_unregister_handler(const char *command) {
    if (!g_initialized)
        return -1;

    if (!command)
        return -1;

    // Convert command to uppercase for comparison
    char cmd_upper[PROTO_MAX_CMD_NAME];
    strncpy(cmd_upper, command, PROTO_MAX_CMD_NAME - 1);
    cmd_upper[PROTO_MAX_CMD_NAME - 1] = '\0';
    to_uppercase(cmd_upper);

    // Find and remove handler
    for (int i = 0; i < CMD_MAX_HANDLERS; i++) {
        if (g_handlers[i].in_use && strcmp(g_handlers[i].command, cmd_upper) == 0) {
            g_handlers[i].in_use = 0;
            g_handlers[i].handler = NULL;
            memset(g_handlers[i].command, 0, PROTO_MAX_CMD_NAME);
            return 0;
        }
    }

    return -1;
}

int cmd_dispatch(cmd_handler_context_t context, const proto_command_t *cmd) {
    if (!g_initialized)
        return -1;

    if (!cmd)
        return -1;

    // Find matching handler
    for (int i = 0; i < CMD_MAX_HANDLERS; i++) {
        if (g_handlers[i].in_use && strcmp(g_handlers[i].command, cmd->command) == 0) {
            // Call handler
            return g_handlers[i].handler(context, cmd);
        }
    }

    // No handler found
    return -1;
}

int cmd_is_registered(const char *command) {
    if (!g_initialized)
        return 0;

    if (!command)
        return 0;

    // Convert command to uppercase for comparison
    char cmd_upper[PROTO_MAX_CMD_NAME];
    strncpy(cmd_upper, command, PROTO_MAX_CMD_NAME - 1);
    cmd_upper[PROTO_MAX_CMD_NAME - 1] = '\0';
    to_uppercase(cmd_upper);

    // Check if handler exists
    for (int i = 0; i < CMD_MAX_HANDLERS; i++) {
        if (g_handlers[i].in_use && strcmp(g_handlers[i].command, cmd_upper) == 0)
            return 1;
    }

    return 0;
}

int cmd_get_handler_count(void) {
    if (!g_initialized)
        return 0;

    int count = 0;
    for (int i = 0; i < CMD_MAX_HANDLERS; i++) {
        if (g_handlers[i].in_use)
            count++;
    }

    return count;
}

// Forward declarations for standard command handlers
// RFC 959 4.1 FTP COMMANDS

// Access Control Commands
// Login
extern int cmd_handle_user(cmd_handler_context_t context, const proto_command_t *cmd); // USERNAME
extern int cmd_handle_pass(cmd_handler_context_t context, const proto_command_t *cmd); // PASSWORD
extern int cmd_handle_acct(cmd_handler_context_t context, const proto_command_t *cmd); // ACCOUNT
extern int cmd_handle_cwd(cmd_handler_context_t context, const proto_command_t *cmd); // CHANGE WORKING DIRECTORY
extern int cmd_handle_cdup(cmd_handler_context_t context, const proto_command_t *cmd); // CHANGE TO PARENT DIRECTORY
extern int cmd_handle_smnt(cmd_handler_context_t context, const proto_command_t *cmd); // STRUCTURE MOUNT

// Logout
extern int cmd_handle_quit(cmd_handler_context_t context, const proto_command_t *cmd); // LOGOUT
extern int cmd_handle_rein(cmd_handler_context_t context, const proto_command_t *cmd); // REINITIALIZE

// Transfer Parameter Commands
extern int cmd_handle_port(cmd_handler_context_t context, const proto_command_t *cmd); // DATA PORT
extern int cmd_handle_pasv(cmd_handler_context_t context, const proto_command_t *cmd); // PASSIVE MODE
extern int cmd_handle_type(cmd_handler_context_t context, const proto_command_t *cmd); // REPRESENTATION TYPE
extern int cmd_handle_stru(cmd_handler_context_t context, const proto_command_t *cmd); // FILE STRUCTURE
extern int cmd_handle_mode(cmd_handler_context_t context, const proto_command_t *cmd); // TRANSFER MODE

// FTP Service Commands
extern int cmd_handle_allo(cmd_handler_context_t context, const proto_command_t *cmd); // ALLOCATE
extern int cmd_handle_rest(cmd_handler_context_t context, const proto_command_t *cmd); // RESTART
extern int cmd_handle_stor(cmd_handler_context_t context, const proto_command_t *cmd); // STORE
extern int cmd_handle_stou(cmd_handler_context_t context, const proto_command_t *cmd); // STORE UNIQUE
extern int cmd_handle_retr(cmd_handler_context_t context, const proto_command_t *cmd); // RETRIEVE
extern int cmd_handle_list(cmd_handler_context_t context, const proto_command_t *cmd); // LIST
extern int cmd_handle_nlst(cmd_handler_context_t context, const proto_command_t *cmd); // NAME LIST
extern int cmd_handle_appe(cmd_handler_context_t context, const proto_command_t *cmd); // APPEND(with create)
extern int cmd_handle_rnfr(cmd_handler_context_t context, const proto_command_t *cmd); // RENAME FROM
extern int cmd_handle_rnto(cmd_handler_context_t context, const proto_command_t *cmd); // RENAME TO
extern int cmd_handle_dele(cmd_handler_context_t context, const proto_command_t *cmd); // DELETE
extern int cmd_handle_rmd(cmd_handler_context_t context, const proto_command_t *cmd); // REMOVE DIRECTORY
extern int cmd_handle_mkd(cmd_handler_context_t context, const proto_command_t *cmd); // MAKE DIRECTORY
extern int cmd_handle_pwd(cmd_handler_context_t context, const proto_command_t *cmd); // PRINT WORKING DIRECTORY
extern int cmd_handle_abor(cmd_handler_context_t context, const proto_command_t *cmd); // ABORT

// Informational commands
extern int cmd_handle_syst(cmd_handler_context_t context, const proto_command_t *cmd); // SYSTEM TYPE
extern int cmd_handle_stat(cmd_handler_context_t context, const proto_command_t *cmd); // STATUS
extern int cmd_handle_help(cmd_handler_context_t context, const proto_command_t *cmd); // HELP

// Miscellaneous commands
extern int cmd_handle_site(cmd_handler_context_t context, const proto_command_t *cmd); // SITE PARAMETERS
extern int cmd_handle_noop(cmd_handler_context_t context, const proto_command_t *cmd); // NO OPERATION


int cmd_register_standard_handlers(void) {
    if (!g_initialized)
        return -1;

    int result = 0;

    // Register all standard FTP commands
    result |= cmd_register_handler("USER", cmd_handle_user);
    result |= cmd_register_handler("PASS", cmd_handle_pass);
    result |= cmd_register_handler("ACCT", cmd_handle_acct);
    result |= cmd_register_handler("CWD", cmd_handle_cwd);
    result |= cmd_register_handler("CDUP", cmd_handle_cdup);
    result |= cmd_register_handler("SMNT", cmd_handle_smnt);

    result |= cmd_register_handler("QUIT", cmd_handle_quit);
    result |= cmd_register_handler("REIN", cmd_handle_rein);

    result |= cmd_register_handler("PORT", cmd_handle_port);
    result |= cmd_register_handler("PASV", cmd_handle_pasv);
    result |= cmd_register_handler("TYPE", cmd_handle_type);
    result |= cmd_register_handler("STRU", cmd_handle_stru);
    result |= cmd_register_handler("MODE", cmd_handle_mode);

    result |= cmd_register_handler("ALLO", cmd_handle_allo);
    result |= cmd_register_handler("REST", cmd_handle_rest);
    result |= cmd_register_handler("STOR", cmd_handle_stor);
    result |= cmd_register_handler("STOU", cmd_handle_stou);
    result |= cmd_register_handler("RETR", cmd_handle_retr);
    result |= cmd_register_handler("LIST", cmd_handle_list);
    result |= cmd_register_handler("NLST", cmd_handle_nlst);
    result |= cmd_register_handler("APPE", cmd_handle_appe);
    result |= cmd_register_handler("RNFR", cmd_handle_rnfr);
    result |= cmd_register_handler("RNTO", cmd_handle_rnto);
    result |= cmd_register_handler("DELE", cmd_handle_dele);
    result |= cmd_register_handler("RMD", cmd_handle_rmd);
    result |= cmd_register_handler("MKD", cmd_handle_mkd);
    result |= cmd_register_handler("PWD", cmd_handle_pwd);
    result |= cmd_register_handler("ABOR", cmd_handle_abor);

    result |= cmd_register_handler("SYST", cmd_handle_syst);
    result |= cmd_register_handler("STAT", cmd_handle_stat);
    result |= cmd_register_handler("HELP", cmd_handle_help);

    result |= cmd_register_handler("SITE", cmd_handle_site);
    result |= cmd_register_handler("NOOP", cmd_handle_noop);

    return (result == 0) ? 0 : -1;
}
