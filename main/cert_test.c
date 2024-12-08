#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
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
void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type, uint16_t len) {
    // Ignore packets of types we're not interested in
    if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) {
        return;
    }
    if (!buff) return;

    const wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buff;
    const wifi_pkt_rx_ctrl_t *rx_ctrl = &pkt->rx_ctrl;

    if (rx_ctrl->channel >= 1 && rx_ctrl->channel <= CONFIG_MAX_WIFI_CHANNELS) {
        int index = rx_ctrl->channel - 1;
        if (packet_count[index] == 0 || rx_ctrl->rssi > rssi_values[index]) {
            rssi_values[index] = rx_ctrl->rssi;
        }
        packet_count[index]++;

        // Check for actual error conditions
        if (rx_ctrl->rx_state != 0) {  // Non-zero state indicates some kind of error
            if ((rx_ctrl->rx_state & WIFI_PKTCTL_CRCERR) ||     // CRC error
                (rx_ctrl->rx_state & WIFI_PKTCTL_PHYERR) ||     // PHY error
                (rx_ctrl->rx_state & WIFI_PKTCTL_ABORT)) {      // Reception aborted
                error_count[index]++;
            }
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
    
    // Reset tracking arrays before new scan
    for (int i = 0; i < CONFIG_MAX_WIFI_CHANNELS; i++) {
        rssi_values[i] = -100;  // Lowest reasonable RSSI
        packet_count[i] = 0;
        error_count[i] = 0;
    }

    // Scan each channel
    for (int channel = 1; channel <= CONFIG_MAX_WIFI_CHANNELS; channel++) {
        ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
        vTaskDelay(pdMS_TO_TICKS(500)); // Allow time for packet collection
    }

    // Print header every 10 iterations
    if ((scan_iteration - 1) % 10 == 0) {
        printf("# Format for each channel: RSSI(dBm)/Packets/Errors\n");
        printf("Scan    ");
        for (int channel = 1; channel <= CONFIG_MAX_WIFI_CHANNELS; channel++) {
            printf("Ch%-2d      ", channel);
        }
        printf("\n");
    }

    // Print results
    printf("%-7d ", scan_iteration++);
    for (int channel = 1; channel <= CONFIG_MAX_WIFI_CHANNELS; channel++) {
        int index = channel - 1;
        int rssi = rssi_values[index];
        int packets = packet_count[index];
        int errors = error_count[index];

        printf("%3d/%-3d/%-3d ", 
               packets > 0 ? rssi : -100, 
               packets, 
               errors);
    }
    printf("\n");
}

void app_main(void) {
    // Initialize NVS and WiFi
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(init_wifi());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Enable promiscuous mode once
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler));

    while (1) {
        if (current_mode == MODE_CHANNEL_STRENGTH) {
            scan_channel_strength();
        } else {
            ESP_LOGI(TAG, "Other modes not implemented");
        }

        // Delay between iterations
        vTaskDelay(pdMS_TO_TICKS(CONFIG_SCAN_DELAY_MS));
    }

    // Cleanup (unreachable in this implementation)
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    ESP_LOGI(TAG, "Application exiting...");
}
