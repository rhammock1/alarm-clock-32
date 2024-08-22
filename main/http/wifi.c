#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "wifi.h"

static const char *TAG = "WIFI";
static EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;
static int max_retry = 5;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    if (s_retry_num < max_retry)
    {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    }
    else
    {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG, "connect to the AP fail");
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void wifi_init_ap(void) {
    esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32_AP",
            .ssid_len = strlen("ESP32_AP"),
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    if (strlen("12345678") == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "wifi_init_ap finished. SSID:%s password:%s",
             "ESP32_AP", "12345678");
}

void wifi_init_sta(void)
{
  s_wifi_event_group = xEventGroupCreate();

  // Initialize the network stack
  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(
    esp_event_handler_instance_register(
      WIFI_EVENT, 
      ESP_EVENT_ANY_ID,
      &event_handler,
      NULL,
      &instance_any_id
    )
  );
  ESP_ERROR_CHECK(
    esp_event_handler_instance_register(
      IP_EVENT,
      IP_EVENT_STA_GOT_IP,
      &event_handler,
      NULL,
      &instance_got_ip
    )
  );

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = WIFI_SSID,
      .password = WIFI_PASS
    },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
   * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(
    s_wifi_event_group,
    WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
    pdFALSE,
    pdFALSE,
    portMAX_DELAY
  );

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT)
  {
    ESP_LOGI(TAG, "connected to ap SSID:%s ", WIFI_SSID);
  }
  else if (bits & WIFI_FAIL_BIT)
  {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s, switching to AP mode ", WIFI_SSID);
    wifi_init_ap();
  }
  else
  {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }
}

// FIXME before testing
// void post_logs(char *log, int len) {
//   // Make sure the wifi is connected
//   if(!(xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT)) {
//     ESP_LOGE(TAG, "Wifi not connected, cannot post logs");
//     return;
//   }
//   // Make a POST request to 192.168.1.1:6969/logs with the log data
//     esp_http_client_config_t config = {
//         .url = "http://192.168.1.1:6969/logs",
//         .method = HTTP_METHOD_POST,
//     };

//     esp_http_client_handle_t client = esp_http_client_init(&config);

//     esp_http_client_set_header(client, "Content-Type", "text/plain");
//     esp_http_client_set_post_field(client, log, len);

//     esp_err_t err = esp_http_client_perform(client);

//     if (err == ESP_OK) {
//         ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
//             esp_http_client_get_status_code(client),
//             esp_http_client_get_content_length(client));
//     } else {
//         ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
//     }

//     esp_http_client_cleanup(client);
// }