#include "lfs.h"
#include "w25q128.h"

esp_err_t init_littlefs(spi_device_handle_t handle);
int lfs_read_string(lfs_file_t *file, char *buffer, size_t size);
int lfs_open(lfs_file_t *file, const char *path, int flags);
void lfs_close(lfs_file_t *file);
void lfs_write(lfs_file_t *file, const void *buffer, size_t size);
int lfs_read(lfs_file_t *file, void *buffer, size_t size);
esp_err_t mount_lfs();
esp_err_t unmount_lfs();
esp_err_t format_lfs();
esp_err_t format_and_mount_lfs();