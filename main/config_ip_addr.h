#ifndef MAIN_CONFIG_IP_ADDR_H_
#define MAIN_CONFIG_IP_ADDR_H_

#include "esp_wifi.h"

void set_static_ip(esp_netif_t *netif);
void initialise_mdns();

#endif /* MAIN_CONFIG_IP_ADDR_H_ */
