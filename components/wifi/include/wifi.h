#ifndef WIFI_H_
#define WIFI_H_

#include <stdbool.h>
#include "esp_wifi.h"

bool wifi_init_sta(void);
void wifi_stop_sta(void);
bool wifi_sta_connected(void);
void wifi_init_softap(void);
esp_err_t wifi_set_credentials(const char *ssid, const char *password);
esp_err_t wifi_get_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len);


#endif /* WIFI_H_ */
