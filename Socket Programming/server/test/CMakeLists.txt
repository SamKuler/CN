# LoggerTest
add_executable(test_logger test_logger.c)
target_link_libraries(test_logger ftpserver)
# Add to CTest
add_test(NAME LoggerTest COMMAND test_logger)

# FilesystemTest
add_executable(test_filesys test_filesys.c)
target_link_libraries(test_filesys ftpserver)
add_test(NAME FilesysTest COMMAND test_filesys)