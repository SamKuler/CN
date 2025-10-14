#ifndef LOGGER_H
#define LOGGER_H

typedef enum LogLevel
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

/**
 * @brief Initialize the log system.
 *
 * @param log_file File to open and write logs in. Null for stdout.
 * @param level Write logs at or above the minimum level
 * @retval 0:    Success open
 * @retval -1:   Failure or error
 */
int logger_init(const char *log_file, LogLevel level);

/**
 * @brief Write actual logs.
 *
 * Log format: [<LOG_LEVEL>][<TIMESTAMP>][<FILE>:<LINE>:<FUNC>] <Message>
 *
 * @param level Log level. Show in log file as [INFO]|[WARN]...
 * @param filename Log source filename
 * @param line Line number in source file
 * @param funcname Function name that calls
 * @param format printf-style format strings
 * @param ... Variable parameters
 */
void logger_log(LogLevel level, const char *filename, int line, const char *funcname, const char *format, ...);

/**
 * @brief Close the log system.
 *
 * Thread-safe. It can be called concurrently with logger_log().
 * After calling this, logger_log() will silently fail until
 * logger_init() is called again.
 */
void logger_close(void);

/**
 * @brief Set the minimum log level.
 *
 * @param level New minimum log level
 */
void logger_set_level(LogLevel level);

// Useful macros
#define LOG_INFO(...) logger_log(LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARN(...) logger_log(LOG_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_ERROR(...) logger_log(LOG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_DEBUG(...) logger_log(LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

#endif