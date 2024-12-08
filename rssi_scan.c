#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_private/wifi.h" // Might be needed for low-level functions

static const char *TAG = "rssi_scan";

// Placeholder for RSSI register address (consult ESP32-S3 TRM)
#define RSSI_REGISTER_ADDRESS (0x3FFXXXXX) 

void measure_rssi_all_channels() {
    // 1. Disable Wi-Fi
    if (esp_wifi_stop() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop Wi-Fi");
        return;
    }
    // esp_wifi_deinit() might be needed here if Wi-Fi was initialized

    // 2. Iterate through channels
    for (int channel = 1; channel <= 14; channel++) {
        // 3. Configure channel
        if (phy_set_channel(channel, PHY_SECOND_CH_NONE) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set channel %d", channel);
            continue;
        }

        // 4. Access and read RSSI register (example)
        uint32_t rssi_raw = READ_PERI_REG(RSSI_REGISTER_ADDRESS);

        // 5. Interpret RSSI value (example)
        int rssi = (int)rssi_raw; // Apply scaling/conversion as needed

        ESP_LOGI(TAG, "Channel %d: RSSI = %d", channel, rssi);

        // Add a small delay if needed to allow the PHY to settle
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    // ... other initialization ...

    measure_rssi_all_channels();

    // ...
}
