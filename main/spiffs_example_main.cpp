#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <esp_log.h>
#include <esp_spiffs.h>
#include <esp_task.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <array>
#include <cstdio>
#include <cstring>

#define READ_FILE_EVERY_SECOND

static constexpr auto TAG{"issue_14152"};

// IO pin which shows if file gets currently red
static constexpr auto read_file_toggle_gpio_num{GPIO_NUM_13};

// IO pin which gets toggled in timer
static constexpr auto timer_gpio_num{GPIO_NUM_14};

gptimer_handle_t gptimer{};

// Timer callback which toggles pin at 20kHz
bool level{};
bool IRAM_ATTR gptimer_callback(gptimer_handle_t,
                                gptimer_alarm_event_data_t const*,
                                void*) {
  gptimer_set_raw_count(gptimer, 0ull);
  gpio_set_level(timer_gpio_num, level = !level);
  return false;
}

// SPIFFS storage
void init_spiffs() {
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
}

// timer_gpio_num -> Timer toggle
// read_file_toggle_gpio_num -> Set high while reading file from SPIFFS
void init_io() {
  ESP_LOGI(TAG, "Initializing IO");
  static constexpr gpio_config_t io_conf{
    .pin_bit_mask = 1ull << timer_gpio_num | 1ull << read_file_toggle_gpio_num,
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE};
  ESP_ERROR_CHECK(gpio_config(&io_conf));
}

// Timer, 1MHz, 50us period
void init_timer() {
  ESP_LOGI(TAG, "Initializing timer");
  static constexpr gptimer_config_t timer_config{
    .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = 1'000'000u,  // 1 MHz
    .intr_priority = 3};
  ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
  gptimer_event_callbacks_t cbs{.on_alarm = gptimer_callback};
  ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));
  ESP_ERROR_CHECK(gptimer_enable(gptimer));
  static constexpr gptimer_alarm_config_t alarm_config{
    .alarm_count = 50,  // period = 50us
    .reload_count = 0,
    .flags = {.auto_reload_on_alarm = true},
  };
  ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
  ESP_ERROR_CHECK(gptimer_start(gptimer));
}

// Read .png file from SPIFFS
void read_file() {
  static std::array<uint8_t, 1024uz> file_buffer;
  auto fd{fopen("/logo.png", "r")};
  if (fd == NULL) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return;
  }
  size_t chunksize;
  do {
    if (!(chunksize = read(fileno(fd), data(file_buffer), size(file_buffer))))
      continue;
  } while (chunksize);
  fclose(fd);
}

extern "C" void app_main() {
  init_spiffs();
  init_io();
  init_timer();

  // Infinite loop reading file
  for (;;) {
#ifdef READ_FILE_EVERY_SECOND
    gpio_set_level(read_file_toggle_gpio_num, 1u);
    read_file();
    gpio_set_level(read_file_toggle_gpio_num, 0u);
#endif
    vTaskDelay(pdMS_TO_TICKS(1000u));
  }
}
