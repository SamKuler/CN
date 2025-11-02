/**
 * @file main.c
 * @brief FTP Server Main Program Entry Point
 * @version 0.1
 * @date 2025-11-02
 */

#include "server.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

// Default configuration
#define DEFAULT_PORT 21
#define DEFAULT_ROOT_DIR "./ftp_root"
#define DEFAULT_MAX_BACKLOG 10
#define DEFAULT_COMMAND_TIMEOUT_MS 300000  // 5 minutes
#define DEFAULT_MAX_CONNECTIONS 100       // Maximum concurrent connections

/**
 * @brief Signal handler for graceful shutdown
 */
void signal_handler(int signum)
{
    (void)signum;
    server_stop();
}

/**
 * @brief Prints usage information
 */
void print_usage(const char *program_name)
{
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -p <port>       Port to listen on (default: %d)\n", DEFAULT_PORT);
    printf("  -r <root_dir>   Root directory for FTP (default: %s)\n", DEFAULT_ROOT_DIR);
    printf("  -l <log_level>  Log level: DEBUG, INFO, WARN, ERROR (default: INFO)\n");
    printf("  -c <max_conn>   Maximum concurrent connections (default: %d, -1 for unlimited)\n", DEFAULT_MAX_CONNECTIONS);
    printf("  -h              Show this help message\n");
}

/**
 * @brief Main entry point
 */
int main(int argc, char *argv[])
{
    // Parse command line arguments
    server_config_t config = {
        .port = DEFAULT_PORT,
        .max_backlog = DEFAULT_MAX_BACKLOG,
        .command_timeout_ms = DEFAULT_COMMAND_TIMEOUT_MS,
        .max_connections = DEFAULT_MAX_CONNECTIONS
    };
    strncpy(config.root_dir, DEFAULT_ROOT_DIR, sizeof(config.root_dir) - 1);
    config.root_dir[sizeof(config.root_dir) - 1] = '\0';

    log_level_t log_level = LOG_LEVEL_INFO;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
        {
            config.port = (uint16_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc)
        {
            strncpy(config.root_dir, argv[++i], sizeof(config.root_dir) - 1);
            config.root_dir[sizeof(config.root_dir) - 1] = '\0';
        }
        else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc)
        {
            i++;
            if (strcmp(argv[i], "DEBUG") == 0)
                log_level = LOG_LEVEL_DEBUG;
            else if (strcmp(argv[i], "INFO") == 0)
                log_level = LOG_LEVEL_INFO;
            else if (strcmp(argv[i], "WARN") == 0)
                log_level = LOG_LEVEL_WARN;
            else if (strcmp(argv[i], "ERROR") == 0)
                log_level = LOG_LEVEL_ERROR;
        }
        else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
        {
            config.max_connections = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-h") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else
        {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    // Initialize logger
    if (logger_init(NULL, log_level) != 0)
    {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    // Set up signal handlers for shutdown
#ifdef _WIN32
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN); // Ignore broken pipe signals
#endif

    // Initialize server
    if (server_init(&config) != 0)
    {
        LOG_ERROR("Failed to initialize server");
        LOG_INFO("=== FTP Server Stopped ===");
        logger_close();
        return 1;
    }

    // Run server (blocks until stopped)
    int result = server_run();

    // Cleanup
    LOG_INFO("Server shutting down...");
    server_cleanup();

    LOG_INFO("=== FTP Server Stopped ===");
    logger_close();

    return result;
}