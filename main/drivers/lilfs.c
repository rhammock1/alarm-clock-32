#include "esp_log.h"
#include "lilfs.h"

static const char *TAG = "LILFS";

lfs_t lfs;

struct lfs_config w25q128_cfg = {
  .context = NULL,
  // block device operations
  .read = NULL,
  .prog = NULL,
  .erase = NULL,
  .sync = NULL,
  // block device configuration
  .read_size = 1,
  .prog_size = 1,
  .block_size = 4096,
  .block_count = 4096,
  .lookahead_size = 16,
  .cache_size = 16,
  .block_cycles = 500,
};

int w25q128_lfs_read(const struct lfs_config *c, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size) {
    spi_device_handle_t handle = (spi_device_handle_t)c->context;
    uint32_t addr = (block * c->block_size) + off;

    esp_err_t ret;
    spi_device_acquire_bus(handle, portMAX_DELAY);

    spi_transaction_t t;

    ret = w25q128_read_data(handle, t, addr, buffer, size);
    if(ret != ESP_OK) {
      ESP_LOGE(TAG, "Error reading data: %d", ret);
      spi_device_release_bus(handle);
      return -1;
    }

    spi_device_release_bus(handle);

    return 0;
}

// Program (write) function for LittleFS
int w25q128_lfs_prog(const struct lfs_config *c, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size) {
  spi_device_handle_t handle = (spi_device_handle_t)c->context;
  uint32_t addr = (block * c->block_size) + off;

  esp_err_t ret;
  spi_device_acquire_bus(handle, portMAX_DELAY);

  spi_transaction_t t;
  
  ret = w25q128_write_data(handle, t, addr, buffer, size);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error writing data: %d", ret);
    spi_device_release_bus(handle);
    return -1;
  }

  spi_device_release_bus(handle);
  return 0;
}

// Erase function for LittleFS
int w25q128_lfs_erase(const struct lfs_config *c, lfs_block_t block) {
  spi_device_handle_t handle = (spi_device_handle_t)c->context;
  uint32_t addr = block * c->block_size;

  spi_device_acquire_bus(handle, portMAX_DELAY);

  spi_transaction_t t;
  esp_err_t ret;

  ret = w25q128_sector_erase(handle, t, addr);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error erasing block: %d", ret);
    spi_device_release_bus(handle);
    return -1;
  }

  spi_device_release_bus(handle);
  return 0;
}

int w25q128_lfs_sync(const struct lfs_config *c) {
  return 0;
}

esp_err_t format_lfs() {
  ESP_LOGE(TAG, "Attempting format");
  // If the mount failed, format the file system and try again
  lfs_format(&lfs, &w25q128_cfg);
  return ESP_OK;
}

esp_err_t format_and_mount_lfs() {
  // If the mount failed, format the file system and try again
  lfs_format(&lfs, &w25q128_cfg);
  int err = lfs_mount(&lfs, &w25q128_cfg);
  if (err) {
      // If the mount still fails, there's a serious problem
      ESP_LOGE(TAG, "Failed to mount or format LittleFS");
      return ESP_FAIL;
  }
  return ESP_OK;
}

int lfs_open(lfs_file_t *file, const char *path, int flags) {
  int err = lfs_file_open(&lfs, file, path, flags);
  if (err) {
    if(err == LFS_ERR_CORRUPT) {
      ESP_LOGE(TAG, "Corrupt filesystem, formatting and trying again");
      format_and_mount_lfs();
      err = lfs_file_open(&lfs, file, path, flags);
      if(err) {
        ESP_LOGE(TAG, "Error opening file: %d", err);
        return ESP_FAIL;
      }
    } else {
      ESP_LOGE(TAG, "Error while attempting to open file: %d", err);
      return err;
    }
  }
  return 0;
}

void lfs_close(lfs_file_t *file) {
  lfs_file_close(&lfs, file);
}

void lfs_write(lfs_file_t *file, const void *buffer, size_t size) {
  lfs_file_write(&lfs, file, buffer, size);
}

int lfs_read(lfs_file_t *file, void *buffer, size_t size) {
  return lfs_file_read(&lfs, file, buffer, size);
}

int lfs_open_dir(lfs_dir_t *dir, const char *path) {
  return lfs_dir_open(&lfs, dir, path);
}

int lfs_read_dir(lfs_dir_t *dir, struct lfs_info *info) {
  return lfs_dir_read(&lfs, dir, info);
}

int lfs_close_dir(lfs_dir_t *dir) {
  return lfs_dir_close(&lfs, dir);
}

int lfs_read_string(lfs_file_t *file, char *buffer, size_t size) {
  if (size == 0) {
    return LFS_ERR_INVAL;
  }

  int result = lfs_read(file, buffer, size - 1);
  if (result < 0) {
    return result;
  }

  buffer[result] = '\0';
  return result;
}

esp_err_t mount_lfs() {
  ESP_LOGI(TAG, "Attempting to mount littleFS");
  int err = lfs_mount(&lfs, &w25q128_cfg);
  if (err) {
    format_and_mount_lfs();
  }

  return ESP_OK;
}

esp_err_t unmount_lfs() {
  int err = lfs_unmount(&lfs);
  if (err) {
    ESP_LOGE(TAG, "Error unmounting LittleFS: %d", err);
    return ESP_FAIL;
  }

  return ESP_OK;
}

esp_err_t init_littlefs(spi_device_handle_t handle) {
  w25q128_cfg.context = handle;
  w25q128_cfg.read = w25q128_lfs_read;
  w25q128_cfg.prog = w25q128_lfs_prog;
  w25q128_cfg.erase = w25q128_lfs_erase;
  w25q128_cfg.sync = w25q128_lfs_sync;

  // w25q128_chip_erase(handle);

  mount_lfs();

  struct lfs_info info;

  // Print the file system contents
  lfs_dir_t dir;
  lfs_dir_open(&lfs, &dir, "/");
  while (lfs_dir_read(&lfs, &dir, &info) > 0) {
    ESP_LOGI(TAG, "File: %s, Size: %lu", info.name, info.size);
  }

  lfs_dir_close(&lfs, &dir);

  int err = lfs_mkdir(&lfs, "/uploads");
  if(err != 0 && err != LFS_ERR_EXIST) {
    ESP_LOGE(TAG, "Error creating uploads directory: %d", err);
  }
  err = lfs_mkdir(&lfs, "/www");
  if(err != 0 && err != LFS_ERR_EXIST) {
    ESP_LOGE(TAG, "Error creating www directory: %d", err);
  }

  lfs_dir_open(&lfs, &dir, "/uploads");
  while (lfs_dir_read(&lfs, &dir, &info) > 0) {
    ESP_LOGI(TAG, "File: %s, Size: %lu", info.name, info.size);
  }

  lfs_dir_close(&lfs, &dir);

  // Clean up
  unmount_lfs();

  return ESP_OK;
}