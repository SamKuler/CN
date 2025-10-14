#include "logger.h"

void test_tty_log()
{
    logger_init(0, LOG_DEBUG);
    LOG_INFO("==============================");
    LOG_INFO(" TEST START ");
    LOG_DEBUG("Hello DEBUG");
    LOG_WARN("Hello WARN");
    LOG_ERROR("Hello ERROR");
    LOG_INFO(" TEST END ");
    LOG_INFO("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
    logger_close();
}

void test_file_log()
{
    logger_init("test.log", LOG_DEBUG);
    LOG_INFO("==============================");
    LOG_INFO(" TEST START ");
    LOG_DEBUG("Hello DEBUG");
    LOG_WARN("Hello WARN");
    LOG_ERROR("Hello ERROR");
    LOG_INFO(" TEST END ");
    LOG_INFO("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
    logger_close();
}

void test_reset_logger_level()
{
    logger_init(0, LOG_DEBUG);
    LOG_INFO("==============================");
    LOG_INFO(" TEST START ");
    LOG_DEBUG("Hello DEBUG");
    LOG_WARN("Hello WARN");
    LOG_ERROR("Hello ERROR");
    LOG_INFO("ENTERING SET LEVEL TEST");
    logger_set_level(LOG_WARN);
    LOG_INFO(" TEST START AGAIN");
    LOG_DEBUG("Hello DEBUG");
    LOG_WARN("Hello WARN");
    LOG_ERROR("Hello ERROR");
    LOG_WARN(" TEST END ");
    LOG_WARN("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
    logger_close();
}

int main()
{
    test_tty_log();
    test_file_log();
    test_reset_logger_level();
    return 0;
}