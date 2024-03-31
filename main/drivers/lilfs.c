#include "esp_log.h"
#include "lilfs.h"

static const char *TAG = "LILFS";

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

void init_littlefs(spi_device_handle_t handle) {
  lfs_t lfs;
  const struct lfs_config cfg = {
    .context = handle,
    // block device operations
    .read = w25q128_lfs_read,
    .prog = w25q128_lfs_prog,
    .erase = w25q128_lfs_erase,
    .sync = w25q128_lfs_sync,
    // block device configuration
    .read_size = 1,
    .prog_size = 1,
    .block_size = 4096,
    .block_count = 4096,
    .lookahead_size = 16,
    .cache_size = 16,
    .block_cycles = 500,
  };

  // w25q128_chip_erase(handle);

  ESP_LOGI(TAG, "Attempting to mount littleFS");
  int err = lfs_mount(&lfs, &cfg);
  if (err) {
    ESP_LOGE(TAG, "Attempting format");
    // If the mount failed, format the file system and try again
    lfs_format(&lfs, &cfg);
    err = lfs_mount(&lfs, &cfg);
    if (err) {
        // If the mount still fails, there's a serious problem
        ESP_LOGE(TAG, "Failed to mount or format LittleFS");
        return;
    }
  }

  struct lfs_info info;

  // Print the file system contents
  lfs_dir_t dir;
  lfs_dir_open(&lfs, &dir, "/");
  while (lfs_dir_read(&lfs, &dir, &info) > 0) {
    ESP_LOGI(TAG, "File: %s, Size: %lu", info.name, info.size);
  }

  lfs_dir_close(&lfs, &dir);

  // add a file
  lfs_file_t file;
  lfs_file_open(&lfs, &file, "hello.txt", LFS_O_WRONLY | LFS_O_CREAT);

  lfs_file_write(&lfs, &file, "Hello, LittleFS!", 16);

  lfs_file_close(&lfs, &file);

  // read the file
  char buffer[16];
  lfs_file_open(&lfs, &file, "hello.txt", LFS_O_RDONLY);
  lfs_file_read(&lfs, &file, buffer, 16);
  lfs_file_close(&lfs, &file);

  ESP_LOGI(TAG, "Read from file: %s", buffer);

  // Clean up
  lfs_unmount(&lfs);
}