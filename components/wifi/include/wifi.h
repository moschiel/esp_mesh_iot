#ifndef WIFI_H_
#define WIFI_H_

#include <stdbool.h>
#include "esp_wifi.h"

bool wifi_init_sta(void);
void wifi_stop(void);
void wifi_init_softap(void);
bool wifi_sta_connected(void);
bool wifi_ap_mode_active(void);
esp_err_t nvs_wifi_set_credentials(const char *ssid, const char *password);
esp_err_t nvs_wifi_get_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len);
void nvs_wifi_set_mode(int mode, bool restart_esp);
int nvs_wifi_get_mode(void);
esp_err_t wifi_get_mac(uint8_t *mac);

#endif /* WIFI_H_ */
