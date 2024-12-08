#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

// Configurable parameters
#ifndef CONFIG_MAX_WIFI_CHANNELS
#define CONFIG_MAX_WIFI_CHANNELS 13
#endif

#ifndef CONFIG_SCAN_DELAY_MS
#define CONFIG_SCAN_DELAY_MS 500
#endif

#define TAG "WIFI_SCAN"

// Define operation modes
typedef enum {
    MODE_CHANNEL_STRENGTH,
    MODE_OTHER
} operation_mode_t;

// Global mode variable
static operation_mode_t current_mode = MODE_CHANNEL_STRENGTH;

// Measurement variables
static int32_t rssi_values[CONFIG_MAX_WIFI_CHANNELS] = {0};
static int32_t packet_count[CONFIG_MAX_WIFI_CHANNELS] = {0};
static int32_t error_count[CONFIG_MAX_WIFI_CHANNELS] = {0};

// Promiscuous mode callback
IRAM_ATTR void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type) {
    if (!buff) return;

    const wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buff;
    const wifi_pkt_rx_ctrl_t *rx_ctrl = &pkt->rx_ctrl;

    if (rx_ctrl->channel >= 1 && rx_ctrl->channel <= CONFIG_MAX_WIFI_CHANNELS) {
        int index = rx_ctrl->channel - 1;
        if (packet_count[index] == 0 || rx_ctrl->rssi > rssi_values[index]) {
            rssi_values[index] = rx_ctrl->rssi;
        }
        packet_count[index]++;

        // Check for various error conditions
        if (rx_ctrl->sig_mode == 0 || rx_ctrl->rx_state != 0 || 
            rx_ctrl->rssi < -100) {
            error_count[index]++;
        }
    }
}

// Initialize NVS (required for WiFi)
static esp_err_t init_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

// Initialize WiFi in NULL mode
static esp_err_t init_wifi(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // Increase buffer sizes for better packet capture
    cfg.static_rx_buf_num = 16;
    cfg.dynamic_rx_buf_num = 32;

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

// Perform channel strength scanning
void scan_channel_strength(void) {
    static int scan_iteration = 1;
    ESP_LOGI(TAG, "\nStarting Channel Strength Measurement...");

    printf("\nScan\t");
    for (int channel = 1; channel <= CONFIG_MAX_WIFI_CHANNELS; channel++) {
        printf("Ch%d\t", channel);
    }
    printf("\n");

    printf("%d\t", scan_iteration++);
    for (int channel = 1; channel <= CONFIG_MAX_WIFI_CHANNELS; channel++) {
        int index = channel - 1;
        int rssi = rssi_values[index];
        int packets = packet_count[index];
        int errors = error_count[index];

        printf("%d/%d/%d\t", rssi, packets, errors);
    }
    printf("\n");

    memset(rssi_values, -100, sizeof(rssi_values)); // Set default RSSI to a very low value
    memset(packet_count, 0, sizeof(packet_count));
    memset(error_count, 0, sizeof(error_count));

    ESP_LOGI(TAG, "Channel Strength Measurement Complete\n");
}

void app_main(void) {
    // Initialize NVS
    ESP_ERROR_CHECK(init_nvs());
    ESP_LOGI(TAG, "NVS initialized");

    // Initialize WiFi
    ESP_ERROR_CHECK(init_wifi());
    ESP_LOGI(TAG, "WiFi initialized in NULL mode");

    // Enable promiscuous mode
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler));
    ESP_LOGI(TAG, "Promiscuous mode enabled");

    // Set WiFi channel to iterate through all channels
    for (int channel = 1; channel <= CONFIG_MAX_WIFI_CHANNELS; channel++) {
        ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
        vTaskDelay(pdMS_TO_TICKS(100)); // Short delay to allow packet collection
    }

    while (1) {
        if (current_mode == MODE_CHANNEL_STRENGTH) {
            scan_channel_strength();
        } else {
            ESP_LOGI(TAG, "Other modes not implemented");
        }

        // Delay between iterations
        vTaskDelay(pdMS_TO_TICKS(CONFIG_SCAN_DELAY_MS));
    }

    // Cleanup
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    ESP_LOGI(TAG, "Application exiting...");
}