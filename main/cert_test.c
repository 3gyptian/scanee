#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "hal/usb_serial_jtag_ll.h"
#include <fcntl.h>
#include "driver/uart.h"
#include "esp_private/wifi.h" // For low-level RF access

// Configurable parameters
#ifndef CONFIG_MAX_WIFI_CHANNELS
#define CONFIG_MAX_WIFI_CHANNELS 13
#endif

#ifndef CONFIG_SCAN_DELAY_MS
#define CONFIG_SCAN_DELAY_MS 10
#endif

#define TAG "WIFI_SCAN"

// RSSI register address for ESP32-S3
#define RSSI_REGISTER_ADDRESS (0x600B1C44)  // This is a placeholder - verify in ESP32-S3 TRM

// Define operation modes
typedef enum {
    MODE_PACKET_RSSI_SCAN,
    MODE_RAW_RF_SCAN,
    MODE_ACCESS_POINT_SCAN
} operation_mode_t;

// Global mode variable (default to packet-based RSSI scan)
static operation_mode_t current_mode = MODE_PACKET_RSSI_SCAN;
static bool header_printed_packet_rssi = false;
static bool header_printed_raw_rf = false;
static bool header_printed_ap = false;

// Measurement variables for packet-based RSSI scan
static int32_t rssi_values[CONFIG_MAX_WIFI_CHANNELS] = {0};
static int32_t packet_count[CONFIG_MAX_WIFI_CHANNELS] = {0};
static int32_t error_count[CONFIG_MAX_WIFI_CHANNELS] = {0};

// Measurement variables for raw RF scan
static int32_t raw_rssi_values[CONFIG_MAX_WIFI_CHANNELS] = {0};

// Function to read a single character from the USB Serial JTAG RX buffer
int usb_serial_jtag_read_char(void) {
    uint8_t char_buf;
    // Check if data is available in the RX buffer
    if (usb_serial_jtag_ll_rxfifo_data_available()) {
        // Read a single character from the RX buffer
        if (usb_serial_jtag_ll_read_rxfifo(&char_buf, 1) > 0) {
            return char_buf;  // Return the character
        }
    }
    return -1; // Return -1 if no character is available
}

// Promiscuous mode callback (for packet-based RSSI scan)
void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type) {
    if (current_mode != MODE_PACKET_RSSI_SCAN) return;

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

        // Check for actual error conditions in rx_state
        if (rx_ctrl->rx_state != 0) {  // Non-zero state indicates some kind of error
            if ((rx_ctrl->rx_state & BIT(0)) ||     // CRC error
                (rx_ctrl->rx_state & BIT(1)) ||     // PHY error
                (rx_ctrl->rx_state & BIT(7))) {     // Incomplete reception
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
    // Increase buffer sizes for better packet capture (for packet-based mode)
    cfg.static_rx_buf_num = 16;
    cfg.dynamic_rx_buf_num = 32;

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

void scan_packet_rssi(void) {
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
        vTaskDelay(pdMS_TO_TICKS(150)); // Allow time for packet collection
    }

    // Print header once at the beginning
    if (!header_printed_packet_rssi) {
        printf("# Format for each channel: RSSI(dBm)/Packets[/Errors if any]\n");
        printf("Scan     ");
        for (int channel = 1; channel <= CONFIG_MAX_WIFI_CHANNELS; channel++) {
            printf("Ch%-4d       ", channel);
        }
        printf("\n");
        header_printed_packet_rssi = true;
    }

    // Print results
    printf("%-6d", scan_iteration++);
    for (int channel = 1; channel <= CONFIG_MAX_WIFI_CHANNELS; channel++) {
        int index = channel - 1;
        int rssi = rssi_values[index];
        int packets = packet_count[index];
        int errors = error_count[index];

        // Print RSSI and packet count, and errors only if present
        if (errors > 0) {
            printf("%5d/%-3d/%-3d ", packets > 0 ? rssi : -100, packets, errors);
        } else {
            printf("%5d/%-3d    ", packets > 0 ? rssi : -100, packets);
        }
    }
    printf("\n");
}

void scan_raw_rf(void) {
    static int scan_iteration = 1;

    // 1. Disable Wi-Fi
    if (esp_wifi_stop() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop Wi-Fi");
        return;
    }

    // Reset tracking array before new scan
    for (int i = 0; i < CONFIG_MAX_WIFI_CHANNELS; i++) {
        raw_rssi_values[i] = -127; // Initialize to a very low value
    }

    // 2. Iterate through channels
    for (int channel = 1; channel <= CONFIG_MAX_WIFI_CHANNELS; channel++) {
        // 3. Configure channel
        if (esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set channel %d", channel);
            continue;
        }

        // 4. Access and read RSSI register (example)
        // Note: This is a placeholder. You MUST replace RSSI_REGISTER_ADDRESS with the correct value from the ESP32-S3 TRM.
        uint32_t rssi_raw = READ_PERI_REG(RSSI_REGISTER_ADDRESS);

        // 5. Interpret RSSI value (example)
        int rssi = (int)rssi_raw; // Apply scaling/conversion as needed

        raw_rssi_values[channel - 1] = rssi;

        // Add a small delay if needed to allow the PHY to settle
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Print header once at the beginning
    if (!header_printed_raw_rf) {
        printf("# Raw RF Scan - RSSI (raw)\n");
        printf("Scan     ");
        for (int channel = 1; channel <= CONFIG_MAX_WIFI_CHANNELS; channel++) {
            printf("Ch%-4d       ", channel);
        }
        printf("\n");
        header_printed_raw_rf = true;
    }

    // Print results
    printf("%-6d", scan_iteration++);
    for (int channel = 0; channel < CONFIG_MAX_WIFI_CHANNELS; channel++) {
        printf("%-13ld", raw_rssi_values[channel]);
    }
    printf("\n");

    // Re-enable Wi-Fi if needed for other modes
    if (esp_wifi_start() != ESP_OK) {
         ESP_LOGE(TAG, "Failed to restart Wi-Fi");
         return;
    }
}

void scan_access_points(void) {
    wifi_ap_record_t ap_records[20];  // Increased number of records
    uint16_t ap_count = 20;  // Set max number of records to retrieve

    // Ensure Wi-Fi is in the correct mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 200
    };

    // Stop any ongoing scan first
    esp_wifi_scan_stop();

    // Start scan with specific configuration
    esp_err_t scan_result = esp_wifi_scan_start(&scan_config, true);
    if (scan_result != ESP_OK) {
        ESP_LOGE(TAG, "Scan start failed with error: %s", esp_err_to_name(scan_result));
        return;
    }

    scan_result = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (scan_result != ESP_OK) {
        ESP_LOGE(TAG, "Get AP records failed with error: %s", esp_err_to_name(scan_result));
        return;
    }

    // Print header once
    if (!header_printed_ap) {
        printf("# Access Point Scan Results\n");
        printf("Channel  RSSI(dBm)  SSID\n");
        header_printed_ap = true;
    }

    // Print access point details
    for (int i = 0; i < ap_count; i++) {
        printf("%-8d %-11d %s\n",
               ap_records[i].primary,
               ap_records[i].rssi,
               ap_records[i].ssid);
    }
    printf("Total APs found: %d\n", ap_count);
}

void app_main(void) {
    // Initialize NVS and WiFi
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(init_wifi());

    // Enable promiscuous mode once for packet-based RSSI scan
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb((wifi_promiscuous_cb_t)wifi_sniffer_packet_handler));

    while (1) {
        // Check for keystrokes
        int ch = usb_serial_jtag_read_char();
        if (ch != -1) {
            if (ch == '1') {
                current_mode = MODE_PACKET_RSSI_SCAN;
                header_printed_packet_rssi = false; // Allow header to be reprinted
                ESP_LOGI(TAG, "Switched to Packet-based RSSI Scan Mode");
            } else if (ch == '2') {
                current_mode = MODE_RAW_RF_SCAN;
                header_printed_raw_rf = false; // Allow header to be reprinted
                ESP_LOGI(TAG, "Switched to Raw RF Scan Mode");
            } else if (ch == '3') {
                current_mode = MODE_ACCESS_POINT_SCAN;
                header_printed_ap = false; // Allow header to be reprinted
                ESP_LOGI(TAG, "Switched to Access Point Scan Mode");
            }
        }

        // Perform scan based on current mode
        switch (current_mode) {
            case MODE_PACKET_RSSI_SCAN:
                scan_packet_rssi();
                break;
            case MODE_RAW_RF_SCAN:
                scan_raw_rf();
                break;
            case MODE_ACCESS_POINT_SCAN:
                scan_access_points();
                break;
            default:
                ESP_LOGE(TAG, "Invalid mode selected");
                break;
        }

        // Delay between iterations
        vTaskDelay(pdMS_TO_TICKS(CONFIG_SCAN_DELAY_MS));
    }

    // Cleanup (unreachable in this implementation)
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    ESP_LOGI(TAG, "Application exiting...");
}
