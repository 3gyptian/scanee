RF Calibration
[中文]

ESP32 supports three RF calibration methods during RF initialization:

Partial calibration

Full calibration

No calibration

Partial Calibration
During RF initialization, the partial calibration method is used by default for RF calibration. It is done based on the full calibration data which is stored in the NVS. To use this method, please go to menuconfig and enable CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE.

Full Calibration
Full calibration is triggered in the following conditions:

NVS does not exist.

The NVS partition to store calibration data has been erased.

Hardware MAC address has changed.

PHY library version has changed.

The RF calibration data loaded from the NVS partition is broken.

Full calibration takes 100 ms longer than the partial calibration method. If boot duration is not of critical importance to the application, the full calibration method is recommended. To switch to the full calibration method, go to menuconfig and disable CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE. If you use the default method of RF calibration, there are two ways to add the function of triggering full calibration as a last-resort remedy.

Erase the NVS partition if you do not mind all of the data stored in the NVS partition is erased. That is indeed the easiest way.

Call API esp_phy_erase_cal_data_in_nvs() before initializing Wi-Fi and Bluetooth®/Bluetooth Low Energy based on some conditions (e.g., an option provided in some diagnostic mode). In this case, only the PHY namespace of the NVS partition is erased.

No Calibration
The no calibration method is only used when the device wakes up from Deep-sleep mode.

PHY Initialization Data
The PHY initialization data is used for RF calibration. There are two ways to get the PHY initialization data.

One is to use the default initialization data which is located in the header file components/esp_phy/esp32/include/phy_init_data.h. It is embedded into the application binary after compiling and then stored into read-only memory (DROM). To use the default initialization data, please go to menuconfig and disable CONFIG_ESP_PHY_INIT_DATA_IN_PARTITION.

An alternative is to store the initialization data in a PHY data partition. A PHY data partition is included in the default partition table. However, when using a custom partition table, please ensure that a PHY data partition (type: data, subtype: phy) is included in the custom partition table. Whether you are using a custom partition table or the default partition table, if initialization data is stored in a partition, it has to be flashed there, otherwise a runtime error occurs. If you want to use initialization data stored in a partition, go to menuconfig and enable the option CONFIG_ESP_PHY_INIT_DATA_IN_PARTITION.

API Reference
Header File
components/esp_phy/include/esp_phy_init.h

This header file can be included with:

#include "esp_phy_init.h"
This header file is a part of the API provided by the esp_phy component. To declare that your component depends on esp_phy, add the following to your CMakeLists.txt:

REQUIRES esp_phy
or

PRIV_REQUIRES esp_phy
Functions
const esp_phy_init_data_t *esp_phy_get_init_data(void)
Get PHY init data.

If "Use a partition to store PHY init data" option is set in menuconfig, This function will load PHY init data from a partition. Otherwise, PHY init data will be compiled into the application itself, and this function will return a pointer to PHY init data located in read-only memory (DROM).

If "Use a partition to store PHY init data" option is enabled, this function may return NULL if the data loaded from flash is not valid.

Note

Call esp_phy_release_init_data to release the pointer obtained using this function after the call to esp_wifi_init.

Returns
pointer to PHY init data structure

void esp_phy_release_init_data(const esp_phy_init_data_t *data)
Release PHY init data.

Parameters
data -- pointer to PHY init data structure obtained from esp_phy_get_init_data function

esp_err_t esp_phy_load_cal_data_from_nvs(esp_phy_calibration_data_t *out_cal_data)
Function called by esp_phy_load_cal_and_init to load PHY calibration data.

This is a convenience function which can be used to load PHY calibration data from NVS. Data can be stored to NVS using esp_phy_store_cal_data_to_nvs function.

If calibration data is not present in the NVS, or data is not valid (was obtained for a chip with a different MAC address, or obtained for a different version of software), this function will return an error.

Parameters
out_cal_data -- pointer to calibration data structure to be filled with loaded data.

Returns
ESP_OK on success

esp_err_t esp_phy_store_cal_data_to_nvs(const esp_phy_calibration_data_t *cal_data)
Function called by esp_phy_load_cal_and_init to store PHY calibration data.

This is a convenience function which can be used to store PHY calibration data to the NVS. Calibration data is returned by esp_phy_load_cal_and_init function. Data saved using this function to the NVS can later be loaded using esp_phy_store_cal_data_to_nvs function.

Parameters
cal_data -- pointer to calibration data which has to be saved.

Returns
ESP_OK on success

esp_err_t esp_phy_erase_cal_data_in_nvs(void)
Erase PHY calibration data which is stored in the NVS.

This is a function which can be used to trigger full calibration as a last-resort remedy if partial calibration is used. It can be called in the application based on some conditions (e.g. an option provided in some diagnostic mode).

Returns
ESP_OK on success

Returns
others on fail. Please refer to NVS API return value error number.

void esp_phy_enable(esp_phy_modem_t modem)
Enable PHY and RF module.

PHY and RF module should be enabled in order to use WiFi or BT. Now PHY and RF enabling job is done automatically when start WiFi or BT. Users should not call this API in their application.

Parameters
modem -- the modem to call the phy enable.

void esp_phy_disable(esp_phy_modem_t modem)
Disable PHY and RF module.

PHY module should be disabled in order to shutdown WiFi or BT. Now PHY and RF disabling job is done automatically when stop WiFi or BT. Users should not call this API in their application.

Parameters
modem -- the modem to call the phy disable.

void esp_btbb_enable(void)
Enable BTBB module.

BTBB module should be enabled in order to use IEEE802154 or BT. Now BTBB enabling job is done automatically when start IEEE802154 or BT. Users should not call this API in their application.

void esp_btbb_disable(void)
Disable BTBB module.

Dsiable BTBB module, used by IEEE802154 or Bluetooth. Users should not call this API in their application.

void esp_phy_load_cal_and_init(void)
Load calibration data from NVS and initialize PHY and RF module.

void esp_phy_modem_init(void)
Initialize backup memory for Phy power up/down.

void esp_phy_modem_deinit(void)
Deinitialize backup memory for Phy power up/down Set phy_init_flag if all modems deinit on ESP32C3.

void esp_phy_common_clock_enable(void)
Enable WiFi/BT common clock.

void esp_phy_common_clock_disable(void)
Disable WiFi/BT common clock.

int64_t esp_phy_rf_get_on_ts(void)
Get the time stamp when PHY/RF was switched on.

Returns
return 0 if PHY/RF is never switched on. Otherwise return time in microsecond since boot when phy/rf was last switched on

esp_err_t esp_phy_update_country_info(const char *country)
Update the corresponding PHY init type according to the country code of Wi-Fi.

Parameters
country -- country code

Returns
ESP_OK on success.

Returns
esp_err_t code describing the error on fail

char *get_phy_version_str(void)
Get PHY lib version.

Returns
PHY lib version.

void phy_init_param_set(uint8_t param)
Set PHY init parameters.

Parameters
param -- is 1 means combo module

void phy_wifi_enable_set(uint8_t enable)
Wi-Fi RX enable.

Parameters
enable -- True for enable wifi receiving mode as default, false for closing wifi receiving mode as default.

Structures
struct esp_phy_init_data_t
Structure holding PHY init parameters.

Public Members

uint8_t params[128]
opaque PHY initialization parameters

struct esp_phy_calibration_data_t
Opaque PHY calibration data.

Public Members

uint8_t version[4]
PHY version

uint8_t mac[6]
The MAC address of the station

uint8_t opaque[1894]
calibration data

Enumerations
enum esp_phy_modem_t
PHY enable or disable modem.

Values:

enumerator PHY_MODEM_WIFI
PHY modem WIFI

enumerator PHY_MODEM_BT
PHY modem BT

enumerator PHY_MODEM_IEEE802154
PHY modem IEEE802154

enum esp_phy_calibration_mode_t
PHY calibration mode.

Values:

enumerator PHY_RF_CAL_PARTIAL
Do part of RF calibration. This should be used after power-on reset.

enumerator PHY_RF_CAL_NONE
Don't do any RF calibration. This mode is only suggested to be used after deep sleep reset.

enumerator PHY_RF_CAL_FULL
Do full RF calibration. Produces best results, but also consumes a lot of time and current. Suggested to be used once.

Header File
components/esp_phy/include/esp_phy_cert_test.h

This header file can be included with:

#include "esp_phy_cert_test.h"
This header file is a part of the API provided by the esp_phy component. To declare that your component depends on esp_phy, add the following to your CMakeLists.txt:

REQUIRES esp_phy
or

PRIV_REQUIRES esp_phy
Functions
void esp_wifi_power_domain_on(void)
Wifi power domain power on.

void esp_wifi_power_domain_off(void)
Wifi power domain power off.

void esp_phy_rftest_config(uint8_t conf)
Environment variable configuration.

Parameters
conf -- Set to 1 to enter RF test mode.

void esp_phy_rftest_init(void)
RF initialization configuration.

void esp_phy_tx_contin_en(bool contin_en)
TX Continuous mode.

Parameters
contin_en -- Set to true for continuous packet sending, which can be used for certification testing; Set to false to cancel continuous mode, which is the default mode and can be used for WLAN tester.

void esp_phy_cbw40m_en(bool en)
HT40/HT20 mode selection.

Parameters
en -- Set to false to enter 11n HT20 mode; Set to true to enter 11n HT40 mode;

void esp_phy_wifi_tx(uint32_t chan, esp_phy_wifi_rate_t rate, int8_t backoff, uint32_t length_byte, uint32_t packet_delay, uint32_t packet_num)
Wi-Fi TX command.

Parameters
chan -- channel setting, 1~14;

rate -- rate setting;

backoff -- Transmit power attenuation, unit is 0.25dB. For example, 4 means that the power is attenuated by 1dB;

length_byte -- TX packet length configuration, indicating PSDU Length, unit is byte;

packet_delay -- TX packet interval configuration, unit is us;

packet_num -- The number of packets sent, 0 means sending packets continuously, other values represent the number of packets to send.

void esp_phy_test_start_stop(uint8_t value)
Test start/stop command, used to stop transmitting or reciving state.

Parameters
value -- Value should be set to 3 before TX/RX. Set value to 0 to end TX/RX state.

void esp_phy_wifi_rx(uint32_t chan, esp_phy_wifi_rate_t rate)
Wi-Fi RX command.

Parameters
chan -- channel setting, 1~14;

rate -- rate setting;

void esp_phy_wifi_tx_tone(uint32_t start, uint32_t chan, uint32_t backoff)
Wi-Fi Carrier Wave(CW) TX command.

Parameters
start -- enable CW, 1 means transmit, 0 means stop transmitting;

chan -- CW channel setting, 1~14;

backoff -- CW power attenuation parameter, unit is 0.25dB. 4 indicates the power is attenuated by 1dB.

void esp_phy_ble_tx(uint32_t txpwr, uint32_t chan, uint32_t len, esp_phy_ble_type_t data_type, uint32_t syncw, esp_phy_ble_rate_t rate, uint32_t tx_num_in)
BLE TX command.

Parameters
txpwr -- Transmit power level. Tx power is about (level-8)*3 dBm, step is 3dB. Level 8 is around 0 dBm;

chan -- channel setting, range is 0~39, corresponding frequency = 2402+chan*2;

len -- Payload length setting, range is 0-255, unit is byte, 37 bytes is employed generally;

data_type -- Data type setting;

syncw -- Packet identification (need to be provided by the packet generator or instrument manufacturer), 0x71764129 is employed generally;

rate -- rate setting;

tx_num_in -- The number of packets sent, 0 means sending packets continuously, other values represent the number of packets to send.

void esp_phy_ble_rx(uint32_t chan, uint32_t syncw, esp_phy_ble_rate_t rate)
BLE RX command.

Parameters
chan -- channel selection, range is 0-39; Channels 0, 1, 2~10 correspond to 2404MHz, 2406MHz, 2408MHz~2424MHz respectively; Channels 11, 12, 13~36 correspond to 2428MHz, 2430MHz, 2432MHz~2478MHz respectively; Channel 37: 2402MHz, Channel 38: 2426MHz, Channel 39: 2480MHz;

syncw -- Packet identification (need to be provided by the packet generator or instrument manufacturer), 0x71764129 is employed generally;

rate -- rate setting;

void esp_phy_bt_tx_tone(uint32_t start, uint32_t chan, uint32_t power)
BLE Carrier Wave(CW) TX command.

Parameters
start -- enable CW, 1 means transmit, 0 means stop transmitting;

chan -- Single carrier transmission channel selection, range is 0~39, corresponding frequency freq = 2402+chan*2;

power -- CW power attenuation parameter, unit is 0.25dB. 4 indicates the power is attenuated by 1dB.

void esp_phy_get_rx_result(esp_phy_rx_result_t *rx_result)
Get some RX information.

Parameters
rx_result -- This struct for storing RX information;

Structures
struct esp_phy_rx_result_t
Structure holding PHY RX result.

Public Members

uint32_t phy_rx_correct_count
The number of desired packets received

int phy_rx_rssi
Average RSSI of desired packets

uint32_t phy_rx_total_count
The number of total packets received

uint32_t phy_rx_result_flag
0 means no RX info; 1 means the lastest Wi-Fi RX info; 2 means the lastest BLE RX info.

Enumerations
enum esp_phy_wifi_rate_t
Values:

enumerator PHY_RATE_1M
enumerator PHY_RATE_2M
enumerator PHY_RATE_5M5
enumerator PHY_RATE_11M
enumerator PHY_RATE_6M
enumerator PHY_RATE_9M
enumerator PHY_RATE_12M
enumerator PHY_RATE_18M
enumerator PHY_RATE_24M
enumerator PHY_RATE_36M
enumerator PHY_RATE_48M
enumerator PHY_RATE_54M
enumerator PHY_RATE_MCS0
enumerator PHY_RATE_MCS1
enumerator PHY_RATE_MCS2
enumerator PHY_RATE_MCS3
enumerator PHY_RATE_MCS4
enumerator PHY_RATE_MCS5
enumerator PHY_RATE_MCS6
enumerator PHY_RATE_MCS7
enumerator PHY_WIFI_RATE_MAX
enum esp_phy_ble_rate_t
Values:

enumerator PHY_BLE_RATE_1M
enumerator PHY_BLE_RATE_2M
enumerator PHY_BLE_RATE_125K
enumerator PHY_BLE_RATE_500k
enumerator PHY_BLE_RATE_MAX
enum esp_phy_ble_type_t
Values:

enumerator PHY_BLE_TYPE_1010
enumerator PHY_BLE_TYPE_00001111
enumerator PHY_BLE_TYPE_prbs9
enumerator PHY_BLE_TYPE_00111100
enumerator PHY_BLE_TYPE_MAX