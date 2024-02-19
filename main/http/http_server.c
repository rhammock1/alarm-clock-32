#include "esp_http_server.h"
#include "esp_log.h"
#include "http_server.h"

static const char *TAG = "HTTP";

esp_err_t get_base_path_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "GET /");
  const char* resp_str = "<h1>Hello, world!</h1>";
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, resp_str, strlen(resp_str));
  return ESP_OK;
}

httpd_uri_t routes[] = {
  {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = get_base_path_handler,
    .user_ctx  = NULL
  }
};

esp_err_t init_http_server(void)
{
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

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
