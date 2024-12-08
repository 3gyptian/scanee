#include <inttypes.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_phy_cert_test.h"
#include <string.h>
#include "esp_vfs_dev.h"
#include "hal/usb_serial_jtag_ll.h"
#include <fcntl.h>
#include "driver/uart.h"

static const char *TAG = "console_echo";


// RF Transmission Configuration Structure
typedef struct {
    uint32_t start_channel;  // Starting channel
    uint32_t end_channel;    // Ending channel
    uint32_t rate;           // Transmission rate (currently set to 0x0 for default)
    uint8_t   backoff;        // Power backoff in 0.25dB units (e.g., 20 = -5dB)
    uint32_t length_byte;    // Length of each packet in bytes
    uint32_t packet_delay;   // Delay between packets in milliseconds
    uint32_t packet_num;     // Number of packets to send per burst
    uint32_t pattern_type;   // Channel hopping pattern (0=sequential, 1=random, 2=fibonacci)
    uint32_t dwell_time;     // Time to stay on each channel in milliseconds
} rf_tx_config_t;

// Default configuration
static rf_tx_config_t default_config = {
    .start_channel = 6,      // Start at channel 6 (focusing on channel 6)
    .end_channel = 6,        // End at channel 6 (only using channel 6)
    .rate = 0x0,             // Default transmission rate (0x0 means unspecified default)
    /* Backoff values control power attenuation:
     * - Each unit = 0.25dB reduction
     * - Common values:
     *   0   = Full power (no attenuation)
     *   20  = -5dB  (20 * 0.25dB)
     *   40  = -10dB (40 * 0.25dB)
     *   80  = -20dB (80 * 0.25dB)
     *   120 = -30dB (120 * 0.25dB)
     *   160 = -40dB (160 * 0.25dB)
     */
    .backoff = 19*4,       // start with -40dB attenuation (160 * 0.25dB)
    .length_byte = 37,     // Packet length set to 37 bytes
    .packet_delay = 2,     // Delay between packets set to 100ms   // HERE
    .packet_num = 10,      // Number of packets per burst set to 10
    .pattern_type = 1,     // Use random channel hopping by default (although only one channel is used here)
    .dwell_time = 4        // Dwell time set to 5ms per channel (since we are focusing on channel 6, this is for packet transmission timing)
};

// Fibonacci sequence for channel hopping
static uint32_t get_fibonacci_channel(uint32_t n, uint32_t start, uint32_t end) {
    uint32_t a = start;
    uint32_t b = start + 1;
    uint32_t c;
    
    for(uint32_t i = 2; i <= n; i++) {
        c = (a + b) % (end - start + 1) + start;
        a = b;
        b = c;
    }
    return b;
}

// Get next channel based on pattern type
static uint32_t get_next_channel(uint32_t current, uint32_t pattern_type, 
                                uint32_t start, uint32_t end, uint32_t iteration) {
    switch(pattern_type) {
        case 0: // Sequential
            return (current >= end) ? start : current + 1;
            
        case 1: // Random
            return (rand() % (end - start + 1)) + start;
            
        case 2: // Fibonacci
            return get_fibonacci_channel(iteration, start, end);
            
        default:
            return current;
    }
}

// Basic command parsing function
int cmd_parse(char* cmd) {
    // Implement basic command parsing logic
    if (strcmp(cmd, "stop") == 0) {
        return 1;  // Stop command
    }
    return 0;
}

// Stub for UartGetCmdLn to satisfy linker
int UartGetCmdLn(char* cmd, int max_len) {
    // In a real implementation, this would read from UART
    return 0;
}

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



void app_main(void)
{
    uint32_t seconds = 0;
    
    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Give USB device time to settle
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Initialize USB CDC
    //esp_vfs_dev_uart_port_set_rx_line_endings(0, ESP_LINE_ENDINGS_CR);
    //esp_vfs_dev_uart_port_set_tx_line_endings(0, ESP_LINE_ENDINGS_CRLF);
    //usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
    //    .rx_buffer_size = 2048,
    //    .tx_buffer_size = 2048,
    //};
    //ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
    //esp_vfs_usb_serial_jtag_use_driver();
    
    // Configure stdin/stdout for non-blocking operation
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    fcntl(fileno(stdin), F_SETFL, O_NONBLOCK);
    
    ESP_LOGI("app_main", "Starting multi-channel RF carrier wave test");
    
    // Power up the WiFi domain
    esp_wifi_power_domain_on();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Configure for RF test mode
    esp_phy_rftest_config(1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Initialize RF
    esp_phy_rftest_init();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI("app_main", "Starting RF transmission with parameters:");
    ESP_LOGI("app_main", "  - Channel Range: %" PRIu32 " to %" PRIu32,
             default_config.start_channel, default_config.end_channel);
    ESP_LOGI("app_main", "  - Pattern Type: %s",
              default_config.pattern_type == 0 ? "Sequential" :
            default_config.pattern_type == 1 ? "Random" : "Fibonacci");
    ESP_LOGI("app_main", "  - Mode: Combined (Carrier Wave + Packet Burst)");
    ESP_LOGI("app_main", "  - Power Level: -%0.1fdB (backoff=%d)",
            default_config.backoff * 0.25, default_config.backoff);
    
    // Transmit on channel 6
    // Set PHY module to only show warnings and errors
    esp_log_level_set("phy", ESP_LOG_WARN);
    
    // Initialize variables for channel hopping
    uint32_t current_channel = default_config.start_channel;
    uint32_t iteration = 0;
    
    while(1) {
        // Check for keyboard input
        int ch = usb_serial_jtag_read_char();
        if (ch != -1) {
            //ESP_LOGI("INPUT", "Received character: %d", ch);  // Add debug output
            if (ch == '=') {
                // Decrease backoff (increase power) by 1dB steps
                if (default_config.backoff >= 4) {
                    default_config.backoff -= 4;
                    ESP_LOGI("BACKOFF", "Decreased: backoff=%d - Increased signal to -%0.1fdB)",
                            default_config.backoff, default_config.backoff * 0.25);
                }
            }
            else if (ch == '-') {
                // Increase backoff (decrease power) by 1dB steps
                if (default_config.backoff <= 240) {  // stop at -60db
                    default_config.backoff += 4;
                    ESP_LOGI("BACKOFF", "Increased: backoff=%d - Decreased signal to -%0.1fdB)", 
                            default_config.backoff, default_config.backoff * 0.25);
                }
            }
        }

        // Start with carrier wave
        esp_phy_wifi_tx_tone(1, current_channel, default_config.backoff);
        vTaskDelay(pdMS_TO_TICKS(default_config.dwell_time / 2)); // Half dwell time
        
        // Stop carrier wave
        esp_phy_wifi_tx_tone(0, current_channel, default_config.backoff);
        
        // Immediately follow with packet burst
        esp_phy_wifi_tx(current_channel,
                       default_config.rate,
                       default_config.backoff,
                       default_config.length_byte,
                       default_config.packet_delay,
                       default_config.packet_num);
        
        // Wait remaining dwell time
        vTaskDelay(pdMS_TO_TICKS(default_config.dwell_time / 2));
        
        // Get next channel using pattern
        current_channel = get_next_channel(current_channel, 
                                         default_config.pattern_type,
                                         default_config.start_channel,
                                         default_config.end_channel,
                                         iteration++);
        
        // Short delay between transmission cycles  
        vTaskDelay(pdMS_TO_TICKS(2));  // HERE
        
        seconds++;
        // Only log every 60 seconds
        if (seconds % 60 == 0) {
            ESP_LOGI("app_main",
                     "RF scrambling active for %" PRIu32 " minutes across channels %" PRIu32 "-%" PRIu32 " (power: -%0.1fdB)",
                     seconds / 60,
                     default_config.start_channel,
                     default_config.end_channel,
                     default_config.backoff * 0.25);
        }
    }
}
