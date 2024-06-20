#ifndef WIFI_CONFIG_H_
#define WIFI_CONFIG_H_

#include <stdbool.h>
#include "esp_wifi.h"

bool wifi_init_sta(void);
void wifi_init_softap(void);
bool wifi_ap_mode_active(void);
bool is_wifi_sta_connected(void);
esp_err_t nvs_wifi_set_credentials(const char *ssid, const char *password);
esp_err_t nvs_wifi_get_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len);
void nvs_wifi_set_mode(int mode);
int nvs_wifi_get_mode(void);
esp_err_t wifi_get_mac(uint8_t *mac);

#endif /* WIFI_CONFIG_H_ */
