#include <esp_log.h>
#include <esp_spiffs.h>
#include <esp_task.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <cstdio>
#include <cstring>

static constexpr auto TAG{"issue_14152"};

extern "C" void app_main() {
  ESP_LOGI(TAG, "Initializing SPIFFS");

  static constexpr esp_vfs_spiffs_conf_t conf{.base_path = "",
                                              .partition_label = NULL,
                                              .max_files = 5,
                                              .format_if_mount_failed = true};

  esp_err_t ret{esp_vfs_spiffs_register(&conf)};
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount or format filesystem");
    } else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "Failed to find SPIFFS partition");
    } else {
      ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    }
    return;
  }

  size_t total{};
  size_t used{};
  ret = esp_spiffs_info(conf.partition_label, &total, &used);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG,
             "Failed to get SPIFFS partition information (%s). Formatting...",
             esp_err_to_name(ret));
    esp_spiffs_format(conf.partition_label);
    return;
  } else {
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
  }

  // Open renamed file for reading
  auto f{fopen("/logo.png", "r")};
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return;
  }

  for (;;) { vTaskDelay(pdMS_TO_TICKS(5000u)); }
}
