#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "filesys.h"

static void fail_and_exit(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

int main()
{
    char template[] = "/tmp/filesys_test_XXXXXX";
    char *dir = mkdtemp(template);
    if (!dir)
        fail_and_exit("mkdtemp failed");

    char path[PATH_MAX];
    const char *fname = "file1.txt";
    snprintf(path, PATH_MAX, "%s/%s", dir, fname);

    const char *text = "Hello World";
    long long written = fs_write_file_all(path, (void *)text, (long long)strlen(text));
    if (written != (long long)strlen(text))
        fail_and_exit("fs_write_file_all failed");

    long long fsize = fs_get_file_size(path);
    if (fsize != (long long)strlen(text))
        fail_and_exit("fs_get_file_size mismatch");

    char buf[128];
    long long readn = fs_read_file_all(path, buf, sizeof(buf));
    if (readn != fsize)
        fail_and_exit("fs_read_file_all size mismatch");
    buf[readn] = '\0';
    if (strcmp(buf, text) != 0)
        fail_and_exit("fs_read_file_all content mismatch");

    /* test chunk write */
    const char *append = "_CHUNK";
    long long chunk_written = fs_write_file_chunk(path, append, fsize, (long long)strlen(append));
    if (chunk_written != (long long)strlen(append))
        fail_and_exit("fs_write_file_chunk failed");

    long long newsize = fs_get_file_size(path);
    if (newsize != fsize + (long long)strlen(append))
        fail_and_exit("fs_get_file_size after chunk write mismatch");

    char buf2[256];
    long long re = fs_read_file_all(path, buf2, sizeof(buf2));
    if (re != newsize)
        fail_and_exit("read after chunk failed");
    buf2[re] = '\0';

    /* verify content contains both parts */
    if (strstr(buf2, text) == NULL || strstr(buf2, append) == NULL)
        fail_and_exit("combined content mismatch");

    /* list directory */
    fs_file_info_t list[16];
    int listed = fs_list_directory(dir, list, 16);
    if (listed <= 0)
        fail_and_exit("fs_list_directory returned no entries");

    int found = 0;
    for (int i = 0; i < listed; ++i)
    {
        if (strcmp(list[i].name, fname) == 0)
        {
            found = 1;
            break;
        }
    }
    if (!found)
        fail_and_exit("fs_list_directory did not find expected file");

    /* directory size */
    long long dsize = fs_get_directory_size(dir);
    if (dsize < newsize)
        fail_and_exit("fs_get_directory_size too small");

    /* delete file */
    if (fs_delete_file(path) != 0)
        fail_and_exit("fs_delete_file failed");

    if (fs_path_exists(path))
        fail_and_exit("file still exists after delete");

    /* create nested dir and file, then remove recursively */
    char subdir[PATH_MAX];
    snprintf(subdir, PATH_MAX, "%s/subdir", dir);
    if (fs_create_directory(subdir) != 0)
        fail_and_exit("fs_create_directory failed");

    char subfile[PATH_MAX];
    snprintf(subfile, PATH_MAX, "%s/%s", subdir, "inner.txt");
    const char *inner = "inner";
    if (fs_write_file_all(subfile, (void *)inner, (long long)strlen(inner)) != (long long)strlen(inner))
        fail_and_exit("write inner file failed");

    if (fs_delete_directory(dir, 1) != 0)
        fail_and_exit("fs_delete_directory recursive failed");

    if (fs_path_exists(dir))
        fail_and_exit("directory still exists after recursive delete");

    printf("All filesys tests passed\n");
    return 0;
}
