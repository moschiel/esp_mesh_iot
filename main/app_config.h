#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#include <stdbool.h>
#include "esp_wifi.h"

#define MAX_SSID_LEN 32  // Comprimento máximo do SSID
#define MAX_PASS_LEN 64  // Comprimento máximo da senha

typedef enum
{
    APP_MODE_WIFI_AP_WEBSERVER,
    APP_MODE_WIFI_MESH_NETWORK
} APP_MODE_t;

esp_err_t nvs_set_wifi_credentials(const char *ssid, const char *password);
esp_err_t nvs_get_wifi_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len);
void nvs_set_app_mode(APP_MODE_t mode);
APP_MODE_t nvs_get_app_mode(void);

#endif /* APP_CONFIG_H_ */
