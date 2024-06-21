#ifndef MAIN_APP_MODES_H_
#define MAIN_APP_MODES_H_

#include <stdbool.h>

typedef enum
{
    APP_MODE_WIFI_AP_WEB_SERVER,
    APP_MODE_WIFI_STA,
    APP_MODE_WIFI_MESH_NETWORK
} APP_MODE_t;

bool wifi_ap_mode_active(void);
bool is_wifi_sta_connected(void);
void init_app_mode(void);

#endif /* MAIN_APP_MODES_H_ */
