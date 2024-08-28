#include "esp_http_server.h"
#include "esp_log.h"
#include "http_server.h"
#include "lilfs.h"
#include "ds1307.h"
#include "audio.h"

static const char *TAG = "HTTP";

#define min(a,b) ((a) < (b) ? (a) : (b))

const char* base_html = "<!DOCTYPE html>"
                    "<html>"
                      "<head>"
                        "<title>ESP32 Alarm Clock</title>"
                        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                      "</head>"
                      "<body>"
                        "<h1 style=\"color: green; text-align: center;\">ESP32 Alarm Clock</h1>"
                        "<form id=\"file-upload\" action=\"/file\" method=\"post\" enctype=\"multipart/form-data\">"
                          "<input id=\"files-input\" type=\"file\" multiple name=\"file\">"
                          "<input id=\"overwrite_html\" type=\"checkbox\" name=\"overwrite_html\">Save to /www/</input>"
                          "<input type=\"submit\">"
                          "<button type=\"button\" id=\"format-fs\">Format littleFS</button>"
                        "</form>"
                        "<script>"
                          "document.getElementById('file-upload').addEventListener('submit', async function(event) {"
                            "event.preventDefault();"
                            "const fileInput = document.getElementById('files-input');"
                            "const {files} = fileInput;"
                            "console.log('event target', event.target, 'files', files);"
                            "if (files.length === 0) {"
                              "return;"
                            "}"
                            "for(const file of files) {"
                              "const formData = new FormData();"
                              "formData.append('file', file);"
                              "fetch(`/file?overwrite_html=${document.getElementById('overwrite_html').checked}`, {"
                                "method: 'POST',"
                                "body: formData"
                              "})"
                              ".then(response => response.ok && response.text())"
                              ".then(result => {"
                                "console.log('Success:', result);"
                              "})"
                              ".catch(error => {"
                                "console.error('Error:', error);"
                              "});"
                            "}"
                          "});"
                          "document.getElementById('format-fs').addEventListener('click', function() {"
                            "fetch('/format')"
                              ".then(response => response.ok && response.text())"
                              ".then(result => {"
                                "console.log('Success:', result);"
                              "})"
                              ".catch(error => {"
                                "console.error('Error:', error);"
                              "});"
                          "});"
                        "</script>"
                      "</body>"
                    "</html>";


esp_err_t set_time_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /time");

  // parse the request to get a tm struct from the ISO 8601 date string
  size_t content_len = req->content_len;
  char* content = malloc(content_len + 1);
  if (!content) {
    ESP_LOGE(TAG, "Failed to allocate memory for content");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  int ret = httpd_req_recv(req, content, content_len);
  if (ret <= 0) {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      // Timeout, continue receiving
      return ESP_OK;
    }
    // Error or connection closed, clean up
    ESP_LOGE(TAG, "Failed constto receive content: %d", ret);
    free(content);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  content[content_len] = '\0';
  ESP_LOGI(TAG, "Received content: %s", content);

  struct tm tm;

  // Parse the date, time, escape double quote at the beginning for reasons
  int items_read = sscanf(content, "\"%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

  if (items_read != 6) {
    ESP_LOGI(TAG, "CONTENT: %s", content);
    ESP_LOGE(TAG, "Failed to parse time: %d", items_read);
    free(content);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  // Adjust the year and month fields, because strptime expects years since 1900 and months starting from 0
  tm.tm_year -= 1900;
  tm.tm_mon -= 1;

  ESP_LOGI(TAG, "Parsed time: %d-%d-%d %d:%d:%d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

  ds1307_set_time(&tm);
  free(content);

  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t play_sound_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /sound");

  esp_err_t ret = audio_test();
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error playing sound: %d", ret);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t get_files_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /files");

  mount_lfs();

  // Get all directories and files from littlefs and respond with a JSON object, where each top-level directory is the key and the value is an array of files
  lfs_dir_t dir;
  lfs_open_dir(&dir, "/");
  struct lfs_info info;
  char buffer[1024];
  char response[1024];
  strcpy(response, "{");
  while (lfs_read_dir(&dir, &info) > 0) {
    if (info.type == LFS_TYPE_DIR) {
      printf("Directory: %s\n", info.name);
      sprintf(buffer, "\"%s\": [", info.name);
      strcat(response, buffer);
      lfs_dir_t subdir;
      lfs_open_dir(&subdir, info.name);
      struct lfs_info subinfo;
      while (lfs_read_dir(&subdir, &subinfo) > 0) {
        printf("File: %s\n", subinfo.name);
        sprintf(buffer, "\"%s\",", subinfo.name);
        strcat(response, buffer);
      }
        if (response[strlen(response) - 1] == ',') {
          response[strlen(response) - 1] = '\0';
        }
      lfs_close_dir(&subdir);
      strcat(response, "],");
    } else {
      sprintf(buffer, "\"%s\": [],", info.name);
      strcat(response, buffer);
    }
  }
  if (response[strlen(response) - 1] == ',') {
    response[strlen(response) - 1] = '\0';
  }
  lfs_close_dir(&dir);
  strcat(response, "}");
  printf("Response: %s\n", response);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response, strlen(response));

  unmount_lfs();
  return ESP_OK;
}

esp_err_t format_fs_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "GET /format");

  // send the response now, so the client doesn't time out
  httpd_resp_send(req, NULL, 0);

  format_lfs();

  return ESP_OK;
}

esp_err_t serve_html(httpd_req_t* req, const char* path) {
  // serve the file
  printf("HTML Path: %s\n", path);
  lfs_file_t file;
  int err = lfs_open(&file, path, LFS_O_RDONLY);
  if (err) {
    ESP_LOGE(TAG, "Error opening file: %d", err);
    lfs_close(&file);
    unmount_lfs();
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  // Read the file and send it in chunks
  char buffer[1024];
  httpd_resp_set_type(req, "text/html");
  while (1) {
    int err = lfs_read_string(&file, buffer, sizeof(buffer));
    printf("Read in base path handler: %d\n", err);
    if (err <= 0) {
      break;
    }
    // print the buffer
    printf("Buffer: %s\n", buffer);
    httpd_resp_send_chunk(req, buffer, err);
  }
  printf("Finished reading file\n");
  // finish response
  httpd_resp_send_chunk(req, NULL, 0);

  // Close the file
  lfs_close(&file);
  unmount_lfs();

  return ESP_OK;
}

esp_err_t get_base_path_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "GET /");
  
  mount_lfs();

  const char* path = "/www/index.html";
  if(lfs_file_exists(path)) {
    esp_err_t ret = serve_html(req, path);
    return ret;
  }

  unmount_lfs();

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, base_html, strlen(base_html));
  return ESP_OK;
}

esp_err_t post_file_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /file");

    mount_lfs();

    // Get the content length of the request
    size_t content_len = req->content_len;

    size_t chunk_size = 1024; // or another size that fits in memory
    char* chunk = malloc(chunk_size);
    if (!chunk) {
        ESP_LOGE(TAG, "Failed to allocate memory for chunk");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Read the file data
    int received = 0;
    lfs_file_t file;
    // read the first chunk of data to get the filename
    int fret = httpd_req_recv(req, chunk, chunk_size);
    if (fret <= 0) {
        if (fret == HTTPD_SOCK_ERR_TIMEOUT) {
            // Timeout, continue receiving
            return ESP_OK;
        }
        // Error or connection closed, clean up
        ESP_LOGE(TAG, "Failed to receive file data: %d", fret);
        free(chunk);
        httpd_resp_send_500(req);
        unmount_lfs();
        return ESP_FAIL;
    }
    // printf("FRET Data: %.*s\n", fret, chunk);
    received += fret;
    // parse the chunk to get the filename
    char* filename_start = strstr(chunk, "filename=\"");
    if (!filename_start) {
        ESP_LOGE(TAG, "Failed to find filename in file data");
        free(chunk);
        httpd_resp_send_500(req);
        unmount_lfs();
        return ESP_FAIL;
    }
    filename_start += strlen("filename=\"");

    // Find the end of the filename
    char* filename_end = strstr(filename_start, "\"");
    if (!filename_end) {
        ESP_LOGE(TAG, "Failed to find end of filename in file data");
        free(chunk);
        httpd_resp_send_500(req);
        unmount_lfs();
        return ESP_FAIL;
    }

    // Copy the filename into a new string
    size_t filename_len = filename_end - filename_start;
    char* filename = malloc(filename_len + 1);
    if (!filename) {
        ESP_LOGE(TAG, "Failed to allocate memory for filename");
        free(chunk);
        httpd_resp_send_500(req);
        unmount_lfs();
        return ESP_FAIL;
    }
    strncpy(filename, filename_start, filename_len);
    filename[filename_len] = '\0';

    // Print the filename
    printf("Received file %s\n", filename);

    // build the file path
    // check if the overwrite_html query parameter is set
    bool overwrite_html = false;
    if (strstr(req->uri, "overwrite_html")) {
      overwrite_html = true;
    }
    const char* path = overwrite_html
      ? "/www/"
      : "/uploads/";
    size_t file_path_len = strlen(path) + filename_len;
    char file_path[file_path_len]; // Ensure the array is large enough
    sprintf(file_path, "%s%s", path, filename);

    printf("File path: %s\n", file_path);

    int err = lfs_open(&file, file_path, LFS_O_RDWR | LFS_O_CREAT);
    if (err) {
        ESP_LOGE(TAG, "Error opening file to write: %d", err);
        free(chunk);
        free(filename);
        httpd_resp_send_500(req);
        lfs_close(&file);
        unmount_lfs();
        return ESP_FAIL;
    }

    if(received >= content_len) {
      // No more data to receive
      printf("Received all data\n");
      printf("Received %d bytes of file data\n", fret);
      printf("Data: %.*s\n", fret, chunk);

      char* file_content = malloc(content_len + 1);
      if (!file_content) {
          ESP_LOGE(TAG, "Failed to allocate memory for file content");
          free(chunk);
          free(filename);
          httpd_resp_send_500(req);
          lfs_close(&file);
          unmount_lfs();
          return ESP_FAIL;
      }
      char* content_start = strstr(chunk, "\r\n\r\n");
      if(!content_start) {
        ESP_LOGE(TAG, "Failed to find content in file data");
        free(file_content);
        free(chunk);
        free(filename);
        httpd_resp_send_500(req);
        lfs_close(&file);
        unmount_lfs();
        return ESP_FAIL;
      }
      content_start += strlen("\r\n\r\n");
      char* content_end = strstr(content_start, "\r\n------");
      if(content_end) {
        size_t content_len = content_end - content_start;
        strncpy(file_content, content_start, content_len);
        file_content[content_len] = '\0';
      } else {
        // no end found, copy the rest of the chunk
        strcpy(file_content, content_start);
      }

      lfs_write(&file, file_content, content_len);
      free(file_content);
    } else {
      char* content_start = strstr(chunk, "\r\n\r\n");
      if(!content_start) {
        ESP_LOGE(TAG, "Failed to find content in file data");
        free(chunk);
        free(filename);
        httpd_resp_send_500(req);
        lfs_close(&file);
        unmount_lfs();
        return ESP_FAIL;
      }
      content_start += strlen("\r\n\r\n");
      char* file_content = malloc(fret - (content_start - chunk) + 1);
      if (!file_content) {
          ESP_LOGE(TAG, "Failed to allocate memory for file content");
          free(chunk);
          free(filename);
          httpd_resp_send_500(req);
          lfs_close(&file);
          unmount_lfs();
          return ESP_FAIL;
      }
      // check for the end of the file data
      char* content_end = strstr(content_start, "\r\n------");
      if(content_end) {
        size_t content_len = content_end - content_start;
        strncpy(file_content, content_start, content_len);
        file_content[content_len] = '\0';
      } else {
        // no end found, copy the rest of the chunk
        strcpy(file_content, content_start);
      }
      // Print the received data
      printf("Received partial data\n");
      printf("Received %d bytes of file data\n", fret);
      printf("Data: %.*s\n", fret, chunk);

      // For example, write it to a file or send it over a network connection
      lfs_write(&file, file_content, fret);
      free(file_content);
      if(content_end) {
        free(chunk);
        free(filename);
        lfs_close(&file);
        unmount_lfs();
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
      }
      while (received < content_len) {
        printf("Waiting for more data\n");
        // print the available heap size
        printf("Free heap size: %lu\n", esp_get_free_heap_size());
        int to_receive = min(chunk_size, content_len - received);
        int ret = httpd_req_recv(req, chunk, to_receive);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                // Timeout, continue receiving
                continue;
            }
            // Error or connection closed, clean up
            ESP_LOGE(TAG, "Failed to receive file data: %d", ret);
            free(chunk);
            free(filename);
            httpd_resp_send_500(req);
            lfs_close(&file);
            unmount_lfs();
            return ESP_FAIL;
        }

        // check for the end of the file data
        char* content_end = strstr(chunk, "\r\n------");
        if(content_end) {
          size_t chunk_len = content_end - chunk;
          char* chunk_content = malloc(chunk_len + 1);
          if (!chunk_content) {
              ESP_LOGE(TAG, "Failed to allocate memory for file content");
              free(chunk);
              free(filename);
              httpd_resp_send_500(req);
              lfs_close(&file);
              unmount_lfs();
              return ESP_FAIL;
          }
          strncpy(chunk_content, chunk, chunk_len);
          chunk_content[content_len] = '\0';
          lfs_write(&file, chunk_content, chunk_len);
          free(chunk_content);
          break;
        }

        file_content = malloc(ret + 1);
        if (!file_content) {
            ESP_LOGE(TAG, "Failed to allocate memory for file content");
            free(chunk);
            free(filename);
            httpd_resp_send_500(req);
            lfs_close(&file);
            unmount_lfs();
            return ESP_FAIL;
        }
        strcpy(file_content, chunk);

        // Print the received data
        printf("Received %d bytes of file data.\n", ret);
        printf("Data: %.*s\n", ret, chunk);

        // For example, write it to a file or send it over a network connection
        lfs_write(&file, file_content, ret);
        free(file_content);

        received += ret;
      }
    }
    lfs_close(&file);
    free(chunk);
    free(filename);

    // Send a response
    httpd_resp_send(req, NULL, 0);
    
    unmount_lfs();
    return ESP_OK;
}

esp_err_t asset_file_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /asset");

    // Get the file path from the request
    const char* path = req->uri + strlen("/");
    char file_path[strlen(path) + 6];
    sprintf(file_path, "/www/%s", path);
    printf("Asset Path: %s\n", file_path);

    mount_lfs();

    // Open the file
    lfs_file_t file;
    int err = lfs_open(&file, file_path, LFS_O_RDONLY);
    if (err) {
      ESP_LOGE(TAG, "Error opening asset file: %d", err);
      unmount_lfs();
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }

    // Set the content type
    if (strstr(file_path, ".css")) {
      httpd_resp_set_type(req, "text/css");
    } else if (strstr(file_path, ".js")) {
      httpd_resp_set_type(req, "application/x-javascript");
    } else if (strstr(file_path, ".ico")) {
      httpd_resp_set_type(req, "image/x-icon");
    }
    // Read the file and send it in chunks
    char buffer[1024];
    while (1) {
      int err = lfs_read_string(&file, buffer, sizeof(buffer));
      if (err <= 0) {
          break;
      }
      httpd_resp_send_chunk(req, buffer, err);
    }
    // finish response
    httpd_resp_send_chunk(req, NULL, 0);
    // Close the file
    lfs_close(&file);
    unmount_lfs();

    return ESP_OK;
}

httpd_uri_t routes[] = {
  {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = get_base_path_handler,
    .user_ctx  = NULL
  }, {
    .uri       = "/style.css",
    .method    = HTTP_GET,
    .handler   = asset_file_handler,
    .user_ctx  = NULL
  }, {
    .uri       = "/favicon.ico",
    .method    = HTTP_GET,
    .handler   = asset_file_handler,
    .user_ctx  = NULL
  }, {
    .uri       = "/script.js",
    .method    = HTTP_GET,
    .handler   = asset_file_handler,
    .user_ctx  = NULL
  }, {
    .uri       = "/file",
    .method    = HTTP_POST,
    .handler   = post_file_handler,
    .user_ctx  = NULL
  }, {
    .uri       = "/format",
    .method    = HTTP_GET,
    .handler   = format_fs_handler,
    .user_ctx  = NULL
  }, {
    .uri       = "/files",
    .method    = HTTP_GET,
    .handler   = get_files_handler,
    .user_ctx  = NULL
  }, {
    .uri       = "/time",
    .method    = HTTP_POST,
    .handler   = set_time_handler,
    .user_ctx  = NULL
  }, {
    .uri       = "/sound",
    .method    = HTTP_GET,
    .handler   = play_sound_handler,
    .user_ctx  = NULL
  }
};

esp_err_t init_http_server(void)
{
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 5120;
  // Increase the maximum number of URI handlers
  config.max_uri_handlers = 10;

  // Start the httpd server
  if (httpd_start(&server, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start httpd server");
    return ESP_FAIL;
  }

  // Register URI handlers
  for (int i = 0; i < sizeof(routes) / sizeof(httpd_uri_t); i++) {
    httpd_register_uri_handler(server, &routes[i]);
  }

  return ESP_OK;
}
